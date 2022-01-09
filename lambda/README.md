## Intro to AWS Lambda ##
- See AWS [explanation](https://docs.aws.amazon.com/lambda/latest/dg/welcome.html).

## How does this function fit in to the stack? ##
- `lambda_handler()` is executed by our AWS API Gateway. 
- AWS API Gateway maintains a URL. When we "hit" that URL (either by viewing the URL in our browser or by making an HTTP request via the arduino) the API Gateway connects to our lambda function.
- We expect power usage data to be in lambda_handler's input (particularly in the `event` variable).