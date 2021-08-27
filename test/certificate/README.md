# pgBackRest Test Certificates

The certificates in this directory are used for testing purposes only and are not used for actual services.  They are used only by the unit and integration tests and there should be no reason to modify them unless new tests are required.

## Generating the Test CA (pgbackrest-test-ca.crt/key)

This is a self-signed CA that is used to sign all server certificates.  No intermediate CAs will be generated since they are not needed for testing.

```
cd [pgbackrest-root]/test/certificate
openssl genrsa -out pgbackrest-test-ca.key 4096
openssl req -new -x509 -extensions v3_ca -key pgbackrest-test-ca.key -out pgbackrest-test-ca.crt -days 99999 \
    -subj "/C=US/ST=All/L=All/O=pgBackRest/CN=test.pgbackrest.org"
openssl x509 -in pgbackrest-test-ca.crt -text -noout
```

## Generating the Server Test Key (pgbackrest-test-server.key)

This key will be used for all server certificates to keep things simple.

```
cd [pgbackrest-root]/test/certificate
openssl genrsa -out pgbackrest-test-server.key 4096
```

## Generating the Server Test Certificate (pgbackrest-test-server.crt/key)

This certificate will be used in unit and integration tests.  It is expected to pass verification but won't be subjected to extensive testing.

```
cd [pgbackrest-root]/test/certificate
openssl req -new -sha256 -nodes -out pgbackrest-test-server.csr -key pgbackrest-test-server.key -config pgbackrest-test-server.cnf
openssl x509 -req -in pgbackrest-test-server.csr -CA pgbackrest-test-ca.crt -CAkey pgbackrest-test-ca.key -CAcreateserial \
    -out pgbackrest-test-server.crt -days 99999 -extensions v3_req -extfile pgbackrest-test-server.cnf
openssl x509 -in pgbackrest-test-server.crt -text -noout
```

## Generating the Server Test Certificate Revocation (pgbackrest-test-server.crl)

```
rm -f index.* && touch index.txt
openssl ca -config pgbackrest-test-server.cnf -keyfile pgbackrest-test-ca.key -cert pgbackrest-test-ca.crt \
    -revoke pgbackrest-test-server.crt
openssl ca -config pgbackrest-test-server.cnf -keyfile pgbackrest-test-ca.key -cert pgbackrest-test-ca.crt \
    -gencrl -out pgbackrest-test-server.crl
```

## Generating the Client Test Key (pgbackrest-test-client.key)

This key will be used for all client certificates to keep things simple.

```
cd [pgbackrest-root]/test/certificate
openssl genrsa -out pgbackrest-test-client.key 4096
```

## Generating the Client Test Certificate (pgbackrest-test-client.crt/key)

This certificate will be used in unit and integration tests. It is expected to pass verification but won't be subjected to extensive testing.

```
cd [pgbackrest-root]/test/certificate
openssl req -new -sha256 -nodes -out pgbackrest-test-client.csr -key pgbackrest-test-client.key -config pgbackrest-test-client.cnf
openssl x509 -req -in pgbackrest-test-client.csr -CA pgbackrest-test-ca.crt -CAkey pgbackrest-test-ca.key -CAcreateserial \
    -out pgbackrest-test-client.crt -days 99999 -extensions v3_req -extfile pgbackrest-test-client.cnf
openssl x509 -in pgbackrest-test-client.crt -text -noout
```

## Generating the Client Test Certificate Revocation (pgbackrest-test-client.crl)

```
rm -f index.* && touch index.txt
openssl ca -config pgbackrest-test-client.cnf -keyfile pgbackrest-test-ca.key -cert pgbackrest-test-ca.crt \
    -revoke pgbackrest-test-client.crt
openssl ca -config pgbackrest-test-client.cnf -keyfile pgbackrest-test-ca.key -cert pgbackrest-test-ca.crt \
    -gencrl -out pgbackrest-test-client.crl
```
