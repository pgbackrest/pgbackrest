/***********************************************************************************************************************************
PostgreSQL 10 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE100_H
#define POSTGRES_INTERFACE_INTERFACE100_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs100(const Buffer *controlFile);
PgControl pgInterfaceControl100(const Buffer *controlFile);
bool pgInterfaceWalIs100(const Buffer *walFile);
PgWal pgInterfaceWal100(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest100(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest100(PgWal pgWal, Buffer *buffer);
#endif

#endif
