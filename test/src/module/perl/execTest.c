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
        char *cmdLineParam[128];
        int cmdLineParamSize = 0;

        cmdLineParam[cmdLineParamSize++] = (char *)TEST_BACKREST_EXE;

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "')|[NULL]", "simple command");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = (char *)"--option";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', '--option')|[NULL]", "simple option");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = (char *)"--option2=value";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', '--option', '--option2=value')|[NULL]",
            "option with = before value");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = (char *)"--option3";
        cmdLineParam[cmdLineParamSize++] = (char *)"value";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', '--option', '--option2=value', '--option3', 'value')|[NULL]",
            "option with space before value");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdLineParam[cmdLineParamSize++] = (char *)"--perl-option=-I.";

        TEST_RESULT_STR(
            strPtr(strLstCat(perlCommand(cmdLineParamSize, cmdLineParam), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|-I.|" TEST_PERL_MAIN " --perl-option=\"-I.\"', '--option', '--option2=value', "
                "'--option3', 'value')|[NULL]",
            "perl option");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlExec()"))
    {
        StringList *param = strLstAdd(strLstAdd(strLstNew(), strNew(BOGUS_STR)), NULL);

        TEST_ERROR(perlExec(param), AssertError, "unable to exec BOGUS: No such file or directory");
    }
}
