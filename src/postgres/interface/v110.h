/***********************************************************************************************************************************
PostgreSQL 11 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE110_H
#define POSTGRES_INTERFACE_INTERFACE110_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs110(const Buffer *controlFile);
PgControl pgInterfaceControl110(const Buffer *controlFile);
bool pgInterfaceWalIs110(const Buffer *walFile);
PgWal pgInterfaceWal110(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest110(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest110(PgWal pgWal, Buffer *buffer);
#endif

#endif
