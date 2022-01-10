This includes components of the full stack for the power sensor.

High-Level Order of Execution:
- `arduino/power-sensor.ino` will execute on the power sensor microcontroller. This converts raw current sensor readings into watt values and makes an HTTP request to a URL.
- AWS API Gateway verifies the HTTP request and forwards the accompanying power usage data to the lambda function, executing `lambda_handler()` in `lambda/lambda_function.py`
- Data is uploaded to an [AWS Postgres DB instance](https://aws.amazon.com/rds/aurora/postgresql-features/). This will serve as the backbone for a website and [subsequent data analysis](https://www.youtube.com/watch?v=27axs9dO7AE).
- We view our data (currently on a local) website. This is currently implemented in Streamlit, which is conveniently implemented in Python.

To Note:
- DO NOT MODIFY `.gitignore`
- annoy @benbroks if something doesn't wokr. it's probably his fault