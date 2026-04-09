**January 19, 2026**: The pgBackRest community is pleased to announce the release of [pgBackRest](https://pgbackrest.org) 2.58.0, the latest version of the reliable, easy-to-use backup and restore solution that can seamlessly scale up to the largest databases and workloads.

pgBackRest supports a robust set of features for managing your backup and recovery infrastructure, including: parallel backup/restore, full/differential/incremental backups, block incremental backup, multiple repositories, delta restore, parallel asynchronous archiving, per-file checksums, page checksums (when enabled) validated during backup, multiple compression types, encryption, partial/failed backup resume, backup from standby, tablespace and link support, S3/Azure/GCS/SFTP support, backup expiration, local/remote operation via SSH or TLS, flexible configuration, and more.

pgBackRest can be installed from the [PostgreSQL Yum Repository](https://yum.postgresql.org) or the [PostgreSQL APT Repository](https://apt.postgresql.org) and packages are also available for many other distributions. Source code can be downloaded from [releases](https://github.com/pgbackrest/pgbackrest/releases).

## New Features and Improvements

- Allow expiration of oldest full backup regardless of current retention (Stefan Fercot)
- HTTP support for S3, GCS, and Azure (Will Morland)
- Support for Azure managed identities (Moiz Ibrar)
- Experimental support for S3 EKS pod identity (Pierre BOUTELOUP)
- Allow configuration of TLS cipher suites (Gunnar "Nick" Bluth)
- Allow process priority to be set (David Steele)
- Allow dots in S3 bucket names when using path-style URIs (Joakim Hindersson)
- Dynamically size S3/GCS/Azure chunks for large uploads and optimize chunk size for small files (David Steele)

See the [2.58.0 Release Notes](https://pgbackrest.org/release.html#2.58.0) for additional features and improvements.

## Important Notes

- The minimum values for the repo-storage-upload-chunk-size option have increased. They now represent the minimum allowed by the vendors.
- TLS >= 1.2 is now required. This requirement is relaxed when verification is disabled with repo-storage-verify-tls (though this is not recommended).
- Per our policy to support five EOL versions of PostgreSQL, 9.5 is no longer supported by pgBackRest.

## Links

- [Website](https://pgbackrest.org)
- [User Guides](https://pgbackrest.org/user-guide-index.html)
- [Release Notes](https://pgbackrest.org/release.html)
- [Support](http://pgbackrest.org/#support)
