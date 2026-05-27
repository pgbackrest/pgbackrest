/***********************************************************************************************************************************
System User/Group Management
***********************************************************************************************************************************/
#include <build.h>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/user.h"

/***********************************************************************************************************************************
User group info
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct

    uid_t userId;                                                   // Real user id of the calling process from getuid()
    bool userRoot;                                                  // Is this the root user?
    const String *userName;                                         // User name if it exists

    gid_t groupId;                                                  // Real group id of the calling process from getgid()
    const String *groupName;                                        // Group name if it exists
#ifdef HAVE_LIBSSH2
    const String *userHome;                                         // User home directory
#endif // HAVE_LIBSSH2
} userLocalData;

/***********************************************************************************************************************************
Per-process uid->name and gid->name caches.

userNameFromId() / groupNameFromId() are called once per file by the manifest builder. On systems where NSS routes uid lookups to a
remote name service (sssd, systemd-userdbd, LDAP via nss-pam-ldapd, etc.), every call costs a Unix-socket round-trip plus its own
audit-and-syscall overhead. Profiling shows this can dominate the manifest-build phase for clusters with millions of files even
though the data files almost always share a single owner (typically "postgres").

The cache is a small fixed-size array because the working set of unique ids on a PG host is tiny (usually 1-3). Linear scan is
faster than a hash table at this size and avoids any allocation in the hot path. Cached String * lives in memContextTop() so it
survives every caller's context.

Cache misses past USER_NAME_CACHE_SIZE fall through to an uncached NSS lookup (correct but slow); in practice we never exceed the
table for a backup of a normally-configured cluster.
***********************************************************************************************************************************/
#define USER_NAME_CACHE_SIZE                                        16

typedef struct UserNameCacheEntry
{
    uid_t id;                                                       // The uid that was looked up
    const String *name;                                             // NSS result owned in memContextTop(), NULL if unresolvable
} UserNameCacheEntry;

typedef struct GroupNameCacheEntry
{
    gid_t id;
    const String *name;
} GroupNameCacheEntry;

static UserNameCacheEntry userNameCache[USER_NAME_CACHE_SIZE];
static unsigned int userNameCacheCount = 0;

static GroupNameCacheEntry groupNameCache[USER_NAME_CACHE_SIZE];
static unsigned int groupNameCacheCount = 0;

/**********************************************************************************************************************************/
static void
userInitInternal(void)
{
    FUNCTION_TEST_VOID();

    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN(UserLocalData, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            userLocalData.memContext = MEM_CONTEXT_NEW();

            userLocalData.userId = getuid();
            userLocalData.userName = userNameFromId(userLocalData.userId);
#ifdef HAVE_LIBSSH2
            userLocalData.userHome = userHomeFromId(userLocalData.userId);
#endif // HAVE_LIBSSH2
            userLocalData.userRoot = userLocalData.userId == 0;

            userLocalData.groupId = getgid();
            userLocalData.groupName = groupNameFromId(userLocalData.groupId);
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
userInit(void)
{
    FUNCTION_TEST_VOID();

    if (!userLocalData.memContext)
        userInitInternal();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN gid_t
groupId(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_TYPE(gid_t, userLocalData.groupId);
}

/**********************************************************************************************************************************/
FN_EXTERN gid_t
groupIdFromName(const String *const groupName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, groupName);
    FUNCTION_TEST_END();

    if (groupName != NULL)
    {
        const struct group *const groupData = getgrnam(strZ(groupName));

        if (groupData != NULL)
            FUNCTION_TEST_RETURN_TYPE(gid_t, groupData->gr_gid);
    }

    FUNCTION_TEST_RETURN_TYPE(gid_t, (gid_t)-1);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
groupName(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_CONST(STRING, userLocalData.groupName);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
groupNameFromId(const gid_t groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, groupId);
    FUNCTION_TEST_END();

    // Cache lookup. Small fixed table -- linear scan is fastest at this size.
    for (unsigned int idx = 0; idx < groupNameCacheCount; idx++)
    {
        if (groupNameCache[idx].id == groupId)
            FUNCTION_TEST_RETURN(STRING, groupNameCache[idx].name == NULL ? NULL : strDup(groupNameCache[idx].name));
    }

    // Cache miss -- do the actual NSS lookup
    const struct group *const groupData = getgrgid(groupId);
    String *const result = groupData != NULL ? strNewZ(groupData->gr_name) : NULL;

    // Record the answer (including the negative case) so we never look this id up again. Past USER_NAME_CACHE_SIZE we just fall
    // back to per-call NSS, which is correct -- only slower for the unusual case of many distinct owners.
    if (groupNameCacheCount < USER_NAME_CACHE_SIZE)
    {
        groupNameCache[groupNameCacheCount].id = groupId;

        MEM_CONTEXT_BEGIN(memContextTop())
        {
            groupNameCache[groupNameCacheCount].name = result == NULL ? NULL : strDup(result);
        }
        MEM_CONTEXT_END();

        groupNameCacheCount++;
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
// Currently userHome() and userHomeFromId() are only used if we are building with libssh2
#ifdef HAVE_LIBSSH2

FN_EXTERN const String *
userHome(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_CONST(STRING, userLocalData.userHome);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
userHomeFromId(const uid_t userId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, userId);
    FUNCTION_TEST_END();

    const struct passwd *const userData = getpwuid(userId);

    if (userData != NULL)
        FUNCTION_TEST_RETURN(STRING, strNewZ(userData->pw_dir));

    FUNCTION_TEST_RETURN(STRING, NULL);
}

#endif // HAVE_LIBSSH2

/**********************************************************************************************************************************/
FN_EXTERN uid_t
userId(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_TYPE(uid_t, userLocalData.userId);
}

/**********************************************************************************************************************************/
FN_EXTERN uid_t
userIdFromName(const String *const userName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, userName);
    FUNCTION_TEST_END();

    if (userName != NULL)
    {
        const struct passwd *const userData = getpwnam(strZ(userName));

        if (userData != NULL)
            FUNCTION_TEST_RETURN_TYPE(uid_t, userData->pw_uid);
    }

    FUNCTION_TEST_RETURN_TYPE(uid_t, (uid_t)-1);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
userName(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_CONST(STRING, userLocalData.userName);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
userNameFromId(const uid_t userId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, userId);
    FUNCTION_TEST_END();

    // Cache lookup. Mirrors groupNameFromId above.
    for (unsigned int idx = 0; idx < userNameCacheCount; idx++)
    {
        if (userNameCache[idx].id == userId)
            FUNCTION_TEST_RETURN(STRING, userNameCache[idx].name == NULL ? NULL : strDup(userNameCache[idx].name));
    }

    const struct passwd *const userData = getpwuid(userId);
    String *const result = userData != NULL ? strNewZ(userData->pw_name) : NULL;

    if (userNameCacheCount < USER_NAME_CACHE_SIZE)
    {
        userNameCache[userNameCacheCount].id = userId;

        MEM_CONTEXT_BEGIN(memContextTop())
        {
            userNameCache[userNameCacheCount].name = result == NULL ? NULL : strDup(result);
        }
        MEM_CONTEXT_END();

        userNameCacheCount++;
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
userRoot(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(BOOL, userLocalData.userRoot);
}
