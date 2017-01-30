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

MODULE = pgBackRest::LibC    PACKAGE = pgBackRest::LibC

INCLUDE: const-xs.inc

U16
pageChecksum(page, blkno, pageSize)
    const char * page
    U32 blkno
    U32 pageSize

bool
pageChecksumTest(page, blockNo, pageSize)
    const char *page
    U32 blockNo
    U32 pageSize

bool
pageChecksumBuffer(pageBuffer, bufferSize, blockNoStart, pageSize, iIgnoreWalId, iIgnoreWalOffset)
    const char *pageBuffer
    U32 bufferSize
    U32 blockNoStart
    U32 pageSize
    U32 iIgnoreWalId
    U32 iIgnoreWalOffset
