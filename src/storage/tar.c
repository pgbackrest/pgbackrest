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
#define TAR_HEADER_NAME_SIZE                                        100
#define TAR_HEADER_MODE_SIZE                                        8
#define TAR_HEADER_UID_SIZE                                         8
#define TAR_HEADER_GID_SIZE                                         8
#define TAR_HEADER_SIZE_SIZE                                        12
#define TAR_HEADER_MTIME_SIZE                                       12
#define TAR_HEADER_CHKSUM_SIZE                                      8
#define TAR_HEADER_VERSION_SIZE                                     2
#define TAR_HEADER_UNAME_SIZE                                       32
#define TAR_HEADER_GNAME_SIZE                                       32
#define TAR_HEADER_DEVMAJOR_SIZE                                    8
#define TAR_HEADER_DEVMINOR_SIZE                                    8

#define TAR_HEADER_TYPEFLAG_FILE                                    '0'

#define TAR_HEADER_MAGIC                                            "ustar"
#define TAR_HEADER_VERSION                                          "00"

#define TAR_HEADER_DEVMAJOR                                         0
#define TAR_HEADER_DEVMINOR                                         0

typedef struct TarHeaderData
{
  char name[TAR_HEADER_NAME_SIZE];
  char mode[TAR_HEADER_MODE_SIZE];
  char uid[TAR_HEADER_UID_SIZE];
  char gid[TAR_HEADER_GID_SIZE];
  char size[TAR_HEADER_SIZE_SIZE];
  char mtime[TAR_HEADER_MTIME_SIZE];
  char chksum[TAR_HEADER_CHKSUM_SIZE];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[TAR_HEADER_VERSION_SIZE];
  char uname[TAR_HEADER_UNAME_SIZE];
  char gname[TAR_HEADER_GNAME_SIZE];
  char devmajor[TAR_HEADER_DEVMAJOR_SIZE];
  char devminor[TAR_HEADER_DEVMINOR_SIZE];
  char prefix[155];
  char padding[12];                                                 // Make the struct exactly 512 bytes
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

/***********************************************************************************************************************************
Calculate the tar checksum for a header
***********************************************************************************************************************************/
static unsigned int
tarHdrChecksum(unsigned char *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
    FUNCTION_TEST_END();

    // Initialize checksum with checksum bytes as spaces
	unsigned int result = TAR_HEADER_CHKSUM_SIZE * ' ';

	// Per POSIX, the checksum is the simple sum of all bytes in the header, treating the bytes as unsigned, and treating the
    // checksum field (at offset 148) as though it contained 8 spaces
	for (unsigned int headerIdx = 0; headerIdx < sizeof(TarHeaderData); headerIdx++)
    {
		if (headerIdx < 148 || headerIdx >= 156)
			result += data[headerIdx];
    }

    FUNCTION_TEST_RETURN(result);
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
            if (strSize(tarHdrName(this)) >= TAR_HEADER_NAME_SIZE)
                THROW_FMT(FormatError, "file name '%s' is too long for the tar format", strZ(tarHdrName(this)));

            strncpy(this->data.name, strZ(tarHdrName(this)), TAR_HEADER_NAME_SIZE - 1);

            // Size
            tarHdrWriteU64(this->data.size, TAR_HEADER_SIZE_SIZE, param.size);

            // Time modified
            tarHdrWriteU64(this->data.mtime, TAR_HEADER_MTIME_SIZE, (uint64_t)param.timeModified);

            // Mode (do not include the file type bits, e.g. S_IFMT)
            tarHdrWriteU64(this->data.mode, TAR_HEADER_MODE_SIZE, param.mode & 07777);

            // User and group
            tarHdrWriteU64(this->data.uid, TAR_HEADER_UID_SIZE, param.userId);
            tarHdrWriteU64(this->data.gid, TAR_HEADER_GID_SIZE, param.groupId);

            if (param.user != NULL)
            {
                if (strSize(param.user) >= TAR_HEADER_UNAME_SIZE)
                    THROW_FMT(FormatError, "user '%s' is too long for the tar format", strZ(param.user));

                strncpy(this->data.uname, strZ(param.user), TAR_HEADER_UNAME_SIZE - 1);
            }

            if (param.group != NULL)
            {
                if (strSize(param.group) >= TAR_HEADER_GNAME_SIZE)
                    THROW_FMT(FormatError, "group '%s' is too long for the tar format", strZ(param.group));

                strncpy(this->data.gname, strZ(param.group), TAR_HEADER_GNAME_SIZE - 1);
            }

            (void)tarHdrReadU64; // !!!

            // File type
            this->data.typeflag = TAR_HEADER_TYPEFLAG_FILE;

            // Magic and version
            strcpy(this->data.magic, TAR_HEADER_MAGIC);
            memcpy(this->data.version, TAR_HEADER_VERSION, TAR_HEADER_VERSION_SIZE);

            // Major/minor device
            tarHdrWriteU64(this->data.devmajor, TAR_HEADER_DEVMAJOR_SIZE, TAR_HEADER_DEVMAJOR);
            tarHdrWriteU64(this->data.devminor, TAR_HEADER_DEVMINOR_SIZE, TAR_HEADER_DEVMINOR);

            // Add checksum
            tarHdrWriteU64(this->data.chksum, TAR_HEADER_CHKSUM_SIZE, tarHdrChecksum((unsigned char *)&this->data));
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
void
tarHdrWrite(const TarHeader *const this, IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TAR_HEADER, this);
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    ioWrite(write, BUF(&this->data, sizeof(TarHeaderData)));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
tarHdrWritePadding(const TarHeader *const this, IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TAR_HEADER, this);
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    if (this->pub.size % sizeof(TarHeaderData) != 0)
    {
        unsigned char padding[sizeof(TarHeaderData)] = {0};

        ioWrite(write, BUF(padding, sizeof(TarHeaderData) - (size_t)(this->pub.size % sizeof(TarHeaderData))));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
tarWritePadding(IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    unsigned char padding[sizeof(TarHeaderData) * 2] = {0};

    ioWrite(write, BUF(padding, sizeof(padding)));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
tarHdrToLog(const TarHeader *this)
{
    return strNewFmt("{name: %s, size: %" PRIu64 "}", strZ(tarHdrName(this)), tarHdrSize(this));
}
