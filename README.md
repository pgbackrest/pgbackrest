# pgBackRest - Simple Postgres Backup & Restore

pgBackRest aims to be a simple backup and restore system that can seamlessly scale up to the largest databases and workloads.

Primary pgBackRest features:

- Local or remote backup
- Multi-threaded backup/restore for performance
- Checksums
- Safe backups (checks that logs required for consistency are present before backup completes)
- Full, differential, and incremental backups
- Backup rotation (and minimum retention rules with optional separate retention for archive)
- In-stream compression/decompression
- Archiving and retrieval of logs for replicas/restores built in
- Async archiving for very busy systems (including space limits)
- Backup directories are consistent PostgreSQL clusters (when hardlinks are on and compression is off)
- Tablespace support
- Restore delta option
- Restore using timestamp/size or checksum
- Restore remapping base/tablespaces
- Support for PostgreSQL >= 8.3

Instead of relying on traditional backup tools like tar and rsync, pgBackRest implements all backup features internally and uses a custom protocol for communicating with remote systems.  Removing reliance on tar and rsync allows for better solutions to database-specific backup issues.  The custom remote protocol limits the types of connections that are required to perform a backup which increases security.

pgBackRest uses the gitflow model of development.  This means that the master branch contains only the release history, i.e. each commit represents a single release and release tags are always from the master branch.  The dev branch contains a single commit for each feature or fix and more accurately depicts the development history.  Actual development is done on feature (dev_*) branches and squashed into dev after regression tests have passed.  In this model dev is considered stable and can be released at any time.  As such, the dev branch does not have any special version modifiers.

## Getting Started

pgBackRest strives to be easy to configure and operate:

* [Installation instructions](USERGUIDE.md#installation) for major operating systems.

* [Sample configurations](USERGUIDE.md#examples) that cover most basic use cases.

* [Command guide](USERGUIDE.md#commands) for command-line operations.

* [Settings documentation](USERGUIDE.md#setttings) for creating complex configurations and more detail on options.

## Contributing

Contributions to pgBackRest are always welcome!

Code fixes or new features can be submitted via pull requests.  Ideas for new features and improvements to existing functionality or documentation can be [submitted as issues](http://github.com/pgmasters/backrest/issues).

Bug reports should be [submitted as issues](http://github.com/pgmasters/backrest/issues).  Please provide as much information as possible to aid in determining the cause of the problem.

You will always receive credit in the [change log](https://github.com/pgmasters/backrest/blob/master/CHANGELOG.md) for your contributions.

## Support

pgBackRest is completely free and open source under the [MIT](https://github.com/pgmasters/backrest/blob/master/LICENSE) license.  You may use it for personal or commercial purposes without any restrictions whatsoever.  Bug reports are taken very seriously and will be addressed as quickly as possible.

Creating a robust disaster recovery policy with proper replication and backup strategies can be a very complex and daunting task.  You may find that you need help during the architecture phase and ongoing support to ensure that your enterprise continues running smoothly.

[Crunchy Data](http://www.crunchydatasolutions.com) provides packaged versions of pgBackRest for major operating systems and expert full life-cycle commercial support for pgBackRest and all things PostgreSQL.  [Crunchy Data](http://www.crunchydatasolutions.com) is committed to providing open source solutions with no vendor lock-in so cross-compatibility with the community version of pgBackRest is always strictly maintained.

Please visit [Crunchy Backup Manager](http://crunchydatasolutions.com/crunchy-backup-manager) for more information.

## Recognition

Primary recognition goes to Stephen Frost for all his valuable advice and criticism during the development of pgBackRest.

[Crunchy Data](http://www.crunchydatasolutions.com) has contributed significant time and resources to pgBackRest and continues to actively support development. [Resonate](http://www.resonate.com) also contributed to the development of pgBackRest and allowed early (but well tested) versions to be installed as their primary PostgreSQL backup solution.
