# pgBackRest <br/> Reliable PostgreSQL Backup & Restore

## Introduction

pgBackRest aims to be a simple, reliable backup and restore system that can seamlessly scale up to the largest databases and workloads.

Instead of relying on traditional backup tools like tar and rsync, pgBackRest implements all backup features internally and uses a custom protocol for communicating with remote systems. Removing reliance on tar and rsync allows for better solutions to database-specific backup issues. The custom remote protocol allows for more flexibility and limits the types of connections that are required to perform a backup which increases security.

## Features

### Multithreaded Backup & Restore

Compression is usually the bottleneck during backup operations, but even with now ubiquitous multi-core servers, most database backup solutions are still single-threaded. pgBackRest solves the compression bottleneck with multithreading.

Utilizing multiple cores for compression makes it possible to achieve 1TB/hr raw throughput even on a 1Gb/s link. More cores and a larger pipe lead to even higher throughput.

### Local or Remote Operation

A custom protocol allows pgBackRest to backup, restore, and archive locally or remotely via SSH with minimal configuration. An interface to query PostgreSQL is also provided via the protocol layer so that remote access to PostgreSQL is never required, which enhances security.

### Full, Incremental, & Differential Backups

Full, differential, and incremental backups are supported. pgBackRest is not susceptible to time resolution issues like rsync, making differential and incremental backups completely safe.

### Backup Rotation & Archive Expiration

Retention polices can be set for full and differential backups to create coverage for any timeframe. WAL archive can be maintained for all backups or strictly for the most recent backups. In the latter case, the WAL required to make older backups consistent will still be maintained in the archive.

### Backup Integrity

Checksums are calculated for every file in the backup and rechecked during a restore. After a backup finishes copying files, it waits until every WAL segment required to make the backup consistent reaches the repository.

Backups in the repository are stored as in the same format as a standard PostgreSQL cluster (including tablespaces). If compression is disabled and hard links are enabled it is possible to snapshot a backup in the repository and bring up a PostgreSQL cluster directly on the snapshot. This is advantageous for terabyte-scale databases that are time consuming to restore in the traditional way.

All operations utilize file and directory level fsyncs to ensure durability.

### Backup Resume

An aborted backup can be resumed from the point where it was stopped. Files that were already copied are compared with the checksums in the manifest to ensure integrity. Since this operation can take place entirely on the backup server, it reduces load on the database server and in the end saves time since checksum calculation is faster than compressing and retransmitting data.

### Streaming Compression & Checksums

Compression and checksum calculations are performed in stream while files are being copied to the repository, whether the repository is located locally or remotely.

If the repository is on a backup server, compression is performed on the database server and files are transmitted in a compressed format and simply stored on the backup server. When compression is disabled, a lower level of compression is utilized to make efficient use of available bandwidth while keeping CPU cost to a minimum.

### Delta Restore

The manifest contains checksums for every file in the backup so that during a restore it is possible to use these checksums to speed processing enormously. On a delta restore, any files not present in the backup are first removed, and then checksums are taken for the remaining files. Files that match the backup are left in place and the rest of the files are restored as usual. Since this process is multithreaded, it can lead to dramatic reductions in restore times.

### Advanced Archiving

Dedicated commands are included for both pushing WAL to the archive and retrieving WAL from the archive.

The push command automatically detects WAL segments that are pushed multiple times and de-duplicates when the segment is identical and returns an error otherwise. Asynchronous archiving allows the compression and transfer to be offloaded to another process which maintains a continuous connection to the remote server, improving throughput significantly.

The 'push' and 'get' commands both ensure that the database and repository match by comparing PostgreSQL versions and system identifiers. This dramatically reduces the possibility of misconfiguring the WAL archive location.

### Tablespace & Link Support

Tablespaces are fully supported and on 'restore', tablespaces can be remapped to any location. It is also possible to remap all tablespaces to one location with a single command which is useful for development restores.

File and directory links are supported for any file or directory in the PostgreSQL cluster. When restoring, it is possible to restore all links to their original locations, remap some or all links, or restore some or all links as normal files or directories within the cluster directory.

### Compatibility with PostgreSQL >= 8.3

pgBackRest includes support for versions down to 8.3, since older versions of PostgreSQL are still regularly utilized.

## Getting Started

pgBackRest strives to be easy to configure and operate:

- [User guide](http://www.pgbackrest.org/user-guide.html) for Ubuntu 12.04 & 14.04 / PostgreSQL 9.4.
- [Command reference](http://www.pgbackrest.org/command.html) for command-line operations.
- [Configuration reference](http://www.pgbackrest.org/configuration.html) for creating custom pgBackRest configurations.

## Contributions

Contributions to pgBackRest are always welcome!

Code fixes or new features can be submitted via pull requests. Ideas for new features and improvements to existing functionality or documentation can be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues).

Bug reports should be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). Please provide as much information as possible to aid in determining the cause of the problem.

You will always receive credit in the [change log](https://github.com/pgbackrest/pgbackrest/blob/master/CHANGELOG.md) for your contributions.

## Support

pgBackRest is completely free and open source under the [MIT](https://github.com/pgbackrest/pgbackrest/blob/master/LICENSE) license. You may use it for personal or commercial purposes without any restrictions whatsoever. Bug reports are taken very seriously and will be addressed as quickly as possible.

Creating a robust disaster recovery policy with proper replication and backup strategies can be a very complex and daunting task. You may find that you need help during the architecture phase and ongoing support to ensure that your environment continues running smoothly.

[Crunchy Data](http://www.crunchydata.com) provides packaged versions of pgBackRest for major operating systems and expert full life-cycle commercial support for pgBackRest and all things PostgreSQL. [Crunchy Data](http://www.crunchydata.com) is committed to providing open source solutions with no vendor lock-in, ensuring that cross-compatibility with the community version of pgBackRest is always strictly maintained.

Please visit [Crunchy Data](http://www.crunchydata.com) for more information.

## Recognition

Primary recognition goes to Stephen Frost for all his valuable advice and criticism during the development of pgBackRest.

[Crunchy Data](http://www.crunchydata.com) has contributed significant time and resources to pgBackRest and continues to actively support its development. [Resonate](http://www.resonate.com) also contributed to the development of pgBackRest and allowed early (and well-tested) versions to be installed as their primary PostgreSQL backup solution.
