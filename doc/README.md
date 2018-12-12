# pgBackRest <br/> Building Documentation

## General Builds

The pgBackRest documentation can output a variety of formats and target several platforms and PostgreSQL versions.

This will build all documentation with defaults:
```bash
./doc.pl
```
The user guide can be built for different platforms: `centos6`, `centos7`, and `debian`. This will build the HTML user guide for CentOS/RHEL 7:
```bash
./doc.pl --out=html --include=user-guide --var=os-type=centos7
```
Documentation generation will build a cache of all executed statements and use the cache to build the documentation quickly if no executed statements have changed. This makes proofing text-only edits very fast, but sometimes it is useful to do a full build without using the cache:
```bash
./doc.pl --out=html --include=user-guide --var=os-type=centos6 --no-cache
```
Each `os-type` has a default container image that will be used as a base for creating hosts. For `centos6`/`centos7` these defaults are generally fine, but for `debian` it can be useful to change the image.
```bash
./doc.pl --out=html --include=user-guide --var=os-type=debian --var=os-image=debian:9
```

## Building with Packages

A user-specified package can be used when building the documentation. Since the documentation exercises most pgBackRest functionality this is a great way to smoke-test packages.

The package must be located within the pgBackRest repo and the specified path should be relative to the repository base. `test/package` is a good default path to use.

Ubuntu 16.04:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=debian --var=os-image=ubuntu:16.04 --var=package=test/package/pgbackrest_2.08-0_amd64.deb
```
CentOS/RHEL 6:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=centos6 --var=package=test/package/pgbackrest-2.08-1.el6.x86_64.rpm
```
CentOS/RHEL 7:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=centos7 --var=package=test/package/pgbackrest-2.08-1.el7.x86_64.rpm
```
