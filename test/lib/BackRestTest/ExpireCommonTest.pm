####################################################################################################################################
# ExpireCommonTest.pm - Common code for expire tests
####################################################################################################################################
package BackRestTest::ExpireCommonTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::BackupCommon;
use BackRest::BackupInfo;
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Config::Config;
use BackRest::File;
use BackRest::Manifest;

use BackRestTest::Common::ExecuteTest;
use BackRestTest::CommonTest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_EXPIRE_COMMON_TEST                                  => 'ExpireCommonTest';

use constant OP_EXPIRE_COMMON_TEST_ARCHIVE_CREATE                   => OP_EXPIRE_COMMON_TEST . "->archiveCreate";
use constant OP_EXPIRE_COMMON_TEST_ARCHIVE_NEXT                     => OP_EXPIRE_COMMON_TEST . "->archiveNext";
use constant OP_EXPIRE_COMMON_TEST_BACKUP_CREATE                    => OP_EXPIRE_COMMON_TEST . "->backupCreate";
use constant OP_EXPIRE_COMMON_TEST_NEW                              => OP_EXPIRE_COMMON_TEST . "->new";
use constant OP_EXPIRE_COMMON_TEST_PROCESS                          => OP_EXPIRE_COMMON_TEST . "->process";
use constant OP_EXPIRE_COMMON_TEST_STANZA_CREATE                    => OP_EXPIRE_COMMON_TEST . "->stanzaCreate";
use constant OP_EXPIRE_COMMON_TEST_SUPPLEMENTAL_LOG                 => OP_EXPIRE_COMMON_TEST . "->supplementalLog";

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oFile},
        $self->{oLogTest}
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_NEW, \@_,
            {name => 'oFile', trace => true},
            {name => 'oLogTest', required => false, trace => true}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# stanzaCreate
####################################################################################################################################
sub stanzaCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strDbVersion
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_STANZA_CREATE, \@_,
            {name => 'strStanza'},
            {name => 'strDbVersion'}
        );

    # Assign variables
    my $oStanza = {};
    $$oStanza{strDbVersion} = $strDbVersion;
    $$oStanza{iDbId} = 1;

    # Create the system id
    my $strDbVersionTemp = $strDbVersion;
    $strDbVersionTemp =~ s/\.//;
    $$oStanza{ullDbSysId} = $strDbVersionTemp . '000000000000000' . $$oStanza{iDbId};
    $$oStanza{iCatalogVersion} = '20' . $strDbVersionTemp . '0101';
    $$oStanza{iControlVersion} = $strDbVersionTemp . '1';

    # Create the stanza backup path
    my $strBackupClusterPath = BackRestTestCommon_RepoPathGet() . '/backup';
    BackRestTestCommon_PathCreate($strBackupClusterPath, undef, true);

    $strBackupClusterPath .= "/${strStanza}";
    BackRestTestCommon_PathCreate($strBackupClusterPath, undef, true);

    $$oStanza{strBackupClusterPath} = $strBackupClusterPath;

    # Create the backup info object
    my $oBackupInfo = new BackRest::BackupInfo($$oStanza{strBackupClusterPath});

    $oBackupInfo->check($$oStanza{strDbVersion}, $$oStanza{iControlVersion}, $$oStanza{iCatalogVersion}, $$oStanza{ullDbSysId});
    $oBackupInfo->save();

    # Create the stanza archive path
    my $strArchiveClusterPath = BackRestTestCommon_RepoPathGet() . '/archive';
    BackRestTestCommon_PathCreate($strArchiveClusterPath, undef, true);

    $strArchiveClusterPath .= "/${strStanza}";
    BackRestTestCommon_PathCreate($strArchiveClusterPath, undef, true);

    # Create the archive info object
    $$oStanza{oArchiveInfo} = new BackRest::ArchiveInfo($strArchiveClusterPath);
    $$oStanza{oArchiveInfo}->check($$oStanza{strDbVersion}, $$oStanza{ullDbSysId});

    # Create the stanza archive version path
    $strArchiveClusterPath .= '/' . $$oStanza{strDbVersion} . '-' . $$oStanza{iDbId};
    BackRestTestCommon_PathCreate($strArchiveClusterPath, undef, true);

    $$oStanza{strArchiveClusterPath} = $strArchiveClusterPath;

    $self->{oStanzaHash}{$strStanza} = $oStanza;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# backupCreate
####################################################################################################################################
sub backupCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strType,
        $lTimestamp,
        $iArchiveBackupTotal,
        $iArchiveBetweenTotal
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_BACKUP_CREATE, \@_,
            {name => 'strStanza'},
            {name => 'strType'},
            {name => 'lTimestamp'},
            {name => 'iArchiveBackupTotal', default => 3},
            {name => 'iArchiveBetweenTotal', default => 3}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    my ($strArchiveStart, $strArchiveStop) = $self->archiveCreate($strStanza, $iArchiveBackupTotal);

    # Create the manifest
    my $oLastManifest = $strType ne BACKUP_TYPE_FULL ? $$oStanza{oManifest} : undef;

    my $strBackupLabel =
        backupLabelFormat($strType,
                          defined($oLastManifest) ? $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) : undef,
                          $lTimestamp);

    my $strBackupClusterSetPath .= "$$oStanza{strBackupClusterPath}/${strBackupLabel}";
    BackRestTestCommon_PathCreate($strBackupClusterSetPath);

    my $oManifest = new BackRest::Manifest("$strBackupClusterSetPath/" . FILE_MANIFEST, false);

    # Store information about the backup into the backup section
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef, true);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef, false);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, true);
    $oManifest->numericSet(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, BACKREST_FORMAT);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, false);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, true);
    $oManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestamp);
    $oManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestamp);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oManifest->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, BACKREST_VERSION);

    if ($strType ne BACKUP_TYPE_FULL)
    {
        if (!defined($oLastManifest))
        {
            confess &log(ERROR, "oLastManifest must be defined when strType = ${strType}");
        }

        push(my @stryReference, $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));

        $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, $stryReference[0]);
    }

    $oManifest->save();
    $$oStanza{oManifest} = $oManifest;

    # Add the backup to info
    my $oBackupInfo = new BackRest::BackupInfo($$oStanza{strBackupClusterPath});

    $oBackupInfo->check($$oStanza{strDbVersion}, $$oStanza{iControlVersion}, $$oStanza{iCatalogVersion}, $$oStanza{ullDbSysId});
    $oBackupInfo->add($self->{oFile}, $oManifest);

    # Create the backup description string
    if (defined($$oStanza{strBackupDescription}))
    {
        $$oStanza{strBackupDescription} .= "\n";
    }

    $$oStanza{strBackupDescription} .=
        "* ${strType} backup: label = ${strBackupLabel}" .
        (defined($oLastManifest) ? ', prior = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) : '') .
        ", start = ${strArchiveStart}, stop = ${strArchiveStop}";

    $self->archiveCreate($strStanza, $iArchiveBetweenTotal);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# archiveNext
####################################################################################################################################
sub archiveNext
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchive,
        $bSkipFF
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_ARCHIVE_NEXT, \@_,
            {name => 'strArchive', trace => true},
            {name => 'bSkipFF', trace => true}
        );

    # Break archive log into components
    my $lTimeline = hex(substr($strArchive, 0, 8));
    my $lMajor = hex(substr($strArchive, 8, 8));
    my $lMinor = hex(substr($strArchive, 16, 8));

    # Increment the minor component (and major when needed)
    $lMinor += 1;

    if ($bSkipFF && $lMinor == 255 || !$bSkipFF && $lMinor == 256)
    {
        $lMajor += 1;
        $lMinor = 0;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveNext', value => uc(sprintf("%08x%08x%08x", $lTimeline, $lMajor, $lMinor)), trace => true}
    );
}

####################################################################################################################################
# archiveCreate
####################################################################################################################################
sub archiveCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $iArchiveTotal
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_ARCHIVE_CREATE, \@_,
            {name => 'strStanza'},
            {name => 'iArchiveTotal'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};
    my $iArchiveIdx = 0;
    my $bSkipFF = $$oStanza{strDbVersion} <= 9.2;

    my $strArchive = defined($$oStanza{strArchiveLast}) ? $self->archiveNext($$oStanza{strArchiveLast}, $bSkipFF) :
                                                          '000000010000000000000000';

    push(my @stryArchive, $strArchive);


    do
    {
        my $strPath = "$$oStanza{strArchiveClusterPath}/" . substr($strArchive, 0, 16);
        BackRestTestCommon_PathCreate($strPath, undef, true);

        my $strFile = "${strPath}/${strArchive}-0000000000000000000000000000000000000000" . ($iArchiveIdx % 2 == 0 ? '.gz' : '');
        BackRestTestCommon_FileCreate($strFile, 'ARCHIVE');

        $iArchiveIdx++;

        if ($iArchiveIdx < $iArchiveTotal)
        {
            $strArchive = $self->archiveNext($strArchive, $bSkipFF);
        }
    }
    while ($iArchiveIdx < $iArchiveTotal);

    push(@stryArchive, $strArchive);
    $$oStanza{strArchiveLast} = $strArchive;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchive', value => \@stryArchive}
    );
}

####################################################################################################################################
# supplementalLog
####################################################################################################################################
sub supplementalLog
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_SUPPLEMENTAL_LOG, \@_,
            {name => 'strStanza'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    if (defined($self->{oLogTest}))
    {
        $self->{oLogTest}->supplementalAdd(BackRestTestCommon_RepoPathGet() .
                                           "/backup/${strStanza}/backup.info", undef, $$oStanza{strBackupDescription});

        executeTest('ls ' . BackRestTestCommon_RepoPathGet() . "/backup/${strStanza} | grep -v \"backup.info\"",
                    {oLogTest => $self->{oLogTest}});
        executeTest('ls -R ' . BackRestTestCommon_RepoPathGet() . "/archive/${strStanza} | grep -v \"archive.info\"",
                    {oLogTest => $self->{oLogTest}});
    }

    return logDebugReturn($strOperation);
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $iExpireFull,
        $iExpireDiff,
        $strExpireArchiveType,
        $iExpireArchive,
        $strDescription
    ) =
        logDebugParam
        (
            OP_EXPIRE_COMMON_TEST_PROCESS, \@_,
            {name => 'strStanza'},
            {name => 'iExpireFull'},
            {name => 'iExpireDiff'},
            {name => 'strExpireArchiveType'},
            {name => 'iExpireArchive'},
            {name => 'strDescription'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    $self->supplementalLog($strStanza);

    undef($$oStanza{strBackupDescription});

    my $strCommand = BackRestTestCommon_CommandMainGet() .
                     ' "--' . OPTION_CONFIG . '=' . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf"' .
                     ' --' . OPTION_STANZA . '=' . $strStanza .
                     ' --' . OPTION_LOG_LEVEL_CONSOLE . '=' . lc(INFO);

    if (defined($iExpireFull))
    {
        $strCommand .= ' --retention-full=' . $iExpireFull;
    }

    if (defined($iExpireDiff))
    {
        $strCommand .= ' --retention-diff=' . $iExpireDiff;
    }

    if (defined($strExpireArchiveType))
    {
        $strCommand .= ' --retention-archive-type=' . $strExpireArchiveType .
                       ' --retention-archive=' . $iExpireArchive;
    }

    $strCommand .= ' expire';

    executeTest($strCommand, {strComment => $strDescription, oLogTest => $self->{oLogTest}});

    $self->supplementalLog($strStanza);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
