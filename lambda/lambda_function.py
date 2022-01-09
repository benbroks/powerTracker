import os
import json
import boto3
from datetime import datetime

def insert_data(entry):
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('PowerTrackerTable')
    entry["SampleTime"] = str(datetime.timestamp(datetime.now()))
    table.put_item(
        Item=entry
    )

def rds_test(entry):
    rds_data = boto3.client('rds-data')
    
    insert_statement = "INSERT INTO {schema_name}.{table_name} VALUES (\'{mac}\',TIMESTAMP \'{timestamp}\',{watts});".format(
        schema_name=os.environ['SCHEMA_NAME'],
        table_name="metrics",
        mac=entry["MacAddress"],
        timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        watts=entry["PowerValue"]
    )
    _ = rds_data.execute_statement(
        database=os.environ['DB_NAME'],
        resourceArn=os.environ['RESOURCE_ARN'],
        secretArn=os.environ['SECRET_ARN'],
        sql=insert_statement
    )

def lambda_handler(event, context):
    """
    We expect rawQueryString to be of the form...
    RandomInput=[INSERT_INPUT]&PowerValue=[INSERT_POWER_VALUE]
    """
    try:
        raw_query_string = event["rawQueryString"]
        pairs = raw_query_string.split('&')
        mac_pair = pairs[0].split('=')
        watt_pair = pairs[-1].split('=')
        entry = {}
        entry["MacAddress"] = mac_pair[1]
        entry["PowerValue"] = int(watt_pair[1])
    except Exception as e:
        return {
            'statusCode': 200,
            'body': json.dumps("Incorrect Formatting. Query String Can't Be Evaluated.")
        }
    try:
        rds_test(entry)
    except Exception as e:
        print(e)
        return {
            'statusCode': 200,
            'body': json.dumps("Item Dictionaries aren't properly formatted. Failure on upload.")
        }
    # Success!
    return {
        'statusCode': 200,
        'body': json.dumps("We have uploaded the data!")
    }