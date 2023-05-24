# pgBackRest File Bundling and Block Incremental Backup

Efficiently storing backups is a major priority for the pgBackRest project but this goal must be tempered by restore speed. Two recent features help in this area: file bundling and block incremental backup.

!!!SETUP
docker exec -it -u postgres doc-pg-primary bash
rm -rf /var/lib/pgbackrest/*
mkdir /var/lib/pgbackrest/1
mkdir /var/lib/pgbackrest/2
vi /etc/pgbackrest/pgbackrest.conf
!!!SETUP

To demonstrate these features we will create two repositories. The first repository will use defaults and the second will have file bundling and block incremental backup enabled.

Configure both repositories:
```
[global]
log-level-console=info
start-fast=y

repo1-path=/var/lib/pgbackrest/1
repo1-retention-full=2

repo2-path=/var/lib/pgbackrest/2
repo2-retention-full=2
repo2-bundle=y
repo2-block=y

[demo]
pg1-path=/var/lib/postgresql/12/demo
```

Create the stanza on both repositories:
```
$ pgbackrest --stanza=demo stanza-create
```

The block incremental backup feature is best demonstrated with a larger dataset. In particular, we would prefer to have at least one
file that is near the maximum segment size of 1GB. This can be accomplished with `pgbench`:
```
$ /usr/lib/postgresql/12/bin/pgbench -i -s 65
```

## File Bundling

File bundling stores files more efficiently in the repository by combining smaller files together. This results in fewer files overall in the backup which improves the speed of all repository operations, especially on object stores like S3, Azure, and GCS. There may also be cost savings on repositories that have a cost per operation since there will be fewer lists, deletes, etc.

To demonstrate this we'll make a backup on repo1, which does not have bundling enabled:
```
$ pgbackrest --stanza=demo --type=full --repo=1 backup
```

Now we check the number of files in repo1 for the latest backup:
```
$ find /var/lib/pgbackrest/1/backup/demo/latest/ -type f | wc -l

991
```

This is pretty normal for a small database without bundling enabled since each file is stored separately. There are also a few metadata files that pgBackRest uses to track the backup.

Now we'll perform the same actions on repo2, which has file bundling enabled:
```
$ pgbackrest --stanza=demo --type=full --repo=2 backup
$ find /var/lib/pgbackrest/2/backup/demo/latest/ -type f | wc -l

7
```

This time there are far fewer files. The small files have been bundled together and zero-length files are stored only in the manifest.

The `repo-bundle-size` option can be used to control the maximum size of bundles before compression and other operations are applied.
The `repo-bundle-limit` option limits the files that will be added to bundles. It is not a good idea to set these options too large because any failure in the bundle on backup or restore will require the entire bundle to be retried. The goal of file bundling is to combine small files -- there is very seldom any benefit in combining larger files.

## Block Incremental Backup

Block incremental backup saves space in the repository by storing only the parts of the file that have changed since the last backup. The block size depends the file size and when the file was last modified, i.e. larger, older files will get larger block sizes. Blocks are compressed and encrypted into super blocks that can be retrieved independently to make restore more efficient.

To demonstrate block incremental we need to make some changes to the database. With `pgbench` we can update 100 random rows in the main table, which is about 1GB in size.
```
$ /usr/lib/postgresql/12/bin/pgbench -n -b simple-update -t 100
```

On repo1 the time to make a differential backup is very similar to making a full backup. That's because most of the data is contained in a single table, so when the table is updated it must be copied in full.
```
$ pgbackrest --stanza=demo --type=diff --repo=1 backup

<...>
INFO: backup command end: completed successfully (12525ms)
```

Here we can see that the differential backup is nearly as large as the full backup, 52.8MB vs 55.5MB. This is expected since the bulk of the database is contained in a single file.
```
> pgbackrest --stanza=demo --repo=1 info

full backup: 20230520-082323F
    database size: 995.7MB, database backup size: 995.7MB
    repo1: backup size: 55.5MB

diff backup: 20230520-082323F_20230520-082934D
    database size: 995.7MB, database backup size: 972.8MB
    repo1: backup size: 52.8MB
```

However, on repo2 with block incremental enabled, the backup is significantly faster.
```
> pgbackrest --stanza=demo --type=diff --repo=2 backup

<...>
INFO: backup command end: completed successfully (3589ms)
```

And also much smaller, 943KB vs 52.8MB on the repo without block incremental enabled. This is more than 50x improvement in backup size!
```
> pgbackrest --stanza=demo --repo=2 info

full backup: 20230520-082438F
    database size: 995.7MB, database backup size: 995.7MB
    repo2: backup size: 56MB

diff backup: 20230520-082438F_20230520-083027D
    database size: 995.7MB, database backup size: 972.8MB
    repo2: backup size: 943.3KB
```
Block incremental also improves the efficiency of the restore command . On repo1 restoring the full backup would require reading the entire 30MB (compressed) `pg_data/base/13427/16501` file, but with block incremental, only 3.MB of blocks are required to restore the file to the full backup.

!!! Avoid long chains of incrementals

pg_data/base/13427/16501
    size: 832.5MB, repo 30.4MB
    block: size 80KB, map size 74.0KB, checksum size 7B
    block delta:
    reference: 20230520-082438F/pg_data/base/13427/16501.pgbi, read: 86/3.5MB, superBlock: 95/96.2MB, block: 100/7.8MB
    total read: 86/3.5MB, superBlock: 95/96.2MB, block: 100/7.8MB

# Conclusion
