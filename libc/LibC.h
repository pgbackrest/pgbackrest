/***********************************************************************************************************************************
Helper macros for LibC.xs
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Package Names
***********************************************************************************************************************************/
#define PACKAGE_NAME                                                "pgBackRest"
#define PACKAGE_NAME_LIBC                                           PACKAGE_NAME "::LibC"

/***********************************************************************************************************************************
Load C error into ERROR_SV_error

#define FUNCTION_NAME_ERROR                                         "libcExceptionNew"

#define ERROR_SV()                                                                                                                 \
    SV *ERROR_SV_error;                                                                                                            \
                                                                                                                                   \
    {                                                                                                                              \
        // Push parameters onto the Perl stack                                                                                     \
        ENTER;                                                                                                                     \
        SAVETMPS;                                                                                                                  \
        PUSHMARK(SP);                                                                                                              \
        EXTEND(SP, 2);                                                                                                             \
        PUSHs(sv_2mortal(newSViv(errorCode())));                                                                                   \
        PUSHs(sv_2mortal(newSVpv(errorMessage(), 0)));                                                                             \
        PUTBACK;                                                                                                                   \
                                                                                                                                   \
        // Call error function                                                                                                     \
        int count = call_pv(PACKAGE_NAME_LIBC "::" FUNCTION_NAME_ERROR, G_SCALAR);                                                 \
        SPAGAIN;                                                                                                                   \
                                                                                                                                   \
        // Check that correct number of parameters was returned                                                                    \
        if (count != 1)                                                                                                            \
            croak("expected 1 return value from " FUNCTION_NAME_ERROR "()");                                                       \
                                                                                                                                   \
        // Make a copy of the error that can be returned                                                                           \
        ERROR_SV_error = newSVsv(POPs);                                                                                            \
                                                                                                                                   \
        // Clean up the stack                                                                                                      \
        PUTBACK;                                                                                                                   \
        FREETMPS;                                                                                                                  \
        LEAVE;                                                                                                                     \
    }

This turned out to be a dead end because Perl 5.10 does not support croak_sv(), but this code has been kept for example purposes.
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Error handling macros that throw a Perl error when a C error is caught
***********************************************************************************************************************************/
#define ERROR_XS_BEGIN()                                                                                                           \
    ERROR_TRY()

#define ERROR_XS()                                                                                                                 \
    croak("PGBRCLIB:%d:%s:%d:%s", errorCode(), errorFileName(), errorFileLine(), errorMessage());

#define ERROR_XS_END()                                                                                                             \
    ERROR_CATCH_ANY()                                                                                                              \
    {                                                                                                                              \
        ERROR_XS();                                                                                                                \
    }
