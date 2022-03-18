/***********************************************************************************************************************************
Repo Command Common
***********************************************************************************************************************************/
#ifndef COMMAND_REPO_COMMON_H
#define COMMAND_REPO_COMMON_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions and Definitions
***********************************************************************************************************************************/
// Path cannot contain //. Strip trailing /. Absolute path must fall under repo path. Throw error or return validated relative path.
String * storageIsValidRepoPath(const String *path);

#endif
