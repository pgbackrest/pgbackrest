# pgBackRest <br/> Reliable PostgreSQL Backup & Restore

## Introduction

pgBackRest aims to be a simple, reliable backup and restore system that can seamlessly scale up to the largest databases and workloads.

Instead of relying on traditional backup tools like tar and rsync, pgBackRest implements all backup features internally and uses a custom protocol for communicating with remote systems. Removing reliance on tar and rsync allows for better solutions to database-specific backup issues. The custom remote protocol limits the types of connections that are required to perform a backup which increases security.

## Features

### Multithreaded Backup & Restore

Blah, blah, blah

### Local or Remote Operation

Blah, blah, blah

### Full, Incremental, & Differential Backups

Blah, blah, blah

### Backup Rotation & Archive Expiration

Expiration policies maintain a set of backups that cover

### Backup Integrity

Checksums are calculated for every file in the backup and rechecked during a restore. After a backup finishes copying files it waits until every WAL segment required to make the backup consistent reaches the repository.

Backups in the repository are stored as in the same format as a standard PostgreSQL cluster. If compression is disabled and hard links are enabled it is possible to snapshot a backup in the repository and bring up a PostgreSQL cluster directly on the snapshot. This can be a boon for very large databases that are time-consuming to restore in the traditional way.

### Backup Resume

An aborted backup can be resumed from the point where it was stopped. Files that were already copied are compared with the checksums in the manifest to ensure integrity. Since this operation can take place entirely on the backup server it reduces load on the database server and in the end saves time since checksum calculation is faster than compressing and retransmitting data.

### Streaming Compression & Checksums

Compression and checksum calculations are performed in stream while files are being copied to the repository, whether the repository is located locally or remotely.

If the repository is on a backup server then compression is performed on the database server and files are transmitted compressed and simply stored on the backup server. When compression is disabled a lower level of compression is utilized to make efficient use of available bandwidth while keeping CPU cost to a minimum.

### Delta Restore

Blah, blah, blah

### Advanced Archiving

Get, push, async

### Tablespace & Link Support

Blah, blah, blah

### Compatibility with PostgreSQL >= 8.3

Blah, blah, blah

## Getting Started

pgBackRest strives to be easy to configure and operate:

- [User guide](http://www.pgbackrest.org/user-guide.html) for Ubuntu 12.04 & 14.04 / PostgreSQL 9.4.
- [Command reference](http://www.pgbackrest.org/command.html) for command-line operations.
- [Configuration reference](http://www.pgbackrest.org/configuration.html) for creating rich pgBackRest configurations.

## Contributions

Contributions to pgBackRest are always welcome!

Code fixes or new features can be submitted via pull requests. Ideas for new features and improvements to existing functionality or documentation can be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues).

Bug reports should be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). Please provide as much information as possible to aid in determining the cause of the problem.

You will always receive credit in the [change log](https://github.com/pgbackrest/pgbackrest/blob/master/CHANGELOG.md) for your contributions.

## Support

pgBackRest is completely free and open source under the [MIT](https://github.com/pgbackrest/pgbackrest/blob/master/LICENSE) license. You may use it for personal or commercial purposes without any restrictions whatsoever. Bug reports are taken very seriously and will be addressed as quickly as possible.

Creating a robust disaster recovery policy with proper replication and backup strategies can be a very complex and daunting task. You may find that you need help during the architecture phase and ongoing support to ensure that your enterprise continues running smoothly.

[Crunchy Data](http://www.crunchydata.com) provides packaged versions of pgBackRest for major operating systems and expert full life-cycle commercial support for pgBackRest and all things PostgreSQL. [Crunchy Data](http://www.crunchydata.com) is committed to providing open source solutions with no vendor lock-in so cross-compatibility with the community version of pgBackRest is always strictly maintained.

Please visit [Crunchy Data](http://www.crunchydata.com) for more information.

## Recognition

Primary recognition goes to Stephen Frost for all his valuable advice and criticism during the development of pgBackRest.

[Crunchy Data](http://www.crunchydata.com) has contributed significant time and resources to pgBackRest and continues to actively support development. [Resonate](http://www.resonate.com) also contributed to the development of pgBackRest and allowed early (but well tested) versions to be installed as their primary PostgreSQL backup solution.
