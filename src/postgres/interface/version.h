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
uint32_t pgInterfaceControlVersion083(void);
bool pgInterfaceWalIs083(const unsigned char *walFile);
PgWal pgInterfaceWal083(const unsigned char *controlFile);

bool pgInterfaceControlIs084(const unsigned char *controlFile);
PgControl pgInterfaceControl084(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion084(void);
bool pgInterfaceWalIs084(const unsigned char *walFile);
PgWal pgInterfaceWal084(const unsigned char *controlFile);

bool pgInterfaceControlIs090(const unsigned char *controlFile);
PgControl pgInterfaceControl090(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion090(void);
bool pgInterfaceWalIs090(const unsigned char *walFile);
PgWal pgInterfaceWal090(const unsigned char *controlFile);

bool pgInterfaceControlIs091(const unsigned char *controlFile);
PgControl pgInterfaceControl091(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion091(void);
bool pgInterfaceWalIs091(const unsigned char *walFile);
PgWal pgInterfaceWal091(const unsigned char *controlFile);

bool pgInterfaceControlIs092(const unsigned char *controlFile);
PgControl pgInterfaceControl092(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion092(void);
bool pgInterfaceWalIs092(const unsigned char *walFile);
PgWal pgInterfaceWal092(const unsigned char *controlFile);

bool pgInterfaceControlIs093(const unsigned char *controlFile);
PgControl pgInterfaceControl093(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion093(void);
bool pgInterfaceWalIs093(const unsigned char *walFile);
PgWal pgInterfaceWal093(const unsigned char *controlFile);

bool pgInterfaceControlIs094(const unsigned char *controlFile);
PgControl pgInterfaceControl094(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion094(void);
bool pgInterfaceWalIs094(const unsigned char *walFile);
PgWal pgInterfaceWal094(const unsigned char *controlFile);

bool pgInterfaceControlIs095(const unsigned char *controlFile);
PgControl pgInterfaceControl095(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion095(void);
bool pgInterfaceWalIs095(const unsigned char *walFile);
PgWal pgInterfaceWal095(const unsigned char *controlFile);

bool pgInterfaceControlIs096(const unsigned char *controlFile);
PgControl pgInterfaceControl096(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion096(void);
bool pgInterfaceWalIs096(const unsigned char *walFile);
PgWal pgInterfaceWal096(const unsigned char *controlFile);

bool pgInterfaceControlIs100(const unsigned char *controlFile);
PgControl pgInterfaceControl100(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion100(void);
bool pgInterfaceWalIs100(const unsigned char *walFile);
PgWal pgInterfaceWal100(const unsigned char *controlFile);

bool pgInterfaceControlIs110(const unsigned char *controlFile);
PgControl pgInterfaceControl110(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion110(void);
bool pgInterfaceWalIs110(const unsigned char *walFile);
PgWal pgInterfaceWal110(const unsigned char *controlFile);

bool pgInterfaceControlIs120(const unsigned char *controlFile);
PgControl pgInterfaceControl120(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion120(void);
bool pgInterfaceWalIs120(const unsigned char *walFile);
PgWal pgInterfaceWal120(const unsigned char *controlFile);

bool pgInterfaceControlIs130(const unsigned char *controlFile);
PgControl pgInterfaceControl130(const unsigned char *controlFile);
uint32_t pgInterfaceControlVersion130(void);
bool pgInterfaceWalIs130(const unsigned char *walFile);
PgWal pgInterfaceWal130(const unsigned char *controlFile);

#endif
