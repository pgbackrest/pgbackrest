/***********************************************************************************************************************************
C to Perl Interface

The following C types are mapped by the current typemap:

'AV *', 'Boolean', 'CV *', 'FILE *', 'FileHandle', 'HV *', 'I16', 'I32', 'I8', 'IV', 'InOutStream', 'InputStream', 'NV',
'OutputStream', 'PerlIO *', 'Result', 'STRLEN', 'SV *', 'SVREF', 'SysRet', 'SysRetLong', 'Time_t *', 'U16', 'U32', 'U8', 'UV',
'bool', 'bool_t', 'caddr_t', 'char', 'char *', 'char **', 'const char *', 'double', 'float', 'int', 'long', 'short', 'size_t',
'ssize_t', 'time_t', 'unsigned', 'unsigned char', 'unsigned char *', 'unsigned int', 'unsigned long', 'unsigned long *',
'unsigned short', 'void *', 'wchar_t', 'wchar_t *'
***********************************************************************************************************************************/
#include "build.auto.h"

#define PERL_NO_GET_CONTEXT

/***********************************************************************************************************************************
Perl includes

Order is critical here so don't change it.
***********************************************************************************************************************************/
#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 8 || (__GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ >= 0)))
    #define WARNING_MAYBE_INITIALIZED 1
#elif __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ >= 0)))
    #define WARNING_INITIALIZED 1
#endif

#if WARNING_MAYBE_INITIALIZED
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#elif WARNING_INITIALIZED
    #pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"

#include <XSUB.h>
#include <EXTERN.h>
#include <perl.h>

#if WARNING_MAYBE_INITIALIZED
    #pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#elif WARNING_INITIALIZED
    #pragma GCC diagnostic warning "-Wuninitialized"
#endif

/***********************************************************************************************************************************
C includes

These includes are from the src directory.  There is no Perl-specific code in them.
***********************************************************************************************************************************/
#include "common/error.h"
#include "common/io/io.h"
#include "config/config.h"
#include "config/define.h"
#include "config/load.h"
#include "config/parse.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Helper macros
***********************************************************************************************************************************/
#include "LibC.h"

/***********************************************************************************************************************************
XSH includes

These includes define data structures that are required for the C to Perl interface but are not part of the regular C source.
***********************************************************************************************************************************/
#include "xs/config/configTest.xsh"
#include "xs/postgres/client.xsh"
#include "xs/storage/storage.xsh"
#include "xs/storage/storageRead.xsh"
#include "xs/storage/storageWrite.xsh"

/***********************************************************************************************************************************
Module definition
***********************************************************************************************************************************/
MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC
PROTOTYPES: DISABLE

# Return UVSIZE to ensure that this Perl supports 64-bit integers
# ----------------------------------------------------------------------------------------------------------------------------------
I32
libcUvSize()
CODE:
    RETVAL = UVSIZE;
OUTPUT:
    RETVAL

# Exported functions and modules
#
# These modules should map 1-1 with C modules in src directory.
# ----------------------------------------------------------------------------------------------------------------------------------
INCLUDE: xs/config/config.xs
INCLUDE: xs/config/configTest.xs
INCLUDE: xs/config/define.xs
INCLUDE: xs/postgres/client.xs
INCLUDE: xs/storage/storage.xs
INCLUDE: xs/storage/storageRead.xs
INCLUDE: xs/storage/storageWrite.xs
