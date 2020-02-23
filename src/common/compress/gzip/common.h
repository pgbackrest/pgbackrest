/***********************************************************************************************************************************
Gzip Common
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_GZIP_COMMON_H
#define COMMON_COMPRESS_GZIP_COMMON_H

/***********************************************************************************************************************************
Gzip extension
***********************************************************************************************************************************/
#define GZIP_EXT                                                    "gz"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define WINDOW_BITS                                                 15
#define WANT_GZIP                                                   16

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int gzipError(int error);

#endif
