#define PERL_NO_GET_CONTEXT

#include "LibC.h"
#include "XSUB.h"
#include "const-c.inc"

/*
 * The following C types are mapped by the current typemap:
 * 'AV *', 'Boolean', 'CV *', 'FILE *', 'FileHandle', 'HV *', 'I16', 'I32', 'I8', 'IV', 'InOutStream', 'InputStream', 'NV',
 * 'OutputStream', 'PerlIO *', 'Result', 'STRLEN', 'SV *', 'SVREF', 'SysRet', 'SysRetLong', 'Time_t *', 'U16', 'U32', 'U8', 'UV',
 * 'bool', 'bool_t', 'caddr_t', 'char', 'char *', 'char **', 'const char *', 'double', 'float', 'int', 'long', 'short', 'size_t',
 * 'ssize_t', 'time_t', 'unsigned', 'unsigned char', 'unsigned char *', 'unsigned int', 'unsigned long', 'unsigned long *',
 * 'unsigned short', 'void *', 'wchar_t', 'wchar_t *'
 */

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC
PROTOTYPES: DISABLE

INCLUDE: const-xs.inc

# Config Rule
# ----------------------------------------------------------------------------------------------------------------------------------
I32
cfgCommandId(szCommandName)
    const char *szCommandName

I32
cfgOptionId(szOptionName)
    const char *szOptionName

bool
cfgOptionRuleAllowList(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgOptionRuleAllowListValue(uiCommandId, uiOptionId, uiValueId)
    U32 uiCommandId
    U32 uiOptionId
    U32 uiValueId

I32
cfgOptionRuleAllowListValueTotal(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgOptionRuleAllowListValueValid(uiCommandId, uiOptionId, szValue);
    U32 uiCommandId
    U32 uiOptionId
    const char *szValue

bool
cfgOptionRuleAllowRange(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

double
cfgOptionRuleAllowRangeMax(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

double
cfgOptionRuleAllowRangeMin(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgOptionRuleDefault(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgOptionRuleDepend(uiCommandId, uiOptionId);
    U32 uiCommandId
    U32 uiOptionId

I32
cfgOptionRuleDependOption(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgOptionRuleDependValue(uiCommandId, uiOptionId, uiValueId)
    U32 uiCommandId
    U32 uiOptionId
    U32 uiValueId

I32
cfgOptionRuleDependValueTotal(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgOptionRuleDependValueValid(uiCommandId, uiOptionId, szValue)
    U32 uiCommandId
    U32 uiOptionId
    const char *szValue

const char *
cfgOptionRuleHint(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgOptionRuleNameAlt(uiOptionId)
    U32 uiOptionId

bool cfgOptionRuleNegate(uiOptionId)
    U32 uiOptionId

const char *
cfgOptionRulePrefix(uiOptionId)
    U32 uiOptionId

bool
cfgOptionRuleRequired(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgOptionRuleSection(uiOptionId)
    U32 uiOptionId

bool
cfgOptionRuleSecure(uiOptionId)
    U32 uiOptionId

I32
cfgOptionRuleType(uiOptionId);
    U32 uiOptionId

bool
cfgOptionRuleValid(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

U32
cfgOptionTotal()

bool
cfgOptionRuleValueHash(uiOptionId)
    U32 uiOptionId

# Config
# ----------------------------------------------------------------------------------------------------------------------------------
const char *
cfgCommandName(uiCommandId)
    U32 uiCommandId

I32
cfgOptionIndexTotal(uiOptionId)
    U32 uiOptionId

const char *
cfgOptionName(uiOptionId)
    U32 uiOptionId

# Page Checksum
# ----------------------------------------------------------------------------------------------------------------------------------
U16
pageChecksum(page, blkno, pageSize)
    const char * page
    U32 blkno
    U32 pageSize

bool
pageChecksumTest(szPage, uiBlockNo, uiPageSize, uiIgnoreWalId, uiIgnoreWalOffset)
    const char *szPage
    U32 uiBlockNo
    U32 uiPageSize
    U32 uiIgnoreWalId
    U32 uiIgnoreWalOffset

bool
pageChecksumBufferTest(szPageBuffer, uiBufferSize, uiBlockNoStart, uiPageSize, uiIgnoreWalId, uiIgnoreWalOffset)
    const char *szPageBuffer
    U32 uiBufferSize
    U32 uiBlockNoStart
    U32 uiPageSize
    U32 uiIgnoreWalId
    U32 uiIgnoreWalOffset
