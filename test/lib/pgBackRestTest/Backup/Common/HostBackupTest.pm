####################################################################################################################################
# HostBackupTest.pm - Backup host
####################################################################################################################################
package pgBackRestTest::Backup::Common::HostBackupTest;
use parent 'pgBackRestTest::Backup::Common::HostBaseTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::BackupCommon;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Version;

use pgBackRestTest::Backup::Common::HostBaseTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::CommonTest;

####################################################################################################################################
# Host constants
####################################################################################################################################
use constant HOST_BACKUP_USER                                       => 'backup-user';
    push @EXPORT, qw(HOST_BACKUP_USER);

####################################################################################################################################
# Host parameters
####################################################################################################################################
use constant HOST_PARAM_BACKREST_CONFIG                             => 'backrest-config';
    push @EXPORT, qw(HOST_PARAM_BACKREST_CONFIG);
use constant HOST_PARAM_BACKREST_EXE                                => 'backrest-exe';
    push @EXPORT, qw(HOST_PARAM_BACKREST_EXE);
use constant HOST_PARAM_LOCK_PATH                                   => 'lock-path';
    push @EXPORT, qw(HOST_PARAM_LOCK_PATH);
use constant HOST_PARAM_LOG_PATH                                    => 'log-path';
    push @EXPORT, qw(HOST_PARAM_LOG_PATH);
use constant HOST_PARAM_REPO_PATH                                   => 'repo-path';
    push @EXPORT, qw(HOST_PARAM_REPO_PATH);
use constant HOST_PARAM_STANZA                                      => 'stanza';
    push @EXPORT, qw(HOST_PARAM_STANZA);
use constant HOST_PARAM_PROCESS_MAX                                 => 'process-max';
    push @EXPORT, qw(HOST_PARAM_PROCESS_MAX);

####################################################################################################################################
# Host paths
####################################################################################################################################
use constant HOST_PATH_LOCK                                         => 'lock';
    push @EXPORT, qw(HOST_PATH_LOCK);
use constant HOST_PATH_LOG                                          => 'log';
    push @EXPORT, qw(HOST_PATH_LOG);
use constant HOST_PATH_REPO                                         => 'repo';
    push @EXPORT, qw(HOST_PATH_REPO);

####################################################################################################################################
# Backup Defaults
####################################################################################################################################
use constant HOST_STANZA                                            => 'db';
    push @EXPORT, qw(HOST_STANZA);
use constant HOST_PROTOCOL_TIMEOUT                                  => 10;
    push @EXPORT, qw(HOST_PROTOCOL_TIMEOUT);

####################################################################################################################################
# Cached data sections
####################################################################################################################################
use constant SECTION_FILE_NAME                                      => 'strFileName';
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

    # If params are not passed
    my $oHostGroup = hostGroupGet();
    my ($strName, $strImage, $strUser, $strVm);

    if (!defined($$oParam{strName}) || $$oParam{strName} eq HOST_BACKUP)
    {
        $strName = HOST_BACKUP;
        $strImage = containerNamespace() . '/' . $oHostGroup->paramGet(HOST_PARAM_VM) . '-backup-test-pre';
        $strUser = $oHostGroup->paramGet(HOST_BACKUP_USER);
        $strVm = $oHostGroup->paramGet(HOST_PARAM_VM);
    }
    else
    {
        $strName = $$oParam{strName};
        $strImage = $$oParam{strImage};
        $strUser = $$oParam{strUser};
        $strVm = $$oParam{strVm};
    }

    # Create the host
    my $self = $class->SUPER::new($strName, {strImage => $strImage, strUser => $strUser, strVm => $strVm});
    bless $self, $class;

    # Set parameters
    $self->paramSet(
        HOST_PARAM_REPO_PATH, $oHostGroup->paramGet(HOST_PARAM_TEST_PATH) . "/$$oParam{strBackupDestination}/" . HOST_PATH_REPO);

    if ($$oParam{strBackupDestination} eq $self->nameGet())
    {
        $self->paramSet(HOST_PARAM_LOG_PATH, $self->repoPath() . '/' . HOST_PATH_LOG);
        $self->paramSet(HOST_PARAM_LOCK_PATH, $self->repoPath() . '/' . HOST_PATH_LOCK);
        filePathCreate($self->repoPath(), '0770');
    }

    $self->paramSet(HOST_PARAM_BACKREST_CONFIG, $self->testPath() . '/' . BACKREST_CONF);
    $self->paramSet(HOST_PARAM_BACKREST_EXE, $oHostGroup->paramGet(HOST_PARAM_BACKREST_EXE));
    $self->paramSet(HOST_PARAM_STANZA, HOST_STANZA);
    $self->paramSet(HOST_PARAM_PROCESS_MAX, $oHostGroup->paramGet(HOST_PARAM_PROCESS_MAX));

    # Set LogTest object
    $self->{oLogTest} = $$oParam{oLogTest};

    # Set synthetic
    $self->{bSynthetic} = defined($$oParam{bSynthetic}) && $$oParam{bSynthetic} ? true : false;

    # Set the backup destination
    $self->{strBackupDestination} = $$oParam{strBackupDestination};

    # Default hardlink to false
    $self->{bHardLink} = false;

    # Create the local file object
    $self->{oFile} = new pgBackRest::File(
        $self->stanza(),
        $self->repoPath(),
        new pgBackRest::Protocol::Common
        (
            OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
            OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
            OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
            HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
        ));

    # Create a placeholder hash for file munging
    $self->{hInfoFile} = {};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# backupBegin
####################################################################################################################################
sub backupBegin
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupBegin', \@_,
            {name => 'strType', trace => true},
            {name => 'strComment', trace => true},
            {name => 'oParam', required => false, trace => true},
        );

    # Set defaults
    my $strTest = defined($$oParam{strTest}) ? $$oParam{strTest} : undef;
    my $fTestDelay = defined($$oParam{fTestDelay}) ? $$oParam{fTestDelay} : .2;
    my $oExpectedManifest = defined($$oParam{oExpectedManifest}) ? $$oParam{oExpectedManifest} : undef;

    $strComment =
        "${strType} backup" . (defined($strComment) ? " - ${strComment}" : '') .
        ' (' . $self->nameGet() . ' host)';

    &log(INFO, "    $strComment");

    # Execute the backup command
    my $oExecuteBackup = $self->execute(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($oExpectedManifest) ? " --no-online" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        (defined($$oParam{bStandby}) && $$oParam{bStandby} ? " --backup-standby" : '') .
        (defined($oParam->{bRepoLink}) && !$oParam->{bRepoLink} ? ' --no-repo-link' : '') .
        ($strType ne 'incr' ? " --type=${strType}" : '') .
        ' --stanza=' . (defined($oParam->{strStanza}) ? $oParam->{strStanza} : $self->stanza()) . ' backup' .
        (defined($strTest) ? " --test --test-delay=${fTestDelay} --test-point=" . lc($strTest) . '=y' : ''),
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus},
         oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    $oExecuteBackup->begin();

    # Return at the test point if one was defined
    if (defined($strTest))
    {
        $oExecuteBackup->end($strTest);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oExecuteBackup', value => $oExecuteBackup, trace => true},
    );
}

####################################################################################################################################
# backupEnd
####################################################################################################################################
sub backupEnd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $oExecuteBackup,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupEnd', \@_,
            {name => 'strType', trace => true},
            {name => 'oExecuteBackup', trace => true},
            {name => 'oParam', required => false, trace => true},
        );

    # Set defaults
    my $oExpectedManifest = defined($$oParam{oExpectedManifest}) ? $$oParam{oExpectedManifest} : undef;

    my $iExitStatus = $oExecuteBackup->end();

    return if ($oExecuteBackup->{iExpectedExitStatus} != 0);

    # If an alternate stanza was specified
    if (defined($oParam->{strStanza}))
    {
        confess &log(ASSERT,
            'if an alternate stanza is specified it must generate an error - the remaining code will not be aware of the stanza');
    }

    my $strBackup = $self->backupLast();

    # If a real backup then load the expected manifest from the actual manifest.  An expected manifest can't be generated perfectly
    # because a running database is always in flux.  Even so, it allows us test many things.
    if (!$self->synthetic())
    {
        $oExpectedManifest =
            iniLoad($self->repoPath() . '/backup/' . $self->stanza() . "/${strBackup}/" . FILE_MANIFEST, $oExpectedManifest);
    }

    # Make sure tablespace links are correct
    if ($strType eq BACKUP_TYPE_FULL || $self->hardLink())
    {
        my $hTablespaceManifest = $self->{oFile}->manifest(
            PATH_BACKUP_CLUSTER, "${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC);

        # Remove . and ..
        delete($hTablespaceManifest->{'.'});
        delete($hTablespaceManifest->{'..'});

        # Only check links if repo links are disabled
        if (!defined($oParam->{bRepoLink}) || $oParam->{bRepoLink})
        {
            # Iterate file links
            for my $strFile (sort(keys(%{$hTablespaceManifest})))
            {
                # Make sure the link is in the expected manifest
                my $hManifestTarget =
                    $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGTBLSPC . "/${strFile}"};

                if (!defined($hManifestTarget) || $hManifestTarget->{&MANIFEST_SUBKEY_TYPE} ne MANIFEST_VALUE_LINK ||
                    $hManifestTarget->{&MANIFEST_SUBKEY_TABLESPACE_ID} ne $strFile)
                {
                    confess &log(ERROR, "'${strFile}' is not in expected manifest as a link with the correct tablespace id");
                }

                # Make sure the link really is a link
                if ($hTablespaceManifest->{$strFile}{type} ne 'l')
                {
                    confess &log(ERROR, "'${strFile}' in tablespace directory is not a link");
                }

                # Make sure the link destination is correct
                my $strLinkDestination = '../../' . MANIFEST_TARGET_PGTBLSPC . "/${strFile}";

                if ($hTablespaceManifest->{$strFile}{link_destination} ne $strLinkDestination)
                {
                    confess &log(ERROR,
                        "'${strFile}' link should reference '${strLinkDestination}' but actually references " .
                        "'$hTablespaceManifest->{$strFile}{link_destination}'");
                }
            }

            # Iterate manifest targets
            for my $strTarget (sort(keys(%{$oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}})))
            {
                my $hManifestTarget = $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget};
                my $strTablespaceId = $hManifestTarget->{&MANIFEST_SUBKEY_TABLESPACE_ID};

                # Make sure the target exists as a link on disk
                if ($hManifestTarget->{&MANIFEST_SUBKEY_TYPE} eq MANIFEST_VALUE_LINK && defined($strTablespaceId) &&
                    !defined($hTablespaceManifest->{$strTablespaceId}))
                {
                    confess &log(ERROR,
                        "target '${strTarget}' does not have a link at '" . DB_PATH_PGTBLSPC. "/${strTablespaceId}'");
                }
            }
        }
        # Else make sure the tablespace directory is empty
        elsif (keys(%{$hTablespaceManifest}) > 0)
        {
            confess &log(ERROR, DB_PATH_PGTBLSPC . ' directory should be empty when --no-' . OPTION_REPO_LINK);
        }
    }
    # Else there should not be a tablespace directory at all
    elsif ($self->{oFile}->exists(PATH_BACKUP_CLUSTER, "${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC))
    {
        confess &log(ERROR, 'backup must be full or hard-linked to have ' . DB_PATH_PGTBLSPC . ' directory');
    }

    # Check that latest link exists unless repo links are disabled
    my $strLatestLink = $self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, LINK_LATEST);
    my $bLatestLinkExists = fileExists($strLatestLink);

    if (!defined($oParam->{bRepoLink}) || $oParam->{bRepoLink})
    {
        my $strLatestLinkDestination = readlink($strLatestLink);

        if ($strLatestLinkDestination ne $strBackup)
        {
            confess &log(ERROR, "'" . LINK_LATEST . "' link should be '${strBackup}' but is '${strLatestLinkDestination}");
        }
    }
    elsif ($bLatestLinkExists)
    {
        confess &log(ERROR, "'" . LINK_LATEST . "' link should not exist when --no-" . OPTION_REPO_LINK);
    }

    # Only do compare for synthetic backups since for real backups the expected manifest *is* the actual manifest.
    if ($self->synthetic())
    {
        # Set backup type in the expected manifest
        ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE} = $strType;

        $self->backupCompare($strBackup, $oExpectedManifest);
    }

    # Add files to expect log
    if (defined($self->{oLogTest}) && (!defined($$oParam{bSupplemental}) || $$oParam{bSupplemental}))
    {
        my $oHostGroup = hostGroupGet();

        if (defined($oHostGroup->hostGet(HOST_DB_MASTER, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_DB_MASTER)->testPath() . '/' . BACKREST_CONF);
        }

        if (defined($oHostGroup->hostGet(HOST_DB_STANDBY, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_DB_STANDBY)->testPath() . '/' . BACKREST_CONF);
        }

        if (defined($oHostGroup->hostGet(HOST_BACKUP, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_BACKUP)->testPath() . '/' . BACKREST_CONF);
        }

        if ($self->synthetic())
        {
            $self->{oLogTest}->supplementalAdd($self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, "${strBackup}/" . FILE_MANIFEST));
            $self->{oLogTest}->supplementalAdd($self->repoPath() . '/backup/' . $self->stanza() . '/backup.info');
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackup', value => $strBackup, trace => true},
    );
}

####################################################################################################################################
# backup
####################################################################################################################################
sub backup
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backup', \@_,
            {name => 'strType'},
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    my $oExecuteBackup = $self->backupBegin($strType, $strComment, $oParam);
    my $strBackup = $self->backupEnd($strType, $oExecuteBackup, $oParam);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackup', value => $strBackup},
    );
}

####################################################################################################################################
# backupCompare
####################################################################################################################################
sub backupCompare
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $oExpectedManifest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupCompare', \@_,
            {name => 'strBackup', trace => true},
            {name => 'oExpectedManifest', trace => true},
        );

    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL} = $strBackup;

    my $oActualManifest = new pgBackRest::Common::Ini(
        $self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, "${strBackup}/" . FILE_MANIFEST));

    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, &MANIFEST_KEY_TIMESTAMP_START);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} =
        $oActualManifest->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_FORMAT} = BACKREST_FORMAT + 0;

    foreach my $strPathKey ($oActualManifest->keys(MANIFEST_SECTION_TARGET_PATH))
    {
        my $strFileSection = MANIFEST_SECTION_TARGET_FILE;

        foreach my $strFileKey ($oActualManifest->keys($strFileSection))
        {
            if ($oActualManifest->test($strFileSection, $strFileKey, MANIFEST_SUBKEY_REPO_SIZE))
            {
                ${$oExpectedManifest}{$strFileSection}{$strFileKey}{&MANIFEST_SUBKEY_REPO_SIZE} =
                    $oActualManifest->get($strFileSection, $strFileKey, MANIFEST_SUBKEY_REPO_SIZE);
            }
        }
    }

    # Set defaults for subkeys that tend to repeat
    foreach my $strSection (&MANIFEST_SECTION_TARGET_FILE, &MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
    {
        foreach my $strSubKey (&MANIFEST_SUBKEY_USER, &MANIFEST_SUBKEY_GROUP, &MANIFEST_SUBKEY_MODE, &MANIFEST_SUBKEY_MASTER)
        {
            my %oDefault;
            my $iSectionTotal = 0;

            foreach my $strFile (keys(%{${$oExpectedManifest}{$strSection}}))
            {
                if (!defined(${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey}) &&
                    defined(${$oExpectedManifest}{"${strSection}:default"}{$strSubKey}))
                {
                    ${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey} =
                        ${$oExpectedManifest}{"${strSection}:default"}{$strSubKey};
                }

                my $strValue = ${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey};

                if (defined($strValue))
                {
                    if (defined($oDefault{$strValue}))
                    {
                        $oDefault{$strValue}++;
                    }
                    else
                    {
                        $oDefault{$strValue} = 1;
                    }
                }

                $iSectionTotal++;
            }

            my $strMaxValue;
            my $iMaxValueTotal = 0;

            foreach my $strValue (keys(%oDefault))
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
                    ${$oExpectedManifest}{"${strSection}:default"}{$strSubKey} = $strMaxValue ? JSON::PP::true : JSON::PP::false;
                }
                else
                {
                    ${$oExpectedManifest}{"${strSection}:default"}{$strSubKey} = $strMaxValue;
                }

                foreach my $strFile (keys(%{${$oExpectedManifest}{$strSection}}))
                {
                    if (defined(${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey}) &&
                        ${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey} eq $strMaxValue)
                    {
                        delete(${$oExpectedManifest}{$strSection}{$strFile}{$strSubKey});
                    }
                }
            }
        }
    }

    my $strTestPath = $self->testPath();

    iniSave("${strTestPath}/actual.manifest", $oActualManifest->{oContent});
    iniSave("${strTestPath}/expected.manifest", $oExpectedManifest);

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    fileRemove("${strTestPath}/expected.manifest");
    fileRemove("${strTestPath}/actual.manifest");

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# backupLast
####################################################################################################################################
sub backupLast
{
    my $self = shift;

    my @stryBackup = $self->{oFile}->list(
        PATH_BACKUP_CLUSTER, undef, '[0-9]{8}-[0-9]{6}F(_[0-9]{8}-[0-9]{6}(D|I)){0,1}', 'reverse');

    if (!defined($stryBackup[0]))
    {
        confess 'no backup was found: ' . join(@stryBackup, ', ');
    }

    return $stryBackup[0];
}

####################################################################################################################################
# check
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'check ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --log-level-console=detail' .
        (defined($$oParam{iTimeout}) ? " --archive-timeout=$$oParam{iTimeout}" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' --stanza=' . $self->stanza() . ' check',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# expire
####################################################################################################################################
sub expire
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'expire' .
        (defined($$oParam{iRetentionFull}) ? " full=$$oParam{iRetentionFull}" : '') .
        (defined($$oParam{iRetentionDiff}) ? " diff=$$oParam{iRetentionDiff}" : '') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Determine whether or not to expect an error
    my $oHostGroup = hostGroupGet();

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --log-level-console=detail' .
        (defined($$oParam{iRetentionFull}) ? " --retention-full=$$oParam{iRetentionFull}" : '') .
        (defined($$oParam{iRetentionDiff}) ? " --retention-diff=$$oParam{iRetentionDiff}" : '') .
        '  --stanza=' . $self->stanza() . ' expire',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'info' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --log-level-console=warn' .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') .
        (defined($$oParam{strOutput}) ? " --output=$$oParam{strOutput}" : '') . ' info',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-create ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{iTimeout}) ? " --archive-timeout=$$oParam{iTimeout}" : '') .
        ' --stanza=' . $self->stanza() . ' stanza-create',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# start
####################################################################################################################################
sub start
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->start', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'start' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') . ' start',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});
}

####################################################################################################################################
# stop
####################################################################################################################################
sub stop
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stop', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'stop' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') .
        (defined($$oParam{bForce}) && $$oParam{bForce} ? ' --force' : '') . ' stop',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# configCreate
####################################################################################################################################
sub configCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stop', \@_,
            {name => 'oParam', required => false},
        );

    my %oParamHash;
    my $strStanza = $self->stanza();
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = $oHostGroup->hostGet($self->backupDestination());
    my $oHostDbMaster = $oHostGroup->hostGet(HOST_DB_MASTER);
    my $oHostDbStandby = $oHostGroup->hostGet(HOST_DB_STANDBY, true);

    my $bArchiveAsync = defined($$oParam{bArchiveAsync}) ? $$oParam{bArchiveAsync} : false;

    # General options
    # ------------------------------------------------------------------------------------------------------------------------------
    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_CONSOLE} = lc(DEBUG);
    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_FILE} = lc(TRACE);
    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_STDERR} = lc(OFF);

    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_REPO_PATH} = $self->repoPath();
    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_PATH} = $self->logPath();
    $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOCK_PATH} = $self->lockPath();

    if ($self->processMax() > 1)
    {
        $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_PROCESS_MAX} = $self->processMax();
    }

    if (defined($$oParam{bCompress}) && !$$oParam{bCompress})
    {
        $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_COMPRESS} = 'n';
    }

    if ($self->isHostBackup())
    {
        if (defined($$oParam{bHardlink}) && $$oParam{bHardlink})
        {
            $self->{bHardLink} = true;
            $oParamHash{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_HARDLINK} = 'y';
        }

        $oParamHash{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_BACKUP_ARCHIVE_COPY} = 'y';
        $oParamHash{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_START_FAST} = 'y';
    }

    # Host specific options
    # ------------------------------------------------------------------------------------------------------------------------------

    # If this is the backup host
    if ($self->isHostBackup())
    {
        my $bForce = false;
        my $oHostDb1 = $oHostDbMaster;
        my $oHostDb2 = $oHostDbStandby;

        if ($self->nameTest(HOST_BACKUP))
        {
            $bForce = defined($oHostDbStandby);
        }
        elsif ($self->nameTest(HOST_DB_STANDBY))
        {
            $oHostDb1 = $oHostDbStandby;
            $oHostDb2 = $oHostDbMaster;
        }

        if ($self->nameTest(HOST_BACKUP))
        {
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_HOST, 1, $bForce)} = $oHostDb1->nameGet();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_USER, 1, $bForce)} = $oHostDb1->userGet();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_CMD, 1, $bForce)} = $oHostDb1->backrestExe();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_CONFIG, 1, $bForce)} = $oHostDb1->backrestConfig();
        }

        $oParamHash{$strStanza}{optionIndex(OPTION_DB_PATH, 1, $bForce)} = $oHostDb1->dbBasePath();

        if (defined($oHostDb2))
        {
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_HOST, 2)} = $oHostDb2->nameGet();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_USER, 2)} = $oHostDb2->userGet();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_CMD, 2)} = $oHostDb2->backrestExe();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_CONFIG, 2)} = $oHostDb2->backrestConfig();
            $oParamHash{$strStanza}{optionIndex(OPTION_DB_PATH, 2)} = $oHostDb2->dbBasePath();
        }
    }

    # If this is a database host
    if ($self->isHostDb())
    {
        $oParamHash{$strStanza}{&OPTION_DB_PATH} = $self->dbBasePath();

        if (!$self->synthetic())
        {
            $oParamHash{$strStanza}{&OPTION_DB_SOCKET_PATH} = $self->dbSocketPath();
            $oParamHash{$strStanza}{&OPTION_DB_PORT} = $self->dbPort();
        }

        if ($bArchiveAsync)
        {
            $oParamHash{&CONFIG_SECTION_GLOBAL . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_ARCHIVE_ASYNC} = 'y';
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_SPOOL_PATH} = $self->spoolPath();
        }

        # If the the backup host is remote
        if (!$self->isHostBackup())
        {
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_BACKUP_HOST} = $oHostBackup->nameGet();
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_BACKUP_USER} = $oHostBackup->userGet();
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_BACKUP_CMD} = $oHostBackup->backrestExe();
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_BACKUP_CONFIG} = $oHostBackup->backrestConfig();

            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_PATH} = $self->logPath();
            $oParamHash{&CONFIG_SECTION_GLOBAL}{&OPTION_LOCK_PATH} = $self->lockPath();
        }
    }

    # Write out the configuration file
    iniSave($self->backrestConfig(), \%oParamHash, true);
}

####################################################################################################################################
# manifestMunge
#
# Allows for munging of the manifest while making it appear to be valid.  This is used to create various error conditions that
# should be caught by the unit tests.
####################################################################################################################################
sub manifestMunge
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $strSection,
        $strKey,
        $strSubKey,
        $strValue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestMunge', \@_,
            {name => '$strBackup'},
            {name => '$strSection'},
            {name => '$strKey'},
            {name => '$strSubKey', required => false},
            {name => '$strValue', required => false},
        );

    my $strManifestFile = "${strBackup}/" . FILE_MANIFEST;

    # Change mode on the backup path so it can be read/written
    if ($self->nameTest(HOST_BACKUP))
    {
        executeTest('sudo chmod g+w ' . $self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, $strManifestFile));
    }

    # Read the manifest
    my %oManifest;
    iniLoad($self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, $strManifestFile), \%oManifest);

    # Write in the munged value
    if (defined($strSubKey))
    {
        if (defined($strValue))
        {
            $oManifest{$strSection}{$strKey}{$strSubKey} = $strValue;
        }
        else
        {
            delete($oManifest{$strSection}{$strKey}{$strSubKey});
        }
    }
    else
    {
        if (defined($strValue))
        {
            $oManifest{$strSection}{$strKey} = $strValue;
        }
        else
        {
            delete($oManifest{$strSection}{$strKey});
        }
    }

    # Remove the old checksum
    delete($oManifest{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

    my $oSHA = Digest::SHA->new('sha1');
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
    $oSHA->add($oJSON->encode(\%oManifest));

    # Set the new checksum
    $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} = $oSHA->hexdigest();

    # Resave the manifest
    iniSave($self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, $strManifestFile), \%oManifest);
}

####################################################################################################################################
# infoMunge
#
# With the file name specified (e.g. /repo/archive/db/archive.info) copy the current values from the file into the global hash and
# update the file with the new values passed. Later, using infoRestore, the global variable will be used to restore the file to its
# original state.
####################################################################################################################################
sub infoMunge
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $hParam
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoMunge', \@_,
            {name => 'strFileName'},
            {name => 'hParam'}
        );

    # If the original file content does not exist then load it
    if (!defined($self->{hInfoFile}{$strFileName}))
    {
        $self->{hInfoFile}{$strFileName} = iniLoad($strFileName);
        # Modify the file permissions so it can be saved
        executeTest("sudo chmod 660 ${strFileName}");
    }

    # Make a copy of the original file contents
    my $hContent = dclone($self->{hInfoFile}{$strFileName});

    # Load params
    foreach my $strSection (sort(keys(%{$hParam})))
    {
        foreach my $strKey (keys(%{$$hParam{$strSection}}))
        {
            # Munge the copy with the new parameter values
            $$hContent{$strSection}{$strKey} = $$hParam{$strSection}{$strKey};
        }
    }

    # Save the munged data to the file
    testIniSave($strFileName, \%{$hContent}, true);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# infoRestore
#
# With the file name specified (e.g. /repo/archive/db/archive.info) use the original file contents in the global hash to restore the
# file to its original state after modifying the values with infoMunge.
####################################################################################################################################
sub infoRestore
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
            __PACKAGE__ . '->infoRestore', \@_,
            {name => 'strFileName'}
        );

    # If the original file content exists in the global hash, then save it to the file
    if (defined($self->{hInfoFile}{$strFileName}))
    {
        iniSave($strFileName, $self->{hInfoFile}{$strFileName});
    }
    else
    {
        confess &log(ASSERT, "There is no original data cached for $strFileName. infoMunge must be called first.");
    }

    # Remove the element from the hash
    delete($self->{hInfoFile}{$strFileName});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub backrestConfig {return shift->paramGet(HOST_PARAM_BACKREST_CONFIG);}
sub backupDestination {return shift->{strBackupDestination};}
sub backrestExe {return shift->paramGet(HOST_PARAM_BACKREST_EXE);}
sub hardLink {return shift->{bHardLink};}
sub isHostBackup {my $self = shift; return $self->backupDestination() eq $self->nameGet();}
sub isHostDbMaster {return shift->nameGet() eq HOST_DB_MASTER;}
sub isHostDbStandby {return shift->nameGet() eq HOST_DB_STANDBY;}
sub isHostDb {my $self = shift; return $self->isHostDbMaster() || $self->isHostDbStandby();}
sub lockPath {return shift->paramGet(HOST_PARAM_LOCK_PATH);}
sub logPath {return shift->paramGet(HOST_PARAM_LOG_PATH);}
sub repoPath {return shift->paramGet(HOST_PARAM_REPO_PATH);}
sub stanza {return shift->paramGet(HOST_PARAM_STANZA);}
sub processMax {return shift->paramGet(HOST_PARAM_PROCESS_MAX);}
sub synthetic {return shift->{bSynthetic};}

1;
