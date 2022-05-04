**February 10, 2022**: [Crunchy Data](https://www.crunchydata.com) is pleased to announce the release of [pgBackRest](https://pgbackrest.org/) 2.37, the latest version of the reliable, easy-to-use backup and restore solution that can seamlessly scale up to the largest databases and workloads.

pgBackRest has recently introduced many exciting new features including a built-in TLS server, binary protocol, new authentication methods, backup history retention, restore enhancements, backup integrity enhancements, and increased option indexes.

IMPORTANT NOTE: pgBackRest 2.37 is the last version to support PostgreSQL 8.3/8.4.

pgBackRest supports a robust set of features for managing your backup and recovery infrastructure, including: parallel backup/restore, full/differential/incremental backups, multiple repositories, delta restore, parallel asynchronous archiving, per-file checksums, page checksums (when enabled) validated during backup, multiple compression types, encryption, partial/failed backup resume, backup from standby, tablespace and link support, S3/Azure/GCS support, backup expiration, local/remote operation via SSH or TLS, flexible configuration, and more.

You can install pgBackRest from the [PostgreSQL Yum Repository](https://yum.postgresql.org/) or the [PostgreSQL APT Repository](https://apt.postgresql.org). Source code can be downloaded from [releases](https://github.com/pgbackrest/pgbackrest/releases).

## Major New Features

### TLS Server

The TLS server provides an alternative to SSH for remote operations such as backup. Containers benefit because pgBackRest can be used as the entry point without any need for SSH. In addition, performance tests have shown TLS to be significantly faster than SSH. See [User Guide](https://pgbackrest.org/user-guide-rhel.html#repo-host/setup-tls).

### Binary Protocol

The binary protocol provides a faster and more memory efficient way for pgBackRest to communicate with local and remote processes while maintaining the ability to communicate between different architectures.

### New Authentication Methods

The GCS storage driver now supports automatic authentication on GCE instances and the S3 storage driver supports WebIdentity authentication. See [Config Reference](https://pgbackrest.org/configuration.html#section-repository/option-repo-gcs-key-type).

### Additional Backup Integrity Checks

A number of integrity checks were added to ensure the backup is valid or errors are detected as early as possible, including: loop while waiting for checkpoint LSN to reach replay LSN, check archive immediately after backup start, timeline and checkpoint checks before backup, check that clusters are alive and correctly configured during a backup, and warn when checkpoint_timeout exceeds db-timeout.

### Increase Maximum Index Allowed for PG/REPO Options to 256

Up to 256 PostgreSQL clusters and repositories may now be configured.

### Restore Enhancements

The restore command has a number of new features, including: db-exclude option (see [Config Reference](https://pgbackrest.org/configuration.html#section-restore/option-db-exclude)), link-map option can create new links (see [Config Reference](https://pgbackrest.org/configuration.html#section-restore/option-link-map)), automatically create data directory, restore --type=lsn (See [Command Reference](https://pgbackrest.org/command.html#command-restore/category-command/option-type)), and error when restore is unable to find a backup to match the time target.

### Backup History Retention

The backup manifest history can now be expired. See [Config Reference](https://pgbackrest.org/configuration.html#section-repository/option-repo-retention-history).

## Links
- [Website](https://pgbackrest.org)
- [User Guides](https://pgbackrest.org/user-guide-index.html)
- [Release Notes](https://pgbackrest.org/release.html)
- [Support](http://pgbackrest.org/#support)

[Crunchy Data](https://www.crunchydata.com) is proud to support the development and maintenance of [pgBackRest](https://github.com/pgbackrest/pgbackrest).
