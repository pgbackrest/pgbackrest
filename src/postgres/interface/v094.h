/***********************************************************************************************************************************
PostgreSQL 9.4 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE094_H
#define POSTGRES_INTERFACE_INTERFACE094_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs094(const Buffer *controlFile);
PgControl pgInterfaceControl094(const Buffer *controlFile);
bool pgInterfaceWalIs094(const Buffer *walFile);
PgWal pgInterfaceWal094(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest094(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest094(PgWal pgWal, Buffer *buffer);
#endif

#endif
