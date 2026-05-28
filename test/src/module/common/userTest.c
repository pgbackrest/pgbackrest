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
        TEST_RESULT_STR(groupName(), TEST_GROUP_STR, "check group name");
        TEST_RESULT_STR_Z(groupNameFromId(77777), NULL, "invalid group name by id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("uid/gid name cache");

        // Positive cache-hit path -- the current uid/gid was already cached by userInit() above so this exercises the cache-hit
        // branch in userNameFromId() / groupNameFromId().
        TEST_RESULT_INT(userLocalData.userNameCache[0].id, userId(), "user id is cached");
        TEST_RESULT_STR(userLocalData.userNameCache[0].name, userName(), "user name is cached");
        TEST_RESULT_STR(userNameFromId(userId()), userName(), "user name from id (cache hit, repeated)");
        TEST_RESULT_INT(userLocalData.groupNameCache[0].id, groupId(), "group id is cached");
        TEST_RESULT_STR(userLocalData.groupNameCache[0].name, groupName(), "group name is cached");
        TEST_RESULT_STR(groupNameFromId(groupId()), groupName(), "group name from id (cache hit, repeated)");

        // Negative cache-hit path -- the invalid id was cached as NULL by the earlier negative test; repeating it now exercises
        // the same cache-hit branch returning NULL.
        TEST_RESULT_STR_Z(userNameFromId(77777), NULL, "invalid user name (negative cache hit)");
        TEST_RESULT_STR_Z(groupNameFromId(77777), NULL, "invalid group name (negative cache hit)");

        // Cache overflow -- look up enough distinct invalid ids to exceed the 16-entry fixed cache and exercise the "table full,
        // fall through uncached" branch in both userNameFromId() and groupNameFromId().
        for (unsigned int idx = 1; idx <= 20; idx++)
        {
            TEST_RESULT_STR_Z(userNameFromId(70000 + idx), NULL, "fill and overflow user cache");
            TEST_RESULT_STR_Z(groupNameFromId(70000 + idx), NULL, "fill and overflow group cache");
        }

#ifdef HAVE_LIBSSH2
        TEST_RESULT_STR(userHome(), STRDEF("/home/" TEST_USER), "check user home directory");
        TEST_RESULT_STR_Z(userHomeFromId(userId()), "/home/" TEST_USER, "user home by id");
        TEST_RESULT_STR_Z(userHomeFromId(77777), NULL, "invalid user home by id");
#endif //  HAVE_LIBSSH2
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
