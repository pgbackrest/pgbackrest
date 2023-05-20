# pgBackRest File Bundling and Block Incremental Backup

!!!SETUP
docker exec -it -u postgres doc-pg-primary bash
rm -rf /var/lib/pgbackrest/*
mkdir /var/lib/pgbackrest/1
mkdir /var/lib/pgbackrest/2
vi /etc/pgbackrest/pgbackrest.conf

```
[global]
log-level-console=info
start-fast=y

repo1-path=/var/lib/pgbackrest/1
repo1-retention-full=1

repo2-path=/var/lib/pgbackrest/2
repo2-retention-full=1
repo2-bundle=y
repo2-block=y

[demo]
pg1-path=/var/lib/postgresql/12/demo
```

```
$ pgbackrest --stanza=demo stanza-create
```

Create some data:
```
$ /usr/lib/postgresql/12/bin/pgbench -i -s 65
```

## File Bundling

File bundling stores files more efficiently in the repository by combining smaller files together. This results in fewer files overall in the backup which improves the speed of all repository operations, especially on object stores like S3, Azure, and GCS. There may also be cost savings on repositories that have a cost per operation since there will be fewer lists, deletes, etc. Another advantage of file bundling is that zero-length files are only stored in the manifest.

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

Now we'll perform the same actions on repo2:
```
$ pgbackrest --stanza=demo --type=full --repo=2 backup
$ find /var/lib/pgbackrest/2/backup/demo/latest/ -type f | wc -l

7
```
This time there are far fewer files. The small files have been bundled together and zero-length files are stored only in the manifest.

To enable file bundling simply add `repo-bundle=y` to your configuration, making sure to add the key for the repository you want to bundle, e.g. `repo1-bundle=y`

The `repo-bundle-size` option can be used to control the maximum size of bundles before compression and other operations are applied.
The `repo-bundle-limit` option limits the files that will be added to bundles. It is not a good idea to set these options too large because any failure in the bundle on backup or restore will require the entire bundle to be retried. The goal of file bundling is to combine small files -- there is very seldom any benefit in combining larger files.

## Block Incremental Backup

Block incremental backup saves space in the repository by storing only the parts of the file that have changed since the last backup. The block size depends the file size and when the file was last modified, i.e. larger, older files will get larger block sizes. Blocks are compressed and encrypted into super blocks that can be retrieved independently to make restore more efficient. Block incremental backup is enabled by adding `repo-block=y` to your configuration and it should be enabled for all backup types.

To demonstrate this, we !!!
```
$ /usr/lib/postgresql/12/bin/pgbench -n -b simple-update -t 100
```

```
$ pgbackrest --stanza=demo --type=diff --repo=1 backup

<...>
INFO: backup command end: completed successfully (12525ms)
```

```
> pgbackrest --stanza=demo --repo=1 info

<...>
full backup: 20230520-082323F
    timestamp start/stop: 2023-05-20 08:23:23 / 2023-05-20 08:23:35
    database size: 995.7MB, database backup size: 995.7MB
    repo1: backup size: 55.5MB

diff backup: 20230520-082323F_20230520-082934D
    timestamp start/stop: 2023-05-20 08:29:34 / 2023-05-20 08:29:45
    database size: 995.7MB, database backup size: 972.8MB
    repo1: backup size: 52.8MB
```

```
> pgbackrest --stanza=demo --type=diff --repo=2 backup

<...>
INFO: backup command end: completed successfully (3589ms)
```

```
> pgbackrest --stanza=demo --repo=2 info

<...abbreviated output...>
full backup: 20230520-082438F
    database size: 995.7MB, database backup size: 995.7MB
    repo2: backup size: 56MB

diff backup: 20230520-082438F_20230520-083027D
    timestamp start/stop: 2023-05-20 08:30:27 / 2023-05-20 08:30:30
    database size: 995.7MB, database backup size: 972.8MB
    repo2: backup size: 943.3KB
    backup reference list: 20230520-082438F
```
!!! Avoid long chains of incrementals
