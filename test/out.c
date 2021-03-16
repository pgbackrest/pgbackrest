# 1 "../../repo/src/protocol/server.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 31 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 32 "<command-line>" 2
# 1 "../../repo/src/protocol/server.c"



# 1 "./build.auto.h" 1




# 1 "../../repo/src/version.h" 1
# 6 "./build.auto.h" 2
# 5 "../../repo/src/protocol/server.c" 2

# 1 "/usr/include/string.h" 1 3 4
# 26 "/usr/include/string.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 33 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 3 4
# 1 "/usr/include/features.h" 1 3 4
# 461 "/usr/include/features.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 1 3 4
# 452 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 453 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/long-double.h" 1 3 4
# 454 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 2 3 4
# 462 "/usr/include/features.h" 2 3 4
# 485 "/usr/include/features.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 1 3 4
# 10 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/gnu/stubs-64.h" 1 3 4
# 11 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 2 3 4
# 486 "/usr/include/features.h" 2 3 4
# 34 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 2 3 4
# 27 "/usr/include/string.h" 2 3 4






# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 209 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4

# 209 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4
typedef long unsigned int size_t;
# 34 "/usr/include/string.h" 2 3 4
# 43 "/usr/include/string.h" 3 4
extern void *memcpy (void *__restrict __dest, const void *__restrict __src,
       size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern void *memmove (void *__dest, const void *__src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));





extern void *memccpy (void *__restrict __dest, const void *__restrict __src,
        int __c, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern void *memset (void *__s, int __c, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern int memcmp (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 91 "/usr/include/string.h" 3 4
extern void *memchr (const void *__s, int __c, size_t __n)
      __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 122 "/usr/include/string.h" 3 4
extern char *strcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern char *strncpy (char *__restrict __dest,
        const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern char *strcat (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern char *strncat (char *__restrict __dest, const char *__restrict __src,
        size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strcmp (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));

extern int strncmp (const char *__s1, const char *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strcoll (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));

extern size_t strxfrm (char *__restrict __dest,
         const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));



# 1 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/__locale_t.h" 1 3 4
# 28 "/usr/include/x86_64-linux-gnu/bits/types/__locale_t.h" 3 4
struct __locale_struct
{

  struct __locale_data *__locales[13];


  const unsigned short int *__ctype_b;
  const int *__ctype_tolower;
  const int *__ctype_toupper;


  const char *__names[13];
};

typedef struct __locale_struct *__locale_t;
# 23 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 2 3 4

typedef __locale_t locale_t;
# 154 "/usr/include/string.h" 2 3 4


extern int strcoll_l (const char *__s1, const char *__s2, locale_t __l)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 3)));


extern size_t strxfrm_l (char *__dest, const char *__src, size_t __n,
    locale_t __l) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 4)));





extern char *strdup (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));






extern char *strndup (const char *__string, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));
# 226 "/usr/include/string.h" 3 4
extern char *strchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 253 "/usr/include/string.h" 3 4
extern char *strrchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 273 "/usr/include/string.h" 3 4
extern size_t strcspn (const char *__s, const char *__reject)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern size_t strspn (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 303 "/usr/include/string.h" 3 4
extern char *strpbrk (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 330 "/usr/include/string.h" 3 4
extern char *strstr (const char *__haystack, const char *__needle)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));




extern char *strtok (char *__restrict __s, const char *__restrict __delim)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));



extern char *__strtok_r (char *__restrict __s,
    const char *__restrict __delim,
    char **__restrict __save_ptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 3)));

extern char *strtok_r (char *__restrict __s, const char *__restrict __delim,
         char **__restrict __save_ptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 3)));
# 385 "/usr/include/string.h" 3 4
extern size_t strlen (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));




extern size_t strnlen (const char *__string, size_t __maxlen)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));




extern char *strerror (int __errnum) __attribute__ ((__nothrow__ , __leaf__));
# 410 "/usr/include/string.h" 3 4
extern int strerror_r (int __errnum, char *__buf, size_t __buflen) __asm__ ("" "__xpg_strerror_r") __attribute__ ((__nothrow__ , __leaf__))

                        __attribute__ ((__nonnull__ (2)));
# 428 "/usr/include/string.h" 3 4
extern char *strerror_l (int __errnum, locale_t __l) __attribute__ ((__nothrow__ , __leaf__));



# 1 "/usr/include/strings.h" 1 3 4
# 23 "/usr/include/strings.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 24 "/usr/include/strings.h" 2 3 4










extern int bcmp (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bcopy (const void *__src, void *__dest, size_t __n)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 68 "/usr/include/strings.h" 3 4
extern char *index (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 96 "/usr/include/strings.h" 3 4
extern char *rindex (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));






extern int ffs (int __i) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));





extern int ffsl (long int __l) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));
__extension__ extern int ffsll (long long int __ll)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern int strcasecmp (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strncasecmp (const char *__s1, const char *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));






extern int strcasecmp_l (const char *__s1, const char *__s2, locale_t __loc)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 3)));



extern int strncasecmp_l (const char *__s1, const char *__s2,
     size_t __n, locale_t __loc)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 4)));



# 433 "/usr/include/string.h" 2 3 4



extern void explicit_bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern char *strsep (char **__restrict __stringp,
       const char *__restrict __delim)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern char *strsignal (int __sig) __attribute__ ((__nothrow__ , __leaf__));


extern char *__stpcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *stpcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));



extern char *__stpncpy (char *__restrict __dest,
   const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *stpncpy (char *__restrict __dest,
        const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
# 499 "/usr/include/string.h" 3 4

# 7 "../../repo/src/protocol/server.c" 2

# 1 "../../repo/src/common/debug.h" 1






# 1 "../../repo/src/common/assert.h" 1






# 1 "../../repo/src/common/error.h" 1
# 40 "../../repo/src/common/error.h"
# 1 "/usr/include/errno.h" 1 3 4
# 28 "/usr/include/errno.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/errno.h" 1 3 4
# 26 "/usr/include/x86_64-linux-gnu/bits/errno.h" 3 4
# 1 "/usr/include/linux/errno.h" 1 3 4
# 1 "/usr/include/x86_64-linux-gnu/asm/errno.h" 1 3 4
# 1 "/usr/include/asm-generic/errno.h" 1 3 4




# 1 "/usr/include/asm-generic/errno-base.h" 1 3 4
# 6 "/usr/include/asm-generic/errno.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/asm/errno.h" 2 3 4
# 1 "/usr/include/linux/errno.h" 2 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/errno.h" 2 3 4
# 29 "/usr/include/errno.h" 2 3 4








extern int *__errno_location (void) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));
# 52 "/usr/include/errno.h" 3 4

# 41 "../../repo/src/common/error.h" 2
# 1 "/usr/include/setjmp.h" 1 3 4
# 27 "/usr/include/setjmp.h" 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/setjmp.h" 1 3 4
# 26 "/usr/include/x86_64-linux-gnu/bits/setjmp.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/setjmp.h" 2 3 4




typedef long int __jmp_buf[8];
# 30 "/usr/include/setjmp.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/__sigset_t.h" 1 3 4




typedef struct
{
  unsigned long int __val[(1024 / (8 * sizeof (unsigned long int)))];
} __sigset_t;
# 31 "/usr/include/setjmp.h" 2 3 4


struct __jmp_buf_tag
  {




    __jmp_buf __jmpbuf;
    int __mask_was_saved;
    __sigset_t __saved_mask;
  };


typedef struct __jmp_buf_tag jmp_buf[1];



extern int setjmp (jmp_buf __env) __attribute__ ((__nothrow__));




extern int __sigsetjmp (struct __jmp_buf_tag __env[1], int __savemask) __attribute__ ((__nothrow__));



extern int _setjmp (struct __jmp_buf_tag __env[1]) __attribute__ ((__nothrow__));
# 67 "/usr/include/setjmp.h" 3 4
extern void longjmp (struct __jmp_buf_tag __env[1], int __val)
     __attribute__ ((__nothrow__)) __attribute__ ((__noreturn__));





extern void _longjmp (struct __jmp_buf_tag __env[1], int __val)
     __attribute__ ((__nothrow__)) __attribute__ ((__noreturn__));







typedef struct __jmp_buf_tag sigjmp_buf[1];
# 93 "/usr/include/setjmp.h" 3 4
extern void siglongjmp (sigjmp_buf __env, int __val)
     __attribute__ ((__nothrow__)) __attribute__ ((__noreturn__));
# 103 "/usr/include/setjmp.h" 3 4

# 42 "../../repo/src/common/error.h" 2
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stdbool.h" 1 3 4
# 43 "../../repo/src/common/error.h" 2





# 47 "../../repo/src/common/error.h"
typedef struct ErrorType ErrorType;






# 1 "../../repo/src/common/error.auto.h" 1
# 12 "../../repo/src/common/error.auto.h"
extern const ErrorType AssertError;
extern const ErrorType ChecksumError;
extern const ErrorType ConfigError;
extern const ErrorType FileInvalidError;
extern const ErrorType FormatError;
extern const ErrorType CommandRequiredError;
extern const ErrorType OptionInvalidError;
extern const ErrorType OptionInvalidValueError;
extern const ErrorType OptionInvalidRangeError;
extern const ErrorType OptionInvalidPairError;
extern const ErrorType OptionDuplicateKeyError;
extern const ErrorType OptionNegateError;
extern const ErrorType OptionRequiredError;
extern const ErrorType PgRunningError;
extern const ErrorType ProtocolError;
extern const ErrorType PathNotEmptyError;
extern const ErrorType FileOpenError;
extern const ErrorType FileReadError;
extern const ErrorType ParamRequiredError;
extern const ErrorType ArchiveMismatchError;
extern const ErrorType ArchiveDuplicateError;
extern const ErrorType VersionNotSupportedError;
extern const ErrorType PathCreateError;
extern const ErrorType CommandInvalidError;
extern const ErrorType HostConnectError;
extern const ErrorType LockAcquireError;
extern const ErrorType BackupMismatchError;
extern const ErrorType FileSyncError;
extern const ErrorType PathOpenError;
extern const ErrorType PathSyncError;
extern const ErrorType FileMissingError;
extern const ErrorType DbConnectError;
extern const ErrorType DbQueryError;
extern const ErrorType DbMismatchError;
extern const ErrorType DbTimeoutError;
extern const ErrorType FileRemoveError;
extern const ErrorType PathRemoveError;
extern const ErrorType StopError;
extern const ErrorType TermError;
extern const ErrorType FileWriteError;
extern const ErrorType ProtocolTimeoutError;
extern const ErrorType FeatureNotSupportedError;
extern const ErrorType ArchiveCommandInvalidError;
extern const ErrorType LinkExpectedError;
extern const ErrorType LinkDestinationError;
extern const ErrorType HostInvalidError;
extern const ErrorType PathMissingError;
extern const ErrorType FileMoveError;
extern const ErrorType BackupSetInvalidError;
extern const ErrorType TablespaceMapError;
extern const ErrorType PathTypeError;
extern const ErrorType LinkMapError;
extern const ErrorType FileCloseError;
extern const ErrorType DbMissingError;
extern const ErrorType DbInvalidError;
extern const ErrorType ArchiveTimeoutError;
extern const ErrorType FileModeError;
extern const ErrorType OptionMultipleValueError;
extern const ErrorType ProtocolOutputRequiredError;
extern const ErrorType LinkOpenError;
extern const ErrorType ArchiveDisabledError;
extern const ErrorType FileOwnerError;
extern const ErrorType UserMissingError;
extern const ErrorType OptionCommandError;
extern const ErrorType GroupMissingError;
extern const ErrorType PathExistsError;
extern const ErrorType FileExistsError;
extern const ErrorType MemoryError;
extern const ErrorType CryptoError;
extern const ErrorType ParamInvalidError;
extern const ErrorType PathCloseError;
extern const ErrorType FileInfoError;
extern const ErrorType JsonFormatError;
extern const ErrorType KernelError;
extern const ErrorType ServiceError;
extern const ErrorType ExecuteError;
extern const ErrorType RepoInvalidError;
extern const ErrorType CommandError;
extern const ErrorType RuntimeError;
extern const ErrorType InvalidError;
extern const ErrorType UnhandledError;
extern const ErrorType UnknownError;
# 55 "../../repo/src/common/error.h" 2
# 68 "../../repo/src/common/error.h"
int errorTypeCode(const ErrorType *errorType);


const ErrorType *errorTypeFromCode(int code);


const char *errorTypeName(const ErrorType *errorType);


const ErrorType *errorTypeParent(const ErrorType *errorType);



# 80 "../../repo/src/common/error.h" 3 4
_Bool 
# 80 "../../repo/src/common/error.h"
    errorTypeExtends(const ErrorType *child, const ErrorType *parent);





const ErrorType *errorType(void);


int errorCode(void);


const char *errorFileName(void);


const char *errorFunctionName(void);


int errorFileLine(void);



# 101 "../../repo/src/common/error.h" 3 4
_Bool 
# 101 "../../repo/src/common/error.h"
    errorInstanceOf(const ErrorType *errorTypeTest);


const char *errorMessage(void);


const char *errorName(void);


const char *errorStackTrace(void);





unsigned int errorTryDepth(void);


typedef void (*const ErrorHandlerFunction)(unsigned int);

void errorHandlerSet(const ErrorHandlerFunction *list, unsigned int total);
# 282 "../../repo/src/common/error.h"

# 282 "../../repo/src/common/error.h" 3 4
_Bool 
# 282 "../../repo/src/common/error.h"
    errorInternalTry(const char *fileName, const char *functionName, int fileLine);


jmp_buf *errorInternalJump(void);



# 288 "../../repo/src/common/error.h" 3 4
_Bool 
# 288 "../../repo/src/common/error.h"
    errorInternalStateTry(void);



# 291 "../../repo/src/common/error.h" 3 4
_Bool 
# 291 "../../repo/src/common/error.h"
    errorInternalStateCatch(const ErrorType *errorTypeCatch);



# 294 "../../repo/src/common/error.h" 3 4
_Bool 
# 294 "../../repo/src/common/error.h"
    errorInternalStateFinal(void);



# 297 "../../repo/src/common/error.h" 3 4
_Bool 
# 297 "../../repo/src/common/error.h"
    errorInternalProcess(
# 297 "../../repo/src/common/error.h" 3 4
                         _Bool 
# 297 "../../repo/src/common/error.h"
                              catch);


void errorInternalPropagate(void) __attribute__((__noreturn__));


void errorInternalThrow(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
    __attribute__((__noreturn__));
void errorInternalThrowFmt(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 5, 6))) __attribute__((__noreturn__));


void errorInternalThrowSys(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
    __attribute__((__noreturn__));


void errorInternalThrowSysFmt(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 6, 7))) __attribute__((__noreturn__));
# 8 "../../repo/src/common/assert.h" 2
# 8 "../../repo/src/common/debug.h" 2
# 1 "../../repo/src/common/stackTrace.h" 1







# 1 "/usr/include/x86_64-linux-gnu/sys/types.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 28 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 1 3 4
# 29 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4



# 31 "/usr/include/x86_64-linux-gnu/bits/types.h" 3 4
typedef unsigned char __u_char;
typedef unsigned short int __u_short;
typedef unsigned int __u_int;
typedef unsigned long int __u_long;


typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;

typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;






typedef __int8_t __int_least8_t;
typedef __uint8_t __uint_least8_t;
typedef __int16_t __int_least16_t;
typedef __uint16_t __uint_least16_t;
typedef __int32_t __int_least32_t;
typedef __uint32_t __uint_least32_t;
typedef __int64_t __int_least64_t;
typedef __uint64_t __uint_least64_t;



typedef long int __quad_t;
typedef unsigned long int __u_quad_t;







typedef long int __intmax_t;
typedef unsigned long int __uintmax_t;
# 141 "/usr/include/x86_64-linux-gnu/bits/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/typesizes.h" 1 3 4
# 142 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/time64.h" 1 3 4
# 143 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4


typedef unsigned long int __dev_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef unsigned long int __ino_t;
typedef unsigned long int __ino64_t;
typedef unsigned int __mode_t;
typedef unsigned long int __nlink_t;
typedef long int __off_t;
typedef long int __off64_t;
typedef int __pid_t;
typedef struct { int __val[2]; } __fsid_t;
typedef long int __clock_t;
typedef unsigned long int __rlim_t;
typedef unsigned long int __rlim64_t;
typedef unsigned int __id_t;
typedef long int __time_t;
typedef unsigned int __useconds_t;
typedef long int __suseconds_t;

typedef int __daddr_t;
typedef int __key_t;


typedef int __clockid_t;


typedef void * __timer_t;


typedef long int __blksize_t;




typedef long int __blkcnt_t;
typedef long int __blkcnt64_t;


typedef unsigned long int __fsblkcnt_t;
typedef unsigned long int __fsblkcnt64_t;


typedef unsigned long int __fsfilcnt_t;
typedef unsigned long int __fsfilcnt64_t;


typedef long int __fsword_t;

typedef long int __ssize_t;


typedef long int __syscall_slong_t;

typedef unsigned long int __syscall_ulong_t;



typedef __off64_t __loff_t;
typedef char *__caddr_t;


typedef long int __intptr_t;


typedef unsigned int __socklen_t;




typedef int __sig_atomic_t;
# 30 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



typedef __u_char u_char;
typedef __u_short u_short;
typedef __u_int u_int;
typedef __u_long u_long;
typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;


typedef __loff_t loff_t;




typedef __ino_t ino_t;
# 59 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
typedef __dev_t dev_t;




typedef __gid_t gid_t;




typedef __mode_t mode_t;




typedef __nlink_t nlink_t;




typedef __uid_t uid_t;





typedef __off_t off_t;
# 97 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
typedef __pid_t pid_t;





typedef __id_t id_t;




typedef __ssize_t ssize_t;





typedef __daddr_t daddr_t;
typedef __caddr_t caddr_t;





typedef __key_t key_t;




# 1 "/usr/include/x86_64-linux-gnu/bits/types/clock_t.h" 1 3 4






typedef __clock_t clock_t;
# 127 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/types/clockid_t.h" 1 3 4






typedef __clockid_t clockid_t;
# 129 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/time_t.h" 1 3 4






typedef __time_t time_t;
# 130 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/timer_t.h" 1 3 4






typedef __timer_t timer_t;
# 131 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 144 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 145 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;




# 1 "/usr/include/x86_64-linux-gnu/bits/stdint-intn.h" 1 3 4
# 24 "/usr/include/x86_64-linux-gnu/bits/stdint-intn.h" 3 4
typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;
# 156 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4


typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;


typedef int register_t __attribute__ ((__mode__ (__word__)));
# 176 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/include/endian.h" 1 3 4
# 24 "/usr/include/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/endian.h" 1 3 4
# 35 "/usr/include/x86_64-linux-gnu/bits/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/endianness.h" 1 3 4
# 36 "/usr/include/x86_64-linux-gnu/bits/endian.h" 2 3 4
# 25 "/usr/include/endian.h" 2 3 4
# 35 "/usr/include/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 1 3 4
# 33 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 3 4
static __inline __uint16_t
__bswap_16 (__uint16_t __bsx)
{

  return __builtin_bswap16 (__bsx);



}






static __inline __uint32_t
__bswap_32 (__uint32_t __bsx)
{

  return __builtin_bswap32 (__bsx);



}
# 69 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 3 4
__extension__ static __inline __uint64_t
__bswap_64 (__uint64_t __bsx)
{

  return __builtin_bswap64 (__bsx);



}
# 36 "/usr/include/endian.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/uintn-identity.h" 1 3 4
# 32 "/usr/include/x86_64-linux-gnu/bits/uintn-identity.h" 3 4
static __inline __uint16_t
__uint16_identity (__uint16_t __x)
{
  return __x;
}

static __inline __uint32_t
__uint32_identity (__uint32_t __x)
{
  return __x;
}

static __inline __uint64_t
__uint64_identity (__uint64_t __x)
{
  return __x;
}
# 37 "/usr/include/endian.h" 2 3 4
# 177 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/sys/select.h" 1 3 4
# 30 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/select.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/select.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 23 "/usr/include/x86_64-linux-gnu/bits/select.h" 2 3 4
# 31 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types/sigset_t.h" 1 3 4






typedef __sigset_t sigset_t;
# 34 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4



# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_timeval.h" 1 3 4







struct timeval
{
  __time_t tv_sec;
  __suseconds_t tv_usec;
};
# 38 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 1 3 4
# 10 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 3 4
struct timespec
{
  __time_t tv_sec;



  __syscall_slong_t tv_nsec;
# 26 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 3 4
};
# 40 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4



typedef __suseconds_t suseconds_t;





typedef long int __fd_mask;
# 59 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
typedef struct
  {






    __fd_mask __fds_bits[1024 / (8 * (int) sizeof (__fd_mask))];


  } fd_set;






typedef __fd_mask fd_mask;
# 91 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4

# 101 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
extern int select (int __nfds, fd_set *__restrict __readfds,
     fd_set *__restrict __writefds,
     fd_set *__restrict __exceptfds,
     struct timeval *__restrict __timeout);
# 113 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
extern int pselect (int __nfds, fd_set *__restrict __readfds,
      fd_set *__restrict __writefds,
      fd_set *__restrict __exceptfds,
      const struct timespec *__restrict __timeout,
      const __sigset_t *__restrict __sigmask);
# 126 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4

# 180 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4





typedef __blksize_t blksize_t;






typedef __blkcnt_t blkcnt_t;



typedef __fsblkcnt_t fsblkcnt_t;



typedef __fsfilcnt_t fsfilcnt_t;
# 227 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 1 3 4
# 23 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 1 3 4
# 44 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 1 3 4
# 21 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 2 3 4
# 45 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4




typedef struct __pthread_internal_list
{
  struct __pthread_internal_list *__prev;
  struct __pthread_internal_list *__next;
} __pthread_list_t;

typedef struct __pthread_internal_slist
{
  struct __pthread_internal_slist *__next;
} __pthread_slist_t;
# 74 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 3 4
struct __pthread_mutex_s
{
  int __lock;
  unsigned int __count;
  int __owner;

  unsigned int __nusers;



  int __kind;

  short __spins;
  short __elision;
  __pthread_list_t __list;
# 53 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 3 4
};
# 75 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4
# 87 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 1 3 4
# 23 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 3 4
struct __pthread_rwlock_arch_t
{
  unsigned int __readers;
  unsigned int __writers;
  unsigned int __wrphase_futex;
  unsigned int __writers_futex;
  unsigned int __pad3;
  unsigned int __pad4;

  int __cur_writer;
  int __shared;
  signed char __rwelision;




  unsigned char __pad1[7];


  unsigned long int __pad2;


  unsigned int __flags;
# 55 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 3 4
};
# 88 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4




struct __pthread_cond_s
{
  __extension__ union
  {
    __extension__ unsigned long long int __wseq;
    struct
    {
      unsigned int __low;
      unsigned int __high;
    } __wseq32;
  };
  __extension__ union
  {
    __extension__ unsigned long long int __g1_start;
    struct
    {
      unsigned int __low;
      unsigned int __high;
    } __g1_start32;
  };
  unsigned int __g_refs[2] ;
  unsigned int __g_size[2];
  unsigned int __g1_orig_size;
  unsigned int __wrefs;
  unsigned int __g_signals[2];
};
# 24 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 2 3 4



typedef unsigned long int pthread_t;




typedef union
{
  char __size[4];
  int __align;
} pthread_mutexattr_t;




typedef union
{
  char __size[4];
  int __align;
} pthread_condattr_t;



typedef unsigned int pthread_key_t;



typedef int pthread_once_t;


union pthread_attr_t
{
  char __size[56];
  long int __align;
};

typedef union pthread_attr_t pthread_attr_t;




typedef union
{
  struct __pthread_mutex_s __data;
  char __size[40];
  long int __align;
} pthread_mutex_t;


typedef union
{
  struct __pthread_cond_s __data;
  char __size[48];
  __extension__ long long int __align;
} pthread_cond_t;





typedef union
{
  struct __pthread_rwlock_arch_t __data;
  char __size[56];
  long int __align;
} pthread_rwlock_t;

typedef union
{
  char __size[8];
  long int __align;
} pthread_rwlockattr_t;





typedef volatile int pthread_spinlock_t;




typedef union
{
  char __size[32];
  long int __align;
} pthread_barrier_t;

typedef union
{
  char __size[4];
  int __align;
} pthread_barrierattr_t;
# 228 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



# 9 "../../repo/src/common/stackTrace.h" 2

# 1 "../../repo/src/common/logLevel.h" 1
# 10 "../../repo/src/common/logLevel.h"

# 10 "../../repo/src/common/logLevel.h"
typedef enum
{
    logLevelOff,
    logLevelAssert,
    logLevelError,
    logLevelProtocol,
    logLevelWarn,
    logLevelInfo,
    logLevelDetail,
    logLevelDebug,
    logLevelTrace,
} LogLevel;
# 11 "../../repo/src/common/stackTrace.h" 2
# 53 "../../repo/src/common/stackTrace.h"
LogLevel stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel);



    void stackTracePop(void);





size_t stackTraceToZ(char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine);


void stackTraceParamLog(void);


const char *stackTraceParam(void);


char *stackTraceParamBuffer(const char *param);


void stackTraceParamAdd(size_t bufferSize);


void stackTraceClean(unsigned int tryDepth);
# 9 "../../repo/src/common/debug.h" 2
# 1 "../../repo/src/common/type/convert.h" 1
# 9 "../../repo/src/common/type/convert.h"
# 1 "/usr/include/inttypes.h" 1 3 4
# 27 "/usr/include/inttypes.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stdint.h" 1 3 4
# 9 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stdint.h" 3 4
# 1 "/usr/include/stdint.h" 1 3 4
# 26 "/usr/include/stdint.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 27 "/usr/include/stdint.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/wchar.h" 1 3 4
# 29 "/usr/include/stdint.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 30 "/usr/include/stdint.h" 2 3 4







# 1 "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h" 1 3 4
# 24 "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h" 3 4

# 24 "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h" 3 4
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;
# 38 "/usr/include/stdint.h" 2 3 4





typedef __int_least8_t int_least8_t;
typedef __int_least16_t int_least16_t;
typedef __int_least32_t int_least32_t;
typedef __int_least64_t int_least64_t;


typedef __uint_least8_t uint_least8_t;
typedef __uint_least16_t uint_least16_t;
typedef __uint_least32_t uint_least32_t;
typedef __uint_least64_t uint_least64_t;





typedef signed char int_fast8_t;

typedef long int int_fast16_t;
typedef long int int_fast32_t;
typedef long int int_fast64_t;
# 71 "/usr/include/stdint.h" 3 4
typedef unsigned char uint_fast8_t;

typedef unsigned long int uint_fast16_t;
typedef unsigned long int uint_fast32_t;
typedef unsigned long int uint_fast64_t;
# 87 "/usr/include/stdint.h" 3 4
typedef long int intptr_t;


typedef unsigned long int uintptr_t;
# 101 "/usr/include/stdint.h" 3 4
typedef __intmax_t intmax_t;
typedef __uintmax_t uintmax_t;
# 10 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stdint.h" 2 3 4
# 28 "/usr/include/inttypes.h" 2 3 4






typedef int __gwchar_t;
# 266 "/usr/include/inttypes.h" 3 4





typedef struct
  {
    long int quot;
    long int rem;
  } imaxdiv_t;
# 290 "/usr/include/inttypes.h" 3 4
extern intmax_t imaxabs (intmax_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern imaxdiv_t imaxdiv (intmax_t __numer, intmax_t __denom)
      __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern intmax_t strtoimax (const char *__restrict __nptr,
      char **__restrict __endptr, int __base) __attribute__ ((__nothrow__ , __leaf__));


extern uintmax_t strtoumax (const char *__restrict __nptr,
       char ** __restrict __endptr, int __base) __attribute__ ((__nothrow__ , __leaf__));


extern intmax_t wcstoimax (const __gwchar_t *__restrict __nptr,
      __gwchar_t **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__));


extern uintmax_t wcstoumax (const __gwchar_t *__restrict __nptr,
       __gwchar_t ** __restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__));
# 432 "/usr/include/inttypes.h" 3 4

# 10 "../../repo/src/common/type/convert.h" 2
# 23 "../../repo/src/common/type/convert.h"

# 23 "../../repo/src/common/type/convert.h"
size_t cvtCharToZ(char value, char *buffer, size_t bufferSize);


size_t cvtDoubleToZ(double value, char *buffer, size_t bufferSize);
double cvtZToDouble(const char *value);


size_t cvtIntToZ(int value, char *buffer, size_t bufferSize);
int cvtZToInt(const char *value);
int cvtZToIntBase(const char *value, int base);


size_t cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize);
int64_t cvtZToInt64(const char *value);
int64_t cvtZToInt64Base(const char *value, int base);





__attribute__((always_inline)) static inline uint32_t
cvtInt32ToZigZag(int32_t value)
{
    return ((uint32_t)value << 1) ^ (uint32_t)(value >> 31);
}

__attribute__((always_inline)) static inline int32_t
cvtInt32FromZigZag(uint32_t value)
{
    return (int32_t)((value >> 1) ^ (~(value & 1) + 1));
}

__attribute__((always_inline)) static inline uint64_t
cvtInt64ToZigZag(int64_t value)
{
    return ((uint64_t)value << 1) ^ (uint64_t)(value >> 63);
}

__attribute__((always_inline)) static inline int64_t
cvtInt64FromZigZag(uint64_t value)
{
    return (int64_t)((value >> 1) ^ (~(value & 1) + 1));
}


size_t cvtModeToZ(mode_t value, char *buffer, size_t bufferSize);
mode_t cvtZToMode(const char *value);


size_t cvtSizeToZ(size_t value, char *buffer, size_t bufferSize);
size_t cvtSSizeToZ(ssize_t value, char *buffer, size_t bufferSize);


size_t cvtTimeToZ(time_t value, char *buffer, size_t bufferSize);


size_t cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize);
unsigned int cvtZToUInt(const char *value);
unsigned int cvtZToUIntBase(const char *value, int base);


size_t cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize);
uint64_t cvtZToUInt64(const char *value);
uint64_t cvtZToUInt64Base(const char *value, int base);


size_t cvtBoolToZ(
# 89 "../../repo/src/common/type/convert.h" 3 4
                 _Bool 
# 89 "../../repo/src/common/type/convert.h"
                      value, char *buffer, size_t bufferSize);
const char *cvtBoolToConstZ(
# 90 "../../repo/src/common/type/convert.h" 3 4
                           _Bool 
# 90 "../../repo/src/common/type/convert.h"
                                value);
# 10 "../../repo/src/common/debug.h" 2
# 1 "../../repo/src/common/type/stringz.h" 1
# 11 "../../repo/src/common/debug.h" 2
# 118 "../../repo/src/common/debug.h"
size_t objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize);


size_t ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize);


size_t strzToLog(const char *string, char *buffer, size_t bufferSize);


size_t typeToLog(const char *typeName, char *buffer, size_t bufferSize);
# 9 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/common/log.h" 1






# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 1 3 4
# 34 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/syslimits.h" 1 3 4






# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 1 3 4
# 194 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 3 4
# 1 "/usr/include/limits.h" 1 3 4
# 26 "/usr/include/limits.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 27 "/usr/include/limits.h" 2 3 4
# 183 "/usr/include/limits.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/posix1_lim.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/posix1_lim.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 28 "/usr/include/x86_64-linux-gnu/bits/posix1_lim.h" 2 3 4
# 161 "/usr/include/x86_64-linux-gnu/bits/posix1_lim.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/local_lim.h" 1 3 4
# 38 "/usr/include/x86_64-linux-gnu/bits/local_lim.h" 3 4
# 1 "/usr/include/linux/limits.h" 1 3 4
# 39 "/usr/include/x86_64-linux-gnu/bits/local_lim.h" 2 3 4
# 162 "/usr/include/x86_64-linux-gnu/bits/posix1_lim.h" 2 3 4
# 184 "/usr/include/limits.h" 2 3 4



# 1 "/usr/include/x86_64-linux-gnu/bits/posix2_lim.h" 1 3 4
# 188 "/usr/include/limits.h" 2 3 4
# 195 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 2 3 4
# 8 "/usr/lib/gcc/x86_64-linux-gnu/9/include/syslimits.h" 2 3 4
# 35 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 2 3 4
# 8 "../../repo/src/common/log.h" 2
# 23 "../../repo/src/common/log.h"
void logInit(
    LogLevel logLevelStdOutParam, LogLevel logLevelStdErrParam, LogLevel logLevelFileParam, 
# 24 "../../repo/src/common/log.h" 3 4
                                                                                           _Bool 
# 24 "../../repo/src/common/log.h"
                                                                                                logTimestampParam,
    unsigned int processId, unsigned int logProcessMax, 
# 25 "../../repo/src/common/log.h" 3 4
                                                       _Bool 
# 25 "../../repo/src/common/log.h"
                                                            dryRun);


void logClose(void);




# 32 "../../repo/src/common/log.h" 3 4
_Bool 
# 32 "../../repo/src/common/log.h"
    logFileSet(const char *logFile);




# 36 "../../repo/src/common/log.h" 3 4
_Bool 
# 36 "../../repo/src/common/log.h"
    logAny(LogLevel logLevel);


LogLevel logLevelEnum(const char *logLevel);
const char *logLevelStr(LogLevel logLevel);
# 154 "../../repo/src/common/log.h"
void logInternal(
    LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *message);


void logInternalFmt(
    LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *format, ...) __attribute__((format(printf, 8, 9)));
# 10 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/common/memContext.h" 1
# 15 "../../repo/src/common/memContext.h"
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 143 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4

# 143 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4
typedef long int ptrdiff_t;
# 321 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4
typedef int wchar_t;
# 415 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4
typedef struct {
  long long __max_align_ll __attribute__((__aligned__(__alignof__(long long))));
  long double __max_align_ld __attribute__((__aligned__(__alignof__(long double))));
# 426 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 3 4
} max_align_t;
# 16 "../../repo/src/common/memContext.h" 2





# 20 "../../repo/src/common/memContext.h"
typedef struct MemContext MemContext;
# 45 "../../repo/src/common/memContext.h"
void *memNew(size_t size);


void *memNewPtrArray(size_t size);


void *memResize(const void *buffer, size_t size);


void memFree(void *buffer);
# 202 "../../repo/src/common/memContext.h"
MemContext *memContextNew(const char *name);


void memContextSwitch(MemContext *this);




void memContextSwitchBack(void);




void memContextKeep(void);




void memContextDiscard(void);



void memContextMove(MemContext *this, MemContext *parentNew);


void memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *);




void memContextCallbackClear(MemContext *this);


void memContextFree(MemContext *this);





MemContext *memContextCurrent(void);



# 244 "../../repo/src/common/memContext.h" 3 4
_Bool 
# 244 "../../repo/src/common/memContext.h"
    memContextFreeing(MemContext *this);


MemContext *memContextPrior(void);



MemContext *memContextTop(void);


const char *memContextName(MemContext *this);


size_t memContextSize(const MemContext *this);
# 271 "../../repo/src/common/memContext.h"
void memContextClean(unsigned int tryDepth);
# 11 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/common/time.h" 1







# 1 "/usr/include/time.h" 1 3 4
# 29 "/usr/include/time.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 30 "/usr/include/time.h" 2 3 4



# 1 "/usr/include/x86_64-linux-gnu/bits/time.h" 1 3 4
# 34 "/usr/include/time.h" 2 3 4





# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_tm.h" 1 3 4







# 7 "/usr/include/x86_64-linux-gnu/bits/types/struct_tm.h" 3 4
struct tm
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;


  long int tm_gmtoff;
  const char *tm_zone;




};
# 40 "/usr/include/time.h" 2 3 4
# 48 "/usr/include/time.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_itimerspec.h" 1 3 4







struct itimerspec
  {
    struct timespec it_interval;
    struct timespec it_value;
  };
# 49 "/usr/include/time.h" 2 3 4
struct sigevent;
# 68 "/usr/include/time.h" 3 4




extern clock_t clock (void) __attribute__ ((__nothrow__ , __leaf__));


extern time_t time (time_t *__timer) __attribute__ ((__nothrow__ , __leaf__));


extern double difftime (time_t __time1, time_t __time0)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern time_t mktime (struct tm *__tp) __attribute__ ((__nothrow__ , __leaf__));





extern size_t strftime (char *__restrict __s, size_t __maxsize,
   const char *__restrict __format,
   const struct tm *__restrict __tp) __attribute__ ((__nothrow__ , __leaf__));
# 104 "/usr/include/time.h" 3 4
extern size_t strftime_l (char *__restrict __s, size_t __maxsize,
     const char *__restrict __format,
     const struct tm *__restrict __tp,
     locale_t __loc) __attribute__ ((__nothrow__ , __leaf__));
# 119 "/usr/include/time.h" 3 4
extern struct tm *gmtime (const time_t *__timer) __attribute__ ((__nothrow__ , __leaf__));



extern struct tm *localtime (const time_t *__timer) __attribute__ ((__nothrow__ , __leaf__));




extern struct tm *gmtime_r (const time_t *__restrict __timer,
       struct tm *__restrict __tp) __attribute__ ((__nothrow__ , __leaf__));



extern struct tm *localtime_r (const time_t *__restrict __timer,
          struct tm *__restrict __tp) __attribute__ ((__nothrow__ , __leaf__));




extern char *asctime (const struct tm *__tp) __attribute__ ((__nothrow__ , __leaf__));


extern char *ctime (const time_t *__timer) __attribute__ ((__nothrow__ , __leaf__));






extern char *asctime_r (const struct tm *__restrict __tp,
   char *__restrict __buf) __attribute__ ((__nothrow__ , __leaf__));


extern char *ctime_r (const time_t *__restrict __timer,
        char *__restrict __buf) __attribute__ ((__nothrow__ , __leaf__));




extern char *__tzname[2];
extern int __daylight;
extern long int __timezone;




extern char *tzname[2];



extern void tzset (void) __attribute__ ((__nothrow__ , __leaf__));



extern int daylight;
extern long int timezone;
# 190 "/usr/include/time.h" 3 4
extern time_t timegm (struct tm *__tp) __attribute__ ((__nothrow__ , __leaf__));


extern time_t timelocal (struct tm *__tp) __attribute__ ((__nothrow__ , __leaf__));


extern int dysize (int __year) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));
# 205 "/usr/include/time.h" 3 4
extern int nanosleep (const struct timespec *__requested_time,
        struct timespec *__remaining);



extern int clock_getres (clockid_t __clock_id, struct timespec *__res) __attribute__ ((__nothrow__ , __leaf__));


extern int clock_gettime (clockid_t __clock_id, struct timespec *__tp) __attribute__ ((__nothrow__ , __leaf__));


extern int clock_settime (clockid_t __clock_id, const struct timespec *__tp)
     __attribute__ ((__nothrow__ , __leaf__));






extern int clock_nanosleep (clockid_t __clock_id, int __flags,
       const struct timespec *__req,
       struct timespec *__rem);


extern int clock_getcpuclockid (pid_t __pid, clockid_t *__clock_id) __attribute__ ((__nothrow__ , __leaf__));




extern int timer_create (clockid_t __clock_id,
    struct sigevent *__restrict __evp,
    timer_t *__restrict __timerid) __attribute__ ((__nothrow__ , __leaf__));


extern int timer_delete (timer_t __timerid) __attribute__ ((__nothrow__ , __leaf__));


extern int timer_settime (timer_t __timerid, int __flags,
     const struct itimerspec *__restrict __value,
     struct itimerspec *__restrict __ovalue) __attribute__ ((__nothrow__ , __leaf__));


extern int timer_gettime (timer_t __timerid, struct itimerspec *__value)
     __attribute__ ((__nothrow__ , __leaf__));


extern int timer_getoverrun (timer_t __timerid) __attribute__ ((__nothrow__ , __leaf__));





extern int timespec_get (struct timespec *__ts, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 301 "/usr/include/time.h" 3 4

# 9 "../../repo/src/common/time.h" 2





# 13 "../../repo/src/common/time.h"
typedef uint64_t TimeMSec;
# 25 "../../repo/src/common/time.h"
void sleepMSec(TimeMSec sleepMSec);


TimeMSec timeMSec(void);


void datePartsValid(int year, int month, int day);


void timePartsValid(int hour, int minute, int second);


void tzPartsValid(int tzHour, int tzMinute);


int tzOffsetSeconds(int tzHour, int tzMinute);



# 43 "../../repo/src/common/time.h" 3 4
_Bool 
# 43 "../../repo/src/common/time.h"
    yearIsLeap(int year);


int dayOfYear(int year, int month, int day);



time_t epochFromParts(int year, int month, int day, int hour, int minute, int second, int tzOffsetSecond);
# 12 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/common/type/json.h" 1






# 1 "../../repo/src/common/type/keyValue.h" 1
# 13 "../../repo/src/common/type/keyValue.h"
typedef struct KeyValue KeyValue;

# 1 "../../repo/src/common/type/variantList.h" 1
# 10 "../../repo/src/common/type/variantList.h"
typedef struct VariantList VariantList;

# 1 "../../repo/src/common/type/stringList.h" 1
# 10 "../../repo/src/common/type/stringList.h"
typedef struct StringList StringList;


# 1 "../../repo/src/common/type/list.h" 1






# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/limits.h" 1 3 4
# 8 "../../repo/src/common/type/list.h" 2







typedef struct List List;


# 1 "../../repo/src/common/type/param.h" 1
# 19 "../../repo/src/common/type/list.h" 2
# 1 "../../repo/src/common/type/string.h" 1
# 36 "../../repo/src/common/type/string.h"
typedef struct String String;


# 1 "../../repo/src/common/encode.h" 1






# 1 "/usr/lib/gcc/x86_64-linux-gnu/9/include/stddef.h" 1 3 4
# 8 "../../repo/src/common/encode.h" 2




typedef enum
{
    encodeBase64,
    encodeBase64Url,
} EncodeType;





void encodeToStr(EncodeType type, const unsigned char *source, size_t sourceSize, char *destination);


size_t encodeToStrSize(EncodeType type, size_t sourceSize);


void decodeToBin(EncodeType type, const char *source, unsigned char *destination);


size_t decodeToBinSize(EncodeType type, const char *source);
# 40 "../../repo/src/common/type/string.h" 2
# 1 "../../repo/src/common/type/buffer.h" 1
# 13 "../../repo/src/common/type/buffer.h"
typedef struct Buffer Buffer;


# 1 "../../repo/src/common/type/string.h" 1
# 17 "../../repo/src/common/type/buffer.h" 2
# 30 "../../repo/src/common/type/buffer.h"
typedef struct BufferConst
{
    size_t sizeAlloc; size_t size; 
# 32 "../../repo/src/common/type/buffer.h" 3 4
   _Bool 
# 32 "../../repo/src/common/type/buffer.h"
   sizeLimit; size_t used;


    const void *buffer;
} BufferConst;




Buffer *bufNew(size_t size);


Buffer *bufNewC(const void *buffer, size_t size);


Buffer *bufNewDecode(EncodeType type, const String *string);


Buffer *bufDup(const Buffer *buffer);





Buffer *bufCat(Buffer *this, const Buffer *cat);


Buffer *bufCatC(Buffer *this, const unsigned char *cat, size_t catOffset, size_t catSize);


Buffer *bufCatSub(Buffer *this, const Buffer *cat, size_t catOffset, size_t catSize);



# 65 "../../repo/src/common/type/buffer.h" 3 4
_Bool 
# 65 "../../repo/src/common/type/buffer.h"
    bufEq(const Buffer *this, const Buffer *compare);


String *bufHex(const Buffer *this);


Buffer *bufMove(Buffer *this, MemContext *parentNew);


Buffer *bufResize(Buffer *this, size_t size);


void bufLimitClear(Buffer *this);
void bufLimitSet(Buffer *this, size_t limit);


__attribute__((always_inline)) static inline size_t
bufSize(const Buffer *this)
{
    ;
    return ((const BufferConst *)this)->size;
}


__attribute__((always_inline)) static inline size_t
bufSizeAlloc(const Buffer *this)
{
    ;
    return ((const BufferConst *)this)->sizeAlloc;
}



__attribute__((always_inline)) static inline size_t
bufUsed(const Buffer *this)
{
    ;
    return ((const BufferConst *)this)->used;
}

void bufUsedInc(Buffer *this, size_t inc);
void bufUsedSet(Buffer *this, size_t used);
void bufUsedZero(Buffer *this);


__attribute__((always_inline)) static inline 
# 110 "../../repo/src/common/type/buffer.h" 3 4
                                            _Bool

# 111 "../../repo/src/common/type/buffer.h"
bufEmpty(const Buffer *this)
{
    return bufUsed(this) == 0;
}


__attribute__((always_inline)) static inline size_t
bufRemains(const Buffer *this)
{
    return bufSize(this) - bufUsed(this);
}


__attribute__((always_inline)) static inline 
# 124 "../../repo/src/common/type/buffer.h" 3 4
                                            _Bool

# 125 "../../repo/src/common/type/buffer.h"
bufFull(const Buffer *this)
{
    return bufUsed(this) == bufSize(this);
}





__attribute__((always_inline)) static inline unsigned char *
bufPtr(Buffer *this)
{
    ;
    return (void *)((BufferConst *)this)->buffer;
}


__attribute__((always_inline)) static inline const unsigned char *
bufPtrConst(const Buffer *this)
{
    ;
    return ((const BufferConst *)this)->buffer;
}


__attribute__((always_inline)) static inline unsigned char *
bufRemainsPtr(Buffer *this)
{
    return bufPtr(this) + bufUsed(this);
}




void bufFree(Buffer *this);
# 202 "../../repo/src/common/type/buffer.h"
extern const Buffer *const BRACEL_BUF;
extern const Buffer *const BRACER_BUF;
extern const Buffer *const BRACKETL_BUF;
extern const Buffer *const BRACKETR_BUF;
extern const Buffer *const COMMA_BUF;
extern const Buffer *const CR_BUF;
extern const Buffer *const DOT_BUF;
extern const Buffer *const EQ_BUF;
extern const Buffer *const LF_BUF;
extern const Buffer *const QUOTED_BUF;




String *bufToLog(const Buffer *this);
# 41 "../../repo/src/common/type/string.h" 2
# 53 "../../repo/src/common/type/string.h"
typedef struct StringConst
{
    uint64_t size:32; uint64_t extra:32; char *buffer;
} StringConst;





String *strNew(const char *string);



String *strNewBuf(const Buffer *buffer);


String *strNewDbl(double value);


String *strNewEncode(EncodeType type, const Buffer *buffer);


String *strNewFmt(const char *format, ...) __attribute__((format(printf, 1, 2)));



String *strNewN(const char *string, size_t size);


String *strDup(const String *this);






String *strBase(const String *this);
const char *strBaseZ(const String *this);



# 93 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 93 "../../repo/src/common/type/string.h"
    strBeginsWith(const String *this, const String *beginsWith);

# 94 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 94 "../../repo/src/common/type/string.h"
    strBeginsWithZ(const String *this, const char *beginsWith);


String *strCat(String *this, const String *cat);
String *strCatZ(String *this, const char *cat);


String *strCatChr(String *this, char cat);


String *strCatEncode(String *this, EncodeType type, const Buffer *buffer);


String *strCatFmt(String *this, const char *format, ...) __attribute__((format(printf, 2, 3)));



String *strCatZN(String *this, const char *cat, size_t size);


int strChr(const String *this, char chr);


int strCmp(const String *this, const String *compare);
int strCmpZ(const String *this, const char *compare);



# 121 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 121 "../../repo/src/common/type/string.h"
    strEmpty(const String *this);



# 124 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 124 "../../repo/src/common/type/string.h"
    strEndsWith(const String *this, const String *endsWith);

# 125 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 125 "../../repo/src/common/type/string.h"
    strEndsWithZ(const String *this, const char *endsWith);



# 128 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 128 "../../repo/src/common/type/string.h"
    strEq(const String *this, const String *compare);

# 129 "../../repo/src/common/type/string.h" 3 4
_Bool 
# 129 "../../repo/src/common/type/string.h"
    strEqZ(const String *this, const char *compare);


String *strFirstUpper(String *this);


String *strFirstLower(String *this);


String *strUpper(String *this);


String *strLower(String *this);


String *strPath(const String *this);


String *strPathAbsolute(const String *this, const String *base);


__attribute__((always_inline)) static inline const char *
strZ(const String *this)
{
    ;
    return ((const StringConst *)this)->buffer;
}

const char *strZNull(const String *this);


String *strQuote(const String *this, const String *quote);
String *strQuoteZ(const String *this, const char *quote);


String *strReplaceChr(String *this, char find, char replace);


__attribute__((always_inline)) static inline size_t
strSize(const String *this)
{
    ;
    return ((const StringConst *)this)->size;
}


String *strSizeFormat(const uint64_t fileSize);


String *strSub(const String *this, size_t start);


String *strSubN(const String *this, size_t start, size_t size);


String *strTrim(String *this);


String *strTrunc(String *this, int idx);




void strFree(String *this);
# 227 "../../repo/src/common/type/string.h"
extern const String *const BRACKETL_STR;
extern const String *const BRACKETR_STR;
extern const String *const COLON_STR;
extern const String *const CR_STR;
extern const String *const CRLF_STR;
extern const String *const DASH_STR;
extern const String *const DOT_STR;
extern const String *const DOTDOT_STR;
extern const String *const EMPTY_STR;
extern const String *const EQ_STR;
extern const String *const FALSE_STR;
extern const String *const FSLASH_STR;
extern const String *const LF_STR;
extern const String *const N_STR;
extern const String *const NULL_STR;
extern const String *const QUOTED_STR;
extern const String *const TRUE_STR;
extern const String *const Y_STR;
extern const String *const ZERO_STR;




typedef String *(*StrObjToLogFormat)(const void *object);

size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize);







String *strToLog(const String *this);
# 20 "../../repo/src/common/type/list.h" 2




typedef enum
{
    sortOrderNone,
    sortOrderAsc,
    sortOrderDesc,
} SortOrder;
# 46 "../../repo/src/common/type/list.h"
typedef int ListComparator(const void *item1, const void *item2);


int lstComparatorStr(const void *item1, const void *item2);




typedef struct ListParam
{
    
# 56 "../../repo/src/common/type/list.h" 3 4
   _Bool 
# 56 "../../repo/src/common/type/list.h"
   dummy;
    SortOrder sortOrder;
    ListComparator *comparator;
} ListParam;




List *lstNew(size_t itemSize, ListParam param);





void *lstAdd(List *this, const void *item);


List *lstClear(List *this);


void *lstGet(const List *this, unsigned int listIdx);
void *lstGetLast(const List *this);



# 80 "../../repo/src/common/type/list.h" 3 4
_Bool 
# 80 "../../repo/src/common/type/list.h"
    lstExists(const List *this, const void *item);


void *lstFind(const List *this, const void *item);
void *lstFindDefault(const List *this, const void *item, void *itemDefault);
unsigned int lstFindIdx(const List *this, const void *item);


unsigned int lstIdx(const List *this, const void *item);


void *lstInsert(List *this, unsigned int listIdx, const void *item);


MemContext *lstMemContext(const List *this);


List *lstMove(List *this, MemContext *parentNew);



# 100 "../../repo/src/common/type/list.h" 3 4
_Bool 
# 100 "../../repo/src/common/type/list.h"
    lstRemove(List *this, const void *item);
List *lstRemoveIdx(List *this, unsigned int listIdx);
List *lstRemoveLast(List *this);


unsigned int lstSize(const List *this);


__attribute__((always_inline)) static inline 
# 108 "../../repo/src/common/type/list.h" 3 4
                                            _Bool

# 109 "../../repo/src/common/type/list.h"
lstEmpty(const List *this)
{
    return lstSize(this) == 0;
}


List *lstSort(List *this, SortOrder sortOrder);





List *lstComparatorSet(List *this, ListComparator *comparator);




void lstFree(List *this);




String *lstToLog(const List *this);
# 14 "../../repo/src/common/type/stringList.h" 2

# 1 "../../repo/src/common/type/variantList.h" 1
# 16 "../../repo/src/common/type/stringList.h" 2




StringList *strLstNew(void);


StringList *strLstNewSplit(const String *string, const String *delimiter);
StringList *strLstNewSplitZ(const String *string, const char *delimiter);



StringList *strLstNewSplitSize(const String *string, const String *delimiter, size_t size);
StringList *strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size);


StringList *strLstNewVarLst(const VariantList *sourceList);


StringList *strLstDup(const StringList *sourceList);





String *strLstAdd(StringList *this, const String *string);
String *strLstAddZ(StringList *this, const char *string);
String *strLstAddIfMissing(StringList *this, const String *string);



# 46 "../../repo/src/common/type/stringList.h" 3 4
_Bool 
# 46 "../../repo/src/common/type/stringList.h"
    strLstExists(const StringList *this, const String *string);

# 47 "../../repo/src/common/type/stringList.h" 3 4
_Bool 
# 47 "../../repo/src/common/type/stringList.h"
    strLstExistsZ(const StringList *this, const char *cstring);


String *strLstInsert(StringList *this, unsigned int listIdx, const String *string);
String *strLstInsertZ(StringList *this, unsigned int listIdx, const char *string);


String *strLstGet(const StringList *this, unsigned int listIdx);


String *strLstJoin(const StringList *this, const char *separator);


String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);



StringList *strLstMergeAnti(const StringList *this, const StringList *anti);


StringList *strLstMove(StringList *this, MemContext *parentNew);



const char **strLstPtr(const StringList *this);



# 74 "../../repo/src/common/type/stringList.h" 3 4
_Bool 
# 74 "../../repo/src/common/type/stringList.h"
    strLstRemove(StringList *this, const String *item);
StringList *strLstRemoveIdx(StringList *this, unsigned int listIdx);


unsigned int strLstSize(const StringList *this);


__attribute__((always_inline)) static inline 
# 81 "../../repo/src/common/type/stringList.h" 3 4
                                            _Bool

# 82 "../../repo/src/common/type/stringList.h"
strLstEmpty(const StringList *this)
{
    return strLstSize(this) == 0;
}


StringList *strLstSort(StringList *this, SortOrder sortOrder);





StringList *strLstComparatorSet(StringList *this, ListComparator *comparator);




void strLstFree(StringList *this);




String *strLstToLog(const StringList *this);
# 13 "../../repo/src/common/type/variantList.h" 2
# 1 "../../repo/src/common/type/variant.h" 1
# 29 "../../repo/src/common/type/variant.h"
typedef struct Variant Variant;




typedef enum
{
    varTypeBool,
    varTypeInt,
    varTypeInt64,
    varTypeKeyValue,
    varTypeString,
    varTypeUInt,
    varTypeUInt64,
    varTypeVariantList,
} VariantType;

# 1 "../../repo/src/common/type/keyValue.h" 1
# 47 "../../repo/src/common/type/variant.h" 2






Variant *varNewBool(
# 53 "../../repo/src/common/type/variant.h" 3 4
                   _Bool 
# 53 "../../repo/src/common/type/variant.h"
                        data);
Variant *varNewInt(int data);
Variant *varNewInt64(int64_t data);



Variant *varNewKv(KeyValue *data);

Variant *varNewStr(const String *data);
Variant *varNewStrZ(const char *data);
Variant *varNewUInt(unsigned int data);
Variant *varNewUInt64(uint64_t data);
Variant *varNewVarLst(const VariantList *data);

Variant *varDup(const Variant *this);






# 73 "../../repo/src/common/type/variant.h" 3 4
_Bool 
# 73 "../../repo/src/common/type/variant.h"
    varEq(const Variant *this1, const Variant *this2);


VariantType varType(const Variant *this);





# 81 "../../repo/src/common/type/variant.h" 3 4
_Bool 
# 81 "../../repo/src/common/type/variant.h"
    varBool(const Variant *this);

# 82 "../../repo/src/common/type/variant.h" 3 4
_Bool 
# 82 "../../repo/src/common/type/variant.h"
    varBoolForce(const Variant *this);

int varInt(const Variant *this);
int varIntForce(const Variant *this);

int64_t varInt64(const Variant *this);
int64_t varInt64Force(const Variant *this);

KeyValue *varKv(const Variant *this);

const String *varStr(const Variant *this);
String *varStrForce(const Variant *this);

unsigned int varUInt(const Variant *this);
unsigned int varUIntForce(const Variant *this);

uint64_t varUInt64(const Variant *this);
uint64_t varUInt64Force(const Variant *this);

VariantList *varVarLst(const Variant *this);




void varFree(Variant *this);
# 120 "../../repo/src/common/type/variant.h"
typedef struct VariantBoolConst
{
    VariantType type;
    const 
# 123 "../../repo/src/common/type/variant.h" 3 4
         _Bool 
# 123 "../../repo/src/common/type/variant.h"
         data;
} VariantBoolConst;




typedef struct VariantIntConst
{
    VariantType type;
    const int data;
} VariantIntConst;




typedef struct VariantInt64Const
{
    VariantType type;
    const int64_t data;
} VariantInt64Const;




typedef struct VariantStringConst
{
    VariantType type;
    const String *data;
} VariantStringConst;




typedef struct VariantUIntConst
{
    VariantType type;
    const unsigned int data;
} VariantUIntConst;




typedef struct VariantUInt64Const
{
    VariantType type;
    const uint64_t data;
} VariantUInt64Const;
# 228 "../../repo/src/common/type/variant.h"
extern const Variant *const BOOL_FALSE_VAR;
extern const Variant *const BOOL_TRUE_VAR;




String *varToLog(const Variant *this);
# 14 "../../repo/src/common/type/variantList.h" 2




VariantList *varLstNew(void);


VariantList *varLstNewStrLst(const StringList *stringList);


VariantList *varLstDup(const VariantList *source);





VariantList *varLstAdd(VariantList *this, Variant *data);


Variant *varLstGet(const VariantList *this, unsigned int listIdx);


VariantList *varLstMove(VariantList *this, MemContext *parentNew);


unsigned int varLstSize(const VariantList *this);


__attribute__((always_inline)) static inline 
# 42 "../../repo/src/common/type/variantList.h" 3 4
                                            _Bool

# 43 "../../repo/src/common/type/variantList.h"
varLstEmpty(const VariantList *this)
{
    return varLstSize(this) == 0;
}




void varLstFree(VariantList *this);
# 16 "../../repo/src/common/type/keyValue.h" 2




KeyValue *kvNew(void);
KeyValue *kvDup(const KeyValue *source);





KeyValue *kvAdd(KeyValue *this, const Variant *key, const Variant *value);


const VariantList *kvKeyList(const KeyValue *this);

KeyValue *kvMove(KeyValue *this, MemContext *parentNew);


KeyValue *kvPut(KeyValue *this, const Variant *key, const Variant *value);



KeyValue *kvPutKv(KeyValue *this, const Variant *key);


const Variant *kvGet(const KeyValue *this, const Variant *key);


const Variant *kvGetDefault(const KeyValue *this, const Variant *key, const Variant *defaultValue);



# 48 "../../repo/src/common/type/keyValue.h" 3 4
_Bool 
# 48 "../../repo/src/common/type/keyValue.h"
    kvKeyExists(const KeyValue *this, const Variant *key);


VariantList *kvGetList(const KeyValue *this, const Variant *key);




void kvFree(KeyValue *this);
# 8 "../../repo/src/common/type/json.h" 2






# 13 "../../repo/src/common/type/json.h" 3 4
_Bool 
# 13 "../../repo/src/common/type/json.h"
    jsonToBool(const String *json);


int jsonToInt(const String *json);
int64_t jsonToInt64(const String *json);
unsigned int jsonToUInt(const String *json);
uint64_t jsonToUInt64(const String *json);


KeyValue *jsonToKv(const String *json);


String *jsonToStr(const String *json);


Variant *jsonToVar(const String *json);


VariantList *jsonToVarLst(const String *json);


const String *jsonFromBool(
# 34 "../../repo/src/common/type/json.h" 3 4
                          _Bool 
# 34 "../../repo/src/common/type/json.h"
                               value);


String *jsonFromInt(int number);
String *jsonFromInt64(int64_t number);
String *jsonFromUInt(unsigned int number);
String *jsonFromUInt64(uint64_t number);


String *jsonFromKv(const KeyValue *kv);


String *jsonFromStr(const String *string);


String *jsonFromVar(const Variant *var);
# 13 "../../repo/src/protocol/server.c" 2


# 1 "../../repo/src/common/type/object.h" 1
# 21 "../../repo/src/common/type/object.h"
# 1 "../../repo/src/common/macro.h" 1
# 22 "../../repo/src/common/type/object.h" 2
# 16 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/protocol/client.h" 1
# 13 "../../repo/src/protocol/client.h"
typedef struct ProtocolClient ProtocolClient;

# 1 "../../repo/src/common/io/read.h" 1
# 18 "../../repo/src/common/io/read.h"
typedef struct IoRead IoRead;

# 1 "../../repo/src/common/io/filter/group.h" 1
# 19 "../../repo/src/common/io/filter/group.h"
typedef struct IoFilterGroup IoFilterGroup;

# 1 "../../repo/src/common/io/filter/filter.h" 1
# 20 "../../repo/src/common/io/filter/filter.h"
typedef struct IoFilter IoFilter;
# 29 "../../repo/src/common/io/filter/filter.h"
Variant *ioFilterResult(const IoFilter *this);


const String *ioFilterType(const IoFilter *this);




void ioFilterFree(IoFilter *this);




String *ioFilterToLog(const IoFilter *this);
# 22 "../../repo/src/common/io/filter/group.h" 2





IoFilterGroup *ioFilterGroupNew(void);





IoFilterGroup *ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter);


IoFilterGroup *ioFilterGroupInsert(IoFilterGroup *this, unsigned int listIdx, IoFilter *filter);


IoFilterGroup *ioFilterGroupClear(IoFilterGroup *this);


void ioFilterGroupOpen(IoFilterGroup *this);


void ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input, Buffer *output);


void ioFilterGroupClose(IoFilterGroup *this);






# 54 "../../repo/src/common/io/filter/group.h" 3 4
_Bool 
# 54 "../../repo/src/common/io/filter/group.h"
    ioFilterGroupDone(const IoFilterGroup *this);




# 58 "../../repo/src/common/io/filter/group.h" 3 4
_Bool 
# 58 "../../repo/src/common/io/filter/group.h"
    ioFilterGroupInputSame(const IoFilterGroup *this);


Variant *ioFilterGroupParamAll(const IoFilterGroup *this);


const Variant *ioFilterGroupResult(const IoFilterGroup *this, const String *filterType);


const Variant *ioFilterGroupResultAll(const IoFilterGroup *this);


void ioFilterGroupResultAllSet(IoFilterGroup *this, const Variant *filterResult);


unsigned int ioFilterGroupSize(const IoFilterGroup *this);




void ioFilterGroupFree(IoFilterGroup *this);




String *ioFilterGroupToLog(const IoFilterGroup *this);
# 21 "../../repo/src/common/io/read.h" 2







# 27 "../../repo/src/common/io/read.h" 3 4
_Bool 
# 27 "../../repo/src/common/io/read.h"
    ioReadOpen(IoRead *this);


size_t ioRead(IoRead *this, Buffer *buffer);


size_t ioReadSmall(IoRead *this, Buffer *buffer);


String *ioReadLine(IoRead *this);


String *ioReadLineParam(IoRead *this, 
# 39 "../../repo/src/common/io/read.h" 3 4
                                     _Bool 
# 39 "../../repo/src/common/io/read.h"
                                          allowEof);



typedef struct IoReadReadyParam
{
    
# 45 "../../repo/src/common/io/read.h" 3 4
   _Bool 
# 45 "../../repo/src/common/io/read.h"
   dummy;
    
# 46 "../../repo/src/common/io/read.h" 3 4
   _Bool 
# 46 "../../repo/src/common/io/read.h"
        error;
} IoReadReadyParam;





# 52 "../../repo/src/common/io/read.h" 3 4
_Bool 
# 52 "../../repo/src/common/io/read.h"
    ioReadReady(IoRead *this, IoReadReadyParam param);


void ioReadClose(IoRead *this);






# 61 "../../repo/src/common/io/read.h" 3 4
_Bool 
# 61 "../../repo/src/common/io/read.h"
    ioReadBlock(const IoRead *this);



# 64 "../../repo/src/common/io/read.h" 3 4
_Bool 
# 64 "../../repo/src/common/io/read.h"
    ioReadEof(const IoRead *this);


IoFilterGroup *ioReadFilterGroup(const IoRead *this);


int ioReadFd(const IoRead *this);




void ioReadFree(IoRead *this);
# 16 "../../repo/src/protocol/client.h" 2
# 1 "../../repo/src/common/io/write.h" 1
# 17 "../../repo/src/common/io/write.h"
typedef struct IoWrite IoWrite;
# 26 "../../repo/src/common/io/write.h"
void ioWriteOpen(IoWrite *this);


void ioWrite(IoWrite *this, const Buffer *buffer);


void ioWriteLine(IoWrite *this, const Buffer *buffer);


typedef struct IoWriteReadyParam
{
    
# 37 "../../repo/src/common/io/write.h" 3 4
   _Bool 
# 37 "../../repo/src/common/io/write.h"
   dummy;
    
# 38 "../../repo/src/common/io/write.h" 3 4
   _Bool 
# 38 "../../repo/src/common/io/write.h"
        error;
} IoWriteReadyParam;





# 44 "../../repo/src/common/io/write.h" 3 4
_Bool 
# 44 "../../repo/src/common/io/write.h"
    ioWriteReady(IoWrite *this, IoWriteReadyParam param);


void ioWriteStr(IoWrite *this, const String *string);


void ioWriteStrLine(IoWrite *this, const String *string);


void ioWriteFlush(IoWrite *this);


void ioWriteClose(IoWrite *this);





IoFilterGroup *ioWriteFilterGroup(const IoWrite *this);


int ioWriteFd(const IoWrite *this);




void ioWriteFree(IoWrite *this);
# 17 "../../repo/src/protocol/client.h" 2
# 1 "../../repo/src/protocol/command.h" 1
# 13 "../../repo/src/protocol/command.h"
typedef struct ProtocolCommand ProtocolCommand;

# 1 "../../repo/src/common/type/stringId.h" 1
# 10 "../../repo/src/common/type/stringId.h"
typedef uint64_t StringId;
# 16 "../../repo/src/protocol/command.h" 2






    extern const String *const PROTOCOL_KEY_COMMAND_STR;

    extern const String *const PROTOCOL_KEY_PARAMETER_STR;




ProtocolCommand *protocolCommandNew(const StringId command);





ProtocolCommand *protocolCommandMove(ProtocolCommand *this, MemContext *parentNew);


ProtocolCommand *protocolCommandParamAdd(ProtocolCommand *this, const Variant *param);





String *protocolCommandJson(const ProtocolCommand *this);




void protocolCommandFree(ProtocolCommand *this);




String *protocolCommandToLog(const ProtocolCommand *this);
# 18 "../../repo/src/protocol/client.h" 2





    extern const String *const PROTOCOL_GREETING_NAME_STR;

    extern const String *const PROTOCOL_GREETING_SERVICE_STR;

    extern const String *const PROTOCOL_GREETING_VERSION_STR;





    extern const String *const PROTOCOL_ERROR_STR;

    extern const String *const PROTOCOL_ERROR_STACK_STR;


    extern const String *const PROTOCOL_OUTPUT_STR;




ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);





const Variant *protocolClientExecute(ProtocolClient *this, const ProtocolCommand *command, 
# 49 "../../repo/src/protocol/client.h" 3 4
                                                                                          _Bool 
# 49 "../../repo/src/protocol/client.h"
                                                                                               outputRequired);
ProtocolClient *protocolClientMove(ProtocolClient *this, MemContext *parentNew);


void protocolClientNoOp(ProtocolClient *this);


String *protocolClientReadLine(ProtocolClient *this);


const Variant *protocolClientReadOutput(ProtocolClient *this, 
# 59 "../../repo/src/protocol/client.h" 3 4
                                                             _Bool 
# 59 "../../repo/src/protocol/client.h"
                                                                  outputRequired);


void protocolClientWriteCommand(ProtocolClient *this, const ProtocolCommand *command);





IoRead *protocolClientIoRead(const ProtocolClient *this);


IoWrite *protocolClientIoWrite(const ProtocolClient *this);




void protocolClientFree(ProtocolClient *this);




String *protocolClientToLog(const ProtocolClient *this);
# 17 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/protocol/helper.h" 1
# 10 "../../repo/src/protocol/helper.h"
typedef enum
{
    protocolStorageTypeRepo,
    protocolStorageTypePg,
} ProtocolStorageType;







    extern const String *const PROTOCOL_SERVICE_LOCAL_STR;

    extern const String *const PROTOCOL_SERVICE_REMOTE_STR;
# 33 "../../repo/src/protocol/helper.h"
void protocolKeepAlive(void);


ProtocolClient *protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostId, unsigned int protocolId);


void protocolLocalFree(unsigned int protocolId);


ProtocolClient *protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostId);


void protocolRemoteFree(unsigned int hostId);






# 51 "../../repo/src/protocol/helper.h" 3 4
_Bool 
# 51 "../../repo/src/protocol/helper.h"
    pgIsLocal(unsigned int pgIdx);


void pgIsLocalVerify(void);



# 57 "../../repo/src/protocol/helper.h" 3 4
_Bool 
# 57 "../../repo/src/protocol/helper.h"
    repoIsLocal(unsigned int repoIdx);


void repoIsLocalVerify(void);
void repoIsLocalVerifyIdx(unsigned int repoIdx);


ProtocolStorageType protocolStorageTypeEnum(const String *type);
const String *protocolStorageTypeStr(ProtocolStorageType type);




void protocolFree(void);
# 18 "../../repo/src/protocol/server.c" 2
# 1 "../../repo/src/protocol/server.h" 1
# 13 "../../repo/src/protocol/server.h"
typedef struct ProtocolServer ProtocolServer;
# 25 "../../repo/src/protocol/server.h"
typedef void (*ProtocolServerCommandHandler)(const VariantList *paramList, ProtocolServer *server);

typedef struct ProtocolServerHandler
{
    StringId command;
    ProtocolServerCommandHandler handler;
} ProtocolServerHandler;






ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);





void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);


void protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize);


void protocolServerResponse(ProtocolServer *this, const Variant *output);


ProtocolServer *protocolServerMove(ProtocolServer *this, MemContext *parentNew);


void protocolServerWriteLine(const ProtocolServer *this, const String *line);





IoRead *protocolServerIoRead(const ProtocolServer *this);


IoWrite *protocolServerIoWrite(const ProtocolServer *this);




void protocolServerFree(ProtocolServer *this);




String *protocolServerToLog(const ProtocolServer *this);
# 19 "../../repo/src/protocol/server.c" 2





struct ProtocolServer
{
    MemContext *memContext;
    const String *name;
    IoRead *read;
    IoWrite *write;
};

ProtocolServer * protocolServerMove(ProtocolServer *this, MemContext *parentNew) { ; ; ; ; ; if (this != 
# 32 "../../repo/src/protocol/server.c" 3 4
((void *)0)
# 32 "../../repo/src/protocol/server.c"
) memContextMove(this->memContext, parentNew); return this; };
void protocolServerFree(ProtocolServer *this) { ; ; ; if (this != 
# 33 "../../repo/src/protocol/server.c" 3 4
((void *)0)
# 33 "../../repo/src/protocol/server.c"
) memContextFree(this->memContext); ; };


ProtocolServer *
protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write)
{
    LogLevel FUNCTION_LOG_logLevel = stackTracePush("../../repo/src/protocol/server.c", __func__, logLevelTrace); if (logAny(FUNCTION_LOG_logLevel)) { stackTraceParamLog();;
        stackTraceParamAdd(strObjToLog(name, (StrObjToLogFormat)strToLog, stackTraceParamBuffer("name"), 4096));
        stackTraceParamAdd(strObjToLog(service, (StrObjToLogFormat)strToLog, stackTraceParamBuffer("service"), 4096));
        stackTraceParamAdd(objToLog(read, "IoRead", stackTraceParamBuffer("read"), 4096));
        stackTraceParamAdd(objToLog(write, "IoWrite", stackTraceParamBuffer("write"), 4096));
    do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 44 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 44 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "(%s)", stackTraceParam()); } } while (0); };

    ;
    ;
    ;

    ProtocolServer *this = 
# 50 "../../repo/src/protocol/server.c" 3 4
                          ((void *)0)
# 50 "../../repo/src/protocol/server.c"
                              ;

    do { MemContext *MEM_CONTEXT_NEW_memContext = memContextNew("ProtocolServer"); memContextSwitch(MEM_CONTEXT_NEW_memContext);
    {
        this = memNew(sizeof(ProtocolServer));

        *this = (ProtocolServer)
        {
            .memContext = memContextCurrent(),
            .name = strDup(name),
            .read = read,
            .write = write,
        };


        do { MemContext *MEM_CONTEXT_TEMP_memContext = memContextNew("temporary"); memContextSwitch(MEM_CONTEXT_TEMP_memContext);
        {
            KeyValue *greetingKv = kvNew();
            kvPut(greetingKv, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_GREETING_NAME_STR}), ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = ((const String *)&(const StringConst){.buffer = (char *)("pgBackRest"), .size = (unsigned int)strlen("pgBackRest")})}));
            kvPut(greetingKv, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_GREETING_SERVICE_STR}), ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = service}));
            kvPut(greetingKv, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_GREETING_VERSION_STR}), ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = ((const String *)&(const StringConst){.buffer = (char *)("2.33dev"), .size = (unsigned int)strlen("2.33dev")})}));

            ioWriteStrLine(this->write, jsonFromKv(greetingKv));
            ioWriteFlush(this->write);
        }
        memContextSwitchBack(); memContextDiscard(); } while (0);
    }
    memContextSwitchBack(); memContextKeep(); } while (0);

    do { ProtocolServer * FUNCTION_LOG_RETURN_result = this; stackTracePop();; if (logAny(FUNCTION_LOG_logLevel)) { char buffer[4096]; strObjToLog(FUNCTION_LOG_RETURN_result, (StrObjToLogFormat)protocolServerToLog, buffer, sizeof(buffer)); do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 79 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 79 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "=> %s", buffer); } } while (0); } return FUNCTION_LOG_RETURN_result; } while (0);
}


void
protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack)
{
    LogLevel FUNCTION_LOG_logLevel = stackTracePush("../../repo/src/protocol/server.c", __func__, logLevelTrace); if (logAny(FUNCTION_LOG_logLevel)) { stackTraceParamLog();;
        stackTraceParamAdd(strObjToLog(this, (StrObjToLogFormat)protocolServerToLog, stackTraceParamBuffer("this"), 4096));
        stackTraceParamAdd(cvtIntToZ(code, stackTraceParamBuffer("code"), 4096));
        stackTraceParamAdd(strObjToLog(message, (StrObjToLogFormat)strToLog, stackTraceParamBuffer("message"), 4096));
        stackTraceParamAdd(strObjToLog(stack, (StrObjToLogFormat)strToLog, stackTraceParamBuffer("stack"), 4096));
    do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 91 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 91 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "(%s)", stackTraceParam()); } } while (0); };

    ;
    ;
    ;
    ;

    KeyValue *error = kvNew();
    kvPut(error, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_ERROR_STR}), ((const Variant *)&(const VariantIntConst){.type = varTypeInt, .data = code}));
    kvPut(error, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_OUTPUT_STR}), ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = message}));
    kvPut(error, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_ERROR_STACK_STR}), ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = stack}));

    ioWriteStrLine(this->write, jsonFromKv(error));
    ioWriteFlush(this->write);

    do { stackTracePop();; do { if (logAny(FUNCTION_LOG_logLevel)) { logInternal(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 106 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 106 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "=> void"); } } while (0); } while (0);
}


void
protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize)
{
    LogLevel FUNCTION_LOG_logLevel = stackTracePush("../../repo/src/protocol/server.c", __func__, logLevelDebug); if (logAny(FUNCTION_LOG_logLevel)) { stackTraceParamLog();;
        stackTraceParamAdd(strObjToLog(this, (StrObjToLogFormat)protocolServerToLog, stackTraceParamBuffer("this"), 4096));
        stackTraceParamAdd(objToLog(retryInterval, "VariantList", stackTraceParamBuffer("retryInterval"), 4096));
        do { char *buffer = stackTraceParamBuffer("handlerList"); if (handlerList == 
# 118 "../../repo/src/protocol/server.c" 3 4
       ((void *)0)
# 118 "../../repo/src/protocol/server.c"
       ) stackTraceParamAdd(typeToLog("null", buffer, 4096)); else { buffer[0] = '*'; stackTraceParamAdd(typeToLog("void", buffer + 1, 4096 - 1) + 1); } } while (0);
        stackTraceParamAdd(cvtUIntToZ(handlerListSize, stackTraceParamBuffer("handlerListSize"), 4096));
    do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 120 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 120 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "(%s)", stackTraceParam()); } } while (0); };

    ;
    ;
    ;


    
# 127 "../../repo/src/protocol/server.c" 3 4
   _Bool 
# 127 "../../repo/src/protocol/server.c"
        exit = 
# 127 "../../repo/src/protocol/server.c" 3 4
               0
# 127 "../../repo/src/protocol/server.c"
                    ;

    do
    {
        do { if (errorInternalTry("../../repo/src/protocol/server.c", __func__, 131) && 
# 131 "../../repo/src/protocol/server.c" 3 4
       _setjmp (
# 131 "../../repo/src/protocol/server.c"
       *errorInternalJump()
# 131 "../../repo/src/protocol/server.c" 3 4
       ) 
# 131 "../../repo/src/protocol/server.c"
       >= 0) { while (errorInternalProcess(
# 131 "../../repo/src/protocol/server.c" 3 4
       0
# 131 "../../repo/src/protocol/server.c"
       )) { if (errorInternalStateTry())
        {
            do { MemContext *MEM_CONTEXT_TEMP_memContext = memContextNew("temporary"); memContextSwitch(MEM_CONTEXT_TEMP_memContext);
            {

                KeyValue *commandKv = jsonToKv(ioReadLine(this->read));
                const StringId command = varUInt64(kvGet(commandKv, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_KEY_COMMAND_STR})));
                VariantList *paramList = varVarLst(kvGet(commandKv, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_KEY_PARAMETER_STR})));


                ProtocolServerCommandHandler handler = 
# 141 "../../repo/src/protocol/server.c" 3 4
                                                      ((void *)0)
# 141 "../../repo/src/protocol/server.c"
                                                          ;

                for (unsigned int handlerIdx = 0; handlerIdx < handlerListSize; handlerIdx++)
                {
                    if (command == handlerList[handlerIdx].command)
                    {
                        handler = handlerList[handlerIdx].handler;
                        break;
                    }
                }


                if (handler != 
# 153 "../../repo/src/protocol/server.c" 3 4
                              ((void *)0)
# 153 "../../repo/src/protocol/server.c"
                                  )
                {


                    do { memContextSwitch(this->memContext);
                    {

                        
# 160 "../../repo/src/protocol/server.c" 3 4
                       _Bool 
# 160 "../../repo/src/protocol/server.c"
                            retry = 
# 160 "../../repo/src/protocol/server.c" 3 4
                                    0
# 160 "../../repo/src/protocol/server.c"
                                         ;
                        unsigned int retryRemaining = retryInterval != 
# 161 "../../repo/src/protocol/server.c" 3 4
                                                                      ((void *)0) 
# 161 "../../repo/src/protocol/server.c"
                                                                           ? varLstSize(retryInterval) : 0;


                        do
                        {
                            retry = 
# 166 "../../repo/src/protocol/server.c" 3 4
                                   0
# 166 "../../repo/src/protocol/server.c"
                                        ;

                            do { if (errorInternalTry("../../repo/src/protocol/server.c", __func__, 168) && 
# 168 "../../repo/src/protocol/server.c" 3 4
                           _setjmp (
# 168 "../../repo/src/protocol/server.c"
                           *errorInternalJump()
# 168 "../../repo/src/protocol/server.c" 3 4
                           ) 
# 168 "../../repo/src/protocol/server.c"
                           >= 0) { while (errorInternalProcess(
# 168 "../../repo/src/protocol/server.c" 3 4
                           0
# 168 "../../repo/src/protocol/server.c"
                           )) { if (errorInternalStateTry())
                            {
                                handler(paramList, this);
                            }
                            else if (errorInternalStateCatch(&RuntimeError))
                            {

                                if (retryRemaining > 0)
                                {

                                    TimeMSec retrySleepMs = varUInt64(
                                        varLstGet(retryInterval, varLstSize(retryInterval) - retryRemaining));


                                    do { if (logAny(logLevelDebug)) { logInternalFmt(logLevelDebug, logLevelAssert, logLevelTrace, 
# 182 "../../repo/src/protocol/server.c" 3 4
                                   (0x7fffffff * 2U + 1U)
# 182 "../../repo/src/protocol/server.c"
                                   , "../../repo/src/protocol/server.c", __func__, 0, "retry %s after %" 
# 182 "../../repo/src/protocol/server.c" 3 4
                                   "l" "u" 
# 182 "../../repo/src/protocol/server.c"
                                   "ms: %s", errorTypeName(errorType()), retrySleepMs, errorMessage()); } } while (0)

                                                       ;


                                    if (retrySleepMs > 0)
                                        sleepMSec(retrySleepMs);


                                    retryRemaining--;
                                    retry = 
# 192 "../../repo/src/protocol/server.c" 3 4
                                           1
# 192 "../../repo/src/protocol/server.c"
                                               ;



                                    protocolKeepAlive();
                                }
                                else
                                    errorInternalPropagate();
                            }
                            } } } while (0);
                        }
                        while (retry);
                    }
                    memContextSwitchBack(); } while (0);
                }

                else
                {
                    switch (command)
                    {
                        case ((((uint64_t)'e' || (uint64_t)'x' << 8) || (uint64_t)'i' << 8) || (uint64_t)'t' << 8):
                            exit = 
# 213 "../../repo/src/protocol/server.c" 3 4
                                  1
# 213 "../../repo/src/protocol/server.c"
                                      ;
                            break;

                        case ((((uint64_t)'n' || (uint64_t)'o' << 8) || (uint64_t)'o' << 8) || (uint64_t)'p' << 8):
                            protocolServerResponse(this, 
# 217 "../../repo/src/protocol/server.c" 3 4
                                                        ((void *)0)
# 217 "../../repo/src/protocol/server.c"
                                                            );
                            break;

                        default:
                            errorInternalThrowFmt(&ProtocolError, "../../repo/src/protocol/server.c", __func__, 221, "invalid command '%s'", strZ(command));
                    }
                }




                protocolKeepAlive();
            }
            memContextSwitchBack(); memContextDiscard(); } while (0);
        }
        else if (errorInternalStateCatch(&RuntimeError))
        {

            protocolServerError(this, errorCode(), ((const String *)&(const StringConst){.buffer = (char *)(errorMessage()), .size = (unsigned int)strlen(errorMessage())}), ((const String *)&(const StringConst){.buffer = (char *)(errorStackTrace()), .size = (unsigned int)strlen(errorStackTrace())}));
        }
        } } } while (0);
    }
    while (!exit);

    do { stackTracePop();; do { if (logAny(FUNCTION_LOG_logLevel)) { logInternal(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 241 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 241 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "=> void"); } } while (0); } while (0);
}


void
protocolServerResponse(ProtocolServer *this, const Variant *output)
{
    LogLevel FUNCTION_LOG_logLevel = stackTracePush("../../repo/src/protocol/server.c", __func__, logLevelTrace); if (logAny(FUNCTION_LOG_logLevel)) { stackTraceParamLog();;
        stackTraceParamAdd(strObjToLog(this, (StrObjToLogFormat)protocolServerToLog, stackTraceParamBuffer("this"), 4096));
        stackTraceParamAdd(strObjToLog(output, (StrObjToLogFormat)varToLog, stackTraceParamBuffer("output"), 4096));
    do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 251 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 251 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "(%s)", stackTraceParam()); } } while (0); };

    KeyValue *result = kvNew();

    if (output != 
# 255 "../../repo/src/protocol/server.c" 3 4
                 ((void *)0)
# 255 "../../repo/src/protocol/server.c"
                     )
        kvAdd(result, ((const Variant *)&(const VariantStringConst){.type = varTypeString, .data = PROTOCOL_OUTPUT_STR}), output);

    ioWriteStrLine(this->write, jsonFromKv(result));
    ioWriteFlush(this->write);

    do { stackTracePop();; do { if (logAny(FUNCTION_LOG_logLevel)) { logInternal(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 261 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 261 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "=> void"); } } while (0); } while (0);
}


void
protocolServerWriteLine(const ProtocolServer *this, const String *line)
{
    LogLevel FUNCTION_LOG_logLevel = stackTracePush("../../repo/src/protocol/server.c", __func__, logLevelTrace); if (logAny(FUNCTION_LOG_logLevel)) { stackTraceParamLog();;
        stackTraceParamAdd(strObjToLog(this, (StrObjToLogFormat)protocolServerToLog, stackTraceParamBuffer("this"), 4096));
        stackTraceParamAdd(strObjToLog(line, (StrObjToLogFormat)strToLog, stackTraceParamBuffer("line"), 4096));
    do { if (logAny(FUNCTION_LOG_logLevel)) { logInternalFmt(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 271 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 271 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "(%s)", stackTraceParam()); } } while (0); };

    ;


    ioWrite(this->write, DOT_BUF);


    if (line != 
# 279 "../../repo/src/protocol/server.c" 3 4
               ((void *)0)
# 279 "../../repo/src/protocol/server.c"
                   )
        ioWriteStr(this->write, line);


    ioWrite(this->write, LF_BUF);

    do { stackTracePop();; do { if (logAny(FUNCTION_LOG_logLevel)) { logInternal(FUNCTION_LOG_logLevel, logLevelAssert, logLevelTrace, 
# 285 "../../repo/src/protocol/server.c" 3 4
   (0x7fffffff * 2U + 1U)
# 285 "../../repo/src/protocol/server.c"
   , "../../repo/src/protocol/server.c", __func__, 0, "=> void"); } } while (0); } while (0);
}


IoRead *
protocolServerIoRead(const ProtocolServer *this)
{
    ;
        ;
    ;

    ;

    return this->read;
}


IoWrite *
protocolServerIoWrite(const ProtocolServer *this)
{
    ;
        ;
    ;

    ;

    return this->write;
}


String *
protocolServerToLog(const ProtocolServer *this)
{
    return strNewFmt("{name: %s}", strZ(this->name));
}
