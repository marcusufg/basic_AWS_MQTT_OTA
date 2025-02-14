echo "updating device with "$1" certificates..."

# remote
aws s3 cp s3://bucketreceivecertificates/$1/certificate.pem.crt ./ > /dev/null
aws s3 cp s3://bucketreceivecertificates/$1/private.pem.key ./ > /dev/null
cp certs/AmazonRootCA1.pem ./ > /dev/null

# # local
# cp certs/factory_FW/certificate.pem.crt ./ > /dev/null
# cp certs/factory_FW/private.pem.key ./ > /dev/null
# cp certs/AmazonRootCA1.pem ./ > /dev/null

a=$(cat certificate.pem.crt)
b=$(cat private.pem.key)
c=$(cat AmazonRootCA1.pem)
json='{"certificate":"%s","private_key":"%s","root_CA":"%s"}\n'
printf "$json" "$a" "$b" "$c" > temp.txt

aws iot-data publish --topic S/$1/certs_AWS --cli-binary-format raw-in-base64-out  --payload file://temp.txt > /dev/null
rm -f temp.txt > /dev/null
rm -f certificate.pem.crt > /dev/null
rm -f private.pem.key > /dev/null
rm -f AmazonRootCA1.pem > /dev/null
echo "done."