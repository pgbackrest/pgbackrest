/***********************************************************************************************************************************
MSVC Comptibility Header

Defines the necessary or missing macros / typedefs, that are available on POSIX plaforms.

***********************************************************************************************************************************/
#ifndef MSVC_COMPAT_H
#define MSVC_COMPAT_H

#ifndef _MSC_VER
//    #error "Only MSVC is supported on Windows platforms."
#endif

#ifndef _WIN64
    #error "Only 64-bit builds are supported on Windows platforms."
#endif

/***********************************************************************************************************************************
Error-silencing macros
***********************************************************************************************************************************/

// Fix for the absence of __attribute__ in msvc
#if defined(_MSC_VER)
    #define __attribute__(...)
#endif

// Silence noisy warnings about non-secure CRT functions
// Don't define if it is passed from the command line
#if !defined(_CRT_SECURE_NO_WARNINGS)
    #define _CRT_SECURE_NO_WARNINGS
#endif

// Silence noisy warnings about old Winsock API
#define _WINSOCK_DEPRECATED_NO_WARNINGS

/***********************************************************************************************************************************
Include
***********************************************************************************************************************************/
//#include <WinSock2.h>

/***********************************************************************************************************************************
Missing types
***********************************************************************************************************************************/
#if defined(_MSC_VER)
    typedef __int64 ssize_t;

    typedef unsigned short mode_t;

    typedef long long time_t;

    typedef unsigned __int32 uid_t;

    typedef unsigned __int32 gid_t;

    typedef unsigned long pid_t; // Same as DWORD
#else
    // MingW
    typedef unsigned short mode_t;

    typedef long long time_t;

    typedef unsigned int uid_t;

    typedef unsigned int gid_t;
#endif

/***********************************************************************************************************************************
Missing routines
***********************************************************************************************************************************/
struct tm *localtime_r(const time_t *timer, struct tm *result);

/***********************************************************************************************************************************
Dependencies
***********************************************************************************************************************************/
#ifdef ENABLE_YAML
    #define YAML_DECLARE_STATIC
#endif

#ifdef ENABLE_REGEX
    #define USE_REGEX_STATIC
#endif

#ifdef ENABLE_XML
    #define LIBXML_STATIC
#endif

#endif
