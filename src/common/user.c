/***********************************************************************************************************************************
System User/Group Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/user.h"

/***********************************************************************************************************************************
User group info
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct

    uid_t userId;                                                   // Real user id of the calling process from getuid()
    bool userRoot;                                                  // Is this the root user?
    const String *userName;                                         // User name if it exists

    gid_t groupId;                                                  // Real group id of the calling process from getgid()
    const String *groupName;                                        // Group name if it exists
} userLocalData;

/**********************************************************************************************************************************/
static void
userInitInternal(void)
{
    FUNCTION_TEST_VOID();

    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN("UserLocalData")
        {
            userLocalData.memContext = MEM_CONTEXT_NEW();

            userLocalData.userId = getuid();
            userLocalData.userName = userNameFromId(userLocalData.userId);
            userLocalData.userRoot = userLocalData.userId == 0;

            userLocalData.groupId = getgid();
            userLocalData.groupName = groupNameFromId(userLocalData.groupId);
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

void
userInit(void)
{
    FUNCTION_TEST_VOID();

    if (!userLocalData.memContext)
        userInitInternal();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
gid_t
groupId(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(userLocalData.groupId);
}

/**********************************************************************************************************************************/
gid_t
groupIdFromName(const String *groupName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, groupName);
    FUNCTION_TEST_END();

    if (groupName != NULL)
    {
        struct group *groupData = getgrnam(strPtr(groupName));

        if (groupData != NULL)
            FUNCTION_TEST_RETURN(groupData->gr_gid);
    }

    FUNCTION_TEST_RETURN((gid_t)-1);
}

/**********************************************************************************************************************************/
const String *
groupName(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(userLocalData.groupName);
}

/**********************************************************************************************************************************/
String *
groupNameFromId(gid_t groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, groupId);
    FUNCTION_TEST_END();

    struct group *groupData = getgrgid(groupId);

    if (groupData != NULL)
        FUNCTION_TEST_RETURN(strNew(groupData->gr_name));

    FUNCTION_TEST_RETURN(NULL);
}

/**********************************************************************************************************************************/
uid_t
userId(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(userLocalData.userId);
}

/**********************************************************************************************************************************/
uid_t
userIdFromName(const String *userName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, userName);
    FUNCTION_TEST_END();

    if (userName != NULL)
    {
        struct passwd *userData = getpwnam(strPtr(userName));

        if (userData != NULL)
            FUNCTION_TEST_RETURN(userData->pw_uid);
    }

    FUNCTION_TEST_RETURN((uid_t)-1);
}

/**********************************************************************************************************************************/
const String *
userName(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(userLocalData.userName);
}

/**********************************************************************************************************************************/
String *
userNameFromId(uid_t userId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, userId);
    FUNCTION_TEST_END();

    struct passwd *userData = getpwuid(userId);

    if (userData != NULL)
        FUNCTION_TEST_RETURN(strNew(userData->pw_name));

    FUNCTION_TEST_RETURN(NULL);
}

/**********************************************************************************************************************************/
bool
userRoot(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(userLocalData.userRoot);
}
