####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package pgBackRestTest::Env::Manifest;
use parent 'pgBackRestDoc::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Time::Local qw(timelocal);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant PATH_BACKUP_HISTORY                                    => 'backup.history';
    push @EXPORT, qw(PATH_BACKUP_HISTORY);
use constant FILE_MANIFEST                                          => 'backup.manifest';
    push @EXPORT, qw(FILE_MANIFEST);
use constant FILE_MANIFEST_COPY                                     => FILE_MANIFEST . INI_COPY_EXT;
    push @EXPORT, qw(FILE_MANIFEST_COPY);

####################################################################################################################################
# Default match factor
####################################################################################################################################
use constant MANIFEST_DEFAULT_MATCH_FACTOR                          => 0.1;
    push @EXPORT, qw(MANIFEST_DEFAULT_MATCH_FACTOR);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant MANIFEST_TARGET_PGDATA                                 => 'pg_data';
    push @EXPORT, qw(MANIFEST_TARGET_PGDATA);
use constant MANIFEST_TARGET_PGTBLSPC                               => 'pg_tblspc';
    push @EXPORT, qw(MANIFEST_TARGET_PGTBLSPC);

use constant MANIFEST_VALUE_PATH                                    => 'path';
    push @EXPORT, qw(MANIFEST_VALUE_PATH);
use constant MANIFEST_VALUE_LINK                                    => 'link';
    push @EXPORT, qw(MANIFEST_VALUE_LINK);

# Manifest sections
use constant MANIFEST_SECTION_BACKUP                                => 'backup';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP);
use constant MANIFEST_SECTION_BACKUP_DB                             => 'backup:db';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_DB);
use constant MANIFEST_SECTION_BACKUP_INFO                           => 'backup:info';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_INFO);
use constant MANIFEST_SECTION_BACKUP_OPTION                         => 'backup:option';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_OPTION);
use constant MANIFEST_SECTION_BACKUP_TARGET                         => 'backup:target';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_TARGET);
use constant MANIFEST_SECTION_DB                                    => 'db';
    push @EXPORT, qw(MANIFEST_SECTION_DB);
use constant MANIFEST_SECTION_TARGET_PATH                           => 'target:path';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_PATH);
use constant MANIFEST_SECTION_TARGET_FILE                           => 'target:file';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_FILE);
use constant MANIFEST_SECTION_TARGET_LINK                           => 'target:link';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_LINK);

# Backup metadata required for restores
use constant MANIFEST_KEY_ARCHIVE_START                             => 'backup-archive-start';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_START);
use constant MANIFEST_KEY_ARCHIVE_STOP                              => 'backup-archive-stop';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_STOP);
use constant MANIFEST_KEY_LABEL                                     => 'backup-label';
    push @EXPORT, qw(MANIFEST_KEY_LABEL);
use constant MANIFEST_KEY_LSN_START                                 => 'backup-lsn-start';
    push @EXPORT, qw(MANIFEST_KEY_LSN_START);
use constant MANIFEST_KEY_LSN_STOP                                  => 'backup-lsn-stop';
    push @EXPORT, qw(MANIFEST_KEY_LSN_STOP);
use constant MANIFEST_KEY_PRIOR                                     => 'backup-prior';
    push @EXPORT, qw(MANIFEST_KEY_PRIOR);
use constant MANIFEST_KEY_TIMESTAMP_COPY_START                      => 'backup-timestamp-copy-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_COPY_START);
use constant MANIFEST_KEY_TIMESTAMP_START                           => 'backup-timestamp-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_START);
use constant MANIFEST_KEY_TIMESTAMP_STOP                            => 'backup-timestamp-stop';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_STOP);
use constant MANIFEST_KEY_TYPE                                      => 'backup-type';
    push @EXPORT, qw(MANIFEST_KEY_TYPE);

# Options that were set when the backup was made
use constant MANIFEST_KEY_BACKUP_STANDBY                            => 'option-backup-standby';
    push @EXPORT, qw(MANIFEST_KEY_BACKUP_STANDBY);
use constant MANIFEST_KEY_HARDLINK                                  => 'option-hardlink';
    push @EXPORT, qw(MANIFEST_KEY_HARDLINK);
use constant MANIFEST_KEY_ARCHIVE_CHECK                             => 'option-archive-check';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_CHECK);
use constant MANIFEST_KEY_ARCHIVE_COPY                              => 'option-archive-copy';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_COPY);
use constant MANIFEST_KEY_BUFFER_SIZE                               => 'option-buffer-size';
    push @EXPORT, qw(MANIFEST_KEY_BUFFER_SIZE);
use constant MANIFEST_KEY_CHECKSUM_PAGE                             => 'option-checksum-page';
    push @EXPORT, qw(MANIFEST_KEY_CHECKSUM_PAGE);
use constant MANIFEST_KEY_COMPRESS                                  => 'option-compress';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS);
use constant MANIFEST_KEY_COMPRESS_TYPE                             => 'option-compress-type';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS_TYPE);
use constant MANIFEST_KEY_COMPRESS_LEVEL                            => 'option-compress-level';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS_LEVEL);
use constant MANIFEST_KEY_COMPRESS_LEVEL_NETWORK                    => 'option-compress-level-network';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS_LEVEL_NETWORK);
use constant MANIFEST_KEY_ONLINE                                    => 'option-online';
    push @EXPORT, qw(MANIFEST_KEY_ONLINE);
use constant MANIFEST_KEY_DELTA                                     => 'option-delta';
    push @EXPORT, qw(MANIFEST_KEY_DELTA);
use constant MANIFEST_KEY_PROCESS_MAX                               => 'option-process-max';
    push @EXPORT, qw(MANIFEST_KEY_PROCESS_MAX);

# Information about the database that was backed up
use constant MANIFEST_KEY_DB_ID                                     => 'db-id';
    push @EXPORT, qw(MANIFEST_KEY_DB_ID);
use constant MANIFEST_KEY_SYSTEM_ID                                 => 'db-system-id';
    push @EXPORT, qw(MANIFEST_KEY_SYSTEM_ID);
use constant MANIFEST_KEY_CATALOG                                   => 'db-catalog-version';
    push @EXPORT, qw(MANIFEST_KEY_CATALOG);
use constant MANIFEST_KEY_CONTROL                                   => 'db-control-version';
    push @EXPORT, qw(MANIFEST_KEY_CONTROL);
use constant MANIFEST_KEY_DB_LAST_SYSTEM_ID                         => 'db-last-system-id';
    push @EXPORT, qw(MANIFEST_KEY_DB_LAST_SYSTEM_ID);
use constant MANIFEST_KEY_DB_VERSION                                => 'db-version';
    push @EXPORT, qw(MANIFEST_KEY_DB_VERSION);

# Subkeys used for path/file/link info
use constant MANIFEST_SUBKEY_CHECKSUM                               => 'checksum';
    push @EXPORT, qw(MANIFEST_SUBKEY_CHECKSUM);
use constant MANIFEST_SUBKEY_CHECKSUM_PAGE                          => 'checksum-page';
    push @EXPORT, qw(MANIFEST_SUBKEY_CHECKSUM_PAGE);
use constant MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR                    => 'checksum-page-error';
    push @EXPORT, qw(MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR);
use constant MANIFEST_SUBKEY_DESTINATION                            => 'destination';
    push @EXPORT, qw(MANIFEST_SUBKEY_DESTINATION);
use constant MANIFEST_SUBKEY_FILE                                   => 'file';
    push @EXPORT, qw(MANIFEST_SUBKEY_FILE);
use constant MANIFEST_SUBKEY_FUTURE                                 => 'future';
    push @EXPORT, qw(MANIFEST_SUBKEY_FUTURE);
use constant MANIFEST_SUBKEY_GROUP                                  => 'group';
    push @EXPORT, qw(MANIFEST_SUBKEY_GROUP);
use constant MANIFEST_SUBKEY_MASTER                                 => 'master';
    push @EXPORT, qw(MANIFEST_SUBKEY_MASTER);
use constant MANIFEST_SUBKEY_MODE                                   => 'mode';
    push @EXPORT, qw(MANIFEST_SUBKEY_MODE);
use constant MANIFEST_SUBKEY_TIMESTAMP                              => 'timestamp';
    push @EXPORT, qw(MANIFEST_SUBKEY_TIMESTAMP);
use constant MANIFEST_SUBKEY_TYPE                                   => 'type';
    push @EXPORT, qw(MANIFEST_SUBKEY_TYPE);
use constant MANIFEST_SUBKEY_PATH                                   => 'path';
    push @EXPORT, qw(MANIFEST_SUBKEY_PATH);
use constant MANIFEST_SUBKEY_REFERENCE                              => 'reference';
    push @EXPORT, qw(MANIFEST_SUBKEY_REFERENCE);
use constant MANIFEST_SUBKEY_REPO_SIZE                              => 'repo-size';
    push @EXPORT, qw(MANIFEST_SUBKEY_REPO_SIZE);
use constant MANIFEST_SUBKEY_SIZE                                   => 'size';
    push @EXPORT, qw(MANIFEST_SUBKEY_SIZE);
use constant MANIFEST_SUBKEY_TABLESPACE_ID                          => 'tablespace-id';
    push @EXPORT, qw(MANIFEST_SUBKEY_TABLESPACE_ID);
use constant MANIFEST_SUBKEY_TABLESPACE_NAME                        => 'tablespace-name';
    push @EXPORT, qw(MANIFEST_SUBKEY_TABLESPACE_NAME);
use constant MANIFEST_SUBKEY_USER                                   => 'user';
    push @EXPORT, qw(MANIFEST_SUBKEY_USER);

####################################################################################################################################
# Database locations for important files/paths
####################################################################################################################################
use constant DB_PATH_ARCHIVESTATUS                                  => 'archive_status';
    push @EXPORT, qw(DB_PATH_ARCHIVESTATUS);
use constant DB_PATH_BASE                                           => 'base';
    push @EXPORT, qw(DB_PATH_BASE);
use constant DB_PATH_GLOBAL                                         => 'global';
    push @EXPORT, qw(DB_PATH_GLOBAL);
use constant DB_PATH_PGDYNSHMEM                                     => 'pg_dynshmem';
    push @EXPORT, qw(DB_PATH_PGDYNSHMEM);
use constant DB_PATH_PGMULTIXACT                                    => 'pg_multixact';
    push @EXPORT, qw(DB_PATH_PGMULTIXACT);
use constant DB_PATH_PGNOTIFY                                       => 'pg_notify';
    push @EXPORT, qw(DB_PATH_PGNOTIFY);
use constant DB_PATH_PGREPLSLOT                                     => 'pg_replslot';
    push @EXPORT, qw(DB_PATH_PGREPLSLOT);
use constant DB_PATH_PGSERIAL                                       => 'pg_serial';
    push @EXPORT, qw(DB_PATH_PGSERIAL);
use constant DB_PATH_PGSNAPSHOTS                                    => 'pg_snapshots';
    push @EXPORT, qw(DB_PATH_PGSNAPSHOTS);
use constant DB_PATH_PGSTATTMP                                      => 'pg_stat_tmp';
    push @EXPORT, qw(DB_PATH_PGSTATTMP);
use constant DB_PATH_PGSUBTRANS                                     => 'pg_subtrans';
    push @EXPORT, qw(DB_PATH_PGSUBTRANS);
use constant DB_PATH_PGTBLSPC                                       => 'pg_tblspc';
    push @EXPORT, qw(DB_PATH_PGTBLSPC);

use constant DB_FILE_BACKUPLABEL                                    => 'backup_label';
    push @EXPORT, qw(DB_FILE_BACKUPLABEL);
use constant DB_FILE_BACKUPLABELOLD                                 => DB_FILE_BACKUPLABEL . '.old';
    push @EXPORT, qw(DB_FILE_BACKUPLABELOLD);
use constant DB_FILE_PGCONTROL                                      => DB_PATH_GLOBAL . '/pg_control';
    push @EXPORT, qw(DB_FILE_PGCONTROL);
use constant DB_FILE_PGFILENODEMAP                                  => 'pg_filenode.map';
    push @EXPORT, qw(DB_FILE_PGFILENODEMAP);
use constant DB_FILE_PGINTERNALINIT                                 => 'pg_internal.init';
    push @EXPORT, qw(DB_FILE_PGINTERNALINIT);
use constant DB_FILE_PGVERSION                                      => 'PG_VERSION';
    push @EXPORT, qw(DB_FILE_PGVERSION);
use constant DB_FILE_POSTGRESQLAUTOCONFTMP                          => 'postgresql.auto.conf.tmp';
    push @EXPORT, qw(DB_FILE_POSTGRESQLAUTOCONFTMP);
use constant DB_FILE_POSTMASTEROPTS                                 => 'postmaster.opts';
    push @EXPORT, qw(DB_FILE_POSTMASTEROPTS);
use constant DB_FILE_POSTMASTERPID                                  => 'postmaster.pid';
    push @EXPORT, qw(DB_FILE_POSTMASTERPID);
use constant DB_FILE_RECOVERYCONF                                   => 'recovery.conf';
    push @EXPORT, qw(DB_FILE_RECOVERYCONF);
use constant DB_FILE_RECOVERYSIGNAL                                 => 'recovery.signal';
    push @EXPORT, qw(DB_FILE_RECOVERYSIGNAL);
use constant DB_FILE_RECOVERYDONE                                   => 'recovery.done';
    push @EXPORT, qw(DB_FILE_RECOVERYDONE);
use constant DB_FILE_STANDBYSIGNAL                                  => 'standby.signal';
    push @EXPORT, qw(DB_FILE_STANDBYSIGNAL);
use constant DB_FILE_TABLESPACEMAP                                  => 'tablespace_map';
    push @EXPORT, qw(DB_FILE_TABLESPACEMAP);

use constant DB_FILE_PREFIX_TMP                                     => 'pgsql_tmp';
    push @EXPORT, qw(DB_FILE_PREFIX_TMP);

####################################################################################################################################
# Manifest locations for important files/paths
####################################################################################################################################
use constant MANIFEST_PATH_BASE                                     => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE;
    push @EXPORT, qw(MANIFEST_PATH_BASE);
use constant MANIFEST_PATH_GLOBAL                                   => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL;
    push @EXPORT, qw(MANIFEST_PATH_GLOBAL);
use constant MANIFEST_PATH_PGDYNSHMEM                               => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGDYNSHMEM;
    push @EXPORT, qw(MANIFEST_PATH_PGDYNSHMEM);
use constant MANIFEST_PATH_PGMULTIXACT                              => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGMULTIXACT;
    push @EXPORT, qw(MANIFEST_PATH_PGMULTIXACT);
use constant MANIFEST_PATH_PGNOTIFY                                 => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGNOTIFY;
    push @EXPORT, qw(MANIFEST_PATH_PGNOTIFY);
use constant MANIFEST_PATH_PGREPLSLOT                               => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGREPLSLOT;
    push @EXPORT, qw(MANIFEST_PATH_PGREPLSLOT);
use constant MANIFEST_PATH_PGSERIAL                                 => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGSERIAL;
    push @EXPORT, qw(MANIFEST_PATH_PGSERIAL);
use constant MANIFEST_PATH_PGSNAPSHOTS                              => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGSNAPSHOTS;
    push @EXPORT, qw(MANIFEST_PATH_PGSNAPSHOTS);
use constant MANIFEST_PATH_PGSTATTMP                                => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGSTATTMP;
    push @EXPORT, qw(MANIFEST_PATH_PGSTATTMP);
use constant MANIFEST_PATH_PGSUBTRANS                               => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGSUBTRANS;
    push @EXPORT, qw(MANIFEST_PATH_PGSUBTRANS);
use constant MANIFEST_PATH_PGTBLSPC                                 => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC;
    push @EXPORT, qw(MANIFEST_PATH_PGTBLSPC);

use constant MANIFEST_FILE_BACKUPLABEL                              => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_BACKUPLABEL;
    push @EXPORT, qw(MANIFEST_FILE_BACKUPLABEL);
use constant MANIFEST_FILE_BACKUPLABELOLD                           => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_BACKUPLABELOLD;
    push @EXPORT, qw(MANIFEST_FILE_BACKUPLABELOLD);
use constant MANIFEST_FILE_PGCONTROL                                => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL;
    push @EXPORT, qw(MANIFEST_FILE_PGCONTROL);
use constant MANIFEST_FILE_POSTGRESQLAUTOCONFTMP                    => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_POSTGRESQLAUTOCONFTMP;
    push @EXPORT, qw(MANIFEST_FILE_PGCONTROL);
use constant MANIFEST_FILE_POSTMASTEROPTS                           => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_POSTMASTEROPTS;
    push @EXPORT, qw(MANIFEST_FILE_POSTMASTEROPTS);
use constant MANIFEST_FILE_POSTMASTERPID                            => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_POSTMASTERPID;
    push @EXPORT, qw(MANIFEST_FILE_POSTMASTERPID);
use constant MANIFEST_FILE_RECOVERYCONF                             => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_RECOVERYCONF;
    push @EXPORT, qw(MANIFEST_FILE_RECOVERYCONF);
use constant MANIFEST_FILE_RECOVERYSIGNAL                           => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_RECOVERYSIGNAL;
    push @EXPORT, qw(MANIFEST_FILE_RECOVERYSIGNAL);
use constant MANIFEST_FILE_RECOVERYDONE                             => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_RECOVERYDONE;
    push @EXPORT, qw(MANIFEST_FILE_RECOVERYDONE);
use constant MANIFEST_FILE_STANDBYSIGNAL                            => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_STANDBYSIGNAL;
    push @EXPORT, qw(MANIFEST_FILE_STANDBYSIGNAL);
use constant MANIFEST_FILE_TABLESPACEMAP                            => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_TABLESPACEMAP;
    push @EXPORT, qw(MANIFEST_FILE_TABLESPACEMAP);

####################################################################################################################################
# Minimum ID for a user object in postgres
####################################################################################################################################
use constant DB_USER_OBJECT_MINIMUM_ID                              => 16384;
    push @EXPORT, qw(DB_USER_OBJECT_MINIMUM_ID);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $bLoad,
        $oStorage,
        $strDbVersion,
        $iDbCatalogVersion,
        $strCipherPass,                                             # Passphrase to open the manifest if encrypted
        $strCipherPassSub,                                          # Passphrase to encrypt the backup files
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strFileName', trace => true},
            {name => 'bLoad', optional => true, default => true, trace => true},
            {name => 'oStorage', optional => true, default => storageRepo(), trace => true},
            {name => 'strDbVersion', optional => true, trace => true},
            {name => 'iDbCatalogVersion', optional => true, trace => true},
            {name => 'strCipherPass', optional => true, redact => true},
            {name => 'strCipherPassSub', optional => true, redact => true},
        );

    # Init object and store variables
    my $self = $class->SUPER::new(
        $oStorage, $strFileName, {bLoad => $bLoad, strCipherPass => $strCipherPass, strCipherPassSub => $strCipherPassSub});

    # If manifest not loaded from a file then the db version and catalog version must be set
    if (!$bLoad)
    {
        if (!(defined($strDbVersion) && defined($iDbCatalogVersion)))
        {
            confess &log(ASSERT, 'strDbVersion and iDbCatalogVersion must be provided with bLoad = false');
        }

        # Force the version to a string since newer versions of JSON::PP lose track of the fact that it is one
        $self->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, $strDbVersion . '');
        $self->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalogVersion);
    }

    # Mark the manifest as built if it was loaded from a file
    $self->{bBuilt} = $bLoad;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# save
#
# Save the manifest.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->save');

    # Call inherited save
    $self->SUPER::save();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# get
#
# Get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    my $oValue = $self->SUPER::get($strSection, $strKey, $strSubKey, false);

    if (!defined($oValue) && defined($strKey) && defined($strSubKey) &&
        ($strSection eq MANIFEST_SECTION_TARGET_FILE || $strSection eq MANIFEST_SECTION_TARGET_PATH ||
         $strSection eq MANIFEST_SECTION_TARGET_LINK) &&
        ($strSubKey eq MANIFEST_SUBKEY_USER || $strSubKey eq MANIFEST_SUBKEY_GROUP ||
         $strSubKey eq MANIFEST_SUBKEY_MODE || $strSubKey eq MANIFEST_SUBKEY_MASTER) &&
        $self->test($strSection, $strKey))
    {
        $oValue = $self->SUPER::get("${strSection}:default", $strSubKey, undef, $bRequired, $oDefault);
    }
    else
    {
        $oValue = $self->SUPER::get($strSection, $strKey, $strSubKey, $bRequired, $oDefault);
    }

    return $oValue;
}

####################################################################################################################################
# boolGet
#
# Get a numeric value.
####################################################################################################################################
sub boolGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $bDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($bDefault) ? ($bDefault ? INI_TRUE : INI_FALSE) : undef) ? true : false;
}

####################################################################################################################################
# numericGet
#
# Get a numeric value.
####################################################################################################################################
sub numericGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $nDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($nDefault) ? $nDefault + 0 : undef) + 0;
}

####################################################################################################################################
# tablespacePathGet
#
# Get the unique path assigned by Postgres for the tablespace.
####################################################################################################################################
sub tablespacePathGet
{
    my $self = shift;

    return('PG_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) .
           '_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG));
}

####################################################################################################################################
# dbPathGet
#
# Convert a repo path to where the file actually belongs in the db.
####################################################################################################################################
sub dbPathGet
{
    my $self = shift;
    my $strDbPath = shift;
    my $strFile = shift;

    my $strDbFile = defined($strDbPath) ? "${strDbPath}/" : '';

    if (index($strFile, MANIFEST_TARGET_PGDATA . '/') == 0)
    {
        $strDbFile .= substr($strFile, length(MANIFEST_TARGET_PGDATA) + 1);
    }
    else
    {
        $strDbFile .= $strFile;
    }

    return $strDbFile;
}

####################################################################################################################################
# repoPathGet
#
# Convert a database path to where to file is located in the repo.
####################################################################################################################################
sub repoPathGet
{
    my $self = shift;
    my $strTarget = shift;
    my $strFile = shift;

    my $strRepoFile = $strTarget;

    if ($self->isTargetTablespace($strTarget) &&
        ($self->dbVersion() >= PG_VERSION_90))
    {
        $strRepoFile .= '/' . $self->tablespacePathGet();
    }

    if (defined($strFile))
    {
        $strRepoFile .= "/${strFile}";
    }

    return $strRepoFile;
}

####################################################################################################################################
# isTargetValid
#
# Determine if a target is valid.
####################################################################################################################################
sub isTargetValid
{
    my $self = shift;
    my $strTarget = shift;
    my $bError = shift;

    if (!defined($strTarget))
    {
        confess &log(ASSERT, 'target is not defined');
    }

    if (!$self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget))
    {
        if (defined($bError) && $bError)
        {
            confess &log(ASSERT, "${strTarget} is not a valid target");
        }

        return false;
    }

    return true;
}

####################################################################################################################################
# isTargetLink
#
# Determine if a target is a link.
####################################################################################################################################
sub isTargetLink
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);
}

####################################################################################################################################
# isTargetFile
#
# Determine if a target is a file link.
####################################################################################################################################
sub isTargetFile
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE);
}

####################################################################################################################################
# isTargetTablespace
#
# Determine if a target is a tablespace.
####################################################################################################################################
sub isTargetTablespace
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_ID);
}

####################################################################################################################################
# checkDelta
#
# Determine if the delta option should be enabled. Only called if delta has not yet been enabled.
####################################################################################################################################
sub checkDelta
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLastBackupSource,
        $bOnlineSame,
        $strTimelineCurrent,
        $strTimelineLast,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->checkDelta', \@_,
            {name => 'strLastBackupSource'},
            {name => 'bOnlineSame'},
            {name => 'strTimelineCurrent', required => false},
            {name => 'strTimelineLast', required => false},
        );

    my $bDelta = false;

    # Determine if a timeline switch has occurred
    if (defined($strTimelineLast) && defined($strTimelineCurrent))
    {
        # If there is a prior backup, check if a timeline switch has occurred since then
        if ($strTimelineLast ne $strTimelineCurrent)
        {
            &log(WARN, "a timeline switch has occurred since the ${strLastBackupSource} backup, enabling delta checksum");
            $bDelta = true;
        }
    }

    # If delta was not set above and there is a change in the online option, then set delta option
    if (!$bDelta && !$bOnlineSame)
    {
        &log(WARN, "the online option has changed since the ${strLastBackupSource} backup, enabling delta checksum");
        $bDelta = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bDelta', value => $bDelta, trace => true},
    );
}

####################################################################################################################################
# checkDeltaFile
#
# Determine if the delta option should be enabled. Only called if delta has not yet been enabled.
####################################################################################################################################
sub checkDeltaFile
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $stryFileList,
        $oPriorManifest,
        $lTimeBegin,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->checkDeltaFile', \@_,
            {name => 'stryFileList'},
            {name => 'oPriorManifest', required => false},
            {name => 'lTimeBegin', required => false},
        );

    my $bDelta = false;

    # Loop though all files
    foreach my $strName (@{$stryFileList})
    {
        # If $lTimeBegin is defined, then this is not an aborted manifest so check if modification time is in the future (in this
        # backup OR the last backup) then enable delta and exit
        if (defined($lTimeBegin) &&
            ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin ||
            (defined($oPriorManifest) &&
             $oPriorManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_FUTURE, 'y'))))
        {
            &log(WARN, "file $strName has timestamp in the future, enabling delta checksum");
            $bDelta = true;
            last;
        }

        # If the time on the file is earlier than the last manifest time or the size is different but the timestamp is the
        # same, then enable delta and exit
        if (defined($oPriorManifest) && $oPriorManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName) &&
            ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) <
            $oPriorManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) ||
            ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) !=
            $oPriorManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) &&
            $self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) ==
            $oPriorManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP))))
        {
            &log(WARN, "file $strName timestamp in the past or size changed but timestamp did not, enabling delta checksum");
            $bDelta = true;
            last;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bDelta', value => $bDelta, trace => true},
    );
}

####################################################################################################################################
# build
#
# Build the manifest object.
####################################################################################################################################
sub build
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorageDbPrimary,
        $strPath,
        $oLastManifest,
        $bOnline,
        $bDelta,
        $hTablespaceMap,
        $hDatabaseMap,
        $rhExclude,
        $strTimelineCurrent,
        $strTimelineLast,
        $strLevel,
        $bTablespace,
        $strParentPath,
        $strFilter,
        $iLevel,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->build', \@_,
            {name => 'oStorageDbPrimary'},
            {name => 'strPath'},
            {name => 'oLastManifest', required => false},
            {name => 'bOnline'},
            {name => 'bDelta'},
            {name => 'hTablespaceMap', required => false},
            {name => 'hDatabaseMap', required => false},
            {name => 'rhExclude', required => false},
            {name => 'strTimelineCurrent', required => false},
            {name => 'strTimelineLast', required => false},
            {name => 'strLevel', required => false},
            {name => 'bTablespace', required => false},
            {name => 'strParentPath', required => false},
            {name => 'strFilter', required => false},
            {name => 'iLevel', required => false, default => 0},
        );

    # Limit recursion to something reasonable (if more then we are very likely in a link loop)
    if ($iLevel >= 16)
    {
        confess &log(
            ERROR,
            "recursion in manifest build exceeds depth of ${iLevel}: ${strLevel}\n" .
                'HINT: is there a link loop in $PGDATA?',
            ERROR_FORMAT);
    }

    if (!defined($strLevel))
    {
        # Don't allow the manifest to be built more than once
        if ($self->{bBuilt})
        {
            confess &log(ASSERT, "manifest has already been built");
        }

        $self->{bBuilt} = true;

        # Set initial level
        $strLevel = MANIFEST_TARGET_PGDATA;

        # If not online then build the tablespace map from pg_tblspc path
        if (!$bOnline && !defined($hTablespaceMap))
        {
            my $hTablespaceManifest = $oStorageDbPrimary->manifest($strPath . '/' . DB_PATH_PGTBLSPC);
            $hTablespaceMap = {};

            foreach my $strOid (sort(CORE::keys(%{$hTablespaceManifest})))
            {
                if ($strOid eq '.' or $strOid eq '..')
                {
                    next;
                }

                logDebugMisc($strOperation, "found tablespace ${strOid} in offline mode");

                $hTablespaceMap->{$strOid} = "ts${strOid}";
            }
        }

        # If there is a last manifest, then check to see if delta checksum should be enabled
        if (defined($oLastManifest) && !$bDelta)
        {
            $bDelta = $self->checkDelta(
                'last', $oLastManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, $bOnline),
                $strTimelineCurrent, $strTimelineLast);
        }
    }

    $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH, $strPath);
    $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TYPE,
               $strLevel eq MANIFEST_TARGET_PGDATA ? MANIFEST_VALUE_PATH : MANIFEST_VALUE_LINK);

    if ($bTablespace)
    {
        my $iTablespaceId = (split('\/', $strLevel))[1];

        if (!defined($hTablespaceMap->{$iTablespaceId}))
        {
            confess &log(ASSERT, "tablespace with oid ${iTablespaceId} not found in tablespace map\n" .
                                 "HINT: was a tablespace created or dropped during the backup?");
        }

        $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TABLESPACE_ID, $iTablespaceId);
        $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TABLESPACE_NAME,
                   $hTablespaceMap->{$iTablespaceId});
    }

    if (index($strPath, '/') != 0)
    {
        if (!defined($strParentPath))
        {
            confess &log(ASSERT, "cannot get manifest for '${strPath}' when no parent path is specified");
        }

        $strPath = $oStorageDbPrimary->pathAbsolute($strParentPath, $strPath);
    }

    # Get the manifest for this level
    my $hManifest = $oStorageDbPrimary->manifest($strPath, {strFilter => $strFilter});
    my $strManifestType = MANIFEST_VALUE_LINK;

    # Loop though all paths/files/links in the manifest
    foreach my $strName (sort(CORE::keys(%{$hManifest})))
    {
        my $strFile = $strLevel;

        if ($strName ne '.')
        {
            if ($strManifestType eq MANIFEST_VALUE_LINK && $hManifest->{$strName}{type} eq 'l')
            {
                confess &log(ERROR, 'link \'' .
                    $self->dbPathGet(
                        $self->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH), $strLevel) .
                        '\' -> \'' . $self->get(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH) .
                        '\' cannot reference another link', ERROR_LINK_DESTINATION);
            }

            if ($strManifestType eq MANIFEST_VALUE_LINK)
            {
                $strFile = dirname($strFile);
                $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH,
                           dirname($self->get(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH)));
                $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_FILE, $strName);
            }

            $strFile .= "/${strName}";
        }
        else
        {
            $strManifestType = MANIFEST_VALUE_PATH;
        }

        # Skip wal directory when doing an online backup.  WAL will be restored from the archive or stored in the wal directory at
        # the end of the backup if the archive-copy option is set.
        next if ($bOnline && $strFile =~ (qw{^} . MANIFEST_TARGET_PGDATA . qw{/} . $self->walPath() . '\/') &&
                 $strFile !~ ('^' . MANIFEST_TARGET_PGDATA . qw{/} . $self->walPath() . qw{/} . DB_PATH_ARCHIVESTATUS . '$'));

        # Skip all directories and files that start with pgsql_tmp.  The files are removed when the server is restarted and the
        # directories are recreated.
        next if $strName =~ ('(^|\/)' . DB_FILE_PREFIX_TMP);

        # Skip pg_dynshmem/* since these files cannot be reused on recovery
        next if $strFile =~ ('^' . MANIFEST_PATH_PGDYNSHMEM . '\/') && $self->dbVersion() >= PG_VERSION_94;

        # Skip pg_notify/* since these files cannot be reused on recovery
        next if $strFile =~ ('^' . MANIFEST_PATH_PGNOTIFY . '\/') && $self->dbVersion() >= PG_VERSION_90;

        # Skip pg_replslot/* since these files are generally not useful after a restore
        next if $strFile =~ ('^' . MANIFEST_PATH_PGREPLSLOT . '\/') && $self->dbVersion() >= PG_VERSION_94;

        # Skip pg_serial/* since these files are reset
        next if $strFile =~ ('^' . MANIFEST_PATH_PGSERIAL . '\/') && $self->dbVersion() >= PG_VERSION_91;

        # Skip pg_snapshots/* since these files cannot be reused on recovery
        next if $strFile =~ ('^' . MANIFEST_PATH_PGSNAPSHOTS . '\/') && $self->dbVersion() >= PG_VERSION_92;

        # Skip temporary statistics in pg_stat_tmp even when stats_temp_directory is set because PGSS_TEXT_FILE is always created
        # there.
        next if $strFile =~ ('^' . MANIFEST_PATH_PGSTATTMP . '\/') && $self->dbVersion() >= PG_VERSION_84;

        # Skip pg_subtrans/* since these files are reset
        next if $strFile =~ ('^' . MANIFEST_PATH_PGSUBTRANS . '\/');

        # Skip pg_internal.init since it is recreated on startup
        next if $strFile =~ (DB_FILE_PGINTERNALINIT . '$');

        # Skip recovery files
        if ($self->dbVersion() >= PG_VERSION_12)
        {
            next if ($strFile eq MANIFEST_FILE_RECOVERYSIGNAL || $strFile eq MANIFEST_FILE_STANDBYSIGNAL);
        }
        else
        {
            next if ($strFile eq MANIFEST_FILE_RECOVERYDONE || $strFile eq MANIFEST_FILE_RECOVERYCONF);
        }

        # Skip ignored files
        if ($strFile eq MANIFEST_FILE_POSTGRESQLAUTOCONFTMP ||      # postgresql.auto.conf.tmp - temp file for safe writes
            $strFile eq MANIFEST_FILE_BACKUPLABELOLD ||             # backup_label.old - old backup labels are not useful
            $strFile eq MANIFEST_FILE_POSTMASTEROPTS ||             # postmaster.opts - not useful for backup
            $strFile eq MANIFEST_FILE_POSTMASTERPID)                # postmaster.pid - to avoid confusing postgres after restore
        {
            next;
        }

        # If version is greater than 9.0, check for files to exclude
        if ($self->dbVersion() >= PG_VERSION_90 && $hManifest->{$strName}{type} eq 'f')
        {
            # Get the directory name from the manifest; it will be used later to search for existence in the keys
            my $strDir = dirname($strName);

            # If it is a database data directory (base or tablespace) then check for files to skip
            if ($strDir =~ '^base\/[0-9]+$' ||
                $strDir =~ ('^' . $self->tablespacePathGet() . '\/[0-9]+$'))
            {
                # Get just the filename
                my $strBaseName = basename($strName);

                # Skip temp tables (lower t followed by numbers underscore numbers and a dot (segment) or underscore (fork) and/or
                # segment, e.g. t1234_123, t1234_123.1, t1234_123_vm, t1234_123_fsm.1
                if ($strBaseName =~ '^t[0-9]+\_[0-9]+(|\_(fsm|vm)){0,1}(\.[0-9]+){0,1}$')
                {
                    next;
                }

                # If version is greater than 9.1 then check for unlogged tables to skip
                if ($self->dbVersion() >= PG_VERSION_91)
                {
                    # Exclude all forks for unlogged tables except the init fork (numbers underscore init and optional dot segment)
                    if ($strBaseName =~ '^[0-9]+(|\_(fsm|vm)){0,1}(\.[0-9]+){0,1}$')
                    {
                        # Get the filenode (OID)
                        my ($strFileNode) = $strBaseName =~ '^(\d+)';

                        # Add _init to the OID to see if this is an unlogged object
                        $strFileNode = $strDir. "/" . $strFileNode . "_init";

                        # If key exists in manifest then skip
                        if (exists($hManifest->{$strFileNode}) && $hManifest->{$strFileNode}{type} eq 'f')
                        {
                            next;
                        }
                    }
                }
            }
        }

        # Exclude files requested by the user
        if (defined($rhExclude))
        {
            # Exclusions are based on the name of the file relative to PGDATA
            my $strPgFile = $self->dbPathGet(undef, $strFile);
            my $bExclude = false;

            # Iterate through exclusions
            foreach my $strExclude (sort(keys(%{$rhExclude})))
            {
                # If the exclusion ends in / then we must do a prefix match
                if ($strExclude =~ /\/$/)
                {
                    if (index($strPgFile, $strExclude) == 0)
                    {
                        $bExclude = true;
                    }
                }
                # Else an exact match or a prefix match with / appended is required
                elsif ($strPgFile eq $strExclude || index($strPgFile, "${strExclude}/") == 0)
                {
                    $bExclude = true;
                }

                # Log everything that gets excluded at a high level so it will hopefully be seen if wrong
                if ($bExclude)
                {
                    &log(INFO, "exclude ${strPgFile} from backup using '${strExclude}' exclusion");
                    last;
                }
            }

            # Skip the file if it was excluded
            next if $bExclude;
        }

        my $cType = $hManifest->{$strName}{type};
        my $strSection = MANIFEST_SECTION_TARGET_PATH;

        if ($cType eq 'f')
        {
            $strSection = MANIFEST_SECTION_TARGET_FILE;
        }
        elsif ($cType eq 'l')
        {
            $strSection = MANIFEST_SECTION_TARGET_LINK;
        }
        elsif ($cType ne 'd')
        {
            &log(WARN, "exclude special file '" . $self->dbPathGet(undef, $strFile) . "' from backup");
            next;
        }

        # Make sure that DB_PATH_PGTBLSPC contains only absolute links that do not point inside PGDATA
        my $bTablespace = false;

        if (index($strName, DB_PATH_PGTBLSPC . '/') == 0 && $strLevel eq MANIFEST_TARGET_PGDATA)
        {
            $bTablespace = true;
            $strFile = MANIFEST_TARGET_PGDATA . '/' . $strName;

            # Check for files in DB_PATH_PGTBLSPC that are not links
            if ($hManifest->{$strName}{type} ne 'l')
            {
                confess &log(ERROR, "${strName} is not a symlink - " . DB_PATH_PGTBLSPC . ' should contain only symlinks',
                             ERROR_LINK_EXPECTED);
            }

            # Check for tablespaces in PGDATA
            if (index($hManifest->{$strName}{link_destination}, "${strPath}/") == 0 ||
                (index($hManifest->{$strName}{link_destination}, '/') != 0 &&
                 index($oStorageDbPrimary->pathAbsolute($strPath . '/' . DB_PATH_PGTBLSPC,
                       $hManifest->{$strName}{link_destination}) . '/', "${strPath}/") == 0))
            {
                confess &log(ERROR, 'tablespace symlink ' . $hManifest->{$strName}{link_destination} .
                             ' destination must not be in $PGDATA', ERROR_LINK_DESTINATION);
            }
        }

        # User and group required for all types
        if (defined($hManifest->{$strName}{user}))
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_USER, $hManifest->{$strName}{user});
        }
        else
        {
            $self->boolSet($strSection, $strFile, MANIFEST_SUBKEY_USER, false);
        }

        if (defined($hManifest->{$strName}{group}))
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_GROUP, $hManifest->{$strName}{group});
        }
        else
        {
            $self->boolSet($strSection, $strFile, MANIFEST_SUBKEY_GROUP, false);
        }

        # Mode for required file and path type only
        if ($cType eq 'f' || $cType eq 'd')
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_MODE, $hManifest->{$strName}{mode});
        }

        # Modification time and size required for file type only
        if ($cType eq 'f')
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_TIMESTAMP,
                       $hManifest->{$strName}{modification_time} + 0);
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_SIZE, $hManifest->{$strName}{size} + 0);
            $self->boolSet($strSection, $strFile, MANIFEST_SUBKEY_MASTER,
                ($strFile eq MANIFEST_FILE_PGCONTROL || $self->isMasterFile($strFile)));
        }

        # Link destination required for link type only
        if ($cType eq 'l')
        {
            my $strLinkDestination = $hManifest->{$strName}{link_destination};
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_DESTINATION, $strLinkDestination);

            # If this is a tablespace then set the filter to use for the next level
            my $strFilter;

            if ($bTablespace)
            {
                # Only versions >= 9.0  have the special top-level tablespace path.  Below 9.0 the database files are stored
                # directly in the path referenced by the symlink.
                if ($self->dbVersion() >= PG_VERSION_90)
                {
                    $strFilter = $self->tablespacePathGet();
                }

                $self->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGTBLSPC, undef,
                           $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA));

                # PGDATA prefix was only needed for the link so strip it off before recursing
                $strFile = substr($strFile, length(MANIFEST_TARGET_PGDATA) + 1);
            }

            $bDelta = $self->build(
                $oStorageDbPrimary, $strLinkDestination, undef, $bOnline, $bDelta, $hTablespaceMap, $hDatabaseMap, $rhExclude,
                undef, undef, $strFile, $bTablespace, dirname("${strPath}/${strName}"), $strFilter, $iLevel + 1);
        }
    }

    # If this is the base level then do post-processing
    if ($strLevel eq MANIFEST_TARGET_PGDATA)
    {
        my $bTimeInFuture = false;

        # Wait for the remainder of the second when doing online backups.  This is done because most filesystems only have a one
        # second resolution and Postgres will still be modifying files during the second that the manifest is built and this could
        # lead to an invalid diff/incr backup later when using timestamps to determine which files have changed.  Offline backups do
        # not wait because it makes testing much faster and Postgres should not be running (if it is the backup will not be
        # consistent anyway and the one-second resolution problem is the least of our worries).
        my $lTimeBegin = waitRemainder($bOnline);

        if (defined($oLastManifest))
        {
            $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                       $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
        }

        # Store database map information when provided during an online backup.
        foreach my $strDbName (sort(keys(%{$hDatabaseMap})))
        {
            $self->numericSet(MANIFEST_SECTION_DB, $strDbName, MANIFEST_KEY_DB_ID,
                              $hDatabaseMap->{$strDbName}{&MANIFEST_KEY_DB_ID});
            $self->numericSet(MANIFEST_SECTION_DB, $strDbName, MANIFEST_KEY_DB_LAST_SYSTEM_ID,
                              $hDatabaseMap->{$strDbName}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID});
        }

        # Determine if delta checksum should be enabled
        if (!$bDelta)
        {
            my @stryFileList = $self->keys(MANIFEST_SECTION_TARGET_FILE);

            if (@stryFileList)
            {
                $bDelta = $self->checkDeltaFile(\@stryFileList, $oLastManifest, $lTimeBegin);
            }
        }

        # Loop though all files
        foreach my $strName ($self->keys(MANIFEST_SECTION_TARGET_FILE))
        {
            # If modification time is in the future (in this backup OR the last backup) set warning flag and do not
            # allow a reference
            if ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin ||
                (defined($oLastManifest) &&
                 $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_FUTURE, 'y')))
            {
                $bTimeInFuture = true;

                # Only mark as future if still in the future in the current backup
                if ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin)
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_FUTURE, 'y');
                }
            }
            # Else check if the size and timestamp match OR if the size matches and the delta option is set, then keep the file.
            # In the latter case, if there had been a timestamp change then rather than removing and recopying the file, the file
            # will be tested in backupFile to see if the db/repo checksum still matches: if so, it is not necessary to recopy,
            # else it will need to be copied to the new backup. For zero sized files, the reference will be set and copying
            # will be skipped later.
            elsif (defined($oLastManifest) && $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName) &&
                   $self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) ==
                       $oLastManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) &&
                   ($bDelta || ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) == 0 ||
                   $self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) ==
                       $oLastManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP))))
            {
                # Copy reference from previous backup if possible
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE,
                        $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE));
                }
                # Otherwise the reference is to the previous backup
                else
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE,
                        $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
                }

                # Copy the checksum from previous manifest (if it exists - zero sized files don't have checksums)
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM,
                               $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM));
                }

                # Copy repo size from the previous manifest (if it exists)
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE,
                        $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE));
                }

                # Copy master flag from the previous manifest (if it exists)
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_MASTER))
                {
                    $self->set(
                        MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_MASTER,
                        $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_MASTER));
                }

                # Copy checksum page from the previous manifest (if it exists)
                my $bChecksumPage = $oLastManifest->get(
                    MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

                if (defined($bChecksumPage))
                {
                    $self->boolSet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE, $bChecksumPage);

                    if (!$bChecksumPage &&
                        $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR))
                    {
                        $self->set(
                            MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR,
                            $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR));
                    }
                }
            }
        }

        # Warn if any files in the current backup are in the future
        if ($bTimeInFuture)
        {
            &log(WARN, "some files have timestamps in the future - they will be copied to prevent possible race conditions");
        }

        # Record the time when copying will start
        $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef, $lTimeBegin + ($bOnline ? 1 : 0));

        # Build default sections
        $self->buildDefault();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bDelta', value => $bDelta, trace => true},
    );
}

####################################################################################################################################
# fileAdd
#
# Add files to the manifest that were generated after the initial manifest build, e.g. backup_label, tablespace_map, and copied WAL
# files.  Since the files were not in the original cluster the user, group, and mode must be defaulted.
####################################################################################################################################
sub fileAdd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strManifestFile,
        $lModificationTime,
        $lSize,
        $strChecksum,
        $bMaster,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->fileAdd', \@_,
            {name => 'strManifestFile'},
            {name => 'lModificationTime'},
            {name => 'lSize'},
            {name => 'lChecksum'},
            {name => 'bMaster'},
        );

    # Set manifest values
    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_USER) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_USER, undef,
                     $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER)))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_USER,
                   $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER));
    }

    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_GROUP) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_GROUP, undef,
                     $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP)))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_GROUP,
                   $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP));
    }

    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_MODE) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_MODE, undef, '0600'))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_MODE, '0600');
    }

    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_TIMESTAMP, $lModificationTime);
    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_SIZE, $lSize);
    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
    $self->boolSet(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_MASTER, $bMaster);
}

####################################################################################################################################
# buildDefault
#
# Builds the default section.
####################################################################################################################################
sub buildDefault
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->buildDefault');

    # Set defaults for subkeys that tend to repeat
    foreach my $strSection (&MANIFEST_SECTION_TARGET_FILE, &MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
    {
        foreach my $strSubKey (&MANIFEST_SUBKEY_USER, &MANIFEST_SUBKEY_GROUP, &MANIFEST_SUBKEY_MODE, &MANIFEST_SUBKEY_MASTER)
        {
            # Links don't have a mode so skip
            next if ($strSection eq MANIFEST_SECTION_TARGET_LINK && $strSubKey eq &MANIFEST_SUBKEY_MODE);

            # Only files have the master subkey
            next if ($strSection ne MANIFEST_SECTION_TARGET_FILE && $strSubKey eq &MANIFEST_SUBKEY_MASTER);

            my %oDefault;
            my $iSectionTotal = 0;

            foreach my $strFile ($self->keys($strSection))
            {
                # Don't count false values when subkey in (MANIFEST_SUBKEY_USER, MANIFEST_SUBKEY_GROUP)
                next if (($strSubKey eq MANIFEST_SUBKEY_USER || $strSubKey eq MANIFEST_SUBKEY_GROUP) &&
                         $self->boolTest($strSection, $strFile, $strSubKey, false));

                my $strValue = $self->get($strSection, $strFile, $strSubKey);

                if (defined($oDefault{$strValue}))
                {
                    $oDefault{$strValue}++;
                }
                else
                {
                    $oDefault{$strValue} = 1;
                }

                $iSectionTotal++;
            }

            my $strMaxValue;
            my $iMaxValueTotal = 0;

            foreach my $strValue (sort(keys(%oDefault)))
            {
                if ($oDefault{$strValue} > $iMaxValueTotal)
                {
                    $iMaxValueTotal = $oDefault{$strValue};
                    $strMaxValue = $strValue;
                }
            }

            if (defined($strMaxValue) > 0 && $iMaxValueTotal > $iSectionTotal * MANIFEST_DEFAULT_MATCH_FACTOR)
            {
                if ($strSubKey eq MANIFEST_SUBKEY_MASTER)
                {
                    $self->boolSet("${strSection}:default", $strSubKey, undef, $strMaxValue);
                }
                else
                {
                    $self->set("${strSection}:default", $strSubKey, undef, $strMaxValue);
                }

                foreach my $strFile ($self->keys($strSection))
                {
                    if ($self->test($strSection, $strFile, $strSubKey, $strMaxValue))
                    {
                        $self->remove($strSection, $strFile, $strSubKey);
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# validate
#
# Checks for any missing values or inconsistencies in the manifest.
####################################################################################################################################
sub validate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . 'validate');

    # Make sure that all files have size and checksum (when size > 0).  Since these values are removed before the backup file copy
    # starts this ensures that all files had results stored in the manifest during the file copy.
    foreach my $strFile ($self->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # Ensure size is set
        if (!$self->test(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE))
        {
            confess &log(ASSERT, "manifest subvalue 'size' not set for file '${strFile}'");
        }

        # If size > 0 then checksum must also be set
        if ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE) > 0 &&
            !$self->test(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM))
        {
            confess &log(ASSERT, "manifest subvalue 'checksum' not set for file '${strFile}'");
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# dbVersion - version of PostgreSQL that the manifest is being built for
####################################################################################################################################
sub dbVersion
{
    my $self = shift;

    return $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION);
}

####################################################################################################################################
# xactPath - return the transaction directory based on the PostgreSQL version
####################################################################################################################################
sub xactPath
{
    my $self = shift;

    return $self->dbVersion() >= PG_VERSION_10 ? 'pg_xact' : 'pg_clog';
}

####################################################################################################################################
# walPath - return the wal directory based on the PostgreSQL version
####################################################################################################################################
sub walPath
{
    my $self = shift;

    return $self->dbVersion() >= PG_VERSION_10 ? 'pg_wal' : 'pg_xlog';
}

####################################################################################################################################
# isMasterFile
#
# Is this file required to be copied from the master?
####################################################################################################################################
sub isMasterFile
{
    my $self = shift;
    my $strFile = shift;

    return
        $strFile !~ ('^(' . MANIFEST_TARGET_PGDATA . '\/' . '(' . DB_PATH_BASE . '|' . DB_PATH_GLOBAL . '|' .
        $self->xactPath() . '|' . DB_PATH_PGMULTIXACT . ')|' . DB_PATH_PGTBLSPC . ')\/');
}

####################################################################################################################################
# isChecksumPage
#
# Can this file have page checksums in PG >= 9.3?
####################################################################################################################################
sub isChecksumPage
{
    my $strFile = shift;

    if (($strFile =~ ('^' . MANIFEST_TARGET_PGDATA . '\/' . DB_PATH_BASE . '\/[0-9]+\/|^' . MANIFEST_TARGET_PGTBLSPC .
            '\/[0-9]+\/[^\/]+\/[0-9]+\/') &&
            $strFile !~ ('(' . DB_FILE_PGFILENODEMAP . '|' . DB_FILE_PGINTERNALINIT . '|' . DB_FILE_PGVERSION . ')$')) ||
        ($strFile =~ ('^' . MANIFEST_TARGET_PGDATA . '\/' . DB_PATH_GLOBAL . '\/') &&
            $strFile !~ ('(' . DB_FILE_PGFILENODEMAP . '|' . DB_FILE_PGINTERNALINIT . '|' . DB_FILE_PGVERSION . '|' .
            DB_FILE_PGCONTROL . ')$')))
    {
        return true;
    }

    return false;
}

push @EXPORT, qw(isChecksumPage);

1;
