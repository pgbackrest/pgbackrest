# pgBackRest Documentation Certificates

The certificates in this directory are used for documentation generation only and should not be used for actual services.

## pgBackRest CA

Generate a CA that will be used to sign documentation certificates.  It can be installed in the documentation containers to make certificates signed by it valid.

```
cd [pgbackrest-root]/doc/resource/fake-cert

openssl ecparam -genkey -name prime256v1 | openssl ec -out ca.key
openssl req -new -x509 -extensions v3_ca -key ca.key -out ca.crt -days 99999 \
    -subj "/C=US/ST=All/L=All/O=pgBackRest/CN=pgbackrest.org"
```

## S3 Certificate

Mimic an S3 certificate for the `us-east-1`/`us-east-2` region to generate S3 documentation.

```
cd [pgbackrest-root]/doc/resource/fake-cert

openssl ecparam -genkey -name prime256v1 | openssl ec -out s3-server.key
openssl req -new -sha256 -nodes -out s3-server.csr -key s3-server.key -config s3.cnf
openssl x509 -req -in s3-server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out s3-server.crt -days 99999 -extensions v3_req -extfile s3.cnf
```

## Azure Certificate

Mimic an Azure certificate for the `*.blob.core.windows.net` hosts to generate Azure documentation.

```
cd [pgbackrest-root]/doc/resource/fake-cert

openssl ecparam -genkey -name prime256v1 | openssl ec -out azure-server.key
openssl req -new -sha256 -nodes -out azure-server.csr -key azure-server.key -config azure.cnf
openssl x509 -req -in azure-server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out azure-server.crt -days 99999 -extensions v3_req -extfile azure.cnf
```
