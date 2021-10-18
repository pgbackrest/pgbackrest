/***********************************************************************************************************************************
Test System User/Group Management
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("all"))
    {
        TEST_RESULT_VOID(userInit(), "initialize info");
        TEST_RESULT_VOID(userInit(), "initialize info again");

        TEST_RESULT_UINT(userId(), TEST_USER_ID, "check user id");
        TEST_RESULT_UINT(userIdFromName(userName()), userId(), "get user id");
        TEST_RESULT_UINT(userIdFromName(NULL), (uid_t)-1, "get null user id");
        TEST_RESULT_UINT(userIdFromName(STRDEF("bogus")), (uid_t)-1, "get bogus user id");
        TEST_RESULT_STR(userName(), TEST_USER_STR, "check user name");
        TEST_RESULT_STR_Z(userNameFromId(77777), NULL, "invalid user name by id");
        TEST_RESULT_BOOL(userRoot(), false, "check user is root");

        TEST_RESULT_UINT(groupId(), TEST_GROUP_ID, "check group id");
        TEST_RESULT_UINT(groupIdFromName(groupName()), groupId(), "get group id");
        TEST_RESULT_UINT(groupIdFromName(NULL), (gid_t)-1, "get null group id");
        TEST_RESULT_UINT(groupIdFromName(STRDEF("bogus")), (uid_t)-1, "get bogus group id");
        TEST_RESULT_STR(groupName(), TEST_GROUP_STR, "check name name");
        TEST_RESULT_STR_Z(groupNameFromId(77777), NULL, "invalid group name by id");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
