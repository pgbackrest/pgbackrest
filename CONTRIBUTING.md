# pgBackRest <br/> Contributing to pgBackRest

## Introduction

This documentation is intended to assist contributors to pgBackRest by outlining some basic steps and guidelines for contributing to the project.

Code fixes or new features can be submitted via pull requests. Ideas for new features and improvements to existing functionality or documentation can be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). You may want to check the [Project Boards](https://github.com/pgbackrest/pgbackrest/projects) to see if your suggestion has already been submitted.

Bug reports should be [submitted as issues](https://github.com/pgbackrest/pgbackrest/issues). Please provide as much information as possible to aid in determining the cause of the problem.

You will always receive credit in the [release notes](http://www.pgbackrest.org/release.html) for your contributions.

Coding standards are defined in [CODING.md](https://github.com/pgbackrest/pgbackrest/blob/master/CODING.md) and some important coding details and an example are provided in the [Coding](#coding) section below. At a minimum, unit tests must be written and run and the documentation generated before submitting a Pull Request; see the [Testing](#testing) section below for details.

## Building a Development Environment

This example is based on Ubuntu 18.04, but it should work on many versions of Debian and Ubuntu.

pgbackrest-dev => Install development tools
```
sudo apt-get install rsync git devscripts build-essential valgrind lcov autoconf \
       autoconf-archive libssl-dev zlib1g-dev libxml2-dev libpq-dev pkg-config \
       libxml-checker-perl libyaml-perl libdbd-pg-perl liblz4-dev liblz4-tool \
       zstd libzstd-dev bzip2 libbz2-dev
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

If using a RHEL system, the CPAN XML parser is required for running `test.pl` and `doc.pl`. Instructions for installing Docker and the XML parse can be found in the `README.md` file of the pgBackRest [doc](https://github.com/pgbackrest/pgbackrest/blob/master/doc) directory in the section "The following is a sample CentOS/RHEL 7 configuration that can be used for building the documentation". NOTE that the `Install latex (for building PDF)` is not required since testing of the docs need only be run for HTML output.

## Coding

The following sections provide information on some important concepts needed for coding within pgBackRest.

### Memory Context

Memory is allocated inside contexts and can be long lasting (for objects) or temporary (for functions). In general, use `MEM_CONTEXT_NEW_BEGIN("somename")` for objects and `MEM_CONTEXT_TEMP_BEGIN()` for functions. See [memContext.h](https://github.com/pgbackrest/pgbackrest/blob/master/src/common/memContext.h) for more details and the [Coding Example](#coding-example) below.

### Logging

Logging is used in debugging with the built-in macros FUNCTION_LOG_* and FUNCTION_TEST_*, which are used to trace parameters passed to/returned from functions. FUNCTION_LOG_* macros are used for production logging whereas FUNCTION_TEST_* macros will be compiled out of production code. For functions where no parameter is valuable for debugging in production, use `FUNCTION_TEST_BEGIN()/FUNCTION_TEST_END()`, else use `FUNCTION_LOG_BEGIN(someLogLevel)/FUNCTION_LOG_END()`. See [debug.h](https://github.com/pgbackrest/pgbackrest/blob/master/src/common/debug.h) for more details and the [Coding Example](#coding-example) below.

Logging is also used for providing information to the user via the LOG_* macros, such as `LOG_INFO("some informational message")` and `LOG_WARN_FMT("no prior backup exists, %s backup has been changed to full", strZ(cfgOptionDisplay(cfgOptType)))` and also via THROW_* macros when throwing an error. See [log.h](https://github.com/pgbackrest/pgbackrest/blob/master/src/common/log.h) and[error.h](https://github.com/pgbackrest/pgbackrest/blob/master/src/common/error.h) for more details and the [Coding Example](#coding-example) below.

### Coding Example

In the hypothetical example below, code comments (double-slash or slash-asterisk) will explain the example. Refer to the sections above and [CODING.md](https://github.com/pgbackrest/pgbackrest/blob/master/CODING.md) for an introduction to the details provided here.

#### Example: basic object construction
```c
// Declare the publicly accessible variables in a structure named the object name with Pub appended
typedef struct MyObjPub         // First letter upper case
{
    MemContext *memContext;     // Pointer to memContext in which this object resides
    unsigned int myData;        // Contents of the myData variable
} MyObjPub;

// Declare the object type
struct MyObj
{
    MyObjPub pub;               // Publicly accessible variables must be first and named "pub"
    const String *name;         // Pointer to lightweight string object - see string.h
};

// Declare getters and setters inline for the publicly visible variables. Only setters require "Set" appended to the name.
__attribute__((always_inline)) static inline unsigned int
myObjMyData(const MyObj *const this)
{
    return THIS_PUB(MyObj)->myData;    // Use the built-in THIS_PUB macro
}

// TYPE and FROMAT macros for function logging
#define FUNCTION_LOG_MY_OBJ_TYPE                                            \
    MyObj *
#define FUNCTION_LOG_MY_OBJ_FORMAT(value, buffer, bufferSize)               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, myObjToLog, buffer, bufferSize)

// Create the logging function for displaying important information from the object
String *
myObjToLog(const MyObj *this)
{
    return strNewFmt(
        "{name: %s, myData: %u}", this->name == NULL ? NULL_Z : strZ(this->name), myObjMyData(this));
}

// Object constructor
MyObj *
myObjNew(unsigned int myData, const String *secretName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);              // Use FUNCTION_LOG_BEGIN with a log level for displaying in production
        FUNCTION_LOG_PARAM(UINT, myData);           // When log level is debug, myData variable will be logged
        FUNCTION_TEST_PARAM(STRING, secretName);    // FUNCTION_TEST_PARAM will not display secretName value in production logging
    FUNCTION_LOG_END();

    ASSERT((secretName != NULL || myData > 0);      // Development-only assertions (will be compiled out of production code)

    MyObj *this = NULL;                 // Declare the object in the parent memory context: it will live only as long as the parent

    MEM_CONTEXT_NEW_BEGIN("MyObj")      // Create a long lasting memory context with the name of the object
    {
        this = memNew(sizeof(MyObj));   // Allocate the memory size

        *this = (MyObj)                 // Initialize the object
        {
            .pub =
            {
                .memContext = memContextCurrent(),      // Set the memory context to the current MyObj memory context
                .myData = myData,                       // Copy the simple data type to the this object's memory context
            },
            .name = strDup(secretName),     // Duplicate the String data type to the this object's memory context
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(MyObj, this);
}

// Function using temporary memory context
String *
myObjDisplay(unsigned int myData)
{
    FUNCTION_TEST_BEGIN();                      // No parameters passed to this function will be logged in production
        FUNCTION_TEST_PARAM(UINT, myData);
    FUNCTION_TEST_END();

    String *result = NULL;     // Result is created in the current memory context  (referred to as "prior context" below)
    MEM_CONTEXT_TEMP_BEGIN()   // begins a new temporary context
    {
        String *resultStr = strNew("Hello");    // Allocates a string in the temporary memory context

        if (myData > 1)
            resultStr = strCatZ(" World");      // Appends a value to the string still in the temporary memory context
        else
            LOG_WARN("Am I not your World?");   // Logs a warning to the user

        MEM_CONTEXT_PRIOR_BEGIN()           // Switch to the prior context so the duplication of the string is in that context
        {
            result = strDup(resultStr);     // Create a copy of the string in the prior context where "result" was created
        }
        MEM_CONTEXT_PRIOR_END();            // Switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END();      // Free everything created inside this temporary memory context - i.e resultStr

    FUNCTION_TEST_RETURN(STRING, result);    // Return result but do not log the value in production
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
Prior to any submission, the html version of the documentation should also be run and the output checked by viewing the generated html on the local file system under `pgbackrest/doc/output/html`. More details can be found in the pgBackRest [doc/README.md](https://github.com/pgbackrest/pgbackrest/blob/master/doc/README.md) file.
```
pgbackrest/doc/doc.pl --out=html
```
> **NOTE:** `ERROR: [028]` regarding cache is invalid is OK; it just means there have been changes and the documentation will be built from scratch. In this case, be patient as the build could take 20 minutes or more depending on your system.

### Running Tests

Examples of test runs are provided in the following sections. There are several important options for running a test:

- `--dry-run` - without any other otpions, this will list all the available tests

- `--module` - identifies the module in which the test is located

- `--test` - the actual test set to be run

- `--run` - a number identifying the run within a test if testing a single run rather than the entire test

- `--dev` - sets several flags that are appropriate for development but should be omitted when performing final testing prior to submitting a Pull Request to the project. Most importantly, it reuses object files from the previous test run to speed testing.

- `--vm-out` - displays the test output (helpful for monitoring the progress)

- `--vm` - identifies the pre-build virtual machine when using Docker, otherwise the setting should be `none`

- `--coverage-only` - selects a vm to use (although best when overridden with `--vm=f32`) and provides a coverage report in `test/result/coverage/coverage.html` or `test/result/coverage/lcov/index.html`

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

    P00   INFO: test begin - log level info
    P00   INFO: builds required: bin
--> P00   INFO: 69 tests selected
                
    P00   INFO: P1-T01/69 - vm=none, module=common, test=error
           [filtered 66 lines of output]
    P00   INFO: P1-T68/69 - vm=none, module=performance, test=type
    P00   INFO: P1-T69/69 - vm=none, module=performance, test=storage
--> P00   INFO: DRY RUN COMPLETED SUCCESSFULLY
```

pgbackrest-dev => Run a test
```
pgbackrest/test/test.pl --vm=none --dev --vm-out --module=common --test=wait

--- output ---

    P00   INFO: test begin - log level info
    P00   INFO: check code autogenerate
    P00   INFO: cleanup old data
    P00   INFO: builds required: none
    P00   INFO: 1 test selected
                
    P00   INFO: P1-T1/1 - vm=none, module=common, test=wait
                
        run 1 - waitNew(), waitMore, and waitFree()
            l0018     expect AssertError: assertion 'waitTime <= 999999000' failed
        
        run 1/1 ------------- l0021 0ms wait
            l0025     new wait
            l0026         check remaining time
            l0027         check wait time
            l0028         check sleep time
            l0029         check sleep prev time
            l0030         no wait more
            l0033     new wait = 0.2 sec
            l0034         check remaining time
            l0035         check wait time
            l0036         check sleep time
            l0037         check sleep prev time
            l0038         check begin time
            l0044         lower range check
            l0045         upper range check
            l0047         free wait
            l0052     new wait = 1.1 sec
            l0053         check wait time
            l0054         check sleep time
            l0055         check sleep prev time
            l0056         check begin time
            l0062         lower range check
            l0063         upper range check
            l0065         free wait
        
        TESTS COMPLETED SUCCESSFULLY
    
    P00   INFO: P1-T1/1 - vm=none, module=common, test=wait
    P00   INFO: tested modules have full coverage
    P00   INFO: writing C coverage report
    P00   INFO: TESTS COMPLETED SUCCESSFULLY
```

An entire module can be run by using only the `--module` option.

pgbackrest-dev => Run a module
```
pgbackrest/test/test.pl --vm=none --dev --module=postgres

--- output ---

    P00   INFO: test begin - log level info
    P00   INFO: check code autogenerate
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

Build a container to run tests. The vm must be pre-configured but a variety are available. A vagrant file is provided in the test directory as an example of running in a virtual environment. The vm names are all three character abbreviations, e.g. `u18` for Ubuntu 18.04.

pgbackrest-dev => Build a VM
```
pgbackrest/test/test.pl --vm-build --vm=u18

--- output ---

    P00   INFO: test begin - log level info
    P00   INFO: Using cached pgbackrest/test:u18-base-20200924A image (d95d53e642fc1cea4a2b8e935ea7d9739f7d1c46) ...
    P00   INFO: Building pgbackrest/test:u18-test image ...
    P00   INFO: Build Complete
```
> **NOTE:** to build all the vms, just omit the `--vm` option above.

pgbackrest-dev => Run a Specific Test Run
```
pgbackrest/test/test.pl --vm=u18 --dev --module=mock --test=archive --run=2

--- output ---

    P00   INFO: test begin - log level info
    P00   INFO: check code autogenerate
    P00   INFO: cleanup old data and containers
    P00   INFO: builds required: bin, bin host
    P00   INFO:     bin dependencies have changed for u18, rebuilding...
    P00   INFO:     build bin for u18 (/home/vagrant/test/bin/u18)
    P00   INFO:     bin dependencies have changed for none, rebuilding...
    P00   INFO:     build bin for none (/home/vagrant/test/bin/none)
    P00   INFO: 1 test selected
                
    P00   INFO: P1-T1/1 - vm=u18, module=mock, test=archive, run=2
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

Assuming that a test file already exists, new unit tests will either go in a new `testBegin()` section or be added to an existing section. Each such section is a test run. The comment string passed to the `testBegin()` should reflect the function(s) being tested in the test run. Tests within a run should use `TEST_TITLE()` with a comment string describing the test.
```
// *****************************************************************************************************************************
if (testBegin("expireBackup()"))
{
    //--------------------------------------------------------------------------------------------------------------------------
    TEST_TITLE("manifest file removal");
```

#### Setting up the command to be run

The [/harnessConfig.h](https://github.com/pgbackrest/pgbackrest/blob/master/test/src/commonharnessConfig.h) describes a list of functions that should be used when configuration options are required for a command being tested. Options are set in a `StringList` which must be defined and passed to the function `harnessCfgLoad()` with the command. For example, the following will set up a test to run `pgbackrest --repo-path=test/test-0/repo info` command:
```
StringList *argList = strLstNew();                                      // create an empty string list
hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH_REPO);                 // add the --repo-path option
harnessCfgLoad(cfgCmdInfo, argList);                                    // load the command and option list into the test harness

TEST_RESULT_STR_Z(
    infoRender(), "No stanzas exist in the repository.\n", "text output - no stanzas");  // run the test
```

#### Storing a file

Sometimes it is desirable to store or manipulate files before or during a test and then confirm the contents. The [harnessStorage.h](https://github.com/pgbackrest/pgbackrest/blob/master/test/src/common/harnessStorage.h) file contains macros (e.g. `HRN_STORAGE_PUT` and `TEST_STORAGE_GET`) for doing this. In addition, `HRN_INFO_PUT` is convenient for writing out info files (archive.info, backup.info, backup.manifest) since it will automatically add header and checksum information.
```
HRN_STORAGE_PUT_EMPTY(
    storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");
```

#### Testing results

Tests are run and results confirmed via macros that are described in [harnessTest.h](https://github.com/pgbackrest/pgbackrest/blob/master/test/src/common/harnessTest.h). With the exception of TEST_ERROR, the third parameter is a short description of the test. Some of the more common macros are:

- `TEST_RESULT_STR` - Test the actual value of the string returned by the function.

- `TEST_RESULT_UINT` / `TEST_RESULT_INT` - Test for an unsigned integer / integer.

- `TEST_RESULT_BOOL` - Test a boolean value.

- `TEST_RESULT_PTR` / `TEST_RESULT_PTR_NE` - Test a pointer: useful for testing if the pointer is `NULL` or not equal (`NE`) to `NULL`.

- `TEST_RESULT_VOID` - The function being tested returns a `void`. This is then usually followed by tests that ensure other actions occurred (e.g. a file was written to disk).

- `TEST_ERROR` / `TEST_ERROR_FMT` - Test that a specific error code was raised with specific wording.

#### Testing a log message

If a function being tested logs something with `LOG_WARN`, `LOG_INFO` or other `LOG_` macro, then the logged message must be cleared before the end of the test by using the `harnessLogResult()` function.
```
harnessLogResult(
    "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");
```

#### Testing using child process

Sometimes it is useful to use a child process for testing. Below is a simple example. See [harnessFork.h](https://github.com/pgbackrest/pgbackrest/blob/master/test/src/common/harnessFork.h) for more details.
```
HARNESS_FORK_BEGIN()
{
    HARNESS_FORK_CHILD_BEGIN(0, false)
    {
        TEST_RESULT_INT_NE(
            lockAcquire(cfgOptionStr(cfgOptLockPath), strNew("stanza1"), STRDEF("999-ffffffff"), lockTypeBackup, 0, true),
            -1, "create backup/expire lock");

        sleepMSec(1000);
        lockRelease(true);
    }
    HARNESS_FORK_CHILD_END();

    HARNESS_FORK_PARENT_BEGIN()
    {
        sleepMSec(250);

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (no valid backups, backup/expire running)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): none present\n",
            "text - single stanza, no valid backups, backup/expire lock detected");

    }
    HARNESS_FORK_PARENT_END();
}
HARNESS_FORK_END();
```

#### Testing using a shim

A PostgreSQL libpq shim is provided to simulate interactions with PostgreSQL. Below is a simple example. See `harnessPq.h` for more details.
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

Unit tests are run and coverage of the code being tested is provided by running the test with the option `--coverage-only`. The following example would run the test set from the **define.yaml** section detailed above.
```
pgbackrest/test/test.pl --vm-out --dev --module=command --test=check --coverage-only
```
> **NOTE:** If the message `
ERROR: [125]: function not found at line` is displayed after `INFO: writing C coverage report` at the end of the test then try rerunning the test with `--vm=f32`

Because no test run is specified and `--coverage-only` has been requested, a coverage report will be generated and written to the local file system under the pgBackRest directory `test/result/coverage/lcov/index.html` and a file with only the highlighted code that has not been covered will be written to `test/result/coverage/coverage.html`.

If 100 percent code coverage has not been achieved, an error message will be displayed, for example: `ERROR: [125]: c module command/check/check is not fully covered`

**Debugging with files**

Sometimes it is useful to look at files that were generated during the test. The default for running any test is that, at the start/end of the test, the test harness will clean up all files and directories created. To override this behavior, a single test run must be specified and the option `--no-cleanup` provided. Again, continuing with the check command, from **define.yaml** above, there are four tests. Below, test one will be run and nothing will be cleaned up so that the files and directories in test/test-0 can be inspected.
```
pgbackrest/test/test.pl --vm-out --dev --module=command --test=check --coverage-only --run=1 --no-cleanup
```

## Adding an Option

Options can be added to a command or multiple commands. Options can be configuration file only, command-line only or valid for both. Once an option is successfully added, `config.auto.*`, `define.auto.*` and `parse.auto.*` files will automatically be generated by the build system.

To add an option, two files need be to be modified:

- `src/build/config/config.yaml`

- `doc/xml/reference.xml`

These files are discussed in the following sections along with how to verify the `help` command output.

### config.yaml

There are detailed comment blocks above each section that explain the rules for defining commands and options. Regarding options, there are two types: 1) command line only, and 2) configuration file. With the exception of passphrases, all configuration file options can be passed on the command line. To configure an option for the configuration file, the `section:` key must be present.

The `option:` section is broken into sub-sections by a simple comment divider (e.g. `# Repository options`) under which the options are organized alphabetically by option name. To better explain this section, the `online` option will be used as an example:

#### Example 1
```
online:
    type: boolean
    default: true
    negate: y
    command:
      backup: {}
      stanza-create: {}
      stanza-upgrade: {}
    command-role:
      default: {}
```

Note that `section:` is not present thereby making this a command-line only option defined as follows:

- `online` - the name of the option

- `type` - the type of the option. Valid values for types are: `boolean`, `hash`, `integer`, `list`, `path`, `size`, `string`, and `time`


- `negate` - being a command-line only boolean option, this rule would automatically default to false so it must be defined if the option is negatable. Ask yourself if negation makes sense, for example, would a --dry-run option make sense as --no-dry-run? If the answer is no, then this rule can be omitted as it would automatically default to false. Any boolean option that cannot be negatable, must be a command-line only and not a configuration file option as all configuration boolean options must be negatable.

- `default` - sets a default for the option if the option is not provided when the command is run. The default can be global or it can be specified for a specific command in the `command` section. However, boolean values always require a default, so if it were desirable for the default to be `false` for the `stanza-create` command then it would be coded as in the [Example 2](#example-2) below.


- `command` - list each command for which the option is valid. If a command is not listed, then the option is not valid for the command and an error will be thrown if it is attempted to be used for that command.

#### Example 2
```
online:
    type: boolean
    default: true
    command:
      backup:
        negate: y
      stanza-create:
        negate: n
      stanza-upgrade:
        negate: y
    command-role:
      default: {}
```

At compile time, the config.auto.h file will be generated to create the constants used in the code for the options. For the C code, any dashes in the option name will be removed, camel-cased and prefixed with `cfgOpt`, e.g. `repo-path` becomes `cfgOptRepoPath`.

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
