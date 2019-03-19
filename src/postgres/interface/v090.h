/***********************************************************************************************************************************
PostgreSQL 9.0 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE090_H
#define POSTGRES_INTERFACE_INTERFACE090_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs090(const Buffer *controlFile);
PgControl pgInterfaceControl090(const Buffer *controlFile);
bool pgInterfaceWalIs090(const Buffer *walFile);
PgWal pgInterfaceWal090(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest090(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest090(PgWal pgWal, Buffer *buffer);
#endif

#endif
