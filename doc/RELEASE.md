# Release Build Instructions

## Set location of the `pgbackrest` repo

This makes the rest of the commands in the document easier to run (change to your repo path):
```
export PGBR_REPO=/backrest
```

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

## Build release documentation.  Be sure to install latex using the instructions from the Vagrantfile before running this step.
```
${PGBR_REPO?}/doc/release.pl --build
```

## Update code counts
```
${PGBR_REPO?}/test/test.pl --code-count
```

## Commit release branch and push to CI for testing
```
git commit -m "Release test"
git push origin release-ci
```

## Clone web documentation into `doc/site`
```
cd ${PGBR_REPO?}/doc
git clone git@github.com:pgbackrest/website.git site
```

## Deploy web documentation to `doc/site`
```
${PGBR_REPO?}/doc/release.pl --deploy
```

## Final commit of release to integration

Create release notes based on the pattern in prior git commits (this should be automated at some point), e.g.
```
v2.14: Bug Fix and Improvements

Bug Fixes:

* Fix segfault when process-max > 8 for archive-push/archive-get. (Reported by Jens Wilke.)

Improvements:

* Bypass database checks when stanza-delete issued with force. (Contributed by Cynthia Shang. Suggested by hatifnatt.)
* Add configure script for improved multi-platform support.

Documentation Features:

* Add user guides for CentOS/RHEL 6/7.
```

Commit to integration with the above message and push to CI.

## Push to master

Push release commit to master once CI testing is complete.

## Create release on github

Create release notes based on pattern in prior releases (this should be automated at some point), e.g.
```
v2.14: Bug Fix and Improvements

**Bug Fixes**:

- Fix segfault when process-max > 8 for archive-push/archive-get. (Reported by Jens Wilke.)

**Improvements**:

- Bypass database checks when stanza-delete issued with force. (Contributed by Cynthia Shang. Suggested by hatifnatt.)
- Add configure script for improved multi-platform support.

**Documentation Features**:

- Add user guides for CentOS/RHEL 6/7.
```

The first line will be the release title and the rest will be the body.  The tag field should be updated with the current version so a tag is created from master. **Be sure to select the release commit explicitly rather than auto-tagging the last commit in master!**

## Push web documentation to master and deploy
```
cd ${PGBR_REPO?}/doc/site
git commit -m "v2.14 documentation."
git push origin master
```

Deploy the documentation on `pgbackrest.org`.

## Notify packagers of new release

## Announce release on Twitter

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
${PGBR_REPO?}/doc/release.pl --build
```

Commit and push to integration:
```
git commit -m "Begin v2.15 development."
git push origin integration
```

## Update automake/config scripts

These scripts are required by `src/config` and should be updated after each release, when needed. Note that these files are updated very infrequently.

Check the latest version of `automake` and see if it is > `1.16.2`:
```
https://git.savannah.gnu.org/gitweb/?p=automake.git
```

If so, update the version above and copy `lib/install-sh` from the `automake` repo to the `pgbackrest` repo at `[repo]/src/build/install-sh`:
```
wget -O ${PGBR_REPO?}/src/build/install-sh '[URL]'
```

Get the latest versions of `config.sub` and `config.guess`. These files are not versioned so the newest version is pulled at the beginning of the release cycle to allow time to test stability.
```
wget -O ${PGBR_REPO?}/src/build/config.guess 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD'
wget -O ${PGBR_REPO?}/src/build/config.sub 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD'
```
