/***********************************************************************************************************************************
PostgreSQL Version Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_VERSION_H
#define POSTGRES_INTERFACE_VERSION_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs083(const unsigned char *controlFile);
PgControl pgInterfaceControl083(const unsigned char *controlFile);
bool pgInterfaceWalIs083(const unsigned char *walFile);
PgWal pgInterfaceWal083(const unsigned char *controlFile);

bool pgInterfaceControlIs084(const unsigned char *controlFile);
PgControl pgInterfaceControl084(const unsigned char *controlFile);
bool pgInterfaceWalIs084(const unsigned char *walFile);
PgWal pgInterfaceWal084(const unsigned char *controlFile);

bool pgInterfaceControlIs090(const unsigned char *controlFile);
PgControl pgInterfaceControl090(const unsigned char *controlFile);
bool pgInterfaceWalIs090(const unsigned char *walFile);
PgWal pgInterfaceWal090(const unsigned char *controlFile);

bool pgInterfaceControlIs091(const unsigned char *controlFile);
PgControl pgInterfaceControl091(const unsigned char *controlFile);
bool pgInterfaceWalIs091(const unsigned char *walFile);
PgWal pgInterfaceWal091(const unsigned char *controlFile);

bool pgInterfaceControlIs092(const unsigned char *controlFile);
PgControl pgInterfaceControl092(const unsigned char *controlFile);
bool pgInterfaceWalIs092(const unsigned char *walFile);
PgWal pgInterfaceWal092(const unsigned char *controlFile);

bool pgInterfaceControlIs093(const unsigned char *controlFile);
PgControl pgInterfaceControl093(const unsigned char *controlFile);
bool pgInterfaceWalIs093(const unsigned char *walFile);
PgWal pgInterfaceWal093(const unsigned char *controlFile);

bool pgInterfaceControlIs094(const unsigned char *controlFile);
PgControl pgInterfaceControl094(const unsigned char *controlFile);
bool pgInterfaceWalIs094(const unsigned char *walFile);
PgWal pgInterfaceWal094(const unsigned char *controlFile);

bool pgInterfaceControlIs095(const unsigned char *controlFile);
PgControl pgInterfaceControl095(const unsigned char *controlFile);
bool pgInterfaceWalIs095(const unsigned char *walFile);
PgWal pgInterfaceWal095(const unsigned char *controlFile);

bool pgInterfaceControlIs096(const unsigned char *controlFile);
PgControl pgInterfaceControl096(const unsigned char *controlFile);
bool pgInterfaceWalIs096(const unsigned char *walFile);
PgWal pgInterfaceWal096(const unsigned char *controlFile);

bool pgInterfaceControlIs100(const unsigned char *controlFile);
PgControl pgInterfaceControl100(const unsigned char *controlFile);
bool pgInterfaceWalIs100(const unsigned char *walFile);
PgWal pgInterfaceWal100(const unsigned char *controlFile);

bool pgInterfaceControlIs110(const unsigned char *controlFile);
PgControl pgInterfaceControl110(const unsigned char *controlFile);
bool pgInterfaceWalIs110(const unsigned char *walFile);
PgWal pgInterfaceWal110(const unsigned char *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest083(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest083(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest084(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest084(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest090(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest090(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest091(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest091(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest092(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest092(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest093(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest093(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest094(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest094(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest095(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest095(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest096(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest096(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest100(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest100(PgWal pgWal, unsigned char *buffer);

    void pgInterfaceControlTest110(PgControl pgControl, unsigned char *buffer);
    void pgInterfaceWalTest110(PgWal pgWal, unsigned char *buffer);
#endif

#endif
