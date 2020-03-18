# pgBackRest <br/> Contributing to pgBackRest

## Introduction

This documentation is intended to assist contributors to pgBackRest by outlining some basic steps and guidelines for contributing to the project. Coding standards to follow are defined in [CODING.md](https://github.com/pgbackrest/pgbackrest/blob/master/CODING.md). At a minimum, unit tests must be written and run and the documentation generated before submitting a Pull Request; see the [Testing](#testing) section below for details.

## Building a Development Environment

This example is based on Ubuntu 19.04, but it should work on many versions of Debian and Ubuntu.

pgbackrest-dev => Install development tools
```
sudo apt-get install rsync git devscripts build-essential valgrind autoconf \
       autoconf-archive libssl-dev zlib1g-dev libxml2-dev libpq-dev \
       libxml-checker-perl libyaml-libyaml-perl libdbd-pg-perl liblz4-dev liblz4-tool
```

Coverage testing is an important component of pgBackRest testing and is integrated directly into the test harness. Unfortunately, the default version of lcov is often not compatible with gcc. lcov 1.14 works up to gcc 8.

pgbackrest-dev => Build lcov 1.14
```
curl -fsSL \
       https://github.com/linux-test-project/lcov/releases/download/v1.14/lcov-1.14.tar.gz | \
       tar zx
sudo make -C lcov-1.14 install
```

Some unit tests and all the integration test require Docker. Running in containers allows us to simulate multiple hosts, test on different distributions and versions of PostgreSQL, and use sudo without affecting the host system.

pgbackrest-dev => Install Docker
```
curl -fsSL https://get.docker.com | sudo sh
sudo usermod -aG docker `whoami`
```

This clone of the pgBackRest repository is sufficient for experimentation. For development, create a fork and clone that instead.

pgbackrest-dev => Clone pgBackRest repository
```
git clone https://github.com/pgbackrest/pgbackrest.git
```

## Running Tests

### Without Docker

If Docker is not installed, then the available tests can be listed using `--vm-none`.

pgbackrest-dev => List tests that don't require a container
```
pgbackrest/test/test.pl --vm=none --dry-run

--- output ---

           [filtered 2 lines of output]
    2019-10-30 10:16:44.828 P00   INFO: check version info
    2019-10-30 10:16:45.318 P00   INFO: builds required: bin
--> 2019-10-30 10:16:45.319 P00   INFO: 64 tests selected

    2019-10-30 10:16:45.319 P00   INFO: P1-T01/64 - vm=none, module=common, test=error
           [filtered 61 lines of output]
    2019-10-30 10:16:45.334 P00   INFO: P1-T63/64 - vm=none, module=command, test=storage
    2019-10-30 10:16:45.334 P00   INFO: P1-T64/64 - vm=none, module=performance, test=type
--> 2019-10-30 10:16:45.389 P00   INFO: DRY RUN COMPLETED SUCCESSFULLY (1s)
```

Once a test has been selected it can be run by specifying the module and test. The `--dev` option sets several flags that are appropriate for development rather than test. Most importantly, it reuses object files from the previous test run to speed testing. The `--vm-out` option displays the test output.

pgbackrest-dev => Run a test
```
pgbackrest/test/test.pl --vm=none --dev --vm-out --module=common --test=wait

--- output ---

    2019-10-30 10:16:46.166 P00   INFO: test begin - log level info
    2019-10-30 10:16:46.743 P00   INFO: check code autogenerate
    2019-10-30 10:16:46.743 P00   INFO: cleanup old data
    2019-10-30 10:16:47.278 P00   INFO: builds required: none
    2019-10-30 10:16:47.282 P00   INFO: 1 test selected

    2019-10-30 10:16:47.283 P00   INFO: P1-T1/1 - vm=none, module=common, test=wait

        run 001 - waitNew(), waitMore, and waitFree()
            000.004s l0018 -     expect AssertError: assertion 'waitTime >= 100 && waitTime <= 999999000' failed
            000.020s l0019 -     expect AssertError: assertion 'waitTime >= 100 && waitTime <= 999999000' failed
            000.024s l0024 -     new wait = 0.2 sec
            000.032s l0025 -         check wait time
            000.046s l0026 -         check sleep time
            000.049s l0027 -         check sleep prev time
            000.053s l0028 -         check begin time
            000.259s l0034 -         lower range check
            000.262s l0035 -         upper range check
            000.265s l0037 -         free wait
            000.270s l0042 -     new wait = 1.1 sec
            000.273s l0043 -         check wait time
            000.276s l0044 -         check sleep time
            000.279s l0045 -         check sleep prev time
            000.282s l0046 -         check begin time
            001.375s l0052 -         lower range check
            001.379s l0053 -         upper range check
            001.382s l0055 -         free wait

        TESTS COMPLETED SUCCESSFULLY

    2019-10-30 10:16:51.145 P00   INFO: P1-T1/1 - vm=none, module=common, test=wait (3.87s)
    2019-10-30 10:16:51.209 P00   INFO: writing C coverage report
    2019-10-30 10:16:51.674 P00   INFO: TESTS COMPLETED SUCCESSFULLY (5s)
```

An entire module can be run by using only the `--module` option.

pgbackrest-dev => Run a module
```
pgbackrest/test/test.pl --vm=none --dev --module=postgres

--- output ---

    2019-10-30 10:16:52.449 P00   INFO: test begin - log level info
    2019-10-30 10:16:52.979 P00   INFO: check code autogenerate
    2019-10-30 10:16:52.979 P00   INFO: cleanup old data
    2019-10-30 10:16:53.513 P00   INFO: builds required: none
    2019-10-30 10:16:53.517 P00   INFO: 3 tests selected

    2019-10-30 10:16:57.501 P00   INFO: P1-T1/3 - vm=none, module=postgres, test=client (3.99s)
    2019-10-30 10:17:00.957 P00   INFO: P1-T2/3 - vm=none, module=postgres, test=interface (3.46s)
    2019-10-30 10:17:03.325 P00   INFO: P1-T3/3 - vm=none, module=postgres, test=page-checksum (2.37s)
    2019-10-30 10:17:03.382 P00   INFO: writing C coverage report
    2019-10-30 10:17:03.944 P00   INFO: TESTS COMPLETED SUCCESSFULLY (11s)
```

### With Docker

Build a container to run tests. The vm must be pre-configured but a variety are available. The vm names are all three character abbreviations, e.g. `u19` for Ubuntu 19.04.

pgbackrest-dev => Build a VM
```
pgbackrest/test/test.pl --vm-build --vm=u19

--- output ---

    2019-10-30 10:17:04.779 P00   INFO: test begin - log level info
    2019-10-30 10:17:04.812 P00   INFO: Using cached pgbackrest/test:u19-base-20191012A image (32d78710c7f60872ee4a8b2374d0110e015e2c24) ...
    2019-10-30 10:17:05.225 P00   INFO: Building pgbackrest/test:u19-build image ...
    2019-10-30 10:17:05.641 P00   INFO: Building pgbackrest/test:u19-test image ...
    2019-10-30 10:17:06.013 P00   INFO: Build Complete
```

pgbackrest-dev => Run a Test
```
pgbackrest/test/test.pl --vm=u19 --dev --module=mock --test=archive --run=2

--- output ---

    2019-10-30 10:17:06.768 P00   INFO: test begin - log level info
    2019-10-30 10:17:07.346 P00   INFO: check code autogenerate
    2019-10-30 10:17:07.346 P00   INFO: cleanup old data and containers
    2019-10-30 10:17:08.403 P00   INFO: builds required: bin, libc host
    2019-10-30 10:17:08.615 P00   INFO: 1 test selected

    2019-10-30 10:17:41.992 P00   INFO: P1-T1/1 - vm=u19, module=mock, test=archive, run=2 (33.38s)
    2019-10-30 10:17:42.163 P00   INFO: no code modules had all tests run required for coverage
    2019-10-30 10:17:42.163 P00   INFO: writing C coverage report
    2019-10-30 10:17:42.224 P00   INFO: TESTS COMPLETED SUCCESSFULLY (36s)
```

## Adding an Option

Options can be added to a command or multiple commands. Options can be configuration file only, command-line only or valid for both. Once an option is added, `config.auto.*`, `define.auto.*` and `parse.auto.*` files will automatically be generated by the build system.

To add an option, two files need be to be modified:

- `build/lib/pgBackRestBuild/Config/Data.pm`

- `doc/xml/reference.xml`

These files are discussed in the following sections.

### Data.pm

There is a detailed comment at the top of this file on the configuration definitions which one can refer to in determining how to define the rules for the option.

#### Command Line Only Options

Command-line only options are options where `CFGDEF_SECTION` rule is not defined. There are two sections to be updated when adding a command-line only option, each of which is marked by the comment `Command-line only options`.

- **Section 1:** Find the first section with the `Command-line only options` comment. This section defines and exports the constant for the actual option.

- **Section 2:** Find the second section with the `Command-line only options` comment. This is where the rules for the option are defined.

The steps for how to update these sections are detailed below.

**Section 1**

Copy the two lines ("use constant"/"push") of an existing option and paste them where the option would be in alphabetical order and rename it to the same name as the new option name. For example CFGOPT_DRY_RUN, defined as "dry-run".

**Section 2**

To better explain this section, `CFGOPT_ONLINE` will be used as an example:
```
&CFGOPT_ONLINE =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_NEGATE => true,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },
```
Note that `CFGDEF_SECTION` is not present thereby making this a command-line only option. Each line is explained below:

- `CFGOPT_ONLINE` - the name of the option as defined in **Section 1**

- `CFGDEF_TYPE` - the type of the option. Valid types are: `CFGDEF_TYPE_BOOLEAN`, `CFGDEF_TYPE_FLOAT`, `CFGDEF_TYPE_HASH`, `CFGDEF_TYPE_INTEGER`, `CFGDEF_TYPE_LIST`, `CFGDEF_TYPE_PATH`, `CFGDEF_TYPE_SIZE`, and `CFGDEF_TYPE_STRING`


- `CFGDEF_NEGATE` - being a command-line only boolean option, this rule would automatically default to false so it must be defined if the option is negatable. Ask yourself if negation makes sense, for example, would a --dry-run option make sense as --no-dry-run? If the answer is no, then this rule can be omitted as it would automatically default to false. Any boolean option that cannot be negatable, must be a command-line only and not a configuration file option as all configuration boolean options must be negatable.

- `CFGDEF_DEFAULT` - sets a default for the option if the option is not provided when the command is run. The default can be global or it can be specified for a specific command in the `CFGDEF_COMMAND` section. For example, if it was desirable for the default to be false for the `CFGCMD_STANZA_CREATE` then CFGDEF_NEGATE => would be set to `true` in each command listed except for `CFGCMD_STANZA_CREATE` where it would be `false` and it would not be specified (as it is here) in the global section (meaning global for all commands listed).

- `CFGDEF_COMMAND` - list each command for which the option is valid. If a command is not listed, then the option is not valid for the command and an error will be thrown if it attempted to be used for that command.

### reference.xml

All options must be documented or the system will error during the build. To add an option, find the command section identified by `command id="COMMAND"` section where `COMMAND` is the name of the command (e.g. `expire`) or, if the option is used by more than one command and the definition for the option is the same for all of the commands, the `operation-general title="General Options"` section.

To add an option, add the following to the `<option-list>` section; if it does not exist, then wrap the following in `<option-list>` `</option-list>`. This example uses the boolean option `force` of the `restore` command. Simply replace that with your new option and the appropriate `summary`, `text` and `example`.
```
<option id="force" name="Force">
    <summary>Force a restore.</summary>

    <text>By itself this option forces the <postgres/> data and tablespace paths to be completely overwritten.  In combination with <br-option>--delta</br-option> a timestamp/size delta will be performed instead of using checksums.</text>

    <example>y</example>
</option>
```
> **IMPORTANT:** currently a period (.) is required to end the `summary` section.

## Testing

For testing, it is recommended that Vagrant and Docker be used; instructions are provided in the `README.md` file of the pgBackRest [test](https://github.com/pgbackrest/pgbackrest/blob/master/test) directory. A list of all possible test combinations can be viewed by running:
```
/backrest/test/test.pl --dry-run
```
> **WARNING:** currently the `BACKREST_USER` in `ContainerTest.pm` must exist, or the test suite will fail with a string concatenation error.

If using a RHEL system, the CPAN XML parser is required for running `test.pl` and `doc.pl`. Instructions for installing Docker and the XML parse can be found in the `README.md` file of the pgBackRest [doc](https://github.com/pgbackrest/pgbackrest/blob/master/doc) directory in the section "The following is a sample CentOS/RHEL 7 configuration that can be used for building the documentation". NOTE that the `Install latex (for building PDF)` is not required since testing of the docs need only be run for HTML output.

While some files are automatically generated during `make`, others are generated by running the test harness as follows:
```
/backrest/test/test.pl --gen-only
```
Prior to any submission, the html version of the documentation should also be run.
```
/backrest/doc/doc.pl --out=html
```
> **NOTE:** `ERROR: [028]` regarding cache is invalid is OK; it just means there have been changes and the documentation will be built from scratch. In this case, be patient as the build could take 20 minutes or more depending on your system.

### Writing a Unit Test

The goal of unit testing is to have 100 percent coverage. Two files will usually be involved in this process:

- **define.yaml** - defines the number of tests to be run for each module and test file. There is a comment at the top of the file that provides more information about this file.

- **src/module/somefileTest.c** - where "somefile" is the path and name of the test file where the unit tests are located for the code being updated (e.g. `src/module/command/expireTest.c`).

#### define.yaml

Each module is separated by a line of asterisks (*) and each test within is separated by a line of dashes (-). In the example below, the module is `command` and the unit test is `check`. The number of calls to `testBegin()` in a unit test file will dictate the number following `total:`, in this case 2. Under `coverage:`, the list of files that will be tested must be listed followed by the coverage level, which should always be `full`.
```
# ********************************************************************************************************************************
  - name: command

    test:
      # ----------------------------------------------------------------------------------------------------------------------------
      - name: check
        total: 2

        coverage:
          command/check/common: full
          command/check/check: full
```

#### somefileTest.c

Assuming that a test file already exists, new unit tests will either go in a new `testBegin()` section or be added to an existing section.

Unit test files are organized in the test/src/module directory with the same directory structure as the source code being tested. For example, if new code is added to src/**command/expire**.c then test/src/module/**command/expire**Test.c will need to be updated.
```
// *****************************************************************************************************************************
if (testBegin("expireBackup()"))
```
**Setting up the command to be run**

If configuration options are required then a string list with the command and options must be defined and passed to the function `harnessCfgLoad()`. For example, the following will set up a test to run `pgbackrest --repo-path=test/test-0/repo info` command:
```
String *repoPath = strNewFmt("%s/repo", testPath());                    // create a string defining the repo path on the test system
StringList *argList = strLstNew();                                      // create an empty string list
strLstAdd(argList, strNewFmt("--repo-path=%s/", strPtr(repoPath)));     // add the --repo-path option as a formatted string
strLstAddZ(argList, "info");                                            // add the command
harnessCfgLoad(cfgCmdExpire, argList);                                  // load the command and option list into the test harness

TEST_RESULT_STR_Z(infoRender(), "No stanzas exist in the repository.\n", "text - no stanzas");  // run the test
```
Tests are run via macros. All test macros expect the first parameter to be the function to call that is being tested. With the exception of TEST_RESULT_VOID, the second parameter is the expected result, and with the exception of TEST_ERROR, the third parameter is a short description of the test. The most common macros are:

- `TEST_RESULT_STR` - Test the actual value of the string returned by the function.

- `TEST_RESULT_UINT` / `TEST_RESULT_INT` - Test for an unsigned integer / integer.

- `TEST_RESULT_BOOL` - Test a boolean return value.

- `TEST_RESULT_PTR` / `TEST_RESULT_PTR_NE` - Test a pointer: useful for testing if the pointer is `NULL` or not equal (`NE`) to `NULL`.

- `TEST_RESULT_VOID` - The function being tested returns a `void`. This is then usually followed by tests that ensure other actions occurred (e.g. a file was written to disk).

- `TEST_ERROR` / `TEST_ERROR_FMT` - Test for that a specific error code was raised with specific wording.

**Storing a file**

Sometimes it is necessary to store a file to the test directory. The following demonstrates that. It is not necessary to wrap the storagePutNP in TEST_RESULT_VOID, but doing so allows a short description to be displayed when running the tests (in this case "store a corrupt backup.info file").
```
String *content = strNew("bad content");
TEST_RESULT_VOID(
    storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/backup/demo/backup.info", strPtr(repoPath))),
        harnessInfoChecksum(content)), "store a corrupt backup.info file");
```
**Testing a log message**

If a function being tested logs something with `LOG_WARN`, `LOG_INFO` or other `LOG_` macro, then the logged message must be cleared before the end of the test by using the `harnessLogResult()` function.
```
harnessLogResult(
    "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");
```

### Running a Unit Test

Unit tests are run, and coverage of the code being tested is provided, by running the following. This example would run the test set from the **define.yaml** section detailed above.
```
/backrest/test/test.pl --vm-out --dev --module=command --test=check --coverage-only
```
Because no test run is specified and `--coverage-only` has been requested, a coverage report will be generated and written to the local file system under the pgBackRest directory `test/result/coverage` (or `test/coverage` prior to version 2.25) and will highlight code that has not been tested.

Sometimes it is useful to look at files that were generated during the test. The default for running any test is that, at the start/end of the test, the test harness will clean up all files and directories created. To override this behavior, a single test run must be specified and the option `--no-cleanup` provided. Again, continuing with the check command, we see in **define.yaml** above that there are two tests. Below, test one will be run and nothing will be cleaned up so that the files and directories in test/test-0 can be inspected.
```
/backrest/test/test.pl --vm-out --dev --module=command --test=check --coverage-only --run=1 --no-cleanup
```
For more details on running tests, again, please refer to the `README.md` file of the pgBackRest [test](https://github.com/pgbackrest/pgbackrest/blob/master/test) directory.
