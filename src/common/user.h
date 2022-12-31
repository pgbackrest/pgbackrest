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
// Call this initializer before using any of the functions below.  Safe to call more than once.
FV_EXTERN void userInit(void);

// Get the primary group id of the current user
FV_EXTERN gid_t groupId(void);

// Get the id of the specified group.  Returns (gid_t)-1 if not found.
FV_EXTERN gid_t groupIdFromName(const String *groupName);

// Get the primary group name of the current user.  Returns NULL if there is no mapping.
FV_EXTERN const String *groupName(void);

// Get the group name from a group id.  Returns NULL if the group id is invalid or there is no mapping.
FV_EXTERN String *groupNameFromId(gid_t groupId);

// Get the id of the current user
FV_EXTERN uid_t userId(void);

// Get the id of the specified user.  Returns (uid_t)-1 if not found.
FV_EXTERN uid_t userIdFromName(const String *userName);

// Get the name of the current user.  Returns NULL if there is no mapping.
FV_EXTERN const String *userName(void);

// Get the user name from a user id.  Returns NULL if the user id is invalid or there is no mapping.
FV_EXTERN String *userNameFromId(uid_t userId);

// Is the current user the root user?
FV_EXTERN bool userRoot(void);

#endif
