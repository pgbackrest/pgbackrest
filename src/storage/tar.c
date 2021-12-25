/***********************************************************************************************************************************
Tape Archive

Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
Portions Copyright (c) 1994, Regents of the University of California

Adapted from PostgreSQL src/port/tar.c and https://www.gnu.org/software/tar/manual/html_node/Standard.html.
***********************************************************************************************************************************/
#include "build.auto.h"

#include "string.h"

#include "common/debug.h"
#include "storage/tar.h"

/***********************************************************************************************************************************
Tar header data
***********************************************************************************************************************************/
#define TAR_HEADER_DATA_NAME_SIZE                                   100
#define TAR_HEADER_DATA_MODE_SIZE                                   8
#define TAR_HEADER_DATA_UID_SIZE                                    8
#define TAR_HEADER_DATA_GID_SIZE                                    8
#define TAR_HEADER_DATA_SIZE_SIZE                                   12
#define TAR_HEADER_DATA_MTIME_SIZE                                  12

typedef struct TarHeaderData
{
  char name[TAR_HEADER_DATA_NAME_SIZE];
  char mode[TAR_HEADER_DATA_MODE_SIZE];
  char uid[TAR_HEADER_DATA_UID_SIZE];
  char gid[TAR_HEADER_DATA_GID_SIZE];
  char size[TAR_HEADER_DATA_SIZE_SIZE];
  char mtime[TAR_HEADER_DATA_MTIME_SIZE];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
} TarHeaderData;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct TarHeader
{
    TarHeaderPub pub;
    TarHeaderData data;
};

/***********************************************************************************************************************************
Read u64 value from tar field

Per POSIX, the way to write a number is in octal with leading zeroes and one trailing space (or NUL, but we use space) at the end of
the specified field width.

However, the given value may not fit in the available space in octal form. If that's true, we use the GNU extension of writing \200
followed by the number in base-256 form (ie, stored in binary MSB-first). Not that we support only non-negative numbers, so we don't
worry about the GNU rules for handling negative numbers.
***********************************************************************************************************************************/
static uint64_t
tarHdrReadU64(const char *field, unsigned int fieldSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, field);
        FUNCTION_TEST_PARAM(UINT, fieldSize);
    FUNCTION_TEST_END();

    uint64_t result = 0;

    // Decode base-256
    if (*field == '\200')
    {
        while (--fieldSize)
        {
            result <<= 8;
            result |= (unsigned char)(*++field);
        }
    }
    // Else decode octal
    else
    {
        while (fieldSize-- && *field >= '0' && *field <= '7')
        {
            result <<= 3;
            result |= (uint64_t)(*field - '0');

            field++;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Write u64 value into tar field

The POSIX-approved format for a number is octal, ending with a space or NUL. However, for values that don't fit, we recognize the
GNU extension of \200 followed by the number in base-256 form (ie, stored in binary MSB-first). Note that we support only
non-negative numbers, so we don't worry about the GNU rules for handling negative numbers.
***********************************************************************************************************************************/
static void
tarHdrWriteU64(char *const field, unsigned int fieldSize, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, field);
        FUNCTION_TEST_PARAM(UINT, fieldSize);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    // Use octal with trailing space when there is enough space
    if (value < (uint64_t)1 << (fieldSize - 1) * 3)
    {
        field[--fieldSize] = ' ';

        while (fieldSize)
        {
            field[--fieldSize] = (char)((value & 7) + '0');
            value >>= 3;
        }
    }
    // Else use base-256 with leading \200
    else
    {
        field[0] = '\200';

        while (fieldSize > 1)
        {
            field[--fieldSize] = (char)value;
            value >>= 8;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
TarHeader *
tarHdrNew(const TarHeaderNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, param.name);
        FUNCTION_TEST_PARAM(UINT64, param.size);
        FUNCTION_TEST_PARAM(TIME, param.timeModified);
        FUNCTION_TEST_PARAM(MODE, param.mode);
        FUNCTION_TEST_PARAM(UINT, param.userId);
        FUNCTION_TEST_PARAM(STRING, param.user);
        FUNCTION_TEST_PARAM(UINT, param.groupId);
        FUNCTION_TEST_PARAM(STRING, param.group);
    FUNCTION_TEST_END();

    ASSERT(param.name != NULL);

    TarHeader *this = NULL;

    OBJ_NEW_BEGIN(TarHeader)
    {
        // Allocate state and set context
        this = OBJ_NEW_ALLOC();

        *this = (TarHeader)
        {
            .pub =
            {
                .name = strDup(param.name),
                .size = param.size,
            },
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Name
            if (strSize(tarHdrName(this)) >= TAR_HEADER_DATA_NAME_SIZE)
                THROW_FMT(FormatError, "file name '%s' is too long for the tar format", strZ(tarHdrName(this)));

            strncpy(this->data.name, strZ(tarHdrName(this)), TAR_HEADER_DATA_NAME_SIZE);

            // Size
            tarHdrWriteU64(this->data.size, TAR_HEADER_DATA_SIZE_SIZE, param.size);

            // Time modified
            tarHdrWriteU64(this->data.mtime, TAR_HEADER_DATA_MTIME_SIZE, (uint64_t)param.timeModified);

            // Mode (do not include the file type bits, e.g. S_IFMT)
            tarHdrWriteU64(this->data.mode, TAR_HEADER_DATA_MODE_SIZE, param.mode & 07777);

            // User and group
            tarHdrWriteU64(this->data.uid, TAR_HEADER_DATA_UID_SIZE, param.userId);
            tarHdrWriteU64(this->data.gid, TAR_HEADER_DATA_GID_SIZE, param.groupId);

            (void)tarHdrReadU64; // !!!

            // /* Mod Time 12 */
            // print_tar_number(&h[136], 12, mtime);

            // /* Checksum 8 cannot be calculated until we've filled all other fields */

            // if (linktarget != NULL)
            // {
            //     /* Type - Symbolic link */
            //     h[156] = '2';
            //     /* Link Name 100 */
            //     strlcpy(&h[157], linktarget, 100);
            // }
            // else if (S_ISDIR(mode))
            // {
            //     /* Type - directory */
            //     h[156] = '5';
            // }
            // else
            // {
            //     /* Type - regular file */
            //     h[156] = '0';
            // }

            // /* Magic 6 */
            // strcpy(&h[257], "ustar");

            // /* Version 2 */
            // memcpy(&h[263], "00", 2);

            // /* User 32 */
            // /* XXX: Do we need to care about setting correct username? */
            // strlcpy(&h[265], "postgres", 32);

            // /* Group 32 */
            // /* XXX: Do we need to care about setting correct group name? */
            // strlcpy(&h[297], "postgres", 32);

            // /* Major Dev 8 */
            // print_tar_number(&h[329], 8, 0);

            // /* Minor Dev 8 */
            // print_tar_number(&h[337], 8, 0);

            // /* Prefix 155 - not used, leave as nulls */

            // /* Finally, compute and insert the checksum */
            // print_tar_number(&h[148], 8, tarChecksum(h));
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
tarHdrToLog(const TarHeader *this)
{
    return strNewFmt("{name: %s, size: %" PRIu64 "}", strZ(tarHdrName(this)), tarHdrSize(this));
}
