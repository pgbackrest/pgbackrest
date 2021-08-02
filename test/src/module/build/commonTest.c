/***********************************************************************************************************************************
Test Build Common
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("Yaml"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and error");

        const Buffer *buffer = BUFSTRZ(
            "test:\n"
            "   main: [0, 1]\n"
            "  default: text\n");

        Yaml *yaml = NULL;
        TEST_ASSIGN(yaml, yamlNew(buffer), "new yaml")
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeMapBegin), "map begin event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeScalar), "scalar event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeMapBegin), "map begin event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeScalar), "scalar event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeSeqBegin), "seq begin event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeScalar), "scalar event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeScalar), "scalar event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeSeqEnd), "seq end event");
        TEST_RESULT_VOID(yamlEventNextCheck(yaml, yamlEventTypeMapEnd), "map end event");
        TEST_ERROR(
            yamlEventNextCheck(yaml, yamlEventTypeScalar), FormatError,
            "yaml parse error: did not find expected key at line: 3 column: 3");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("boolean parse");

        TEST_RESULT_BOOL(yamlBoolParse((YamlEvent){.value = STRDEF("true")}), true, "true");
        TEST_RESULT_BOOL(yamlBoolParse((YamlEvent){.value = STRDEF("false")}), false, "false");
        TEST_ERROR(yamlBoolParse((YamlEvent){.value = STRDEF("ack")}), FormatError, "invalid boolean 'ack'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("type map (remaining types)");

        TEST_RESULT_UINT(yamlEventType(YAML_STREAM_END_EVENT), yamlEventTypeStreamEnd, "stream end");
        TEST_RESULT_UINT(yamlEventType(YAML_DOCUMENT_END_EVENT), yamlEventTypeDocEnd, "doc end");
        TEST_RESULT_UINT(yamlEventType(YAML_ALIAS_EVENT), yamlEventTypeAlias, "alias");
        TEST_RESULT_UINT(yamlEventType(YAML_NO_EVENT), yamlEventTypeNone, "none");

        TEST_ERROR(
            yamlEventCheck((YamlEvent){.type = yamlEventTypeAlias}, yamlEventTypeScalar), FormatError,
                "expected event type 'scalar' but got 'alias' at line 0, column 0");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
