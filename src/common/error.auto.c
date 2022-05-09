/***********************************************************************************************************************************
Error Type Definition

Automatically generated by 'make build-error' -- do not modify directly.
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Error type definitions
***********************************************************************************************************************************/
ERROR_DEFINE( 25, AssertError, true, RuntimeError);
ERROR_DEFINE( 26, ChecksumError, false, RuntimeError);
ERROR_DEFINE( 27, ConfigError, false, RuntimeError);
ERROR_DEFINE( 28, FileInvalidError, false, RuntimeError);
ERROR_DEFINE( 29, FormatError, false, RuntimeError);
ERROR_DEFINE( 30, CommandRequiredError, false, RuntimeError);
ERROR_DEFINE( 31, OptionInvalidError, false, RuntimeError);
ERROR_DEFINE( 32, OptionInvalidValueError, false, RuntimeError);
ERROR_DEFINE( 33, OptionInvalidRangeError, false, RuntimeError);
ERROR_DEFINE( 34, OptionInvalidPairError, false, RuntimeError);
ERROR_DEFINE( 35, OptionDuplicateKeyError, false, RuntimeError);
ERROR_DEFINE( 36, OptionNegateError, false, RuntimeError);
ERROR_DEFINE( 37, OptionRequiredError, false, RuntimeError);
ERROR_DEFINE( 38, PgRunningError, false, RuntimeError);
ERROR_DEFINE( 39, ProtocolError, false, RuntimeError);
ERROR_DEFINE( 40, PathNotEmptyError, false, RuntimeError);
ERROR_DEFINE( 41, FileOpenError, false, RuntimeError);
ERROR_DEFINE( 42, FileReadError, false, RuntimeError);
ERROR_DEFINE( 43, ParamRequiredError, false, RuntimeError);
ERROR_DEFINE( 44, ArchiveMismatchError, false, RuntimeError);
ERROR_DEFINE( 45, ArchiveDuplicateError, false, RuntimeError);
ERROR_DEFINE( 46, VersionNotSupportedError, false, RuntimeError);
ERROR_DEFINE( 47, PathCreateError, false, RuntimeError);
ERROR_DEFINE( 48, CommandInvalidError, false, RuntimeError);
ERROR_DEFINE( 49, HostConnectError, false, RuntimeError);
ERROR_DEFINE( 50, LockAcquireError, false, RuntimeError);
ERROR_DEFINE( 51, BackupMismatchError, false, RuntimeError);
ERROR_DEFINE( 52, FileSyncError, false, RuntimeError);
ERROR_DEFINE( 53, PathOpenError, false, RuntimeError);
ERROR_DEFINE( 54, PathSyncError, false, RuntimeError);
ERROR_DEFINE( 55, FileMissingError, false, RuntimeError);
ERROR_DEFINE( 56, DbConnectError, false, RuntimeError);
ERROR_DEFINE( 57, DbQueryError, false, RuntimeError);
ERROR_DEFINE( 58, DbMismatchError, false, RuntimeError);
ERROR_DEFINE( 59, DbTimeoutError, false, RuntimeError);
ERROR_DEFINE( 60, FileRemoveError, false, RuntimeError);
ERROR_DEFINE( 61, PathRemoveError, false, RuntimeError);
ERROR_DEFINE( 62, StopError, false, RuntimeError);
ERROR_DEFINE( 63, TermError, false, RuntimeError);
ERROR_DEFINE( 64, FileWriteError, false, RuntimeError);
ERROR_DEFINE( 66, ProtocolTimeoutError, false, RuntimeError);
ERROR_DEFINE( 67, FeatureNotSupportedError, false, RuntimeError);
ERROR_DEFINE( 68, ArchiveCommandInvalidError, false, RuntimeError);
ERROR_DEFINE( 69, LinkExpectedError, false, RuntimeError);
ERROR_DEFINE( 70, LinkDestinationError, false, RuntimeError);
ERROR_DEFINE( 72, HostInvalidError, false, RuntimeError);
ERROR_DEFINE( 73, PathMissingError, false, RuntimeError);
ERROR_DEFINE( 74, FileMoveError, false, RuntimeError);
ERROR_DEFINE( 75, BackupSetInvalidError, false, RuntimeError);
ERROR_DEFINE( 76, TablespaceMapError, false, RuntimeError);
ERROR_DEFINE( 77, PathTypeError, false, RuntimeError);
ERROR_DEFINE( 78, LinkMapError, false, RuntimeError);
ERROR_DEFINE( 79, FileCloseError, false, RuntimeError);
ERROR_DEFINE( 80, DbMissingError, false, RuntimeError);
ERROR_DEFINE( 81, DbInvalidError, false, RuntimeError);
ERROR_DEFINE( 82, ArchiveTimeoutError, false, RuntimeError);
ERROR_DEFINE( 83, FileModeError, false, RuntimeError);
ERROR_DEFINE( 84, OptionMultipleValueError, false, RuntimeError);
ERROR_DEFINE( 85, ProtocolOutputRequiredError, false, RuntimeError);
ERROR_DEFINE( 86, LinkOpenError, false, RuntimeError);
ERROR_DEFINE( 87, ArchiveDisabledError, false, RuntimeError);
ERROR_DEFINE( 88, FileOwnerError, false, RuntimeError);
ERROR_DEFINE( 89, UserMissingError, false, RuntimeError);
ERROR_DEFINE( 90, OptionCommandError, false, RuntimeError);
ERROR_DEFINE( 91, GroupMissingError, false, RuntimeError);
ERROR_DEFINE( 92, PathExistsError, false, RuntimeError);
ERROR_DEFINE( 93, FileExistsError, false, RuntimeError);
ERROR_DEFINE( 94, MemoryError, true, RuntimeError);
ERROR_DEFINE( 95, CryptoError, false, RuntimeError);
ERROR_DEFINE( 96, ParamInvalidError, false, RuntimeError);
ERROR_DEFINE( 97, PathCloseError, false, RuntimeError);
ERROR_DEFINE( 98, FileInfoError, false, RuntimeError);
ERROR_DEFINE( 99, JsonFormatError, false, RuntimeError);
ERROR_DEFINE(100, KernelError, false, RuntimeError);
ERROR_DEFINE(101, ServiceError, false, RuntimeError);
ERROR_DEFINE(102, ExecuteError, false, RuntimeError);
ERROR_DEFINE(103, RepoInvalidError, false, RuntimeError);
ERROR_DEFINE(104, CommandError, false, RuntimeError);
ERROR_DEFINE(105, AccessError, false, RuntimeError);
ERROR_DEFINE(106, ClockError, false, RuntimeError);
ERROR_DEFINE(122, RuntimeError, false, RuntimeError);
ERROR_DEFINE(123, InvalidError, false, RuntimeError);
ERROR_DEFINE(124, UnhandledError, false, RuntimeError);
ERROR_DEFINE(125, UnknownError, false, RuntimeError);

/***********************************************************************************************************************************
Error type array
***********************************************************************************************************************************/
static const ErrorType *errorTypeList[] =
{
    &AssertError,
    &ChecksumError,
    &ConfigError,
    &FileInvalidError,
    &FormatError,
    &CommandRequiredError,
    &OptionInvalidError,
    &OptionInvalidValueError,
    &OptionInvalidRangeError,
    &OptionInvalidPairError,
    &OptionDuplicateKeyError,
    &OptionNegateError,
    &OptionRequiredError,
    &PgRunningError,
    &ProtocolError,
    &PathNotEmptyError,
    &FileOpenError,
    &FileReadError,
    &ParamRequiredError,
    &ArchiveMismatchError,
    &ArchiveDuplicateError,
    &VersionNotSupportedError,
    &PathCreateError,
    &CommandInvalidError,
    &HostConnectError,
    &LockAcquireError,
    &BackupMismatchError,
    &FileSyncError,
    &PathOpenError,
    &PathSyncError,
    &FileMissingError,
    &DbConnectError,
    &DbQueryError,
    &DbMismatchError,
    &DbTimeoutError,
    &FileRemoveError,
    &PathRemoveError,
    &StopError,
    &TermError,
    &FileWriteError,
    &ProtocolTimeoutError,
    &FeatureNotSupportedError,
    &ArchiveCommandInvalidError,
    &LinkExpectedError,
    &LinkDestinationError,
    &HostInvalidError,
    &PathMissingError,
    &FileMoveError,
    &BackupSetInvalidError,
    &TablespaceMapError,
    &PathTypeError,
    &LinkMapError,
    &FileCloseError,
    &DbMissingError,
    &DbInvalidError,
    &ArchiveTimeoutError,
    &FileModeError,
    &OptionMultipleValueError,
    &ProtocolOutputRequiredError,
    &LinkOpenError,
    &ArchiveDisabledError,
    &FileOwnerError,
    &UserMissingError,
    &OptionCommandError,
    &GroupMissingError,
    &PathExistsError,
    &FileExistsError,
    &MemoryError,
    &CryptoError,
    &ParamInvalidError,
    &PathCloseError,
    &FileInfoError,
    &JsonFormatError,
    &KernelError,
    &ServiceError,
    &ExecuteError,
    &RepoInvalidError,
    &CommandError,
    &AccessError,
    &ClockError,
    &RuntimeError,
    &InvalidError,
    &UnhandledError,
    &UnknownError,
    NULL,
};
