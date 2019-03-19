/***********************************************************************************************************************************
PostgreSQL 9.1 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE091_H
#define POSTGRES_INTERFACE_INTERFACE091_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs091(const Buffer *controlFile);
PgControl pgInterfaceControl091(const Buffer *controlFile);
bool pgInterfaceWalIs091(const Buffer *walFile);
PgWal pgInterfaceWal091(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest091(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest091(PgWal pgWal, Buffer *buffer);
#endif

#endif
