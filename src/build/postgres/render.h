/***********************************************************************************************************************************
Render PostgreSQL Interface
***********************************************************************************************************************************/
#ifndef BUILD_POSTGRES_RENDER_H
#define BUILD_POSTGRES_RENDER_H

#include "build/postgres/parse.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Render auto-generated PostgreSQL files
void bldPgRender(const Storage *const storageRepo, const BldPg bldPg);
void bldPgVersionRender(const Storage *const storageRepo, const BldPg bldPg);

#endif
