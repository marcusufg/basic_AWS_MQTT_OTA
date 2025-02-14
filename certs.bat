@echo off
IF EXIST temp.txt DEL /F temp.txt

echo { > temp.txt
echo "certificate": ^">> temp.txt
python truncar_last.py
python truncar_last.py
@REM type ..\certificados\%1\certificate.pem.crt >> temp.txt
aws s3 cp s3://bucketreceivecertificates/%1/certificate.pem.crt ./
type certificate.pem.crt >> temp.txt
IF EXIST certificate.pem.crt DEL /F certificate.pem.crt
python truncar_last.py
echo ^", >> temp.txt

echo "private_key": ^">> temp.txt
python truncar_last.py
python truncar_last.py
@rem type ..\certificados\%1\private.pem.key >> temp.txt
aws s3 cp s3://bucketreceivecertificates/%1/private.pem.key ./
type private.pem.key >> temp.txt
IF EXIST private.pem.key DEL /F private.pem.key
python truncar_last.py
echo ^", >> temp.txt

echo "root_CA": ^">> temp.txt
python truncar_last.py
python truncar_last.py
@rem type ..\certificados\AmazonRootCA1.pem >> temp.txt
@REM curl -o ./AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
aws s3 cp s3://bucketreceivecertificates/AmazonRootCA1.pem ./
@REM copy certs/AmazonRootCA1.pem ./AmazonRootCA1.pem CONFERIR
type AmazonRootCA1.pem >> temp.txt
IF EXIST AmazonRootCA1.pem DEL /F AmazonRootCA1.pem
@REM python truncar_last.py
echo ^" >> temp.txt
echo } >> temp.txt
python truncar_last.py

aws iot-data publish --topic S/%1/certs_AWS --cli-binary-format raw-in-base64-out  --payload file://temp.txt
IF EXIST temp.txt DEL /F temp.txt