import os
import boto3
import streamlit as st
import pandas as pd
import numpy as np
from dotenv import load_dotenv
import altair as alt

MAC_ADDRESS = "E8:DB:84:99:37:2F"

@st.cache
def load_data(mac_address):
    load_dotenv()
    rds_data = boto3.client('rds-data')
    
    read_statement = "SELECT time, watts FROM {schema_name}.{table_name} WHERE mac_address=\'{mac}\'ORDER BY time DESC LIMIT 50".format(
        schema_name=os.environ['SCHEMA_NAME'],
        table_name="metrics",
        mac=mac_address
    )
    
    # TODO: Create another secret that authenticates the web app in a different way than Lambda
    aws_results = rds_data.execute_statement(
        database=os.environ['DB_NAME'],
        resourceArn=os.environ['RESOURCE_ARN'],
        secretArn=os.environ['SECRET_ARN'],
        sql=read_statement
    )
    result = {}
    result['time'] = [s[0]['stringValue'] for s in aws_results['records']]
    result['watts'] = [int(s[1]['stringValue']) for s in aws_results['records']]
    table = pd.DataFrame.from_dict(result)
    return table

st.title('Power Readings')
mac_address = st.text_input('What is your device name?') 
data_load_state = st.text('Loading data...')
data = load_data(mac_address)
data_load_state.text("")


my_chart = alt.Chart(data).mark_line().encode(
    x='time',
    y='watts'
)

st.altair_chart(my_chart, use_container_width=True)



