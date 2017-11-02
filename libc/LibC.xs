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
#include <XSUB.h>
#include <EXTERN.h>
#include <perl.h>

/***********************************************************************************************************************************
C includes

These includes are from the src directory.  There is no Perl-specific code in them.
***********************************************************************************************************************************/
#include "common/encode.h"
#include "common/error.h"
#include "config/config.h"
#include "config/define.h"
#include "postgres/pageChecksum.h"

/***********************************************************************************************************************************
Helper macros
***********************************************************************************************************************************/
#include "LibC.h"

/***********************************************************************************************************************************
XSH includes

These includes define data structures that are required for the C to Perl interface but are not part of the regular C source.
***********************************************************************************************************************************/
#include "xs/common/encode.xsh"
#include "xs/config/config.auto.xsh"
#include "xs/config/define.auto.xsh"

/***********************************************************************************************************************************
Constant include

Auto generated code that handles exporting C constants to Perl.
***********************************************************************************************************************************/
#include "const-c.inc"

/***********************************************************************************************************************************
Module definition
***********************************************************************************************************************************/
MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC
PROTOTYPES: DISABLE

# Exported constants
#
# The XS portion of the code that handles exporting C constants to Perl.
# ----------------------------------------------------------------------------------------------------------------------------------
INCLUDE: const-xs.inc

# Exported functions and modules
#
# These modules should map 1-1 with C modules in src directory.
# ----------------------------------------------------------------------------------------------------------------------------------
INCLUDE: xs/common/encode.xs
INCLUDE: xs/config/config.xs
INCLUDE: xs/config/define.xs
INCLUDE: xs/postgres/pageChecksum.xs
