/***********************************************************************************************************************************
System User/Group Management
***********************************************************************************************************************************/
#ifndef COMMON_USER_H
#define COMMON_USER_H

#include <sys/types.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Call this initializer before using any of the functions below. Safe to call more than once.
FN_EXTERN void userInit(void);

// Get the primary group id of the current user
FN_EXTERN gid_t groupId(void);

// Get the id of the specified group. Returns (gid_t)-1 if not found.
FN_EXTERN gid_t groupIdFromName(const String *groupName);

// Get the primary group name of the current user. Returns NULL if there is no mapping.
FN_EXTERN const String *groupName(void);

// Get the group name from a group id. Returns NULL if the group id is invalid or there is no mapping.
FN_EXTERN String *groupNameFromId(gid_t groupId);

#ifdef HAVE_LIBSSH2

// Get the home directory of the current user. Returns NULL if there is no mapping.
FN_EXTERN const String *userHome(void);

// Get the user home directory from a user id. Returns NULL if the user id is invalid or there is no mapping.
FN_EXTERN String *userHomeFromId(uid_t userId);

#endif // HAVE_LIBSSH2

// Get the id of the current user
FN_EXTERN uid_t userId(void);

// Get the id of the specified user. Returns (uid_t)-1 if not found.
FN_EXTERN uid_t userIdFromName(const String *userName);

// Get the name of the current user. Returns NULL if there is no mapping.
FN_EXTERN const String *userName(void);

// Get the user name from a user id. Returns NULL if the user id is invalid or there is no mapping.
FN_EXTERN String *userNameFromId(uid_t userId);

// Is the current user the root user?
FN_EXTERN bool userRoot(void);

#endif
