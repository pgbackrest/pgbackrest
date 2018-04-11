/***********************************************************************************************************************************
C to Perl Interface

The following C types are mapped by the current typemap:

'AV *', 'Boolean', 'CV *', 'FILE *', 'FileHandle', 'HV *', 'I16', 'I32', 'I8', 'IV', 'InOutStream', 'InputStream', 'NV',
'OutputStream', 'PerlIO *', 'Result', 'STRLEN', 'SV *', 'SVREF', 'SysRet', 'SysRetLong', 'Time_t *', 'U16', 'U32', 'U8', 'UV',
'bool', 'bool_t', 'caddr_t', 'char', 'char *', 'char **', 'const char *', 'double', 'float', 'int', 'long', 'short', 'size_t',
'ssize_t', 'time_t', 'unsigned', 'unsigned char', 'unsigned char *', 'unsigned int', 'unsigned long', 'unsigned long *',
'unsigned short', 'void *', 'wchar_t', 'wchar_t *'
***********************************************************************************************************************************/
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

#include <XSUB.h>
#include <EXTERN.h>
#include <perl.h>

#if WARNING_MAYBE_INITIALIZED || WARNING_INITIALIZED
    #pragma GCC diagnostic pop
#endif

/***********************************************************************************************************************************
C includes

These includes are from the src directory.  There is no Perl-specific code in them.
***********************************************************************************************************************************/
#include "cipher/random.h"
#include "common/error.h"
#include "config/config.h"
#include "config/define.h"
#include "config/load.h"
#include "perl/config.h"
#include "postgres/pageChecksum.h"
#include "storage/driver/posix.h"

/***********************************************************************************************************************************
Helper macros
***********************************************************************************************************************************/
#include "LibC.h"

/***********************************************************************************************************************************
XSH includes

These includes define data structures that are required for the C to Perl interface but are not part of the regular C source.
***********************************************************************************************************************************/
#include "xs/cipher/block.xsh"
#include "xs/common/encode.xsh"
#include "xs/config/config.auto.xsh"
#include "xs/config/define.auto.xsh"

/***********************************************************************************************************************************
Constant include

Auto generated code that handles exporting C constants to Perl.
***********************************************************************************************************************************/
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "const-c.inc"

#pragma GCC diagnostic warning "-Wunused-parameter"

/***********************************************************************************************************************************
Module definition
***********************************************************************************************************************************/
MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC
PROTOTYPES: DISABLE

# Exported constants
#
# The XS portion of the code that handles exporting C constants to Perl.
# ----------------------------------------------------------------------------------------------------------------------------------
#pragma GCC diagnostic ignored "-Wuninitialized"

INCLUDE: const-xs.inc

#pragma GCC diagnostic warning "-Wuninitialized"

# Exported functions and modules
#
# These modules should map 1-1 with C modules in src directory.
# ----------------------------------------------------------------------------------------------------------------------------------
INCLUDE: xs/cipher/block.xs
INCLUDE: xs/cipher/random.xs
INCLUDE: xs/common/encode.xs
INCLUDE: xs/config/config.xs
INCLUDE: xs/config/configTest.xs
INCLUDE: xs/config/define.xs
INCLUDE: xs/postgres/pageChecksum.xs
INCLUDE: xs/storage/storage.xs
