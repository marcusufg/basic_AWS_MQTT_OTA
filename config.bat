@echo off
IF EXIST config.json DEL /F config.json

aws s3 cp s3://bucketreceivecertificates/%2/config.json ./
aws iot-data publish --topic S/%1/config --cli-binary-format raw-in-base64-out  --payload file://.\config.json
IF EXIST config.json DEL /F config.json
