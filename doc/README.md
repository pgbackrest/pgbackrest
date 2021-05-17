# pgBackRest <br/> Building Documentation

## General Builds

The pgBackRest documentation can output a variety of formats and target several platforms and PostgreSQL versions.

This will build all documentation with defaults:
```bash
./doc.pl
```
The user guide can be built for `centos` and `debian`. This will build the HTML user guide for CentOS/RHEL:
```bash
./doc.pl --out=html --include=user-guide --var=os-type=centos
```
Documentation generation will build a cache of all executed statements and use the cache to build the documentation quickly if no executed statements have changed. This makes proofing text-only edits very fast, but sometimes it is useful to do a full build without using the cache:
```bash
./doc.pl --out=html --include=user-guide --var=os-type=centos --no-cache
```
Each `os-type` has a default container image that will be used as a base for creating hosts but it may be useful to change the image:
```bash
./doc.pl --out=html --include=user-guide --var=os-type=debian --var=os-image=debian:9
./doc.pl --out=html --include=user-guide --var=os-type=centos --var=os-image=centos:7
```
The following is a sample CentOS/RHEL 7 configuration that can be used for building the documentation.
```bash
# Install docker
sudo yum install -y yum-utils device-mapper-persistent-data lvm2
sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
sudo yum install -y docker-ce
sudo systemctl start docker

# Install tools
sudo yum install -y git wget

# Install latex (for building PDF)
sudo yum install -y texlive texlive-titlesec texlive-sectsty texlive-framed texlive-epstopdf ghostscript

# Install Perl modules that do not have CentOS packages via CPAN
sudo yum install -y yum cpanminus
sudo yum groupinstall -y "Development Tools" "Development Libraries"
sudo cpanm install --force XML::Checker::Parser

# Add documentation test user
sudo groupadd test
sudo adduser -gtest -n testdoc
sudo usermod -aG docker testdoc
```

## Building with Packages

A user-specified package can be used when building the documentation. Since the documentation exercises most pgBackRest functionality this is a great way to smoke-test packages.

The package must be located within the pgBackRest repo and the specified path should be relative to the repository base. `test/package` is a good default path to use.

Ubuntu 16.04:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=debian --var=os-image=ubuntu:16.04 --var=package=test/package/pgbackrest_2.08-0_amd64.deb
```
CentOS/RHEL 7:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=centos --var=os-image=centos:7  --var=package=test/package/pgbackrest-2.08-1.el7.x86_64.rpm
```
CentOS/RHEL 8:
```bash
./doc.pl --out=html --include=user-guide --no-cache --var=os-type=centos --var=os-image=centos:8 --var=package=test/package/pgbackrest-2.08-1.el8.x86_64.rpm
```
Packages can be built with `test.pl` using the following configuration on top of the configuration given for building the documentation.
```bash
# Install recent git
sudo yum remove -y git
sudo yum install -y https://centos7.iuscommunity.org/ius-release.rpm
sudo yum install -y git2u-all

# Install Perl modules
sudo yum install -y perl-ExtUtils-ParseXS perl-ExtUtils-Embed perl-ExtUtils-MakeMaker perl-YAML-LibYAML

# Install dev libraries
sudo yum install -y libxml2-devel openssl-devel

# Add test user with sudo privileges
sudo adduser -gtest -n test
sudo usermod -aG docker test
sudo chmod 750 /home/test

sudo echo 'test ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pgbackrest

# Add pgbackrest user required by tests
sudo adduser -gtest -n pgbackrest
```
