/***********************************************************************************************************************************
Render Error Data
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/common/render.h"
#include "build/error/render.h"

/***********************************************************************************************************************************
Build error name from a string
***********************************************************************************************************************************/
static String *
bldErrName(const String *const value)
{
    String *const result = strNew();
    const char *const valuePtr = strZ(value);

    bool upper = true;

    for (unsigned int valueIdx = 0; valueIdx < strSize(value); valueIdx++)
    {
        strCatChr(result, upper ? (char)toupper(valuePtr[valueIdx]) : valuePtr[valueIdx]);
        upper = false;

        if (valuePtr[valueIdx + 1] == '-')
        {
            upper = true;
            valueIdx++;
        }
    }

    strCatZ(result, "Error");

    return result;
}

/***********************************************************************************************************************************
Render error.auto.h
***********************************************************************************************************************************/
#define ERROR_MODULE                                               "error"
#define ERROR_AUTO_COMMENT                                         "Error Type Definition"

static void
bldErrRenderErrorAutoH(const Storage *const storageRepo, const BldErr bldErr)
{
    String *error = strNewFmt(
        "%s"
        "#ifndef COMMON_ERROR_AUTO_H\n"
        "#define COMMON_ERROR_AUTO_H\n",
        strZ(bldHeader(ERROR_MODULE, ERROR_AUTO_COMMENT)));

    // Error declarations
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        error,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Error type declarations\n"
        COMMENT_BLOCK_END "\n");

    for (unsigned int errIdx = 0; errIdx < lstSize(bldErr.errList); errIdx++)
    {
        const BldErrError *const err = lstGet(bldErr.errList, errIdx);

        strCatFmt(error, "ERROR_DECLARE(%s);\n", strZ(bldErrName(err->name)));
    }

    // End and save
    strCatZ(
        error,
        "\n"
        "#endif\n");

    bldPut(storageRepo, "src/common/error.auto.h", BUFSTR(error));
}

/***********************************************************************************************************************************
Render error.auto.c
***********************************************************************************************************************************/
static void
bldErrRenderErrorAutoC(const Storage *const storageRepo, const BldErr bldErr)
{
    String *error = bldHeader(ERROR_MODULE, ERROR_AUTO_COMMENT);

    // Error type definitions
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        error,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Error type definitions\n"
        COMMENT_BLOCK_END "\n");

    for (unsigned int errIdx = 0; errIdx < lstSize(bldErr.errList); errIdx++)
    {
        const BldErrError *const err = lstGet(bldErr.errList, errIdx);

        strCatFmt(error, "ERROR_DEFINE(%3u, %s, RuntimeError);\n", err->code, strZ(bldErrName(err->name)));
    }

    // Error type array
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        error,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Error type array\n"
        COMMENT_BLOCK_END "\n"
        "static const ErrorType *errorTypeList[] =\n"
        "{\n");

    for (unsigned int errIdx = 0; errIdx < lstSize(bldErr.errList); errIdx++)
    {
        const BldErrError *const err = lstGet(bldErr.errList, errIdx);

        strCatFmt(error, "    &%s,\n", strZ(bldErrName(err->name)));
    }

    strCatZ(
        error, 
        "    NULL,\n"
        "};\n");

    bldPut(storageRepo, "src/common/error.auto.c", BUFSTR(error));
}

/**********************************************************************************************************************************/
void
bldErrRender(const Storage *const storageRepo, const BldErr bldErr)
{
    bldErrRenderErrorAutoH(storageRepo, bldErr);
    bldErrRenderErrorAutoC(storageRepo, bldErr);
}
