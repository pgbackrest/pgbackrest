####################################################################################################################################
# HostDbTest.pm - Database host
####################################################################################################################################
package pgBackRestTest::Backup::Common::HostDbCommonTest;
use parent 'pgBackRestTest::Backup::Common::HostBackupTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl ':mode';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Version;

use pgBackRestTest::Backup::Common::HostBackupTest;
use pgBackRestTest::Backup::Common::HostBaseTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::CommonTest;

####################################################################################################################################
# Host constants
####################################################################################################################################
use constant HOST_DB_MASTER                                         => 'db-master';
    push @EXPORT, qw(HOST_DB_MASTER);
use constant HOST_DB_MASTER_USER                                    => 'db-master-user';
    push @EXPORT, qw(HOST_DB_MASTER_USER);

####################################################################################################################################
# Host parameters
####################################################################################################################################
use constant HOST_PARAM_DB_BASE_PATH                                => 'db-base-path';
    push @EXPORT, qw(HOST_PARAM_DB_BASE_PATH);
use constant HOST_PARAM_DB_PATH                                     => 'db-path';
    push @EXPORT, qw(HOST_PARAM_DB_PATH);
use constant HOST_PARAM_SPOOL_PATH                                  => 'spool-path';
    push @EXPORT, qw(HOST_PARAM_SPOOL_PATH);
use constant HOST_PARAM_TABLESPACE_PATH                             => 'tablespace-path';
    push @EXPORT, qw(HOST_PARAM_TABLESPACE_PATH);

####################################################################################################################################
# Host paths
####################################################################################################################################
use constant HOST_PATH_SPOOL                                        => 'spool';
    push @EXPORT, qw(HOST_PATH_SPOOL);
use constant HOST_PATH_DB                                           => 'db';
    push @EXPORT, qw(HOST_PATH_DB);
use constant HOST_PATH_DB_BASE                                      => 'base';
    push @EXPORT, qw(HOST_PATH_DB_BASE);

####################################################################################################################################
# Cached data sections
####################################################################################################################################
use constant SECTION_FILE_NAME                                        => 'strFileName';
    push @EXPORT, qw(SECTION_FILE_NAME);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParam', required => false, trace => true},
        );

    my $self = $class->SUPER::new(
        {
            strName => $$oParam{strName},
            strImage => $$oParam{strImage},
            strUser => $$oParam{strUser},
            strVm => $$oParam{strVm},
            oLogTest => $$oParam{oLogTest},
            bSynthetic => $$oParam{bSynthetic},
            oHostBackup => $$oParam{oHostBackup},
        });
    bless $self, $class;

    # Set parameters
    $self->paramSet(HOST_PARAM_DB_PATH, $self->testPath() . '/' . HOST_PATH_DB);
    $self->paramSet(HOST_PARAM_DB_BASE_PATH, $self->dbPath() . '/' . HOST_PATH_DB_BASE);
    $self->paramSet(HOST_PARAM_TABLESPACE_PATH, $self->dbPath() . '/tablespace');

    filePathCreate($self->dbBasePath(), undef, undef, true);

    if (defined($$oParam{oHostBackup}))
    {
        $self->paramSet(HOST_PARAM_REPO_PATH, $$oParam{oHostBackup}->repoPath());
        $self->paramSet(HOST_PARAM_SPOOL_PATH, $self->testPath() . '/' . HOST_PATH_SPOOL);
        $self->paramSet(HOST_PARAM_LOG_PATH, $self->spoolPath() . '/' . HOST_PATH_LOG);
        $self->paramSet(HOST_PARAM_LOCK_PATH, $self->spoolPath() . '/' . HOST_PATH_LOCK);

        filePathCreate($self->spoolPath());
    }
    else
    {
        $self->paramSet(HOST_PARAM_SPOOL_PATH, $self->repoPath());
    }
#CSHANG I am using an array for the hash in case we need to update more than one file for any test, but this may be overkill...
    # Create a placeholder array hash for info file munging
    $self->{hyInfoFile} = ();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# archivePush
####################################################################################################################################
sub archivePush
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strXlogPath,
        $strArchiveTestFile,
        $iArchiveNo,
        $iExpectedError,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->archivePush', \@_,
            {name => 'strXlogPath'},
            {name => 'strArchiveTestFile'},
            {name => 'iArchiveNo'},
            {name => 'iExpectedError', required => false},
        );

    my $strSourceFile = "${strXlogPath}/" . uc(sprintf('0000000100000001%08x', $iArchiveNo));

    $self->{oFile}->copy(
        PATH_DB_ABSOLUTE, $strArchiveTestFile,                      # Source file
        PATH_DB_ABSOLUTE, $strSourceFile,                           # Destination file
        false,                                                      # Source is not compressed
        false,                                                      # Destination is not compressed
        undef, undef, undef,                                        # Unused params
        true);                                                      # Create path if it does not exist

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --archive-max-mb=24 --no-fork --stanza=' . $self->stanza() .
        (defined($iExpectedError) && $iExpectedError == ERROR_HOST_CONNECT ? ' --backup-host=bogus' : '') .
        " archive-push ${strSourceFile}",
        {iExpectedExitStatus => $iExpectedError, oLogTest => $self->{oLogTest}});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# configRecovery
####################################################################################################################################
sub configRecovery
{
    my $self = shift;
    my $oHostBackup = shift;
    my $oRecoveryHashRef = shift;

    # Get stanza
    my $strStanza = $self->stanza();

    # Load db config file
    my %oConfig;
    iniLoad($self->backrestConfig(), \%oConfig, true);

    # Load backup config file
    my %oRemoteConfig;

    if ($oHostBackup->nameTest(HOST_BACKUP))
    {
        iniLoad($oHostBackup->backrestConfig(), \%oRemoteConfig, true);
    }

    # Rewrite recovery options
    my @stryRecoveryOption;

    foreach my $strOption (sort(keys(%$oRecoveryHashRef)))
    {
        push (@stryRecoveryOption, "${strOption}=${$oRecoveryHashRef}{$strOption}");
    }

    if (@stryRecoveryOption)
    {
        $oConfig{$strStanza}{&OPTION_RESTORE_RECOVERY_OPTION} = \@stryRecoveryOption;
    }

    # Save db config file
    iniSave($self->backrestConfig(), \%oConfig, true);

    # Save backup config file
    if ($oHostBackup->nameTest(HOST_BACKUP))
    {
        iniSave($oHostBackup->backrestConfig(), \%oRemoteConfig, true);
    }
}

####################################################################################################################################
# configRemap
####################################################################################################################################
sub configRemap
{
    my $self = shift;
    my $oHostBackup = shift;
    my $oRemapHashRef = shift;
    my $oManifestRef = shift;

    # Get stanza name
    my $strStanza = $self->stanza();

    # Load db config file
    my %oConfig;
    iniLoad($self->backrestConfig(), \%oConfig, true);

    # Load backup config file
    my %oRemoteConfig;

    if ($oHostBackup->nameTest(HOST_BACKUP))
    {
        iniLoad($oHostBackup->backrestConfig(), \%oRemoteConfig, true);
    }

    # Rewrite recovery section
    delete($oConfig{"${strStanza}:restore"}{&OPTION_TABLESPACE_MAP});
    my @stryTablespaceMap;

    foreach my $strRemap (sort(keys(%$oRemapHashRef)))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq MANIFEST_TARGET_PGDATA)
        {
            $oConfig{$strStanza}{'db-path'} = $strRemapPath;
            ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;

            if ($oHostBackup->nameTest(HOST_BACKUP))
            {
                $oRemoteConfig{$strStanza}{'db-path'} = $strRemapPath;
            }
        }
        else
        {
            my $strTablespaceOid = (split('\/', $strRemap))[1];
            push (@stryTablespaceMap, "${strTablespaceOid}=${strRemapPath}");

            ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strRemap}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;
            ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{MANIFEST_TARGET_PGDATA . "/${strRemap}"}{destination} = $strRemapPath;
        }
    }

    if (@stryTablespaceMap)
    {
        $oConfig{"${strStanza}:restore"}{&OPTION_TABLESPACE_MAP} = \@stryTablespaceMap;
    }

    # Save db config file
    iniSave($self->backrestConfig(), \%oConfig, true);

    # Save backup config file
    if ($oHostBackup->nameTest(HOST_BACKUP))
    {
        iniSave($oHostBackup->backrestConfig(), \%oRemoteConfig, true);
    }
}

####################################################################################################################################
# restore
####################################################################################################################################
sub restore
{
    my $self = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;
    my $oRemapHashRef = shift;
    my $bDelta = shift;
    my $bForce = shift;
    my $strType = shift;
    my $strTarget = shift;
    my $bTargetExclusive = shift;
    my $strTargetAction = shift;
    my $strTargetTimeline = shift;
    my $oRecoveryHashRef = shift;
    my $strComment = shift;
    my $iExpectedExitStatus = shift;
    my $strOptionalParam = shift;
    my $bTablespace = shift;
    my $strUser = shift;

    # Set defaults
    $bDelta = defined($bDelta) ? $bDelta : false;
    $bForce = defined($bForce) ? $bForce : false;

    $strComment = 'restore' .
                  ($bDelta ? ' delta' : '') .
                  ($bForce ? ', force' : '') .
                  ($strBackup ne OPTION_DEFAULT_RESTORE_SET ? ", backup '${strBackup}'" : '') .
                  ($strType ? ", type '${strType}'" : '') .
                  ($strTarget ? ", target '${strTarget}'" : '') .
                  ($strTargetTimeline ? ", timeline '${strTargetTimeline}'" : '') .
                  (defined($bTargetExclusive) && $bTargetExclusive ? ', exclusive' : '') .
                  (defined($strTargetAction) && $strTargetAction ne OPTION_DEFAULT_RESTORE_TARGET_ACTION
                      ? ', ' . OPTION_TARGET_ACTION . "=${strTargetAction}" : '') .
                  (defined($oRemapHashRef) ? ', remap' : '') .
                  (defined($iExpectedExitStatus) ? ", expect exit ${iExpectedExitStatus}" : '') .
                  (defined($strComment) ? " - ${strComment}" : '') .
                  ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    if (!defined($oExpectedManifestRef))
    {
        # Change mode on the backup path so it can be read
        my $oExpectedManifest = new pgBackRest::Manifest(
            $self->{oFile}->pathGet(
                PATH_BACKUP_CLUSTER,
                ($strBackup eq 'latest' ? $oHostBackup->backupLast() : $strBackup) . '/' . FILE_MANIFEST),
                true);

        $oExpectedManifestRef = $oExpectedManifest->{oContent};
    }

    # Get the backup host
    if (defined($oRemapHashRef))
    {
        $self->configRemap($oHostBackup, $oRemapHashRef, $oExpectedManifestRef);
    }

    if (defined($oRecoveryHashRef))
    {
        $self->configRecovery($oHostBackup, $oRecoveryHashRef);
    }

    # Create the restorecommand
    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($bDelta) && $bDelta ? ' --delta' : '') .
        (defined($bForce) && $bForce ? ' --force' : '') .
        ($strBackup ne OPTION_DEFAULT_RESTORE_SET ? " --set=${strBackup}" : '') .
        (defined($strOptionalParam) ? " ${strOptionalParam} " : '') .
        (defined($strType) && $strType ne RECOVERY_TYPE_DEFAULT ? " --type=${strType}" : '') .
        (defined($strTarget) ? " --target=\"${strTarget}\"" : '') .
        (defined($strTargetTimeline) ? " --target-timeline=\"${strTargetTimeline}\"" : '') .
        (defined($bTargetExclusive) && $bTargetExclusive ? ' --target-exclusive' : '') .
        ($self->synthetic() ? '' : ' --link-all') .
        (defined($strTargetAction) && $strTargetAction ne OPTION_DEFAULT_RESTORE_TARGET_ACTION
            ? ' --' . OPTION_TARGET_ACTION . "=${strTargetAction}" : '') .
        ' --stanza=' . $self->stanza() . ' restore',
        {strComment => $strComment, iExpectedExitStatus => $iExpectedExitStatus, oLogTest => $self->{oLogTest}},
        $strUser);

    if (!defined($iExpectedExitStatus))
    {
        $self->restoreCompare($strBackup, $oExpectedManifestRef, $bTablespace);

        if (defined($self->{oLogTest}))
        {
            $self->{oLogTest}->supplementalAdd(
                $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} .
                "/recovery.conf");
        }
    }
}

####################################################################################################################################
# restoreCompare
####################################################################################################################################
sub restoreCompare
{
    my $self = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;
    my $bTablespace = shift;

    my $strTestPath = $self->testPath();

    # Load module dynamically
    require pgBackRestTest::CommonTest;
    pgBackRestTest::CommonTest->import();

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    # Load the last manifest if it exists
    my $oLastManifest = undef;

    if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR}))
    {
        my $oExpectedManifest =
            new pgBackRest::Manifest(
                $self->{oFile}->pathGet(
                    PATH_BACKUP_CLUSTER,
                    ($strBackup eq 'latest' ? $oHostBackup->backupLast() : $strBackup) .
                    '/'. FILE_MANIFEST), true);

        $oLastManifest =
            new pgBackRest::Manifest(
                $self->{oFile}->pathGet(
                    PATH_BACKUP_CLUSTER,
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} .
                    '/' . FILE_MANIFEST), true);
    }

    # Generate the tablespace map for real backups
    my $oTablespaceMap = undef;

    if (!$self->synthetic())
    {
        # Tablespace_map file is not restored in versions >= 9.5 because it interferes with internal remapping features.
        if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_95)
        {
            delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/tablespace_map'});
        }

        foreach my $strTarget (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}}))
        {
            if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}))
            {
                my $iTablespaceId =
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID};

                $$oTablespaceMap{oid}{$iTablespaceId}{name} =
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_NAME};
            }
        }
    }

    # Generate the actual manifest
    my $strDbClusterPath =
        ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH};

    if (defined($bTablespace) && !$bTablespace)
    {
        foreach my $strTarget (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}}))
        {
            if ($$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TYPE} eq
                MANIFEST_VALUE_LINK &&
                defined($$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}))
            {
                my $strRemapPath;
                my $iTablespaceName =
                    $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_NAME};

                $strRemapPath = "../../tablespace/${iTablespaceName}";

                $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;
                $$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK}{MANIFEST_TARGET_PGDATA . "/${strTarget}"}
                    {&MANIFEST_SUBKEY_DESTINATION} = $strRemapPath;
            }
        }
    }

    my $oActualManifest = new pgBackRest::Manifest("${strTestPath}/" . FILE_MANIFEST, false);

    $oActualManifest->set(
        MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef,
        $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION});
    $oActualManifest->numericSet(
        MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
        $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG});

    my $oTablespaceMapRef = undef;
    $oActualManifest->build($self->{oFile}, $strDbClusterPath, $oLastManifest, false, $oTablespaceMap);

    my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strName ($oActualManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        if (!$self->synthetic())
        {
            $oActualManifest->set(
                MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE,
                ${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{size});
        }

        if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REPO_SIZE}))
        {
            $oActualManifest->numericSet(
                MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE,
                ${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REPO_SIZE});
        }

        if ($oActualManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) != 0)
        {
            my $oStat = fileStat($oActualManifest->dbPathGet($strSectionPath, $strName));

            if ($oStat->blocks > 0 || S_ISLNK($oStat->mode))
            {
                $oActualManifest->set(
                    MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM,
                    $self->{oFile}->hash(PATH_DB_ABSOLUTE, $oActualManifest->dbPathGet($strSectionPath, $strName)));
            }
            else
            {
                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM);
                delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM});
            }
        }
    }

    # If the link section is empty then delete it and the default section
    if (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_LINK}}) == 0)
    {
        delete($$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK});
        delete($$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK . ':default'});
    }

    # Set actual to expected for settings that always change from backup to backup
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE});

    $oActualManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_ID});

    $oActualManifest->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef,
                          ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_VERSION});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE});
    $oActualManifest->set(INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef,
                          ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

    if (!$self->synthetic())
    {
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_START, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LSN_START});
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_STOP, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LSN_STOP});

        if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_START}))
        {
            $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef,
                                  ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_START});
        }

        if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_STOP})
        {
            $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef,
                                  ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_STOP});
        }
    }

    # Delete the list of DBs
    delete($$oExpectedManifestRef{&MANIFEST_SECTION_DB});

    iniSave("${strTestPath}/actual.manifest", $oActualManifest->{oContent});
    iniSave("${strTestPath}/expected.manifest", $oExpectedManifestRef);

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    fileRemove("${strTestPath}/expected.manifest");
    fileRemove("${strTestPath}/actual.manifest");
}

####################################################################################################################################
# mungeInfo
#
# With the file name specified (e.g. archive.info) save the current values within the file into the global variable and replace them
# in the file with the values passed ito the function. Later, using restoreInfo, the global variables will be used to restore the
# file to its original state.
####################################################################################################################################
sub mungeInfo
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $oParam
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->mungeInfo', \@_,
            {name => 'strFileName'},
            {name => 'oParam'}
        );

    my $hContent = {};
    foreach my $hFileContent ( @{$self->{hyInfoFile}} )
    {
        if ($hFileContent->{&SECTION_FILE_NAME} eq $strFileName)
        {
            $hContent = $hFileContent;
            last;
        }
    }
#CSHANG Not sure I like this - I know we need to be sure to clean up, but it would also be nice to be able to manipulate different data without having to reload the file each time, i.e. just overwrite the cached values but then I worry that if we forget to clean up with restoreInfo we could be left in a bad state
    # If the content does not exist, then munge the file
    if (!defined($hContent->{&SECTION_FILE_NAME}))
    {
        # Store the file name
        $hContent->{&SECTION_FILE_NAME} = $strFileName;

        # Change file permissions and load the file contents
        executeTest("sudo chmod 660 ${strFileName}");
        my %hInfo;
        iniLoad($strFileName, \%hInfo);

        # Load params
        foreach my $strSection (sort(keys(%{$oParam})))
        {
            foreach my $strKey (keys(%{$oParam->{$strSection}}))
            {
                # Save the original values
                $hContent->{$strSection}{$strKey} = $hInfo{$strSection}{$strKey};

                # Munge the file with the new values
                $hInfo{$strSection}{$strKey} = $oParam->{$strSection}{$strKey};
            }
        }

        # Cache the original values
        push @{$self->{hyInfoFile}}, $hContent;

        # Save the munged data to the file
        testIniSave($strFileName, \%hInfo, true);
    }
    else
    {
        confess &log(ASSERT, "Cached data already exists for $strFileName. Call restoreInfo before proceeding");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# restoreInfo
#
# With the file name specified (e.g. archive.info) uses the cached variable global to restore the file to its original state after
# modifying the values with mungeInfo.
####################################################################################################################################
sub restoreInfo
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->restoreInfo', \@_,
            {name => 'strFileName'}
        );


    # Load the current file contents
    my %hInfo;
    iniLoad($strFileName, \%hInfo);

    # Find the original values in the cache
    my $hContent = {};
    foreach my $hFileContent ( @{$self->{hyInfoFile}} )
    {
        if ($hFileContent->{&SECTION_FILE_NAME} eq $strFileName)
        {
            $hContent = $hFileContent;
            last;
        }
    }

    # If the content exists, then restore the file
    if (defined($hContent->{&SECTION_FILE_NAME}))
    {
        # Load params
        foreach my $strSection (sort(keys(%{$hContent})))
        {
            if ($strSection ne SECTION_FILE_NAME)
            {
                # If the section to restore exists in the file then check the values
                if (exists($hInfo{$strSection}))
                {
                    # For each value stored in the cache, restore it to the file
                    foreach my $strKey (keys(%{$$hContent{$strSection}}))
                    {
                        if (exists($hInfo{$strSection}{$strKey}))
                        {
                            # Restore the original values
                            $hInfo{$strSection}{$strKey} = $hContent->{$strSection}{$strKey};
                        }
                        else
                        {
                            confess &log(ASSERT, "The cached key does not already exists for $strFileName.");
                        }
                    }
                }
                else
                {
                    confess &log(ASSERT, "The cached section does not already exists for $strFileName.");
                }
            }
        }
        testIniSave($strFileName, \%hInfo, true);

        # Get the index for the cached data to remove
        my ($index) = grep { $self->{hyInfoFile}[$_]->{strFileName} eq $strFileName } (0 ..  @{$self->{hyInfoFile}}-1);

        # Remove the element from the array
        splice(@{$self->{hyInfoFile}}, $index, 1);
    }
    else
    {
        confess &log(ASSERT, "There is no original data cached for $strFileName. You must call mungeInfo first.");
    }

    return;
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub dbPath {return shift->paramGet(HOST_PARAM_DB_PATH);}

sub dbBasePath
{
    my $self = shift;
    my $iIndex = shift;

    return $self->paramGet(HOST_PARAM_DB_BASE_PATH) . (defined($iIndex) ? "-${iIndex}" : '');
}

sub spoolPath {return shift->paramGet(HOST_PARAM_SPOOL_PATH);}

sub tablespacePath
{
    my $self = shift;
    my $iTablespace = shift;
    my $iIndex = shift;

    return
        $self->paramGet(HOST_PARAM_TABLESPACE_PATH) .
        (defined($iTablespace) ? "/ts${iTablespace}" .
        (defined($iIndex) ? "-${iIndex}" : '') : '');
}

1;
