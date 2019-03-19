/***********************************************************************************************************************************
PostgreSQL 9.5 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE095_H
#define POSTGRES_INTERFACE_INTERFACE095_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs095(const Buffer *controlFile);
PgControl pgInterfaceControl095(const Buffer *controlFile);
bool pgInterfaceWalIs095(const Buffer *walFile);
PgWal pgInterfaceWal095(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest095(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest095(PgWal pgWal, Buffer *buffer);
#endif

#endif
