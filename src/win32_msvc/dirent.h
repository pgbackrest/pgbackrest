/***********************************************************************************************************************************
MSVC compatibility header for <dirent.h>
***********************************************************************************************************************************/
#ifndef DIRENT_WIN32_H
#define DIRENT_WIN32_H

#if defined(_WIN64) && defined(_MSC_VER)

// From https://www.gnu.org/software/libc/manual/html_node/Permission-Bits.html
#define S_IRUSR 0400
#define S_IREAD 0400

#define S_IWUSR 0200
#define S_IWRITE 0200

#define S_IXUSR 0100
#define S_IEXEC 0100

#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP 040

#define S_IWGRP 020

#define S_IXGRP 010

#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH 04

#define S_IWOTH 02

#define S_IXOTH 01

#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

#endif

#endif
