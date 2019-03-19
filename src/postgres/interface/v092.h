/***********************************************************************************************************************************
PostgreSQL 9.2 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE092_H
#define POSTGRES_INTERFACE_INTERFACE092_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs092(const Buffer *controlFile);
PgControl pgInterfaceControl092(const Buffer *controlFile);
bool pgInterfaceWalIs092(const Buffer *walFile);
PgWal pgInterfaceWal092(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest092(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest092(PgWal pgWal, Buffer *buffer);
#endif

#endif
