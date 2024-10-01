#ifndef COMMON_WALFILTER_WALFILTER_H
#define COMMON_WALFILTER_WALFILTER_H

#include "command/archive/get/file.h"
#include "postgres/interface.h"

FN_EXTERN IoFilter *walFilterNew(PgControl pgControl, const ArchiveGetFile *archiveInfo);

#endif // COMMON_WALFILTER_WALFILTER_H
