/***********************************************************************************************************************************
libssh2 Test Harness

Scripted testing for libssh2 so exact results can be returned for unit testing. See sftp unit tests for usage examples.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_LIBSSH2_H
#define TEST_COMMON_HARNESS_LIBSSH2_H

#ifdef HAVE_LIBSSH2

#ifndef HARNESS_LIBSSH2_REAL

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/macro.h"
#include "common/time.h"
#include "version.h"

/***********************************************************************************************************************************
libssh2 authorization constants
***********************************************************************************************************************************/
#define KEYPRIV                                                     STRDEF("/home/" TEST_USER "/.ssh/id_rsa")
#define KEYPUB                                                      STRDEF("/home/" TEST_USER "/.ssh/id_rsa.pub")
#define KEYPRIV_CSTR                                                "/home/" TEST_USER "/.ssh/id_rsa"
#define KEYPUB_CSTR                                                 "/home/" TEST_USER "/.ssh/id_rsa.pub"
#define KNOWNHOSTS_FILE_CSTR                                        "/home/" TEST_USER "/.ssh/known_hosts"
#define KNOWNHOSTS2_FILE_CSTR                                       "/home/" TEST_USER "/.ssh/known_hosts2"
#define ETC_KNOWNHOSTS_FILE_CSTR                                    "/etc/ssh/ssh_known_hosts"
#define ETC_KNOWNHOSTS2_FILE_CSTR                                   "/etc/ssh/ssh_known_hosts2"
#define HOSTKEY                                                     "12345678901234567890"

// Sentinel .uid/.gid for scripted stat responses selecting root (0). An omitted (0) .uid/.gid instead defaults to the test
// user/group, so use HRN_LIBSSH2_OWNER_ROOT when a response must report ownership by root.
#define HRN_LIBSSH2_OWNER_ROOT                                      UINT_MAX

/***********************************************************************************************************************************
Function constants
***********************************************************************************************************************************/
#define HRNLIBSSH2_HOSTKEY_HASH                                     "libssh2_hostkey_hash"
#define HRNLIBSSH2_INIT                                             "libssh2_init"
#define HRNLIBSSH2_KNOWNHOST_ADDC                                   "libssh2_knownhost_addc"
#define HRNLIBSSH2_KNOWNHOST_CHECKP                                 "libssh2_knownhost_checkp"
#define HRNLIBSSH2_KNOWNHOST_FREE                                   "libssh2_knownhost_free"
#define HRNLIBSSH2_KNOWNHOST_INIT                                   "libssh2_knownhost_init"
#define HRNLIBSSH2_KNOWNHOST_READFILE                               "libssh2_knownhost_readfile"
#define HRNLIBSSH2_KNOWNHOST_WRITEFILE                              "libssh2_knownhost_writefile"
#define HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS                         "libssh2_session_block_directions"
#define HRNLIBSSH2_SESSION_DISCONNECT_EX                            "libssh2_session_disconnect_ex"
#define HRNLIBSSH2_SESSION_FREE                                     "libssh2_session_free"
#define HRNLIBSSH2_SESSION_HANDSHAKE                                "libssh2_session_handshake"
#define HRNLIBSSH2_SESSION_HOSTKEY                                  "libssh2_session_hostkey"
#define HRNLIBSSH2_SESSION_INIT_EX                                  "libssh2_session_init_ex"
#define HRNLIBSSH2_SESSION_LAST_ERRNO                               "libssh2_session_last_errno"
#define HRNLIBSSH2_SESSION_LAST_ERROR                               "libssh2_session_last_error"
#define HRNLIBSSH2_SFTP_CLOSE_HANDLE                                "libssh2_sftp_close_handle"
#define HRNLIBSSH2_SFTP_FSYNC                                       "libssh2_sftp_fsync"
#define HRNLIBSSH2_SFTP_INIT                                        "libssh2_sftp_init"
#define HRNLIBSSH2_SFTP_LAST_ERROR                                  "libssh2_sftp_last_error"
#define HRNLIBSSH2_SFTP_MKDIR_EX                                    "libssh2_sftp_mkdir_ex"
#define HRNLIBSSH2_SFTP_OPEN_EX                                     "libssh2_sftp_open_ex"
#define HRNLIBSSH2_SFTP_READ                                        "libssh2_sftp_read"
#define HRNLIBSSH2_SFTP_READDIR_EX                                  "libssh2_sftp_readdir_ex"
#define HRNLIBSSH2_SFTP_RENAME_EX                                   "libssh2_sftp_rename_ex"
#define HRNLIBSSH2_SFTP_RMDIR_EX                                    "libssh2_sftp_rmdir_ex"
#define HRNLIBSSH2_SFTP_SEEK64                                      "libssh2_sftp_seek64"
#define HRNLIBSSH2_SFTP_SHUTDOWN                                    "libssh2_sftp_shutdown"
#define HRNLIBSSH2_SFTP_STAT_EX                                     "libssh2_sftp_stat_ex"
#define HRNLIBSSH2_SFTP_SYMLINK_EX                                  "libssh2_sftp_symlink_ex"
#define HRNLIBSSH2_SFTP_UNLINK_EX                                   "libssh2_sftp_unlink_ex"
#define HRNLIBSSH2_SFTP_WRITE                                       "libssh2_sftp_write"
#define HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX                   "libssh2_userauth_publickey_fromfile_ex"

/***********************************************************************************************************************************
Structure for scripting libssh2 responses
***********************************************************************************************************************************/
typedef struct HrnLibSsh2
{
    unsigned int session;                                           // Session number when multiple sessions are run concurrently
    const char *function;                                           // Function call expected
    const char *param;                                              // Params expected by the function for verification
    int resultInt;                                                  // Int result value
    uint64_t resultUInt;                                            // UInt result value
    const char *resultZ;                                            // Zero-terminated result value
    bool resultNull;                                                // Return null from function that normally returns a struct ptr
    uint64_t flags;                                                 // libssh2 flags
    uint64_t attr;                                                  // libssh2 attributes (file type and mode bits)
    uint64_t mode;                                                  // libssh2_sftp_open_ex file mode (write open defaults to 0640)
    bool follow;                                                    // stat_ex follows symlinks (LIBSSH2_SFTP_STAT vs LSTAT)
    uint64_t atime, mtime;                                          // libssh2 timestamps
    uint64_t uid, gid;                                              // libssh2 uid/gid
    uint64_t filesize;                                              // libssh2 filesize
    uint64_t offset;                                                // libssh2 seek offset
    const String *target;                                           // libssh2_sftp_symlink_ex target
    const String *fileName;                                         // libssh2_readdir* libssh2_stat* filename
    const String *readBuffer;                                       // what to copy into read buffer
    TimeMSec sleep;                                                 // Sleep specified milliseconds before returning from function
    size_t len;                                                     // libssh2_session_hostkey len
    int type;                                                       // libssh2_session_hostkey type
    const char *errMsg;                                             // libssh2_session_last_error error msg
} HrnLibSsh2;

/***********************************************************************************************************************************
Older systems do not support LIBSSH2_HOSTKEY_HASH_SHA256
***********************************************************************************************************************************/
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
#define HOSTKEY_HASH_ENTRY()                                        HRN_LIBSSH2_HOSTKEY_HASH(3, .resultZ = "12345678910123456789")
#else
#define HOSTKEY_HASH_ENTRY()                                        HRN_LIBSSH2_HOSTKEY_HASH(2, .resultZ = "12345678910123456789")
#endif

/***********************************************************************************************************************************
Scripted response macros, one per libssh2 shim function

Each expands to an HrnLibSsh2 script entry that names the function, bakes its fixed parameters, and defaults the common result, so a
test supplies only the values that vary as trailing designated initializers (e.g. .resultInt, .resultNull). The associated helper
macros build the values those entries take (.attr, .flags, ...).
***********************************************************************************************************************************/
// Sentinel .flags reporting a path that exists but carries no attributes (the harness maps it to libssh2 flags 0). Uses a value
// that cannot collide with a real LIBSSH2_SFTP_ATTR_* combination.
#define HRN_LIBSSH2_ATTR_EXISTENCE                                  UINT64_MAX

// .attr shorthand: file type OR'd with an octal mode (the LIBSSH2_SFTP_S_I* permission bits are the octal mode bits)
#define HRN_LIBSSH2_DIR(mode)                                       (LIBSSH2_SFTP_S_IFDIR | (mode))
#define HRN_LIBSSH2_FILE(mode)                                      (LIBSSH2_SFTP_S_IFREG | (mode))
#define HRN_LIBSSH2_LINK(mode)                                      (LIBSSH2_SFTP_S_IFLNK | (mode))
#define HRN_LIBSSH2_FIFO(mode)                                      (LIBSSH2_SFTP_S_IFIFO | (mode))

// libssh2_sftp_readdir_ex() response returning a directory entry (an empty name signals end of directory)
#define HRN_LIBSSH2_READDIR(name, ...)                                                                                             \
    {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[4095,null,0]", .fileName = STRDEF(name), __VA_ARGS__}

// libssh2_sftp_stat_ex() response. The path is the full server path (e.g. TEST_PATH "/sub"). Attributes are trailing arguments
// (.attr, .flags, .mtime, .uid, .gid, .filesize, ...); the harness defaults omitted ones: .attr (0) to a regular file (set it with
// the HRN_LIBSSH2_DIR/FILE/LINK/FIFO() helpers), .uid/.gid (0) to the test user/group (HRN_LIBSSH2_OWNER_ROOT for root), and
// .flags (0) to the standard attribute set (permissions, times, owner) plus a size for a regular file. Reports success
// (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure. Set .follow = true to require LIBSSH2_SFTP_STAT.
#define HRN_LIBSSH2_STAT(path, ...)                                                                                                \
    {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" path "\"]", __VA_ARGS__}

// libssh2_sftp_close_handle() response. Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_CLOSE(...)                                      {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, __VA_ARGS__}

// libssh2_sftp_fsync() response. Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_FSYNC(...)                                      {.function = HRNLIBSSH2_SFTP_FSYNC, __VA_ARGS__}

// libssh2_session_block_directions() response. Reports no blocked direction (SSH2_NO_BLOCK_READING_WRITING) by default; set
// .resultInt to an SSH2_BLOCK_* direction to make the production code wait.
#define HRN_LIBSSH2_BLOCK(...)                                      {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, __VA_ARGS__}

// libssh2_session_last_errno() response returning the given libssh2 error
#define HRN_LIBSSH2_ERRNO(error, ...)                                                                                              \
    {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = (error), __VA_ARGS__}

// libssh2_sftp_last_error() response returning the given SFTP status (LIBSSH2_FX_*)
#define HRN_LIBSSH2_SFTP_ERROR(code, ...)                                                                                          \
    {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = (code), __VA_ARGS__}

// libssh2_session_last_error() response returning the given libssh2 error and, via .errMsg, the message string
#define HRN_LIBSSH2_SESSION_ERROR(error, ...)                                                                                      \
    {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .resultInt = (error), __VA_ARGS__}

// libssh2_init() response (always called with flags 0). Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a
// failure.
#define HRN_LIBSSH2_INIT(...)                                                                                                      \
    {.function = HRNLIBSSH2_INIT, .param = "[0]", __VA_ARGS__}

// libssh2_session_init_ex() response (the production code always passes NULL allocators). Returns a session by default; set
// .resultNull = true for a failure.
#define HRN_LIBSSH2_SESSION_INIT(...)                                                                                              \
    {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]", __VA_ARGS__}

// libssh2_session_handshake() response. Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_HANDSHAKE(...)                                                                                                 \
    {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, __VA_ARGS__}

// libssh2_session_hostkey() response. The harness defaults the host key length (20), type (LIBSSH2_HOSTKEY_TYPE_RSA), and
// value (HOSTKEY); set .type for another key type or .resultNull = true for a failure.
#define HRN_LIBSSH2_HOSTKEY(...)                                                                                                   \
    {.function = HRNLIBSSH2_SESSION_HOSTKEY, __VA_ARGS__}

// libssh2_hostkey_hash() response for the given hash type. Set .resultZ to the hash, or .resultNull = true for a failure.
#define HRN_LIBSSH2_HOSTKEY_HASH(hashType, ...)                                                                                    \
    {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[" #hashType "]", __VA_ARGS__}

// libssh2_knownhost_init() response. Returns a known-hosts collection by default; set .resultNull = true for a failure.
#define HRN_LIBSSH2_KNOWNHOST_INIT(...)                                                                                            \
    {.function = HRNLIBSSH2_KNOWNHOST_INIT, __VA_ARGS__}

// libssh2_knownhost_readfile() response for the given file (LIBSSH2_KNOWNHOST_FILE_OPENSSH). Reports 0 hosts read by default; set
// .resultInt to the host count or a failure.
#define HRN_LIBSSH2_KNOWNHOST_READFILE(file, ...)                                                                                  \
    {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" file "\",1]", __VA_ARGS__}

// libssh2_knownhost_writefile() response for the given file (LIBSSH2_KNOWNHOST_FILE_OPENSSH). Reports success (LIBSSH2_ERROR_NONE)
// by default; set .resultInt for a failure.
#define HRN_LIBSSH2_KNOWNHOST_WRITEFILE(file, ...)                                                                                 \
    {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" file "\",1]", __VA_ARGS__}

// libssh2_knownhost_checkp() response for localhost returning the given LIBSSH2_KNOWNHOST_CHECK_* result
#define HRN_LIBSSH2_KNOWNHOST_CHECK(result)                                                                                        \
    {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]", .resultInt = (result)}

// libssh2_knownhost_addc() response adding localhost with the given type mask. Reports success by default; set .resultInt for a
// failure.
#define HRN_LIBSSH2_KNOWNHOST_ADD(typeMask, ...)                                                                                   \
    {.function = HRNLIBSSH2_KNOWNHOST_ADDC,                                                                                        \
     .param = "[\"localhost\",null,\"" HOSTKEY "\",20,\"Generated from pgBackRest\",25," #typeMask "]", __VA_ARGS__}

// libssh2_userauth_publickey_fromfile_ex() response for the standard public-key login (TEST_USER with KEYPUB_CSTR / KEYPRIV_CSTR
// and no passphrase). Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure. A login that omits the public
// key or supplies a passphrase is scripted longhand.
#define HRN_LIBSSH2_USERAUTH(...)                                                                                                  \
    {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,                                                                        \
     .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]", __VA_ARGS__}

// libssh2_sftp_init() response. Returns an SFTP session by default; set .resultNull = true for a failure.
#define HRN_LIBSSH2_SFTP_INIT(...)                                                                                                 \
    {.function = HRNLIBSSH2_SFTP_INIT, __VA_ARGS__}

// libssh2_sftp_shutdown() response. Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_SHUTDOWN(...)                                   {.function = HRNLIBSSH2_SFTP_SHUTDOWN, __VA_ARGS__}

// libssh2_session_disconnect_ex() response (the production code always passes the standard shutdown reason). Reports success
// (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_DISCONNECT(...)                                                                                                \
    {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]", __VA_ARGS__}

// libssh2_session_free() response. Reports success (LIBSSH2_ERROR_NONE) by default; set .resultInt for a failure.
#define HRN_LIBSSH2_SESSION_FREE(...)                               {.function = HRNLIBSSH2_SESSION_FREE, __VA_ARGS__}

// libssh2_sftp_seek64() expectation verifying the production code seeks to the given byte offset (the function returns void)
#define HRN_LIBSSH2_SEEK(offset, ...)                                                                                              \
    {.function = HRNLIBSSH2_SFTP_SEEK64, .param = "[" #offset "]", __VA_ARGS__}

// libssh2_sftp_read() response (size is the production buffer_maxlen). Set .readBuffer to the bytes returned; the result
// (bytes read) defaults to the read buffer size, so set .resultInt only for a short read or a failure.
#define HRN_LIBSSH2_READ(size, ...)                                                                                                \
    {.function = HRNLIBSSH2_SFTP_READ, .param = "[" #size "]", __VA_ARGS__}

// libssh2_sftp_write() response (size is the production byte size). The result (bytes written) defaults to the full size, so set
// .resultInt only for a short write or a failure.
#define HRN_LIBSSH2_WRITE(size, ...)                                                                                              \
    {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[" #size "]", __VA_ARGS__}

// libssh2_sftp_open_ex() responses, one per production open path (param is ["path",flags,open_type]): a directory (flags 0,
// LIBSSH2_SFTP_OPENDIR), a file for reading (LIBSSH2_FXF_READ, LIBSSH2_SFTP_OPENFILE), or a file for writing
// (LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE | LIBSSH2_FXF_TRUNC = 26, LIBSSH2_SFTP_OPENFILE). The path is the full server path (e.g.
// TEST_PATH "/sub"). The harness verifies the open mode separately; a write open defaults to 0640, so set .mode only when it
// differs. Pass .resultNull = true (optionally with .resultInt) for a failed open.
#define HRN_LIBSSH2_OPENDIR(path, ...)                                                                                             \
    {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" path "\",0,1]", __VA_ARGS__}
#define HRN_LIBSSH2_OPEN_READ(path, ...)                                                                                           \
    {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" path "\",1,0]", __VA_ARGS__}
#define HRN_LIBSSH2_OPEN_WRITE(path, ...)                                                                                          \
    {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" path "\",26,0]", __VA_ARGS__}

// libssh2_sftp_symlink_ex() response reading a link destination (param is ["path","",LIBSSH2_SFTP_READLINK]; the empty target is
// the production output buffer). The path is the full server path (e.g. TEST_PATH "/sub"). Reports success by default; set .target
// to the destination, or .resultInt for a failure.
#define HRN_LIBSSH2_READLINK(path, ...)                                                                                            \
    {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" path "\",\"\",1]", __VA_ARGS__}

// libssh2_sftp_unlink_ex() response. The path is the full server path (e.g. TEST_PATH "/sub"). Reports success (LIBSSH2_ERROR_NONE)
// by default; set .resultInt for a failure.
#define HRN_LIBSSH2_UNLINK(path, ...)                                                                                              \
    {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" path "\"]", __VA_ARGS__}

// libssh2_sftp_mkdir_ex() response. The path is the full server path (e.g. TEST_PATH "/sub"). The harness verifies the mode
// separately and defaults an omitted .mode to 0750. Reports success by default; set .resultInt for a failure.
#define HRN_LIBSSH2_MKDIR(path, ...)                                                                                               \
    {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" path "\"]", __VA_ARGS__}

// libssh2_sftp_rmdir_ex() response. The path is the full server path (e.g. TEST_PATH "/sub"). Reports success by default; set
// .resultInt for a failure.
#define HRN_LIBSSH2_RMDIR(path, ...)                                                                                              \
    {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" path "\"]", __VA_ARGS__}

// libssh2_sftp_rename_ex() response (flags 7 = LIBSSH2_SFTP_RENAME_OVERWRITE | _ATOMIC | _NATIVE, as the production code always
// passes). Both paths are full server paths (e.g. TEST_PATH "/sub"). Reports success by default; set .resultInt for a failure.
#define HRN_LIBSSH2_RENAME(source, dest, ...)                                                                                      \
    {.function = HRNLIBSSH2_SFTP_RENAME_EX, .param = "[\"" source "\",\"" dest "\",7]", __VA_ARGS__}

/***********************************************************************************************************************************
Macros for defining groups of functions that implement commands
***********************************************************************************************************************************/
// Set of functions mimicking libssh2 initialization and authorization
#define HRNLIBSSH2_MACRO_STARTUP()                                                                                                 \
    HRN_LIBSSH2_INIT(),                                                                                                            \
    HRN_LIBSSH2_SESSION_INIT(),                                                                                                    \
    HRN_LIBSSH2_HANDSHAKE(),                                                                                                       \
    HRN_LIBSSH2_KNOWNHOST_INIT(),                                                                                                  \
    HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),                                                          \
    HRN_LIBSSH2_HOSTKEY(),                                                                                                         \
    HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),                                                                    \
    HRN_LIBSSH2_USERAUTH(),                                                                                                        \
    HRN_LIBSSH2_SFTP_INIT()

// Set of functions mimicking libssh2 shutdown and disconnect
#define HRNLIBSSH2_MACRO_SHUTDOWN()                                                                                                \
    HRN_LIBSSH2_SHUTDOWN(),                                                                                                        \
    HRN_LIBSSH2_DISCONNECT(),                                                                                                      \
    HRN_LIBSSH2_SESSION_FREE()

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Set a libssh2 script. The prior script must have completed. The script length is computed, so no terminator entry is needed.
#define HRN_LIBSSH2_SCRIPT_SET(...)                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnLibSsh2 script[] = {__VA_ARGS__};                                                                                 \
        hrnLibSsh2ScriptSet(script, LENGTH_OF(script));                                                                           \
    }                                                                                                                              \
    while (0)

void hrnLibSsh2ScriptSet(const HrnLibSsh2 *script, unsigned int scriptSize);

#endif // HARNESS_LIBSSH2_REAL

#endif // HAVE_LIBSSH2

#endif // TEST_COMMON_HARNESS_LIBSSH2_H
