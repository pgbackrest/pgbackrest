**June 12, 2023**: [Crunchy Data](https://www.crunchydata.com) is pleased to announce the release of [pgBackRest](https://pgbackrest.org/) 2.46, the latest version of the reliable, easy-to-use backup and restore solution that can seamlessly scale up to the largest databases and workloads.

Over the last year pgBackRest has introduced many exciting new features including block incremental backup, file bundling, repository verification, backup annotations, and SFTP repository storage.

IMPORTANT NOTE: pgBackRest 2.44 is the last version to support PostgreSQL 9.0/9.1/9.2.

pgBackRest supports a robust set of features for managing your backup and recovery infrastructure, including: parallel backup/restore, full/differential/incremental backups, block incremental backup, multiple repositories, delta restore, parallel asynchronous archiving, per-file checksums, page checksums (when enabled) validated during backup, multiple compression types, encryption, partial/failed backup resume, backup from standby, tablespace and link support, S3/Azure/GCS/SFTP support, backup expiration, local/remote operation via SSH or TLS, flexible configuration, and more.

pgBackRest can be installed from the [PostgreSQL Yum Repository](https://yum.postgresql.org/) or the [PostgreSQL APT Repository](https://apt.postgresql.org). Source code can be downloaded from [releases](https://github.com/pgbackrest/pgbackrest/releases).

## Major New Features

### Block Incremental Backup

Block incremental backup saves space in the repository by only storing file parts that have changed since the prior backup. In addition to space savings, this feature makes backup faster since there is less data to compress and transfer. Delta restore is also improved because less data from the repository is required to restore files. See [User Guide](https://pgbackrest.org/user-guide-rhel.html#backup/block) and [pgBackRest File Bundling and Block Incremental Backup](https://www.crunchydata.com/blog/pgbackrest-file-bundling-and-block-incremental-backup).

### File Bundling

File bundling combines smaller files to improve the efficiency of repository reads and writes, especially on object stores such as S3, Azure, and GCS. Zero-length files are stored only in the manifest. See [User Guide](https://pgbackrest.org/user-guide-rhel.html#backup/bundle) and [pgBackRest File Bundling and Block Incremental Backup](https://www.crunchydata.com/blog/pgbackrest-file-bundling-and-block-incremental-backup).

### Verify

The `verify` command checks that files in the repository have not been lost or corrupted and generates a report when problems are found. See [Command Reference](https://pgbackrest.org/command.html#command-verify).

### Backup Key/Value Annotations

Backup annotations allow custom annotations to be stored with a backup and queried with the `info` command. See [User Guide](https://pgbackrest.org/user-guide-rhel.html#backup/annotate).

### SFTP Repository Storage

Repositories can now be stored on an SFTP server. See [User Guide](https://pgbackrest.org/user-guide-rhel.html#sftp-support).

## Links
- [Website](https://pgbackrest.org)
- [User Guides](https://pgbackrest.org/user-guide-index.html)
- [Release Notes](https://pgbackrest.org/release.html)
- [Support](http://pgbackrest.org/#support)

[Crunchy Data](https://www.crunchydata.com) is proud to support the development and maintenance of [pgBackRest](https://github.com/pgbackrest/pgbackrest).
