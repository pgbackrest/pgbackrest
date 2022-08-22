# pgBackRest <br/> Contributing to pgBackRest

## Table of Contents

[Introduction](#introduction)

[Building a Development Environment](#building-a-development-environment)

[Coding](#coding)

[Testing](#testing)

[Submitting a Pull Request](#submitting-a-pull-request)

## Introduction

This documentation is intended to assist contributors to pgBackRest by outlining some basic steps and guidelines for contributing to the project.

Code fixes or new features can be submitted via pull requests. Ideas for new features and improvements to existing functionality or documentation can be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). You may want to check the [Project Boards](https://github.com/pgbackrest/pgbackrest/projects) to see if your suggestion has already been submitted.

Bug reports should be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). Please provide as much information as possible to aid in determining the cause of the problem.

You will always receive credit in the [release notes](http://www.pgbackrest.org/release.html) for your contributions.

Coding standards are defined in [CODING.md](https://github.com/pgbackrest/pgbackrest/blob/main/CODING.md) and some important coding details and an example are provided in the [Coding](#coding) section below. At a minimum, unit tests must be written and run and the documentation generated before [submitting a Pull Request](#submitting-a-pull-request); see the [Testing](#testing) section below for details.

## Building a Development Environment

This example is based on Ubuntu 20.04, but it should work on many versions of Debian and Ubuntu.

pgbackrest-dev => Install development tools
```
sudo apt-get install rsync git devscripts build-essential valgrind lcov autoconf \
       autoconf-archive libssl-dev zlib1g-dev libxml2-dev libpq-dev pkg-config \
       libxml-checker-perl libyaml-perl libdbd-pg-perl liblz4-dev liblz4-tool \
       zstd libzstd-dev bzip2 libbz2-dev libyaml-dev ccache meson
```

Some unit tests and all the integration tests require Docker. Running in containers allows us to simulate multiple hosts, test on different distributions and versions of PostgreSQL, and use sudo without affecting the host system.

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

If using a RHEL-based system, the CPAN XML parser is required to run `test.pl` and `doc.pl`. Instructions for installing Docker and the XML parser can be found in the `README.md` file of the pgBackRest [doc](https://github.com/pgbackrest/pgbackrest/blob/main/doc) directory in the section "The following is a sample RHEL 7 configuration that can be used for building the documentation". NOTE that the "Install latex (for building PDF)" section is not required since testing of the docs need only be run for HTML output.

## Coding

The following sections provide information on some important concepts needed for coding within pgBackRest.

### Memory Contexts

Memory is allocated inside contexts and can be long lasting (for objects) or temporary (for functions). In general, use `OBJ_NEW_BEGIN(MyObj)` for objects and `MEM_CONTEXT_TEMP_BEGIN()` for functions. See [memContext.h](https://github.com/pgbackrest/pgbackrest/blob/main/src/common/memContext.h) for more details and the [Coding Example](#coding-example) below.

### Logging

Logging is used for debugging with the built-in macros `FUNCTION_LOG_*()` and `FUNCTION_TEST_*()` which are used to trace parameters passed to/returned from functions. `FUNCTION_LOG_*()` macros are used for production logging whereas `FUNCTION_TEST_*()` macros will be compiled out of production code. For functions where no parameter is valuable enough to justify the cost of debugging in production, use `FUNCTION_TEST_BEGIN()/FUNCTION_TEST_END()`, else use `FUNCTION_LOG_BEGIN(someLogLevel)/FUNCTION_LOG_END()`. See [debug.h](https://github.com/pgbackrest/pgbackrest/blob/main/src/common/debug.h) for more details and the [Coding Example](#coding-example) below.

Logging is also used for providing information to the user via the `LOG_*()` macros, such as `LOG_INFO("some informational message")` and `LOG_WARN_FMT("no prior backup exists, %s backup has been changed to full", strZ(cfgOptionDisplay(cfgOptType)))` and also via `THROW_*()` macros for throwing an error. See [log.h](https://github.com/pgbackrest/pgbackrest/blob/main/src/common/log.h) and [error.h](https://github.com/pgbackrest/pgbackrest/blob/main/src/common/error.h) for more details and the [Coding Example](#coding-example) below.

### Coding Example

The example below is not structured like an actual implementation and is intended only to provide an understanding of some of the more common coding practices. The comments in the example are only here to explain the example and are not representative of the coding standards. Refer to the Coding Standards document ([CODING.md](https://github.com/pgbackrest/pgbackrest/blob/main/CODING.md)) and sections above for an introduction to the concepts provided here. For an actual implementation, see [db.h](https://github.com/pgbackrest/pgbackrest/blob/main/src/db/db.h) and [db.c](https://github.com/pgbackrest/pgbackrest/blob/main/src/db/db.c).

#### Example: hypothetical basic object construction
```c
/*
 *  HEADER FILE - see db.h for a complete implementation example
 */

// Typedef the object declared in the C file
typedef struct MyObj MyObj;

// Constructor, and any functions in the header file, are all declared on one line
MyObj *myObjNew(unsigned int myData, const String *secretName);

// Declare the publicly accessible variables in a structure with Pub appended to the name
typedef struct MyObjPub         // First letter upper case
{
    unsigned int myData;        // Contents of the myData variable
} MyObjPub;

// Declare getters and setters inline for the publicly visible variables
// Only setters require "Set" appended to the name
__attribute__((always_inline)) static inline unsigned int
myObjMyData(const MyObj *const this)
{
    return THIS_PUB(MyObj)->myData;    // Use the built-in THIS_PUB macro
}

// Destructor
__attribute__((always_inline)) static inline void
myObjFree(MyObj *const this)
{
    objFree(this);
}

// TYPE and FORMAT macros for function logging
#define FUNCTION_LOG_MY_OBJ_TYPE                                            \
    MyObj *
#define FUNCTION_LOG_MY_OBJ_FORMAT(value, buffer, bufferSize)               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, myObjToLog, buffer, bufferSize)

/*
 * C FILE - see db.c for a more complete and actual implementation example
 */

// Declare the object type
struct MyObj
{
    MyObjPub pub;               // Publicly accessible variables must be first and named "pub"
    const String *name;         // Pointer to lightweight string object - see string.h
};

// Object constructor, and any functions in the C file, have the return type and function signature on separate lines
MyObj *
myObjNew(unsigned int myData, const String *secretName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);              // Use FUNCTION_LOG_BEGIN with a log level for displaying in production
        FUNCTION_LOG_PARAM(UINT, myData);           // When log level is debug, myData variable will be logged
        FUNCTION_TEST_PARAM(STRING, secretName);    // FUNCTION_TEST_PARAM will not display secretName value in production logging
    FUNCTION_LOG_END();

    ASSERT(secretName != NULL || myData > 0);       // Development-only assertions (will be compiled out of production code)

    MyObj *this = NULL;                 // Declare the object in the parent memory context: it will live only as long as the parent

    OBJ_NEW_BEGIN(MyObj)                // Create a long lasting memory context with the name of the object
    {
        this = OBJ_NEW_ALLOC();         // Allocate the memory required by the object

        *this = (MyObj)                 // Initialize the object
        {
            .pub =
            {
                .myData = myData,                       // Copy the simple data type to this object
            },
            .name = strDup(secretName),     // Duplicate the String data type to the this object's memory context
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(MyObj, this);
}

// Function using temporary memory context
String *
myObjDisplay(unsigned int myData)
{
    FUNCTION_TEST_BEGIN();                      // No parameters passed to this function will be logged in production
        FUNCTION_TEST_PARAM(UINT, myData);
    FUNCTION_TEST_END();

    String *result = NULL;     // Result is created in the caller's memory context (referred to as "prior context" below)

    MEM_CONTEXT_TEMP_BEGIN()   // Begin a new temporary context
    {
        String *resultStr = strNewZ("Hello");    // Allocate a string in the temporary memory context

        if (myData > 1)
            resultStr = strCatZ(" World");      // Append a value to the string still in the temporary memory context
        else
            LOG_WARN("Am I not your World?");   // Log a warning to the user

        MEM_CONTEXT_PRIOR_BEGIN()           // Switch to the prior context so the string duplication is in the caller's context
        {
            result = strDup(resultStr);     // Create a copy of the string in the caller's context
        }
        MEM_CONTEXT_PRIOR_END();            // Switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END();      // Free everything created inside this temporary memory context - i.e resultStr

    FUNCTION_TEST_RETURN(STRING, result);    // Return result but do not log the value in production
}

// Create the logging function for displaying important information from the object
String *
myObjToLog(const MyObj *this)
{
    return strNewFmt(
        "{name: %s, myData: %u}", this->name == NULL ? NULL_Z : strZ(this->name), myObjMyData(this));
}
```

## Testing

A list of all possible test combinations can be viewed by running:
```
pgbackrest/test/test.pl --dry-run
```
While some files are automatically generated during `make`, others are generated by running the test harness as follows:
```
pgbackrest/test/test.pl --gen-only
```
Prior to any submission, the html version of the documentation should also be run and the output checked by viewing the generated html on the local file system under `pgbackrest/doc/output/html`. More details can be found in the pgBackRest [doc/README.md](https://github.com/pgbackrest/pgbackrest/blob/main/doc/README.md) file.
```
pgbackrest/doc/doc.pl --out=html
```
> **NOTE:** `ERROR: [028]` regarding cache is invalid is OK; it just means there have been changes and the documentation will be built from scratch. In this case, be patient as the build could take 20 minutes or more depending on your system.

### Running Tests

Examples of test runs are provided in the following sections. There are several important options for running a test:

- `--dry-run` - without any other options, this will list all the available tests

- `--module` - identifies the module in which the test is located

- `--test` - the actual test set to be run

- `--run` - a number identifying the run within a test if testing a single run rather than the entire test

- `--vm-out` - displays the test output (helpful for monitoring the progress)

- `--vm` - identifies the pre-built container when using Docker, otherwise the setting should be `none`. See [test.yml](https://github.com/pgbackrest/pgbackrest/blob/main/.github/workflows/test.yml) for a list of valid vm codes noted by `param: test`.

For more options, run the test or documentation engine with the `--help` option:
```
pgbackrest/test/test.pl --help
pgbackrest/doc/doc.pl --help
```

#### Without Docker

If Docker is not installed, then the available tests can be listed using `--vm=none`, and each test must then be run with `--vm=none`.

pgbackrest-dev => List tests that don't require a container
```
pgbackrest/test/test.pl --vm=none --dry-run

--- output ---

    P00   INFO: test begin on x86_64 - log level info
    P00   INFO: clean autogenerate code
    P00   INFO: builds required: bin
--> P00   INFO: 74 tests selected
                
    P00   INFO: P1-T01/74 - vm=none, module=common, test=error
           [filtered 71 lines of output]
    P00   INFO: P1-T73/74 - vm=none, module=performance, test=type
    P00   INFO: P1-T74/74 - vm=none, module=performance, test=storage
--> P00   INFO: DRY RUN COMPLETED SUCCESSFULLY
```

pgbackrest-dev => Run a test
```
pgbackrest/test/test.pl --vm=none --vm-out --module=common --test=wait

--- output ---

    P00   INFO: test begin on x86_64 - log level info
    P00   INFO: autogenerate configure
    P00   INFO:     autogenerated version in configure.ac script: no changes
    P00   INFO:     autogenerated configure script: no changes
    P00   INFO: autogenerate code
    P00   INFO: cleanup old data
    P00   INFO: builds required: none
    P00   INFO: 1 test selected
                
    P00   INFO: P1-T1/1 - vm=none, module=common, test=wait
                
        run 1 - waitNew(), waitMore, and waitFree()
            L0018     expect AssertError: assertion 'waitTime <= 999999000' failed
        
        run 1/1 ------------- L0021 0ms wait
            L0025     new wait
            L0026         check remaining time
            L0027         check wait time
            L0028         check sleep time
            L0029         check sleep prev time
            L0030         no wait more
            L0033     new wait = 0.2 sec
            L0034         check remaining time
            L0035         check wait time
            L0036         check sleep time
            L0037         check sleep prev time
            L0038         check begin time
            L0044         lower range check
            L0045         upper range check
            L0047         free wait
            L0052     new wait = 1.1 sec
            L0053         check wait time
            L0054         check sleep time
            L0055         check sleep prev time
            L0056         check begin time
            L0062         lower range check
            L0063         upper range check
            L0065         free wait
        
        TESTS COMPLETED SUCCESSFULLY
    
    P00   INFO: P1-T1/1 - vm=none, module=common, test=wait
    P00   INFO: tested modules have full coverage
    P00   INFO: writing C coverage report
    P00   INFO: TESTS COMPLETED SUCCESSFULLY
```

An entire module can be run by using only the `--module` option.

pgbackrest-dev => Run a module
```
pgbackrest/test/test.pl --vm=none --module=postgres

--- output ---

    P00   INFO: test begin on x86_64 - log level info
    P00   INFO: autogenerate configure
    P00   INFO:     autogenerated version in configure.ac script: no changes
    P00   INFO:     autogenerated configure script: no changes
    P00   INFO: autogenerate code
    P00   INFO: cleanup old data
    P00   INFO: builds required: none
    P00   INFO: 2 tests selected
                
    P00   INFO: P1-T1/2 - vm=none, module=postgres, test=client
    P00   INFO: P1-T2/2 - vm=none, module=postgres, test=interface
    P00   INFO: tested modules have full coverage
    P00   INFO: writing C coverage report
    P00   INFO: TESTS COMPLETED SUCCESSFULLY
```

#### With Docker

Build a container to run tests. The vm must be pre-configured but a variety are available. A vagrant file is provided in the test directory as an example of running in a virtual environment. The vm names are all three character abbreviations, e.g. `u20` for Ubuntu 20.04.

pgbackrest-dev => Build a VM
```
pgbackrest/test/test.pl --vm-build --vm=u20

--- output ---

    P00   INFO: test begin on x86_64 - log level info
    P00   INFO: Using cached pgbackrest/test:u20-base-20220519A image (17c74ed3fd3d76119f672740d77caf873fc57bac) ...
    P00   INFO: Building pgbackrest/test:u20-test image ...
    P00   INFO: Build Complete
```
> **NOTE:** to build all the vms, just omit the `--vm` option above.

pgbackrest-dev => Run a Specific Test Run
```
pgbackrest/test/test.pl --vm=u20 --module=mock --test=archive --run=2

--- output ---

    P00   INFO: test begin on x86_64 - log level info
    P00   INFO: autogenerate configure
    P00   INFO:     autogenerated version in configure.ac script: no changes
    P00   INFO:     autogenerated configure script: no changes
    P00   INFO: autogenerate code
    P00   INFO: cleanup old data and containers
    P00   INFO: builds required: bin, bin host
    P00   INFO:     bin build for u20 (/home/vagrant/test/bin/u20)
    P00   INFO:         bin dependencies have changed, rebuilding
    P00   INFO:     clean bin build for none (/home/vagrant/test/bin/none)
    P00   INFO: 1 test selected
                
    P00   INFO: P1-T1/1 - vm=u20, module=mock, test=archive, run=2
    P00   INFO: no code modules had all tests run required for coverage
    P00   INFO: TESTS COMPLETED SUCCESSFULLY
```

### Writing a Unit Test

The goal of unit testing is to have 100 percent code coverage. Two files will usually be involved in this process:

- **define.yaml** - defines the number of tests to be run for each module and test file. There is a comment at the top of the file that provides more information about this file.

- **src/module/somefileTest.c** - where "somefile" is the path and name of the test file where the unit tests are located for the code being updated (e.g. `src/module/command/expireTest.c`).

#### define.yaml

Each module is separated by a line of asterisks (*) and each test within is separated by a line of dashes (-). In the example below, the module is `command` and the unit test is `check`. The number of calls to `testBegin()` in a unit test file will dictate the number following `total:`, in this case 4. Under `coverage:`, the list of files that will be tested.
```
# ********************************************************************************************************************************
  - name: command

    test:
      # ----------------------------------------------------------------------------------------------------------------------------
      - name: check
        total: 4
        containerReq: true

        coverage:
          - command/check/common
          - command/check/check
```

#### somefileTest.c

Unit test files are organized in the `test/src/module` directory with the same directory structure as the source code being tested. For example, if new code is added to src/**command/expire**.c then test/src/module/**command/expire**Test.c will need to be updated.

Assuming that a test file already exists, new unit tests will either go in a new `testBegin()` section or be added to an existing section. Each such section is a test run. The comment string passed to `testBegin()` should reflect the function(s) being tested in the test run. Tests within a run should use `TEST_TITLE()` with a comment string describing the test.
```
// *****************************************************************************************************************************
if (testBegin("expireBackup()"))
{
    // -------------------------------------------------------------------------------------------------------------------------
    TEST_TITLE("manifest file removal");
```

#### Setting up the command to be run

The [harnessConfig.h](https://github.com/pgbackrest/pgbackrest/blob/main/test/src/common/harnessConfig.h) describes a list of functions that should be used when configuration options are required for a command being tested. Options are set in a `StringList` which must be defined and passed to the `HRN_CFG_LOAD()` macro with the command. For example, the following will set up a test to run `pgbackrest --repo-path=test/test-0/repo info` command on multiple repositories, one of which is encrypted:
```
StringList *argList = strLstNew();                                  // Create an empty string list
hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");          // Add the --repo-path option
hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");   // Add the --repo2-path option
hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);  // Add the --repo2-cipher-type option
hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);        // Set environment variable for the --repo2-cipher-pass option
HRN_CFG_LOAD(cfgCmdInfo, argList);                                  // Load the command and option list into the test harness
```

#### Storing a file

Sometimes it is desirable to store or manipulate files before or during a test and then confirm the contents. The [harnessStorage.h](https://github.com/pgbackrest/pgbackrest/blob/main/test/src/common/harnessStorage.h) file contains macros (e.g. `HRN_STORAGE_PUT` and `TEST_STORAGE_GET`) for doing this. In addition, `HRN_INFO_PUT` is convenient for writing out info files (archive.info, backup.info, backup.manifest) since it will automatically add header and checksum information.
```
HRN_STORAGE_PUT_EMPTY(
    storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");
```

#### Testing results

Tests are run and results confirmed via macros that are described in [harnessTest.h](https://github.com/pgbackrest/pgbackrest/blob/main/test/src/common/harnessTest.h). With the exception of TEST_ERROR, the third parameter is a short description of the test. Some of the more common macros are:

- `TEST_RESULT_STR` - Test the actual value of the string returned by the function.

- `TEST_RESULT_UINT` / `TEST_RESULT_INT` - Test for an unsigned integer / integer.

- `TEST_RESULT_BOOL` - Test a boolean value.

- `TEST_RESULT_PTR` / `TEST_RESULT_PTR_NE` - Test a pointer: useful for testing if the pointer is `NULL` or not equal (`NE`) to `NULL`.

- `TEST_RESULT_VOID` - The function being tested returns a `void`. This is then usually followed by tests that ensure other actions occurred (e.g. a file was written to disk).

- `TEST_ERROR` / `TEST_ERROR_FMT` - Test that a specific error code was raised with specific wording.
> **NOTE:** `HRN_*` macros should be used only for test setup and cleanup. `TEST_*` macros must be used for testing results.

#### Testing a log message

If a function being tested logs something with `LOG_WARN`, `LOG_INFO` or other `LOG_*()` macro, then the logged message must be cleared before the end of the test by using the `TEST_RESULT_LOG()/TEST_RESULT_LOG_FMT()` macros.
```
TEST_RESULT_LOG(
    "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");
```
In the above, `Pxx` indicates the process (P) and the process number (xx), e.g. P00, P01.

#### Testing using child process

Sometimes it is useful to use a child process for testing. Below is a simple example. See [harnessFork.h](https://github.com/pgbackrest/pgbackrest/blob/main/test/src/common/harnessFork.h) for more details.
```
HRN_FORK_BEGIN()
{
    HRN_FORK_CHILD_BEGIN()
    {
        TEST_RESULT_INT_NE(
            lockAcquire(cfgOptionStr(cfgOptLockPath), STRDEF("stanza1"), STRDEF("999-ffffffff"), lockTypeBackup, 0, true),
            -1, "create backup/expire lock");

        // Notify parent that lock has been acquired
        HRN_FORK_CHILD_NOTIFY_PUT();

        // Wait for parent to allow release lock
        HRN_FORK_CHILD_NOTIFY_GET();

        lockRelease(true);
    }
    HRN_FORK_CHILD_END();

    HRN_FORK_PARENT_BEGIN()
    {
        // Wait for child to acquire lock
        HRN_FORK_PARENT_NOTIFY_GET(0);

        HRN_CFG_LOAD(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (no valid backups, backup/expire running)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): none present\n",
            "text - single stanza, no valid backups, backup/expire lock detected");

        // Notify child to release lock
        HRN_FORK_PARENT_NOTIFY_PUT(0);
    }
    HRN_FORK_PARENT_END();
}
HRN_FORK_END();
```

#### Testing using a shim

A PostgreSQL libpq shim is provided to simulate interactions with PostgreSQL. Below is a simple example. See [harnessPq.h](https://github.com/pgbackrest/pgbackrest/blob/main/test/src/common/harnessPq.h) for more details.
```
// Set up two standbys but no primary
harnessPqScriptSet((HarnessPq [])
{
    HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
    HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/pgdata", true, NULL, NULL),

    // Close the "inner" session first (8) then the outer (1)
    HRNPQ_MACRO_CLOSE(8),
    HRNPQ_MACRO_CLOSE(1),

    HRNPQ_MACRO_DONE()
});

TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");
```

### Running a Unit Test

**Code Coverage**

Unit tests are run for all files that are listed in `define.yaml` and a coverage report generated for each file listed under the tag `coverage:`. Note that some files are listed in multiple `coverage:` sections for a module; in this case, each test for the file being modified should be specified for the module in which the file exists (e.g. `--module=storage --test=posix --test=gcs`, etc.) or, alternatively, simply run the module without the `--test` option. It is recommended that a `--vm` be specified since running the same test for multiple vms is unnecessary for coverage. The following example would run the test set from the **define.yaml** section detailed above.
```
pgbackrest/test/test.pl --vm-out --module=command --test=check --vm=u20
```
> **NOTE:** Not all systems perform at the same speed, so if a test is timing out, try rerunning with another vm.

Because a test run has not been specified, a coverage report will be generated and written to the local file system under the pgBackRest directory `test/result/coverage/lcov/index.html` and a file with only the highlighted code that has not been covered will be written to `test/result/coverage/coverage.html`.

If 100 percent code coverage has not been achieved, an error message will be displayed, for example: `ERROR: [125]: c module command/check/check is not fully covered`

**Debugging with files**

Sometimes it is useful to look at files that were generated during the test. The default for running any test is that, at the start/end of the test, the test harness will clean up all files and directories created. To override this behavior, a single test run must be specified and the option `--no-cleanup` provided. Again, continuing with the check command, from **define.yaml** above, there are four tests. Below, test one will be run and nothing will be cleaned up so that the files and directories in `test/test-0` can be inspected.
```
pgbackrest/test/test.pl --vm-out --module=command --test=check --run=1 --no-cleanup
```

### Understanding Test Output

The following is a small sample of a typical test output.
```
run 8 - expireTimeBasedBackup()

run 8/1 ------------- L2285 no current backups
    000.002s          L2298     empty backup.info
    000.009s 000.007s L2300     no backups to expire
```
**run 8 - expireTimeBasedBackup()** - indicates the run number (8) within the module and the parameter provided to testBegin, e.g. `testBegin("expireTimeBasedBackup()")`

**run 8/1 ------------- L2285 no current backups** - this is the first test (1) in run 8 which is the `TEST_TITLE("no current backups");` at line number 2285.

**000.002s L2298 empty backup.info** - the first number, 000.002s, is the time in seconds that the test started from the beginning of the run. L2298 is the line number of the test and `empty backup.info` is the test comment.

**000.009s 000.007s L2300 no backups to expire** - again, 000.009s, is the time in seconds that the test started from the beginning of the run. The second number, 000.007s, is the run time of the **previous** test (i.e. `empty backup.info` test took 000.007 seconds to execute). L2300 is the line number of the test and `no backups to expire` is the test comment.

## Adding an Option

Options can be added to a command or multiple commands. Options can be configuration file only, command-line only or valid for both. Once an option is successfully added, the `config.auto.h` and `parse.auto.c.inc` files will automatically be generated by the build system.

To add an option, two files need be to be modified:

- `src/build/config/config.yaml`

- `src/build/help/help.xml`

These files are discussed in the following sections along with how to verify the `help` command output.

### config.yaml

There are detailed comment blocks above each section that explain the rules for defining commands and options. Regarding options, there are two types: 1) command line only, and 2) configuration file. With the exception of secrets, all configuration file options can be passed on the command line. To configure an option for the configuration file, the `section:` key must be present.

The `option:` section is broken into sub-sections by a simple comment divider (e.g. `# Repository options`) under which the options are organized alphabetically by option name. To better explain this section, two hypothetical examples will be discussed. For more details, see [config.yaml](https://github.com/pgbackrest/pgbackrest/blob/main/src/build/config/config.yaml).

#### EXAMPLE 1 hypothetical command line only option
```
set:
    type: string
    command:
      backup:
        depend:
          option: stanza
        required: false
      restore:
        default: latest
    command-role:
      main: {}
```

Note that `section:` is not present thereby making this a command-line only option defined as follows:

- `set` - the name of the option

- `type` - the type of the option. Valid values for types are: `boolean`, `hash`, `integer`, `list`, `path`, `size`, `string`, and `time`

- `command` - list each command for which the option is valid. If a command is not listed, then the option is not valid for the command and an error will be thrown if it is attempted to be used for that command. In this case the valid commands are `backup` and `restore`.

- `backup` - details the requirements for the `--set` option for the `backup` command. It is dependent on the option `--stanza`, meaning it is only allowed to be specified for the `backup` command if the `--stanza` option has been specified. And `required: false` indicates that the `--set` option is never required, even with the dependency.

- `restore` - details the requirements for the `--set` option for the `restore` command. Since `required:` is omitted, it is not required to be set by the user but it is required by the command and will default to `latest` if it has not been specified by the user.

- `command-role` - defines the processes for which the option is valid. `main` indicates the option will be used by the main process and not be passed on to other local/remote processes.

#### EXAMPLE 2 hypothetical configuration file option
```
repo-test-type:
    section: global
    type: string
    group: repo
    default: full
    allow-list:
      - full
      - diff
      - incr
    command:
      backup: {}
      restore: {}
    command-role:
      main: {}
```

- `repo-test-type` - the name of the option

- `section` - the section of the configuration file where this option is valid (omitted for command line only options, see [Example 1](#example-1-hypothetical-command-line-only-option) above)

- `type` - the type of the option. Valid values for types are: `boolean`, `hash`, `integer`, `list`, `path`, `size`, `string`, and `time`

- `group` - indicates that this option is part of the `repo` group of indexed options and therefore will follow the indexing rules e.g. `repo1-test-type`.

- `default` - sets a default for the option if the option is not provided when the command is run. The default can be global (as it is here) or it can be specified for a specific command in the command section (as in [Example 1](#example-1-hypothetical-command-line-only-option) above).

- `allow-list` - lists the allowable values for the option for all commands for which the option is valid.

- `command` - list each command for which the option is valid. If a command is not listed, then the option is not valid for the command and an error will be thrown if it is attempted to be used for that command. In this case the valid commands are `backup` and `restore`.

- `command-role` - defines the processes for which the option is valid. `main` indicates the option will be used by the main process and not be passed on to other local/remote processes.

At compile time, the `config.auto.h` file will be generated to contain the constants used for options in the code. For the C enums, any dashes in the option name will be removed, camel-cased and prefixed with `cfgOpt`, e.g. `repo-path` becomes `cfgOptRepoPath`.

### help.xml

All options must be documented or the system will error during the build. To add an option, find the command section identified by `command id="COMMAND"` section where `COMMAND` is the name of the command (e.g. `expire`) or, if the option is used by more than one command and the definition for the option is the same for all of the commands, the `operation-general title="General Options"` section.

To add an option, add the following to the `<option-list>` section; if it does not exist, then wrap the following in `<option-list>` `</option-list>`. This example uses the boolean option `force` of the `restore` command. Simply replace that with your new option and the appropriate `summary`, `text` and `example`.
```
<option id="force" name="Force">
    <summary>Force a restore.</summary>

    <text>By itself this option forces the <postgres/> data and tablespace paths to be completely overwritten.  In combination with <br-option>--delta</br-option> a timestamp/size delta will be performed instead of using checksums.</text>

    <example>y</example>
</option>
```
> **IMPORTANT:** A period (.) is required to end the `summary` section.

### Testing the help

It is important to run the `help` command unit test after adding an option in case a change is required:
```
pgbackrest/test/test.pl --module=command --test=help --vm-out
```
To verify the `help` command output, build the pgBackRest executable:
```
pgbackrest/test/test.pl --vm=none --build-only
```
Use the pgBackRest executable to test the help output:
```
test/bin/none/pgbackrest help backup repo-type
```

### Testing the documentation

To quickly view the HTML documentation, the `--no-exe` option can be passed to the documentation generator in order to bypass executing the code elements:
```
pgbackrest/doc/doc.pl --out=html --no-exe
```
The generated HTML files will be placed in the `doc/output/html` directory where they can be viewed locally in a browser.

If Docker is installed, it will be used by the documentation generator to execute the code elements while building the documentation, therefore, the `--no-exe` should be omitted, (i.e. `pgbackrest/doc/doc.pl --output=html`). `--no-cache` may be used to force a full build even when no code elements have changed since the last build. `--pre` will reuse the container definitions from the prior build and saves time during development.

The containers created for documentation builds can be useful for manually testing or trying out new code or features. The following demonstrates building through just the `quickstart` section of the `user-guide` without encryption.
```
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/quickstart --var=encrypt=n --no-cache --pre
```
The resulting Docker containers can be listed with `docker ps` and the container can be entered with `docker exec doc-pg-primary bash`. Additionally, the `-u` option can be added for entering the container as a specific user (e.g. `postgres`).

## Submitting a Pull Request

Before submitting a Pull Request:

- Does it meet the [coding standards](https://github.com/pgbackrest/pgbackrest/blob/main/CODING.md)?

- Have [Unit Tests](#writing-a-unit-test) been written and [run](#running-a-unit-test) with 100% coverage?

- If your submission includes changes to the help or online documentation, have the [help](#testing-the-help) and [documentation](#testing-the-documentation) tests been run?

- Has it passed continuous integration testing? Simply renaming your branch with the appendix `-cig` and pushing it to your GitHub account will initiate GitHub Actions to run CI tests.

When submitting a Pull Request:

- Provide a short submission title.

- Write a detailed comment to describe the purpose of your submission and any issue(s), if any, it is resolving; a link to the GitHub issue is also helpful.

- Select the `integration` branch as the base for your PR, do not select `main` nor any other branch.

After submitting a Pull Request:

- One or more reviewers will be assigned.

- Respond to any issues (conversations) in GitHub but do not resolve the conversation; the reviewer is responsible for ensuring the issue raised has been resolved and marking the conversation resolved. It is helpful to supply the commit in your reply if one was submitted to fix the issue.

Lastly, thank you for contributing to pgBackRest!
