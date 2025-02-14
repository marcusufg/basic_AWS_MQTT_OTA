aws s3 cp s3://bucketreceivecertificates/$2/config.json ./ > /dev/null
aws iot-data publish --topic S/$1/config --cli-binary-format raw-in-base64-out  --payload file://./config.json > /dev/null
if [ "config.json" ]; then
  rm -f config.json > /dev/null
fi
