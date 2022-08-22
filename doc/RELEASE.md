# Release Build Instructions

## Create a branch to test the release

```
git checkout -b release-ci
```

## Update the date, version, and release title

Edit the latest release in `doc/xml/release.xml`, e.g.:
```
        <release date="XXXX-XX-XX" version="2.14dev" title="UNDER DEVELOPMENT">
```
to:
```
        <release date="2019-05-20" version="2.14" title="Bug Fix and Improvements">
```

Edit version in `src/version.h`, e.g.:
```
#define PROJECT_VERSION                                             "2.14dev"
```
to:
```
#define PROJECT_VERSION                                             "2.14"
```

## Update code counts
```
pgbackrest/test/test.pl --code-count
```

## Build release documentation.  Be sure to install latex using the instructions from the Vagrantfile before running this step.
```
pgbackrest/doc/release.pl --build
```

## Commit release branch and push to CI for testing
```
git commit -m "Release test"
git push origin release-ci
```

## Perform stress testing on release

- Build the documentation with stress testing enabled:
```
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/stress --var=stress=y --var=stress-scale-table=100 --var=stress-scale-data=1000 --pre --no-cache
```

During data load the archive-push and archive-get processes can be monitored with:
```
docker exec -it doc-pg-primary tail -f /var/log/pgbackrest/demo-archive-push-async.log
docker exec -it doc-pg-standby tail -f /var/log/pgbackrest/demo-archive-get-async.log
```

During backup/restore the processes can be monitored with:
```
docker exec -it doc-repository tail -f /var/log/pgbackrest/demo-backup.log
docker exec -it doc-pg-standby tail -f /var/log/pgbackrest/demo-restore.log
```

Processes can generally be monitored using 'top'. Once `top` is running, press `o` then enter `COMMAND=pgbackrest`. This will filter output to pgbackrest processes.

- Check for many log entries in the `archive-push`/`archive-get` logs to ensure async archiving was enabled:
```
docker exec -it doc-pg-primary vi /var/log/pgbackrest/demo-archive-push-async.log
docker exec -it doc-pg-standby vi /var/log/pgbackrest/demo-archive-get-async.log
```

- Check the backup log to ensure the correct tables/data were created and backed up. It should look something like:
```
INFO: full backup size = 14.9GB, file total = 101004
```

- Check the restore log to ensure the correct tables/data were restored. The size and file total should match exactly.

## Clone web documentation into `doc/site`
```
cd pgbackrest/doc
git clone git@github.com:pgbackrest/website.git site
```

## Deploy web documentation to `doc/site`
```
pgbackrest/doc/release.pl --deploy
```

## Final commit of release to integration

Create release notes based on the pattern in prior git commits (this should be automated at some point), e.g.
```
v2.14: Bug Fix and Improvements

Bug Fixes:

* Fix segfault when process-max > 8 for archive-push/archive-get. (Reported by User.)

Improvements:

* Bypass database checks when stanza-delete issued with force. (Contributed by User. Suggested by User.)
* Add configure script for improved multi-platform support.

Documentation Features:

* Add user guide for Debian.
```

Commit to integration with the above message and push to CI.

## Push to main

Push release commit to main once CI testing is complete.

## Create release on github

Create release notes based on pattern in prior releases (this should be automated at some point), e.g.
```
v2.14: Bug Fix and Improvements

**Bug Fixes**:

- Fix segfault when process-max > 8 for archive-push/archive-get. (Reported by User.)

**Improvements**:

- Bypass database checks when stanza-delete issued with force. (Contributed by User. Suggested by User.)
- Add configure script for improved multi-platform support.

**Documentation Features**:

- Add user guide for Debian.
```

The first line will be the release title and the rest will be the body.  The tag field should be updated with the current version so a tag is created from main. **Be sure to select the release commit explicitly rather than auto-tagging the last commit in main!**

## Push web documentation to main and deploy
```
cd pgbackrest/doc/site
git commit -m "v2.14 documentation."
git push origin main
```

Deploy the documentation on `pgbackrest.org`.

## Notify packagers of new release

## Announce release on Twitter

## Publish a postgresql.org news item when there are major new features

Start from NEWS.md and update with the new date, version, and interesting features added since the last release. News items are automatically sent to the `pgsql-announce` mailing list once they have been approved.

## Prepare for the next release

Add new release in `doc/xml/release.xml`, e.g.:
```
        <release date="XXXX-XX-XX" version="2.15dev" title="UNDER DEVELOPMENT">
```

Edit version in `src/version.h`, e.g.:
```
#define PROJECT_VERSION                                             "2.14"
```
to:
```
#define PROJECT_VERSION                                             "2.15dev"
```

Run deploy to generate git history (ctrl-c as soon as the file is generated):
```
pgbackrest/doc/release.pl --build
```

Commit and push to integration:
```
git commit -m "Begin v2.15 development."
git push origin integration
```

## Update automake/config scripts

These scripts are required by `src/config` and should be updated after each release, when needed. Note that these files are updated very infrequently.

Check the latest version of `automake` and see if it is > `1.16.5`:
```
https://git.savannah.gnu.org/gitweb/?p=automake.git
```

If so, update the version above and copy `lib/install-sh` from the `automake` repo to the `pgbackrest` repo at `[repo]/src/build/install-sh`:
```
wget -O pgbackrest/src/build/install-sh '[URL]'
```

Get the latest versions of `config.sub` and `config.guess`. These files are not versioned so the newest version is pulled at the beginning of the release cycle to allow time to test stability.
```
wget -O pgbackrest/src/build/config.guess 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD'
wget -O pgbackrest/src/build/config.sub 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD'
```
