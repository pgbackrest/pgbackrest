#ifndef PGBACKREST_PARTIALRESTORE_H
#define PGBACKREST_PARTIALRESTORE_H

#include "postgres/interface/static.vendor.h"

FN_EXTERN bool isRelationNeeded(Oid dbNode, Oid spcNode, Oid relNode);
FN_EXTERN void relationFilterInit(void);
#endif // PGBACKREST_PARTIALRESTORE_H
