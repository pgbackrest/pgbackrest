set -o vi
et -o vi
pgbackrest/doc/doc.pl --out=html  --pre --no-exe
pgbackrest/doc/doc.pl --out=html  --pre --var=os-type=rhel
pgbackrest/doc/doc.pl --out=html  --pre 
pgbackrest/test/test.pl  --module=command --test=info --vm=u20
pgbackrest/test/test.pl  --module=command --vm=u20 # --test=info
pgbackrest/test/test.pl  --module=common --vm=u20 # --test=info
pgbackrest/test/test.pl  --module=common #--vm=u20 # --test=info
pgbackrest/test/test.pl  --module=command #--vm=u20 # --test=info
pgbackrest/test/test.pl  --module=common #--vm=u20 # --test=info
pgbackrest/test/test.pl #  --module=common #--vm=u20 # --test=info
exit
set -o vi
pgbackrest/test/test.pl #  --module=common --vm=u20  --test=common
pgbackrest/test/test.pl   --module=common --vm=u20  --test=common
pgbackrest/test/test.pl   --module=common --vm=u20  #--test=common
pgbackrest/test/test.pl   --module=build --vm=u20  --test=render
pgbackrest/test/test.pl   --module=build --vm=u20  #--test=render
git status
pgbackrest/test/test.pl   --module=common --vm=u20  --test=lock
pgbackrest/test/test.pl   --module=common --test=lock #--vm=u20
git status
pgbackrest/test/test.pl   --module=command --test=control #--vm=u20
pgbackrest/test/test.pl   --module=command #--test=control #--vm=u20
pgbackrest/test/test.pl   --module=common #--test=control #--vm=u20
pgbackrest/test/test.pl   --module=common --test=exit --vm=u20
exit
set -o v
set -o vi
pgbackrest/doc/doc.pl --out=html --include=user-guide --pre 
html/user-guide-index.html
pgbackrest/doc/doc.pl --out=html
exit
set -o vi
pgbackrest/test/test.pl   --module=command --vm=u20
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/verify --pre 
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/backup --pre 
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/replication --pre 
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker ps
docker exec -it -u pgbackrest doc-repository /bin/bash
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo backup # --output=text --verbose=y
docker exec -it -u pgbackrest doc-repository /bin/bash
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository /bin/bash
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository /bin/bash
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache --help
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
exit
set -o vi
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker ps
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker ps
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker ps
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/test/test.pl   --module=command --vm=u20 #--test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker ps
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker ps
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
exit
docker ps
set -o vi
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
docker ps
cd /var/lib/pg
pgbackrest/doc/doc.pl --out=html --include=user-guide --pre --no-cache 
docker ps
history |grep repo-host
history |grep doc.pl
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker exec -it -u pgbackrest doc-repository /bin/bash
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
#pgbackrest/doc/doc.pl --out=html --include=user-guide --pre --no-cache 
history |grep doc.pl
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache
docker ps
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/repo-host/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker exec -it -u pgbackrest doc-repository /bin/bash
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/repo-host/perform-backup --pre --no-cache
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker exec -it -u pgbackrest doc-repository pgbackrest mv /var/lib/pgbackrest/backup/demo/pg_data/pg_control* /tmp
docker exec -it -u pgbackrest doc-repository mv /var/lib/pgbackrest/backup/demo/pg_data/pg_control* /tmp
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/pg_data/
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/latest/pg_data/
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/latest/pg_data/global
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/latest/pg_data/global/pg_control /tmp
docker exec -it -u pgbackrest doc-repository ls /var/lib/pgbackrest/backup/demo/latest/pg_data/global/pg_control.gz /tmp
docker exec -it -u pgbackrest doc-repository mv /var/lib/pgbackrest/backup/demo/latest/pg_data/global/pg_control.gz /tmp
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y --log-level-console=info
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
docker exec -it -u pgbackrest doc-repository pgbackrest --stanza=demo verify --output=text --verbose=y --log-level-console=info
pgbackrest/test/test.pl   --module=command --vm=u20 --test=verify
pgbackrest/test/test.pl   --module=command  --test=verify
pgbackrest/test/test.pl   --module=command  
pgbackrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/test/test.pl --vm=none --build-only
test/bin/none/pgbackrest help verify
pgbackrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/test/test.pl --vm=none --build-only
test/bin/none/pgbackrest help verify
pgbackrest/doc/doc.pl --out=html --no-exe
exit
set -o vi
pgbackrest/test/test.pl --module=command --vm-out
pgbackrest/test/test.pl --module=mock --test=real
pgbackrest/test/test.pl --module=mock 
pgbackrest/test/test.pl --module=mock --vm-u20
pgbackrest/test/test.pl --module=mock --vm=u20
pgbackrest/test/test.pl  --vm=u20
pgbackrest/test/test.pl  --vm=u20  --module=real  --test=all --run=1 --pg-version=15
exit
set -o vi
pgbackrest/test/test.pl  --vm=u20  --module=real  --test=all --run=1 --pg-version=15
pgbackrest/test/test.pl   --module=command  --test=verify
pgbackrest/test/test.pl   --module=command  --test=verify --vm=rh7
pgbackrest/test/test.pl   --module=command  --test=verify --vm=u20
pgbackrest/test/test.pl   --module=command  --test=verify --vm=d9
pgbackrest/test/test.pl   --module=command  --test=verify 
pgbackrest/test/test.pl   --module=command  --test=verify  -vm=rh7u
pgbackrest/test/test.pl   --module=command  --test=verify  -vm=rh7
pgbackrest/test/test.pl   --module=command  --test=verify  -vm=u20
pgbackrest/test/test.pl   --module=command  --test=verify  -vm=d9
pgbackrest/test/test.pl --module=mock --vm=u20
pgbackrest/test/test.pl   --module=command  --test=verify  -vm=u20
git log --name-only
git log
exit
set -o vi
exit
mark
set -o vi
history
history|grep doc
pgbackrest/doc/doc.pl --out=html --no-exe
which pgbackrest
docker ps
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/repo-host/perform-backup --pre --no-cache
docker ps
/bash
docker exec -it -u pgbackrest doc-repository /bin/bash
docker exec -it -u postgres doc-pg-primary /bin/bash
docker exec -it -u pgbackrest doc-repository /bin/bash
docker exec -it -u postgres doc-pg-primary /bin/bash
exit
set -o vi
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/repo-host/perform-backup --pre --no-cache
/test
pgbackrest/test/test.pl   --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --help --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --log-level-test=info --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --log-level-test=warn --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --log-level-test=info --vm-out --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --log-level-test=warn --vm-out --module=config  --test=parse  -vm=u20
pgbackrest/test/test.pl   --log-level-test=info --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/test/test.pl   --log-level-test=error --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/test/test.pl   --log-level-test=detail --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/test/test.pl   --log-level-test=detail --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=config  --test=parse  -vm=u20 --run=5
pgbackrest/doc/doc.pl --out=html --no-exe
exit
set -o vi
pgbackrest/doc/doc.pl --out=html --no-exe
exit
which gcc
vi a.c
gcc a.c
vi a.c
gcc a.c
vi a.c
gcc a.c
vi a.c
gcc a.c
man write
man 2 write
vi a.c
gcc a.c
./a.out 
vi a.c
gcc a.c
./a.out 
gcc a.c
vi a.c
gcc a.c
vi a.c
exit
set -o vi
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --test=expire  -vm=u20 
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --test=expire  -vm=u20  | tee -a /tmp/out
rg '!!!' /tmp/out
grep '!!!' /tmp/out
grep 'ERRNO' /tmp/out
vi /tmp/out
pgbackrest/doc/doc.pl --out=html --no-exe
'ckrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/test/test.pl --module=command  --vm-out
pgbackrest/test/test.pl --module=command  
pgbackrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/test/test.pl --vm=none --build-only
test/bin/none/pgbackrest help backup repo-type
test/bin/none/pgbackrest help restore
test/bin/none/pgbackrest help restore buffer-size 
test/bin/none/pgbackrest help backup buffer-size 
test/bin/none/pgbackrest help backup 
test/bin/none/pgbackrest help backup --repo-bundle-size
test/bin/none/pgbackrest help backup repo-bundle-size
test/bin/none/pgbackrest help backup repo-bundle-limit
test/bin/none/pgbackrest help backup manifest-save-threshold
pgbackrest/doc/doc.pl --out=html --no-exe
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart --var=encrypt=n --no-cache --pre
'ckrest/test/test.pl --vm=u20
pgbackrest/test/test.pl --vm=u20
pgbackrest/test/test.pl --vm=d9
pgbackrest/test/test.pl --vm=rh7
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --test=expire  -vm=u20  | tee  /tmp/out
vi /tmp/out
grep RMDIR /tmp/out
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --test=expire  -vm=u20  | tee  /tmp/out
grep RMDIR /tmp/out
ls -rlth /tmp
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --vm=u20 | tee  /tmp/out
ls -rlt /tmp
ls -hrlt /tmp
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache --help
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
pgbackrest/doc/doc.pl --out=html  --pre --no-cache 
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart/perform-backup --pre --no-cache 
pgbackrest/doc/doc.pl --out=html  --pre --no-cache 
s
history|grep help
pgbackrest/test/test.pl --module=command --test=help --vm-out
pgbackrest/doc/doc.pl --out=html  --pre --no-cache 
pgbackrest/test/test.pl   --log-level-test=trace --vm-out --module=command  --test=expire  -vm=u20  | tee  /tmp/out
grep '!!!' /tmp/out
grep '!!!' /tmp/out |grep TYPE
grep '!!!' /tmp/out
set -o vi
pgbackrest/test/test.pl --module=command --test=backup --vm-out
pgbackrest/test/test.pl --module=command #--test=backup --vm-out
pgbackrest/test/test.pl --module=command --test=backup --vm-out --log-level-test=debug |grep jrt
pgbackrest/test/test.pl --module=command --test=backup --vm-out --log-level-test=debug | tee /tmp/out
vi /tmp/put
vi /tmp/out
pgbackrest/test/test.pl --module=command --test=backup --vm-out --log-level-test=debug | tee /tmp/out
pgbackrest/test/test.pl --module=command --test=backup --vm-out --log-level-test=debug --vm=u20 | tee /tmp/out
grep jrt /tmp/out 
pgbackrest/test/test.pl --module=command --test=backup --vm-out --log-level-test=debug --vm=u20 | tee /tmp/out
grep jrt /tmp/out 
exit
set -o vi
pgbackrest/test/test.pl --module=command --test=backup #--vm-out
exit
