## GPDB integration tests

This directory contains automated integration tests for validating the functionality and compatibility of pgBackRest with [Greenplum Database (GPDB)](https://github.com/arenadata/gpdb). These tests are designed primarily to be executed within a GPDB Docker container environment.

**Prerequisites**
To run these integration tests, you will need a Docker environment where you can spin up a GPDB container.
It is preferable to have a container build from GPDB repo's [Dockerfile](https://github.com/arenadata/gpdb/blob/adb-6.x/arenadata/readme.md) .

**Launch**
To run the tests, execute the script:
```
<path_to_pgbackrest>/arenadata/run_docker.sh <image_id> [arch type]
```
[arch type] stands for CPU architecture. Default value is x86-64. This argument is needed only for log files namings.

After launching the container the test scripts from `scripts` directory are executed. The logs will be stored in `<path_to_pgbackrest>/arenadata/logs` directory.
