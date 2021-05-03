**May 3, 2021**: [Crunchy Data](https://www.crunchydata.com) is pleased to announce the release of [pgBackRest](https://pgbackrest.org/) 2.33, the latest version of the reliable, easy-to-use backup and restore solution that can seamlessly scale up to the largest databases and workloads.

pgBackRest has recently introduced many exciting new features including multiple repository support, GCS support for repository storage, automatic temporary S3 credentials, repository list/get commands, and page checksum error reporting.

pgBackRest supports a robust set of features for managing your backup and recovery infrastructure, including: parallel backup/restore, full/differential/incremental backups, multiple repositories, delta restore, parallel asynchronous archiving, per-file checksums, page checksums (when enabled) validated during backup, multiple compression types, encryption, partial/failed backup resume, backup from standby, tablespace and link support, S3/Azure/GCS support, backup expiration, local/remote operation via SSH, flexible configuration, and more.

You can install pgBackRest from the [PostgreSQL Yum Repository](https://yum.postgresql.org/) or the [PostgreSQL APT Repository](https://apt.postgresql.org). Source code can be downloaded from [releases](https://github.com/pgbackrest/pgbackrest/releases).

## Major New Features

### Multiple Repository Support

Backups already provide redundancy by creating an offline copy of a PostgreSQL cluster that can be used in disaster recovery. Multiple repositories allow multiple copies of backups and WAL archives in separate locations to increase redundancy and provide even more protection for valuable data. See [User Guide](https://pgbackrest.org/user-guide.html#multi-repo) and [Blog](https://blog.crunchydata.com/blog/introducing-pgbackrest-multiple-repository-support).

### GCS Support for Repository Storage

Repositories may now be located on Google Cloud Storage using service key authentication. See [User Guide](https://pgbackrest.org/user-guide.html#gcs-support) and [Blog](https://blog.crunchydata.com/blog/announcing-google-cloud-storage-gcs-support-for-pgbackrest).

### Automatic Temporary S3 Credentials

Temporary credentials will automatically be retrieved when a role with the required permissions is associated with an instance in AWS. See [User Guide](https://pgbackrest.org/user-guide.html#s3-support).

### Repository List/Get Commands

The `repo-ls` and `repo-get` commands allow the contents of any repository to be listed and fetched, respectively, regardless of which storage type is used for the repository. See [Command Reference](https://pgbackrest.org/command.html).

### Page Checksum Error Reporting

Page checksum errors are included when getting detailed information for a backup using the `--set` option of the `info` command. See [Command Reference](https://pgbackrest.org/command.html#command-info).

## Links
- [Website](https://pgbackrest.org)
- [User Guides](https://pgbackrest.org/user-guide-index.html)
- [Release Notes](https://pgbackrest.org/release.html)
- [Support](http://pgbackrest.org/#support)

[Crunchy Data](https://www.crunchydata.com) is proud to support the development and maintenance of [pgBackRest](https://github.com/pgbackrest/pgbackrest).
