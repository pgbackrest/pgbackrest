/***********************************************************************************************************************************
Helper macros for LibC.xs
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Package Names
***********************************************************************************************************************************/
#define PACKAGE_NAME                                                "pgBackRest"
#define PACKAGE_NAME_LIBC                                           PACKAGE_NAME "::LibC"

/***********************************************************************************************************************************
Error handling macros that throw a Perl error when a C error is caught
***********************************************************************************************************************************/
#define ERROR_XS_BEGIN()                                                                                                           \
    TRY_BEGIN()

#define ERROR_XS()                                                                                                                 \
    croak("PGBRCLIB:%d:%s:%d:%s", errorCode(), errorFileName(), errorFileLine(), errorMessage());

#define ERROR_XS_END()                                                                                                             \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        ERROR_XS();                                                                                                                \
    }                                                                                                                              \
    TRY_END();

/***********************************************************************************************************************************
Simplifies switching to a temp memory context in functions and includes error handling
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_TEMP()                                                                                                      \
    MEM_CONTEXT_XS_TEMP_memContext

#define MEM_CONTEXT_XS_TEMP_BEGIN()                                                                                                \
{                                                                                                                                  \
    /* Create temp memory context */                                                                                               \
    MemContext *MEM_CONTEXT_XS_TEMP() = memContextNew("temporary");                                                                \
                                                                                                                                   \
    /* Switch to temp memory context */                                                                                            \
    memContextPush(MEM_CONTEXT_XS_TEMP());                                                                                         \
                                                                                                                                   \
    /* Store any errors to be croaked to Perl at the end */                                                                        \
    bool MEM_CONTEXT_XS_croak = false;                                                                                             \
                                                                                                                                   \
    /* Try the statement block */                                                                                                  \
    TRY_BEGIN()

#define MEM_CONTEXT_XS_TEMP_END()                                                                                                  \
    /* Set error to be croak to Perl later */                                                                                      \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        MEM_CONTEXT_XS_croak = true;                                                                                               \
    }                                                                                                                              \
    /* Free the context on error */                                                                                                \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
        memContextPop();                                                                                                           \
        memContextDiscard();                                                                                                       \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
    /* Croak on error */                                                                                                           \
    if (MEM_CONTEXT_XS_croak)                                                                                                      \
    {                                                                                                                              \
        ERROR_XS()                                                                                                                 \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
Create new string from an SV
***********************************************************************************************************************************/
#define STR_NEW_SV(param)                                                                                                          \
    (SvOK(param) ? strNewN(SvPV_nolen(param), SvCUR(param)) : NULL)

/***********************************************************************************************************************************
Create const buffer from an SV
***********************************************************************************************************************************/
#define BUF_CONST_SV(param)                                                                                                        \
    (SvOK(param) ? BUF(SvPV_nolen(param), SvCUR(param)) : NULL)
