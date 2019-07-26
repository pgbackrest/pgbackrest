/***********************************************************************************************************************************
Database Helper
***********************************************************************************************************************************/
#include "build.auto.h"

// #include "common/crypto/common.h"
// #include "common/debug.h"
// #include "common/exec.h"
// #include "common/memContext.h"
// #include "config/config.h"
// #include "config/exec.h"
// #include "config/protocol.h"
#include "db/helper.h"

/***********************************************************************************************************************************
Init local mem context and data structure
***********************************************************************************************************************************/
DbGetResult dbGet(bool primaryOnly)
{
    (void)primaryOnly;
    return (DbGetResult){0};
}
