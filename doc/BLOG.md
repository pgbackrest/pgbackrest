# pgBackRest File Bundling and Block Incremental Backup

Efficiently storing backups is a major priority for the pgBackRest project but we also strive to balance this goal with restore performance. Two recent features help with both efficiency and performance: file bundling and block incremental backup.

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
table that is near the maximum segment size of 1GB. This can be accomplished by creating data with `pgbench`:
```
$ /usr/lib/postgresql/12/bin/pgbench -i -s 65
```

PostgreSQL splits tables into segment files of 1GB each, so the main table that `pgbench` created above will be contained in a single file. The format PostgreSQL uses to store tables on disk will be important in the examples below.

## File Bundling

File bundling stores data in the repository more efficiently by combining smaller files together. This results in fewer files overall in the backup which improves the speed of all repository operations, especially on object stores like S3, Azure, and GCS. There may also be cost savings on repositories that have a cost per operation since there will be fewer lists, deletes, etc.

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

Block incremental backup saves space in the repository by storing only the parts of the file that have changed since the last backup. The block size depends on the file size and when the file was last modified, i.e. larger, older files will get larger block sizes. Blocks are compressed and encrypted into super blocks that can be retrieved independently to make restore more efficient.

To demonstrate block incremental we need to make some changes to the database. With `pgbench` we can update 100 random rows in the main table, which is about 1GB in size.
```
$ /usr/lib/postgresql/12/bin/pgbench -n -b simple-update -t 100
```

On repo1 the time to make a incremental backup is very similar to making a full backup. As previously discussed, PostgreSQL breaks tables up into 1GB segments so in our case the main table consists of a single file that contains most of the data in our database.
```
$ pgbackrest --stanza=demo --type=incr --repo=1 backup

<...>
INFO: backup command end: completed successfully (12525ms)
```

Here we can see that the incremental backup is nearly as large as the full backup, 52.8MB vs 55.5MB. This is expected since the bulk of the database is contained in a single file and by default incremental backups copy the entire file if any part of the file has changed.
```
$ pgbackrest --stanza=demo --repo=1 info

full backup: 20230520-082323F
    database size: 995.7MB, database backup size: 995.7MB
    repo1: backup size: 55.5MB

incr backup: 20230520-082323F_20230520-082934I
    database size: 995.7MB, database backup size: 972.8MB
    repo1: backup size: 52.8MB
```

However, on repo2 with block incremental enabled, the backup is significantly faster.
```
$ pgbackrest --stanza=demo --type=incr --repo=2 backup

<...>
INFO: backup command end: completed successfully (3589ms)
```

And also much smaller, 943KB vs 52.8MB on the repo without block incremental enabled. This is more than 50x improvement in backup size! Note that the block incremental backup feature also works with differential backups.
```
$ pgbackrest --stanza=demo --repo=2 info

full backup: 20230520-082438F
    database size: 995.7MB, database backup size: 995.7MB
    repo2: backup size: 56MB

incr backup: 20230520-082438F_20230520-083027I
    database size: 995.7MB, database backup size: 972.8MB
    repo2: backup size: 943.3KB
```

Block incremental also improves the efficiency of the delta restore command. Here we stop the cluster and perform a delta restore back to the full backup in repo 1:
```
$ pg_ctlcluster 12 demo stop
$ pgbackrest --stanza=demo --delta --repo=1 --set=20230526-053458F restore

<...>
INFO: restore command end: completed successfully (3697ms)
```
As we saw above the main table is contained in a single file, so the restore must copy and decompress the entire file from repo 1 (compressed size 30.4MB) because it was changed since the full backup.

To test the full backup restore in repo 2 we need to first restore the cluster to the most recent backup in repo 2:
```
pgbackrest --stanza=demo --delta --repo=2 restore
```

And then perform a delta restore back to the full backup in repo 2:
```
pgbackrest --stanza=demo --delta --repo=2 --set=20230526-053406F restore

<...>
INFO: restore command end: completed successfully (1536ms)
```
This is noticeably faster even on our fairly small demo database. When storage latency is high (e.g. S3) the performance improvement will be more pronounced. With block incremental enabled, delta restore only had to copy 3.5MB of the main table file from repo 2, as compared to 30.4MB from repo 1.

It is best to avoid long chains of block incremental backups since they can have a negative impact on restore performance. In this case pgBackRest may be forced to pull from many backups to restore a file.

# Conclusion

Block incremental and file bundling both help make backup and restore more efficient and they are a powerful combination when used together. In general you should consider enabling both on all your repositories, with the caveat that these features are not backward compatible with older versions of pgBackRest.

# !!! NOTES -- IGNORE FOR REVIEW

!!!SETUP:
pgbackrest/doc/doc.pl --out=html --include=user-guide --pre --no-cache --var=host-mem-limit=n --require=/quickstart/check-configuration
docker exec -it -u postgres doc-pg-primary bash
rm -rf /var/lib/pgbackrest/*
mkdir /var/lib/pgbackrest/1
mkdir /var/lib/pgbackrest/2
vi /etc/pgbackrest/pgbackrest.conf

!!!BLOCK DELTA:
pg_data/base/13427/16501
    size: 832.5MB, repo 30.4MB
    block: size 80KB, map size 74.0KB, checksum size 7B
    block delta:
    reference: 20230520-082438F/pg_data/base/13427/16501.pgbi, read: 86/3.5MB, superBlock: 95/96.2MB, block: 100/7.8MB
    total read: 86/3.5MB, superBlock: 95/96.2MB, block: 100/7.8MB
