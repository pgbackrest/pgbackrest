# ----------------------------------------------------------------------------------------------------------------------------------
# PostgreSQL Query Client
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::PgClient

####################################################################################################################################
pgBackRest::LibC::PgClient
new(class, host, port, database, queryTimeout)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    const String *class = STR_NEW_SV($arg);
    const String *host = STR_NEW_SV($arg);
    U32 port
    const String *database = STR_NEW_SV($arg);
    UV queryTimeout
CODE:
    CHECK(strEqZ(class, PACKAGE_NAME_LIBC "::PgClient"));

    memContextSwitch(MEM_CONTEXT_XS_OLD());
    RETVAL = pgClientNew(host, port, database, NULL, queryTimeout);
    memContextSwitch(MEM_CONTEXT_XS_TEMP());
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
open(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::PgClient self
CODE:
    pgClientOpen(self);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
query(self, query)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::PgClient self
    const String *query = STR_NEW_SV($arg);
CODE:
    VariantList *result = pgClientQuery(self, query);
    RETVAL = result ? strPtr(jsonFromVar(varNewVarLst(result), 0)) : NULL;
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
close(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::PgClient self
CODE:
    pgClientClose(self);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
DESTROY(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::PgClient self
CODE:
    pgClientFree(self);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();
