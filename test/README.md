# pgBackRest <br/> Regression, Unit, & Integration Testing

## Introduction

pgBackRest uses Docker to run tests and generate documentation. Docker's light-weight virualization provides the a good balance between proper OS emulation and performance (especially startup)

A `Vagrantfile` is provided that contains the complete configuration required to run pgBackRest tests and build documentation. If Vagrant is not suitable then the `Vagrantfile` still contains the configuration steps required to build a test system.

Note that this is not required for normal operation of pgBackRest.

## Testing

The easiest way to start testing pgBackRest is with the included `Vagrantfile`.

_Build Vagrant and Logon_:
```
cd test
vagrant up
vagrant ssh
```
The `vagrant up` command may take some time as a number of Docker containers must also be built. The `vagrant ssh` command automatically logs onto the VM.

_Run All Tests_:
```
/backrest/test/test.pl
```
_Run Tests for a Specific OS_:
```
/backrest/test/test.pl --vm=co6
```
_Run Tests for a Specific OS and Module_:
```
/backrest/test/test.pl --vm=co6 --module=backup
```
_Run Tests for a Specific OS, Module, and Test_:
```
/backrest/test/test.pl --vm=co6 --module=backup --test=full
```
_Run Tests for a Specific OS, Module, Test, and Run_:
```
/backrest/test/test.pl --vm=co6 --module=backup --test=full --run=1
```
_Run Tests for a Specific OS, Module, Test, and Process Max_:
```
/backrest/test/test.pl --vm=co6 --module=backup --test=full --process-max=4
```
Note that process-max is only applicable to the `synthetic` and `full` tests in the `backup` module.

_Run Tests for a Specific OS, Module, Test, Process Max, and Database Version_:
```
/backrest/test/test.pl --vm=co6 --module=backup --test=full --process-max=4 --db-version=9.4
```
Note that db-version is only applicable to the `full` test in the `backup` module.

_Iterate All Possible Test Combinations_:
```
/backrest/test/test.pl --dry-run
```
