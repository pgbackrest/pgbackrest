/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/

#define TEST_ENV_EXE                                                "/usr/bin/env"
#define TEST_PERL_EXE                                               "perl"
#define TEST_BACKREST_EXE                                           "/path/to/pgbackrest"
#define TEST_PERL_MAIN                                                                                                             \
    "-MpgBackRest::Main|-e|pgBackRest::Main::main('" TEST_BACKREST_EXE ""

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlCommand()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        const char *cmdLineParam[128];
        int cmdLineParamSize = 0;

        cmdLineParam[cmdLineParamSize++] = TEST_BACKREST_EXE;
        cmdLineParam[cmdLineParamSize++] = "backup";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', 'backup')|[NULL]", "simple command");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "--compress";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', '--compress', 'backup')|[NULL]", "simple option");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "--db-host=db1";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', '--compress', '--db1-host', 'db1', 'backup')|[NULL]",
            "option with = before value");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "--db-user";
        cmdLineParam[cmdLineParamSize++] = "postgres";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN
                "', '--compress', '--db1-host', 'db1', '--db1-user', 'postgres', 'backup')|[NULL]",
            "option with space before value");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "--perl-option=-I.";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|-I.|" TEST_PERL_MAIN
                " --perl-option=\"-I.\"', '--compress', '--db1-host', 'db1', '--db1-user', 'postgres', 'backup')|[NULL]",
            "perl option");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "--no-online";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|-I.|" TEST_PERL_MAIN
                " --perl-option=\"-I.\"', '--compress', '--db1-host', 'db1', '--db1-user', 'postgres', '--no-online',"
                " 'backup')|[NULL]",
            "perl option");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = "cmdarg1";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|-I.|" TEST_PERL_MAIN
                " --perl-option=\"-I.\"', '--compress', '--db1-host', 'db1', '--db1-user', 'postgres', '--no-online', 'backup',"
                " 'cmdarg1')|[NULL]",
            "perl option");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlExec()"))
    {
        StringList *param = strLstAdd(strLstAdd(strLstNew(), strNew(BOGUS_STR)), NULL);

        TEST_ERROR(perlExec(param), AssertError, "unable to exec BOGUS: No such file or directory");
    }
}
