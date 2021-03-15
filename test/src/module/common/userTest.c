/***********************************************************************************************************************************
Test System User/Group Management
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("all"))
    {
        TEST_RESULT_VOID(userInit(), "initialize info");
        TEST_RESULT_VOID(userInit(), "initialize info again");

        TEST_RESULT_UINT(userId(), getuid(), "check user id");
        TEST_RESULT_UINT(userIdFromName(userName()), userId(), "get user id");
        TEST_RESULT_UINT(userIdFromName(NULL), (uid_t)-1, "get null user id");
        TEST_RESULT_UINT(userIdFromName(STRDEF("bogus")), (uid_t)-1, "get bogus user id");
        TEST_RESULT_STR_Z(userName(), testUser(), "check user name");
        TEST_RESULT_STR_Z(userNameFromId(77777), NULL, "invalid user name by id");
        TEST_RESULT_BOOL(userRoot(), false, "check user is root");

        TEST_RESULT_UINT(groupId(), getgid(), "check group id");
        TEST_RESULT_UINT(groupIdFromName(groupName()), groupId(), "get group id");
        TEST_RESULT_UINT(groupIdFromName(NULL), (gid_t)-1, "get null group id");
        TEST_RESULT_UINT(groupIdFromName(STRDEF("bogus")), (uid_t)-1, "get bogus group id");
        TEST_RESULT_STR_Z(groupName(), testGroup(), "check name name");
        TEST_RESULT_STR_Z(groupNameFromId(77777), NULL, "invalid group name by id");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
