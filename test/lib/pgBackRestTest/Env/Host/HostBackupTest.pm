####################################################################################################################################
# HostBackupTest.pm - Backup host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostBackupTest;
use parent 'pgBackRestTest::Env::Host::HostBaseTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl ':mode';
use File::Basename qw(dirname);
use File::stat qw{lstat};
use Storable qw(dclone);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::StorageBase;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::Host::HostAzureTest;
use pgBackRestTest::Env::Host::HostGcsTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Env::Manifest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Error constants
####################################################################################################################################
use constant ERROR_REPO_INVALID                                     => 103;
push @EXPORT, qw(ERROR_REPO_INVALID);

####################################################################################################################################
# Latest backup link constant
####################################################################################################################################
use constant LINK_LATEST                                            => 'latest';
    push @EXPORT, qw(LINK_LATEST);

####################################################################################################################################
# Host defaults
####################################################################################################################################
use constant HOST_PATH_LOCK                                         => 'lock';
    push @EXPORT, qw(HOST_PATH_LOCK);
use constant HOST_PATH_LOG                                          => 'log';
    push @EXPORT, qw(HOST_PATH_LOG);
use constant HOST_PATH_REPO                                         => 'repo';

use constant HOST_PROTOCOL_TIMEOUT                                  => 10;
    push @EXPORT, qw(HOST_PROTOCOL_TIMEOUT);

####################################################################################################################################
# Configuration constants
####################################################################################################################################
use constant CFGDEF_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CFGDEF_SECTION_GLOBAL);
use constant CFGDEF_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CFGDEF_SECTION_STANZA);

use constant CFGOPTVAL_BACKUP_TYPE_FULL                             => 'full';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_FULL);
use constant CFGOPTVAL_BACKUP_TYPE_DIFF                             => 'diff';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_DIFF);
use constant CFGOPTVAL_BACKUP_TYPE_INCR                             => 'incr';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_INCR);

use constant CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC                 => 'aes-256-cbc';
    push @EXPORT, qw(CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);

use constant AZURE                                                  => 'azure';
    push @EXPORT, qw(AZURE);
use constant CIFS                                                   => 'cifs';
    push @EXPORT, qw(CIFS);
use constant GCS                                                    => 'gcs';
    push @EXPORT, qw(GCS);
use constant POSIX                                                  => STORAGE_POSIX;
    push @EXPORT, qw(POSIX);
use constant S3                                                     => 's3';
    push @EXPORT, qw(S3);

use constant CFGOPTVAL_RESTORE_TYPE_DEFAULT                         => 'default';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_DEFAULT);
use constant CFGOPTVAL_RESTORE_TYPE_IMMEDIATE                       => 'immediate';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_IMMEDIATE);
use constant CFGOPTVAL_RESTORE_TYPE_NAME                            => 'name';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_NAME);
use constant CFGOPTVAL_RESTORE_TYPE_PRESERVE                        => 'preserve';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_PRESERVE);
use constant CFGOPTVAL_RESTORE_TYPE_STANDBY                         => 'standby';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_STANDBY);
use constant CFGOPTVAL_RESTORE_TYPE_TIME                            => 'time';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_TIME);
use constant CFGOPTVAL_RESTORE_TYPE_XID                             => 'xid';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_XID);

use constant NONE                                                   => 'none';
    push @EXPORT, qw(NONE);
use constant BZ2                                                    => 'bz2';
    push @EXPORT, qw(BZ2);
use constant GZ                                                     => 'gz';
    push @EXPORT, qw(GZ);
use constant LZ4                                                    => 'lz4';
    push @EXPORT, qw(LZ4);
use constant ZST                                                    => 'zst';
    push @EXPORT, qw(ZST);

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

    my ($strName, $strImage, $strUser);

    if (!defined($$oParam{strName}) || $$oParam{strName} eq HOST_BACKUP)
    {
        $strName = HOST_BACKUP;
        $strImage = containerRepo() . ':' . testRunGet()->vm() . '-test';
    }
    else
    {
        $strName = $$oParam{strName};
        $strImage = $$oParam{strImage};
    }

    $strUser = testRunGet()->pgUser();

    # Create the host
    my $self = $class->SUPER::new($strName, {strImage => $strImage, strUser => $strUser, bTls => $oParam->{bTls}});
    bless $self, $class;

    # If repo is on local filesystem then set the repo-path locally
    if ($oParam->{bRepoLocal})
    {
        $self->{strRepoPath} = $self->testRunGet()->testPath() . "/$$oParam{strBackupDestination}/" . HOST_PATH_REPO;
    }
    # Else on KV store and repo will be in root
    else
    {
        $self->{strRepoPath} = '/';
    }

    # If there is a repo2 it will always be posix on the repo host
    $self->{strRepo2Path} = $self->testRunGet()->testPath() . "/$$oParam{strBackupDestination}/" . HOST_PATH_REPO . "2";

    # Set log/lock paths
    $self->{strLogPath} = $self->testPath() . '/' . HOST_PATH_LOG;
    storageTest()->pathCreate($self->{strLogPath}, {strMode => '0770'});
    $self->{strLockPath} = $self->testPath() . '/' . HOST_PATH_LOCK;

    # Set conf file
    $self->{strBackRestConfig} = $self->testPath() . '/' . PROJECT_CONF;

    # Set synthetic
    $self->{bSynthetic} = defined($$oParam{bSynthetic}) && $$oParam{bSynthetic} ? true : false;

    # Set the backup destination
    $self->{strBackupDestination} = $$oParam{strBackupDestination};

    # Default hardlink to false
    $self->{bHardLink} = false;

    # By default there is no bogus host
    $self->{bBogusHost} = false;

    # Create a placeholder hash for file munging
    $self->{hInfoFile} = {};

    # Set whether repo should be encrypted or not
    $self->{bRepoEncrypt} = defined($$oParam{bRepoEncrypt}) ? $$oParam{bRepoEncrypt} : false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# timestampFileFormat
####################################################################################################################################
sub timestampFileFormat
{
    my $strFormat = shift;
    my $lTime = shift;

    return timestampFormat(defined($strFormat) ? $strFormat : '%4d%02d%02d-%02d%02d%02d', $lTime);
}

push @EXPORT, qw(timestampFileFormat);

####################################################################################################################################
# backupLabelFormat
#
# Format the label for a backup.
####################################################################################################################################
sub backupLabelFormat
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strBackupLabelLast,
        $lTimestampStart
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupLabelFormat', \@_,
            {name => 'strType', trace => true},
            {name => 'strBackupLabelLast', required => false, trace => true},
            {name => 'lTimestampTart', trace => true}
        );

    # Full backup label
    my $strBackupLabel;

    if ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        # Last backup label must not be defined
        if (defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must not be defined when strType = '${strType}'");
        }

        # Format the timestamp and add the full indicator
        $strBackupLabel = timestampFileFormat(undef, $lTimestampStart) . 'F';
    }
    # Else diff or incr label
    else
    {
        # Last backup label must be defined
        if (!defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must be defined when strType = '${strType}'");
        }

        # Get the full backup portion of the last backup label
        $strBackupLabel = substr($strBackupLabelLast, 0, 16);

        # Format the timestamp
        $strBackupLabel .= '_' . timestampFileFormat(undef, $lTimestampStart);

        # Add the diff indicator
        if ($strType eq CFGOPTVAL_BACKUP_TYPE_DIFF)
        {
            $strBackupLabel .= 'D';
        }
        # Else incr indicator
        else
        {
            $strBackupLabel .= 'I';
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackupLabel', value => $strBackupLabel, trace => true}
    );
}

push @EXPORT, qw(backupLabelFormat);

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
        (defined($oParam->{strRepoType}) ? " --repo1-type=$oParam->{strRepoType}" : '') .
        (defined($oParam->{iRepo}) ? ' --repo=' . $oParam->{iRepo} : '') .
        ($strType ne 'incr' ? " --type=${strType}" : '') .
        ' --stanza=' . (defined($oParam->{strStanza}) ? $oParam->{strStanza} : $self->stanza()) . ' backup',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});

    $oExecuteBackup->begin();

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
        $bManifestCompare,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupEnd', \@_,
            {name => 'strType', trace => true},
            {name => 'oExecuteBackup', trace => true},
            {name => 'oParam', required => false, trace => true},
            {name => 'bManifestCompare', required => false, default => true, trace => true},
        );

    # Set defaults
    my $oExpectedManifest = defined($$oParam{oExpectedManifest}) ? dclone($$oParam{oExpectedManifest}) : undef;

    my $iExitStatus = $oExecuteBackup->end();

    return if ($oExecuteBackup->{iExpectedExitStatus} != 0);

    # If an alternate stanza was specified
    if (defined($oParam->{strStanza}))
    {
        confess &log(ASSERT,
            'if an alternate stanza is specified it must generate an error - the remaining code will not be aware of the stanza');
    }

    my $strBackup = $self->backupLast($oParam->{iRepo});

    # Only compare backups that are in repo1. There is not a lot of value in comparing backups in other repos and it would require a
    # lot of changes to the test harness.
    if (!defined($oParam->{iRepo}) || $oParam->{iRepo} == 1)
    {
        # If a real backup then load the expected manifest from the actual manifest. An expected manifest can't be generated
        # perfectly because a running database is always in flux. Even so, it allows us to test many things.
        if (!$self->synthetic())
        {
            $oExpectedManifest = iniParse(
                ${storageRepo()->get(
                    storageRepo()->openRead(
                        'backup/' . $self->stanza() . "/${strBackup}/" . FILE_MANIFEST,
                        {strCipherPass => $self->cipherPassManifest()}))});
        }

        # Make sure tablespace links are correct
        if ($self->hasLink())
        {
            if (($strType eq CFGOPTVAL_BACKUP_TYPE_FULL || $self->hardLink()) &&
                !$oExpectedManifest->{&MANIFEST_SECTION_BACKUP}{'backup-bundle'})
            {
                my $hTablespaceManifest = storageTest()->manifest(
                    $self->repoBackupPath("${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC));

                # Remove . and ..
                delete($hTablespaceManifest->{'.'});
                delete($hTablespaceManifest->{'..'});

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
            # Else there should not be a tablespace directory at all.  This is only valid for storage that supports links.
            elsif (storageRepo()->capability(STORAGE_CAPABILITY_LINK) &&
                   storageTest()->pathExists(
                       $self->repoBackupPath("${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC)))
            {
                confess &log(ERROR, 'backup must be full or hard-linked to have ' . DB_PATH_PGTBLSPC . ' directory');
            }
        }

        # Check that latest link exists unless repo links are disabled
        my $strLatestLink = $self->repoBackupPath(LINK_LATEST);
        my $bLatestLinkExists = storageRepo()->exists($strLatestLink);

        if ((!defined($oParam->{strRepoType}) || $oParam->{strRepoType} eq POSIX) && $self->hasLink())
        {
            my $strLatestLinkDestination = readlink($strLatestLink);

            if ($strLatestLinkDestination ne $strBackup)
            {
                confess &log(ERROR, "'" . LINK_LATEST . "' link should be '${strBackup}' but is '${strLatestLinkDestination}");
            }
        }
        elsif ($bLatestLinkExists)
        {
            confess &log(ERROR, "'" . LINK_LATEST . "' link should not exist");
        }

        # Only do compare for synthetic backups since for real backups the expected manifest *is* the actual manifest.
        if ($self->synthetic())
        {
            # Compare only if expected to do so
            if ($bManifestCompare)
            {
                # Set backup type in the expected manifest
                ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE} = $strType;

                $self->backupCompare($strBackup, $oExpectedManifest);
            }
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
        $bManifestCompare,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backup', \@_,
            {name => 'strType'},
            {name => 'strComment'},
            {name => 'oParam', required => false},
            {name => 'bManifestCompare', required => false, default => true},
        );

    my $oExecuteBackup = $self->backupBegin($strType, $strComment, $oParam);
    my $strBackup = $self->backupEnd($strType, $oExecuteBackup, $oParam, $bManifestCompare);

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

    my $oActualManifest = new pgBackRestTest::Env::Manifest(
        $self->repoBackupPath("${strBackup}/" . FILE_MANIFEST), {strCipherPass => $self->cipherPassManifest()});

    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, &MANIFEST_KEY_TIMESTAMP_START);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} =
        $oActualManifest->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_FORMAT} = REPOSITORY_FORMAT + 0;

    if (defined($oExpectedManifest->{&INI_SECTION_CIPHER}) &&
        defined($oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS}) &&
        $oActualManifest->test(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS))
    {
        $oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS} =
            $oActualManifest->get(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS);
    }

    # Update the expected manifest with whether the --delta option was used or not to perform the backup.
    $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA} =
        $oActualManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA) ? INI_TRUE : INI_FALSE;

    my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strFileKey ($oActualManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # Determine repo size if compression or encryption is enabled
        my $strCompressType = $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_TYPE};

        if ($strCompressType ne NONE ||
            (defined($oExpectedManifest->{&INI_SECTION_CIPHER}) &&
                defined($oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS})))
        {

            my $lRepoSize =
                $oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REFERENCE) ?
                    $oActualManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REPO_SIZE, false) :
                    (storageRepo()->info(
                        $self->repoBackupPath("${strBackup}/${strFileKey}") .
                        ($strCompressType eq NONE ? '' : ".${strCompressType}")))->{size};

            if (defined($lRepoSize) &&
                $lRepoSize != $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_SIZE})
            {
                $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_REPO_SIZE} = $lRepoSize;
            }
        }

        # If the backup does not have page checksums then no need to compare
        if (!$oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE})
        {
            delete($oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE});
            delete($oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR});
        }
        # Else make sure things that should have checks do have checks
        elsif ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_CHECKSUM_PAGE) !=
               isChecksumPage($strFileKey))
        {
            confess
                "check-page actual for ${strFileKey} is " .
                ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey,
                    MANIFEST_SUBKEY_CHECKSUM_PAGE) ? 'set' : '[undef]') .
                ' but isChecksumPage() says it should be ' .
                (isChecksumPage($strFileKey) ? 'set' : 'undef') . '.';
        }
    }

    $self->manifestDefault($oExpectedManifest);

    my $strTestPath = $self->testPath();

    storageTest()->put("${strTestPath}/actual.manifest", iniRender($oActualManifest->{oContent}));
    storageTest()->put("${strTestPath}/expected.manifest", iniRender($oExpectedManifest));

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    storageTest()->remove("${strTestPath}/expected.manifest");
    storageTest()->remove("${strTestPath}/actual.manifest");

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestDefault
####################################################################################################################################
sub manifestDefault
{
    my $self = shift;
    my $oExpectedManifest = shift;

    # Defaults for subkeys that tend to repeat
    my $strDefaultUser = $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_USER};
    my $strDefaultGroup = $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_GROUP};
    my $strDefaultPathMode = $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_MODE};
    my $strDefaultFileMode = sprintf('%04o', oct($strDefaultPathMode) & (S_IRUSR | S_IWUSR | S_IRGRP));

    # Remove subkeys that match the defaults
    foreach my $strSection (&MANIFEST_SECTION_TARGET_FILE, &MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
    {
        next if !defined($oExpectedManifest->{$strSection});

        foreach my $strFile (keys(%{$oExpectedManifest->{$strSection}}))
        {
            if (defined($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_USER}) &&
                $oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_USER} eq $strDefaultUser)
            {
                delete($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_USER});
            }

            if (defined($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_GROUP}) &&
                $oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_GROUP} eq $strDefaultGroup)
            {
                delete($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_GROUP});
            }

            if (defined($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_MODE}) &&
                $oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_MODE} eq
                    ($strSection eq MANIFEST_SECTION_TARGET_PATH ? $strDefaultPathMode : $strDefaultFileMode))
            {
                delete($oExpectedManifest->{$strSection}{$strFile}{&MANIFEST_SUBKEY_MODE});
            }
        }
    }

    # Write defaults
    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE . ':default'}{&MANIFEST_SUBKEY_USER} = $strDefaultUser;
    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE . ':default'}{&MANIFEST_SUBKEY_GROUP} = $strDefaultGroup;
    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE . ':default'}{&MANIFEST_SUBKEY_MODE} = $strDefaultFileMode;

    if (defined($oExpectedManifest->{&MANIFEST_SECTION_TARGET_LINK}))
    {
        $oExpectedManifest->{&MANIFEST_SECTION_TARGET_LINK . ':default'}{&MANIFEST_SUBKEY_USER} = $strDefaultUser;
        $oExpectedManifest->{&MANIFEST_SECTION_TARGET_LINK . ':default'}{&MANIFEST_SUBKEY_GROUP} = $strDefaultGroup;
    }

    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH . ':default'}{&MANIFEST_SUBKEY_USER} = $strDefaultUser;
    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH . ':default'}{&MANIFEST_SUBKEY_GROUP} = $strDefaultGroup;
    $oExpectedManifest->{&MANIFEST_SECTION_TARGET_PATH . ':default'}{&MANIFEST_SUBKEY_MODE} = $strDefaultPathMode;
}

####################################################################################################################################
# backupLast
####################################################################################################################################
sub backupLast
{
    my $self = shift;
    my $iRepo = shift;

    my @stryBackup = storageRepo({iRepo => $iRepo})->list(
        $self->repoBackupPath(undef, $iRepo),
        {strExpression => '[0-9]{8}-[0-9]{6}F(_[0-9]{8}-[0-9]{6}(D|I)){0,1}', strSortOrder => 'reverse'});

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
        (defined($$oParam{iTimeout}) ? " --archive-timeout=$$oParam{iTimeout}" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' --stanza=' . $self->stanza() . ' check',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});

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
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Determine whether or not to expect an error
    my $oHostGroup = hostGroupGet();

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{iRetentionFull}) ? " --repo1-retention-full=$$oParam{iRetentionFull}" : '') .
        (defined($$oParam{iRetentionDiff}) ? " --repo1-retention-diff=$$oParam{iRetentionDiff}" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' --repo=' . (defined($oParam->{iRepo}) ? $oParam->{iRepo} : '1') .
        '  --stanza=' . $self->stanza() . ' expire',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});
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
        {strComment => $strComment, bLogOutput => $self->synthetic()});

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
            __PACKAGE__ . '->stanzaCreate', \@_,
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
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-create',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});

    if (storageRepo()->exists('backup/' . $self->stanza() . qw{/} . FILE_BACKUP_INFO))
    {
        # Get the passphrase for accessing the manifest file
        $self->{strCipherPassManifest} = (new pgBackRestTest::Env::BackupInfo($self->repoBackupPath()))->cipherPassSub();
    }

    if (storageRepo()->exists('archive/' . $self->stanza() . qw{/} . ARCHIVE_INFO_FILE))
    {

        # Get the passphrase for accessing the archived files
        $self->{strCipherPassArchive} =
            (new pgBackRestTest::Env::ArchiveInfo($self->repoArchivePath()))->cipherPassSub();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaUpgrade
####################################################################################################################################
sub stanzaUpgrade
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
            __PACKAGE__ . '->stanzaUpgrade', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-upgrade ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-upgrade',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaDelete
####################################################################################################################################
sub stanzaDelete
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
            __PACKAGE__ . '->stanzaDelete', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-delete ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --repo=' . (defined($oParam->{iRepo}) ? $oParam->{iRepo} : '1') .
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-delete',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, bLogOutput => $self->synthetic()});

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
        {strComment => $strComment, bLogOutput => $self->synthetic()});
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
        {strComment => $strComment, bLogOutput => $self->synthetic()});

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
    my $oHostDbPrimary = $oHostGroup->hostGet(HOST_DB_PRIMARY);
    my $oHostDbStandby = $oHostGroup->hostGet(HOST_DB_STANDBY, true);

    my $bArchiveAsync = defined($$oParam{bArchiveAsync}) ? $$oParam{bArchiveAsync} : false;

    my $iRepoTotal = defined($oParam->{iRepoTotal}) ? $oParam->{iRepoTotal} : 1;

    if ($iRepoTotal < 1 || $iRepoTotal > 2)
    {
        confess "invalid repo total ${iRepoTotal}";
    }

    # General options
    # ------------------------------------------------------------------------------------------------------------------------------
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'job-retry'} = 0;

    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-level-console'} = lc(DETAIL);
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-level-file'} = testRunGet()->logLevelTestFile();
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-level-stderr'} = lc(OFF);
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-subprocess'} =
        testRunGet()->logLevelTestFile() eq lc(OFF) ? 'n' : 'y';
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-timestamp'} = 'n';
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'buffer-size'} = '64k';

    if ($oParam->{bBundle})
    {
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-bundle'} = 'y';
        # Set bundle size/limit smaller for testing and because FakeGCS does not do multi-part upload
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-bundle-size'} = '1MiB';
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-bundle-limit'} = '64KiB';
    }

    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-path'} = $self->logPath();
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'lock-path'} = $self->lockPath();

    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'protocol-timeout'} = 60;
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'db-timeout'} = 45;

    # Set to make sure that changing the default works and to speed compression for testing
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'compress-level'} = 3;

    # Only set network compress level if there is more than one host
    if ($oHostBackup != $oHostDbPrimary)
    {
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'compress-level-network'} = 1;
    }

    if (defined($oParam->{strCompressType}) && $oParam->{strCompressType} ne 'gz')
    {
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'compress-type'} = $oParam->{strCompressType};
    }

    if ($self->isHostBackup())
    {
        if ($self->repoEncrypt())
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-cipher-type'} = CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-cipher-pass'} = 'x';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-path'} = $self->repoPath();

        # S3 settings
        if ($oParam->{strStorage} eq S3)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-type'} = S3;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-key'} = HOST_S3_ACCESS_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-key-secret'} = HOST_S3_ACCESS_SECRET_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-bucket'} = HOST_S3_BUCKET;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-endpoint'} = HOST_S3_ENDPOINT;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-region'} = HOST_S3_REGION;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-verify-ssl'} = 'n';
        }
        elsif ($oParam->{strStorage} eq AZURE)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-type'} = AZURE;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-account'} = HOST_AZURE_ACCOUNT;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-key'} = HOST_AZURE_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-container'} = HOST_AZURE_CONTAINER;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-host'} = HOST_AZURE;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-verify-tls'} = 'n';
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-azure-uri-style'} = 'path';
        }
        elsif ($oParam->{strStorage} eq GCS)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-type'} = GCS;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-gcs-bucket'} = HOST_GCS_BUCKET;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-gcs-key-type'} = HOST_GCS_KEY_TYPE;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-gcs-key'} = HOST_GCS_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-gcs-endpoint'} = HOST_GCS . ':' . HOST_GCS_PORT;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-storage-verify-tls'} = 'n';
        }

        if ($iRepoTotal == 2)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-path'} = $self->repo2Path();
        }

        if (defined($$oParam{bHardlink}) && $$oParam{bHardlink})
        {
            $self->{bHardLink} = true;
            $oParamHash{&CFGDEF_SECTION_GLOBAL . ':backup'}{'repo1-s3-hardlink'} = 'y';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL . ':backup'}{'archive-copy'} = 'y';
        $oParamHash{&CFGDEF_SECTION_GLOBAL . ':backup'}{'start-fast'} = 'y';
    }

    # Host specific options
    # ------------------------------------------------------------------------------------------------------------------------------

    # If this is the backup host
    if ($self->isHostBackup())
    {
        my $oHostDb1 = $oHostDbPrimary;
        my $oHostDb2 = $oHostDbStandby;

        if ($self->nameTest(HOST_DB_STANDBY))
        {
            $oHostDb1 = $oHostDbStandby;
            $oHostDb2 = $oHostDbPrimary;
        }

        if ($self->nameTest(HOST_BACKUP))
        {
            $oParamHash{$strStanza}{'pg1-host'} = $oHostDb1->nameGet();
            $oParamHash{$strStanza}{'pg1-host-user'} = $oHostDb1->userGet();
            $oParamHash{$strStanza}{'pg1-host-cmd'} = $oHostDb1->backrestExe();
            $oParamHash{$strStanza}{'pg1-host-config'} = $oHostDb1->backrestConfig();

            if ($oParam->{bTls})
            {
                $oParamHash{$strStanza}{'pg1-host-type'} = 'tls';
                $oParamHash{$strStanza}{'pg1-host-cert-file'} = testRunGet()->basePath() . HOST_CLIENT_CERT;
                $oParamHash{$strStanza}{'pg1-host-key-file'} = testRunGet()->basePath() . HOST_CLIENT_KEY;
            }

            # Port can't be configured for a synthetic host
            if (!$self->synthetic())
            {
                $oParamHash{$strStanza}{'pg1-port'} = $oHostDb1->pgPort();
            }
        }

        $oParamHash{$strStanza}{'pg1-path'} = $oHostDb1->dbBasePath();

        if (defined($oHostDb2))
        {
            # Add an invalid replica to simulate more than one replica. A warning should be thrown when a stanza is created and a
            # valid replica should be chosen. Only do this for SSH since TLS takes longer to timeout.
            if (!$oParam->{bTls})
            {
                $oParamHash{$strStanza}{"pg2-host"} = BOGUS;
                $oParamHash{$strStanza}{"pg2-host-user"} = $oHostDb2->userGet();
                $oParamHash{$strStanza}{"pg2-host-cmd"} = $oHostDb2->backrestExe();
                $oParamHash{$strStanza}{"pg2-host-config"} = $oHostDb2->backrestConfig();
                $oParamHash{$strStanza}{"pg2-path"} = $oHostDb2->dbBasePath();
            }

            # Set a flag so we know there's a bogus host
            $self->{bBogusHost} = true;

            # Set a valid replica to a higher index to ensure skipping indexes does not make a difference
            $oParamHash{$strStanza}{"pg256-host"} = $oHostDb2->nameGet();
            $oParamHash{$strStanza}{"pg256-host-user"} = $oHostDb2->userGet();
            $oParamHash{$strStanza}{"pg256-host-cmd"} = $oHostDb2->backrestExe();
            $oParamHash{$strStanza}{"pg256-host-config"} = $oHostDb2->backrestConfig();
            $oParamHash{$strStanza}{"pg256-path"} = $oHostDb2->dbBasePath();

            if ($oParam->{bTls})
            {
                $oParamHash{$strStanza}{'pg256-host-type'} = 'tls';
                $oParamHash{$strStanza}{'pg256-host-cert-file'} = testRunGet()->basePath() . HOST_CLIENT_CERT;
                $oParamHash{$strStanza}{'pg256-host-key-file'} = testRunGet()->basePath() . HOST_CLIENT_KEY;
            }

            # Only test explicit ports on the backup server.  This is so locally configured ports are also tested.
            if (!$self->synthetic() && $self->nameTest(HOST_BACKUP))
            {
                $oParamHash{$strStanza}{"pg256-port"} = $oHostDb2->pgPort();
            }
        }
    }

    # If this is a database host
    if ($self->isHostDb())
    {
        $oParamHash{$strStanza}{'pg1-path'} = $self->dbBasePath();

        if (!$self->synthetic())
        {
            $oParamHash{$strStanza}{'pg1-socket-path'} = $self->pgSocketPath();
            $oParamHash{$strStanza}{'pg1-port'} = $self->pgPort();
        }

        if ($bArchiveAsync)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL . ':archive-push'}{'archive-async'} = 'y';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL}{'spool-path'} = $self->spoolPath();

        # If the backup host is remote
        if (!$self->isHostBackup())
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host'} = $oHostBackup->nameGet();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-user'} = $oHostBackup->userGet();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-cmd'} = $oHostBackup->backrestExe();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-config'} = $oHostBackup->backrestConfig();

            if ($oParam->{bTls})
            {
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-type'} = 'tls';
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-cert-file'} = testRunGet()->basePath() . HOST_CLIENT_CERT;
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-host-key-file'} = testRunGet()->basePath() . HOST_CLIENT_KEY;
            }

            if ($iRepoTotal == 2)
            {
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host'} = $oHostBackup->nameGet();
                $oParam->{bTls} ? $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-type'} = 'tls' : undef;
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-user'} = $oHostBackup->userGet();
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-cmd'} = $oHostBackup->backrestExe();
                $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-config'} = $oHostBackup->backrestConfig();

                if ($oParam->{bTls})
                {
                    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-type'} = 'tls';
                    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-cert-file'} = testRunGet()->basePath() . HOST_CLIENT_CERT;
                    $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo2-host-key-file'} = testRunGet()->basePath() . HOST_CLIENT_KEY;
                }
            }

            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'log-path'} = $self->logPath();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'lock-path'} = $self->lockPath();
        }
    }

    # Write out the configuration file
    storageTest()->put($self->backrestConfig(), iniRender(\%oParamHash, true));
}

####################################################################################################################################
# configUpdate - update configuration with new options
####################################################################################################################################
sub configUpdate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $hParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->configUpdate', \@_,
            {name => 'hParam'},
        );

    # Load db config file
    my $oConfig = iniParse(${storageTest()->get($self->backrestConfig())}, {bRelaxed => true});

    # Load params
    foreach my $strSection (keys(%{$hParam}))
    {
        foreach my $strKey (keys(%{$hParam->{$strSection}}))
        {
            if (defined($hParam->{$strSection}{$strKey}))
            {
                $oConfig->{$strSection}{$strKey} = $hParam->{$strSection}{$strKey};
            }
            else
            {
                delete($oConfig->{$strSection}{$strKey});
            }
        }
    }

    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $hParam,
        $bCache,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestMunge', \@_,
            {name => 'strBackup'},
            {name => '$hParam'},
            {name => 'bCache', default => true},
        );

    $self->infoMunge($self->repoBackupPath("${strBackup}/" . FILE_MANIFEST), $hParam, $bCache, true);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestRestore
####################################################################################################################################
sub manifestRestore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestRestore', \@_,
            {name => 'strBackup'},
            {name => 'bSave', default => true},
        );

    $self->infoRestore($self->repoBackupPath("${strBackup}/" . FILE_MANIFEST), $bSave);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $hParam,
        $bCache,
        $bManifest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoMunge', \@_,
            {name => 'strFileName'},
            {name => 'hParam'},
            {name => 'bCache', default => true},
            {name => 'bManifest', default => false},
        );

    # If the original file content does not exist then load it
    if (!defined($self->{hInfoFile}{$strFileName}))
    {
        $self->{hInfoFile}{$strFileName} = new pgBackRestDoc::Common::Ini(
        storageRepo(), $strFileName,
        {strCipherPass => !$bManifest ? undef : $self->cipherPassManifest()});
    }

    # Make a copy of the original file contents
    my $oMungeIni = new pgBackRestDoc::Common::Ini(
        storageRepo(), $strFileName,
        {bLoad => false, strContent => iniRender($self->{hInfoFile}{$strFileName}->{oContent}),
        strCipherPass => !$bManifest ? undef : $self->cipherPassManifest()});

    # Load params
    foreach my $strSection (keys(%{$hParam}))
    {
        foreach my $strKey (keys(%{$hParam->{$strSection}}))
        {
            if (ref($hParam->{$strSection}{$strKey}) eq 'HASH')
            {
                foreach my $strSubKey (keys(%{$hParam->{$strSection}{$strKey}}))
                {
                    # Munge the copy with the new parameter values
                    if (defined($hParam->{$strSection}{$strKey}{$strSubKey}))
                    {
                        $oMungeIni->set($strSection, $strKey, $strSubKey, $hParam->{$strSection}{$strKey}{$strSubKey});
                    }
                    else
                    {
                        $oMungeIni->remove($strSection, $strKey, $strSubKey);
                    }
                }
            }
            else
            {
                # Munge the copy with the new parameter values
                if (defined($hParam->{$strSection}{$strKey}))
                {
                    $oMungeIni->set($strSection, $strKey, undef, $hParam->{$strSection}{$strKey});
                }
                else
                {
                    $oMungeIni->remove($strSection, $strKey);
                }
            }
        }
    }

    # Save the munged data to the file
    $oMungeIni->save();

    # Clear the cache is requested
    if (!$bCache)
    {
        delete($self->{hInfoFile}{$strFileName});
    }

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
        $strFileName,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoRestore', \@_,
            {name => 'strFileName'},
            {name => 'bSave', default => true},
        );

    # If the original file content exists in the global hash, then save it to the file
    if (defined($self->{hInfoFile}{$strFileName}))
    {
        if ($bSave)
        {
            # Save the munged data to the file
            $self->{hInfoFile}{$strFileName}->{bModified} = true;
            $self->{hInfoFile}{$strFileName}->save();
        }
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
    my $oConfig = iniParse(${storageTest->get($self->backrestConfig())}, {bRelaxed => true});

    # Rewrite recovery options
    my @stryRecoveryOption;

    foreach my $strOption (sort(keys(%$oRecoveryHashRef)))
    {
        push (@stryRecoveryOption, "${strOption}=${$oRecoveryHashRef}{$strOption}");
    }

    if (@stryRecoveryOption)
    {
        $oConfig->{$strStanza}{'recovery-option'} = \@stryRecoveryOption;
    }

    # Save db config file
    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));
}

####################################################################################################################################
# configRemap
####################################################################################################################################
sub configRemap
{
    my $self = shift;
    my $oRemapHashRef = shift;
    my $oManifestRef = shift;

    # Get stanza name
    my $strStanza = $self->stanza();

    # Load db config file
    my $oConfig = iniParse(${storageTest()->get($self->backrestConfig())}, {bRelaxed => true});

    # Load backup config file
    my $oRemoteConfig;
    my $oHostBackup =
        !$self->standby() && !$self->nameTest($self->backupDestination()) ?
            hostGroupGet()->hostGet($self->backupDestination()) : undef;

    if (defined($oHostBackup))
    {
        $oRemoteConfig = iniParse(${storageTest()->get($oHostBackup->backrestConfig())}, {bRelaxed => true});
    }

    # Rewrite recovery section
    delete($oConfig->{"${strStanza}:restore"}{'tablespace-map'});
    my @stryTablespaceMap;

    foreach my $strRemap (sort(keys(%$oRemapHashRef)))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq MANIFEST_TARGET_PGDATA)
        {
            $oConfig->{$strStanza}{'pg1-path'} = $strRemapPath;

            ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;

            if (defined($oHostBackup))
            {
                $oRemoteConfig->{$strStanza}{'pg1-path'} = $strRemapPath;
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
        $oConfig->{"${strStanza}:restore"}{'tablespace-map'} = \@stryTablespaceMap;
    }

    # Save db config file
    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));

    # Save backup config file (but not if this is the standby which is not the source of backups)
    if (defined($oHostBackup))
    {
        storageTest()->put($oHostBackup->backrestConfig(), iniRender($oRemoteConfig, true));
    }
}

####################################################################################################################################
# restore
####################################################################################################################################
sub restore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $strBackup,
        $rhExpectedManifest,
        $rhRemapHash,
        $bDelta,
        $bForce,
        $strType,
        $strTarget,
        $bTargetExclusive,
        $strTargetAction,
        $strTargetTimeline,
        $rhRecoveryHash,
        $iExpectedExitStatus,
        $strOptionalParam,
        $bTablespace,
        $strUser,
        $strBackupExpected,
        $iRepo,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->restore', \@_,
            {name => 'strComment', required => false},
            {name => 'strBackup'},
            {name => 'rhExpectedManifest', optional => true},
            {name => 'rhRemapHash', optional => true},
            {name => 'bDelta', optional => true, default => false},
            {name => 'bForce', optional => true, default => false},
            {name => 'strType', optional => true},
            {name => 'strTarget', optional => true},
            {name => 'bTargetExclusive', optional => true, default => false},
            {name => 'strTargetAction', optional => true},
            {name => 'strTargetTimeline', optional => true},
            {name => 'rhRecoveryHash', optional => true},
            {name => 'iExpectedExitStatus', optional => true},
            {name => 'strOptionalParam', optional => true},
            {name => 'bTablespace', optional => true},
            {name => 'strUser', optional => true},
            {name => 'strBackupExpected', optional => true},
            {name => 'iRepo', optional => true},
        );

    # Build link map options
    my $strLinkMap;

    foreach my $strTarget (sort(keys(%{$self->{hLinkRemap}})))
    {
        $strLinkMap .= " --link-map=\"${strTarget}=${$self->{hLinkRemap}}{$strTarget}\"";
    }

    $strComment = 'restore' .
                  ($bDelta ? ' delta' : '') .
                  ($bForce ? ', force' : '') .
                  ($strBackup ne 'latest' ? ", backup '${strBackup}'" : '') .
                  # This does not output 'default' for synthetic tests to make expect logs match up (may change later)
                  ($strType ? ", type '${strType}'" : (defined($rhExpectedManifest) ? '' : ", type 'default'")) .
                  ($strTarget ? ", target '${strTarget}'" : '') .
                  ($strTargetTimeline ? ", timeline '${strTargetTimeline}'" : '') .
                  ($bTargetExclusive ? ', exclusive' : '') .
                  (defined($strTargetAction) && $strTargetAction ne 'pause' ? ", target-action=${strTargetAction}" : '') .
                  (defined($rhRemapHash) ? ', remap' : '') .
                  (defined($iExpectedExitStatus) ? ", expect exit ${iExpectedExitStatus}" : '') .
                  (defined($strComment) ? " - ${strComment}" : '') .
                  ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    # If the repo was not passed, then use repo1 as the repo for getting the expected manifest/backup
    my $iRepoDefault = !defined($iRepo) ? 1 : $iRepo;

    # Load the expected manifest if it was not defined
    my $oExpectedManifest = undef;

    # If an expected backup is defined, then the strBackup should be the default to allow the restore process to select the backup
    # - which should be the backup passed as strBackupExpected. If it is not defined, then set it based on the strBackup passed.
    if (!defined($strBackupExpected))
    {
        $strBackupExpected = $strBackup eq 'latest' ? $oHostBackup->backupLast($iRepoDefault) : $strBackup;
    }

    if (!defined($rhExpectedManifest))
    {
        # Load the manifest from the backup expected to be chosen/processed by restore
        my $oExpectedManifest = new pgBackRestTest::Env::Manifest(
            $self->repoBackupPath($strBackupExpected . qw{/} . FILE_MANIFEST, $iRepoDefault),
            {strCipherPass => $oHostBackup->cipherPassManifest(), oStorage => storageRepo({iRepo => $iRepoDefault})});

        $rhExpectedManifest = $oExpectedManifest->{oContent};

        # Remap links in the expected manifest
        foreach my $strTarget (sort(keys(%{$self->{hLinkRemap}})))
        {
            my $strDestination = ${$self->{hLinkRemap}}{$strTarget};
            my $strTarget = 'pg_data/' . $strTarget;
            my $strTargetPath = $strDestination;

            # If this link is to a file then the specified path must be split into file and path parts
            if ($oExpectedManifest->isTargetFile($strTarget))
            {
                $strTargetPath = dirname($strTargetPath);

                # Error when the path is not deep enough to be valid
                if (!defined($strTargetPath))
                {
                    confess &log(ERROR, "${strDestination} is not long enough to be target for ${strTarget}");
                }

                # Set the file part
                $oExpectedManifest->set(
                    MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE,
                    substr($strDestination, length($strTargetPath) + 1));

                # Set the link target
                $oExpectedManifest->set(
                    MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, $strDestination);
            }
            else
            {
                # Set the link target
                $oExpectedManifest->set(MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, $strTargetPath);
            }

            # Set the target path
            $oExpectedManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH, $strTargetPath);
        }
    }

    # Get the backup host
    if (defined($rhRemapHash))
    {
        $self->configRemap($rhRemapHash, $rhExpectedManifest);
    }

    if (defined($rhRecoveryHash))
    {
        $self->configRecovery($oHostBackup, $rhRecoveryHash);
    }

    # Create the restore command
    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ($bDelta ? ' --delta' : '') .
        ($bForce ? ' --force' : '') .
        ($strBackup ne 'latest' ? " --set=${strBackup}" : '') .
        (defined($strOptionalParam) ? " ${strOptionalParam} " : '') .
        (defined($strType) && $strType ne CFGOPTVAL_RESTORE_TYPE_DEFAULT ? " --type=${strType}" : '') .
        (defined($strTarget) ? " --target=\"${strTarget}\"" : '') .
        (defined($strTargetTimeline) ? " --target-timeline=\"${strTargetTimeline}\"" : '') .
        ($bTargetExclusive ? ' --target-exclusive' : '') .
        (defined($strLinkMap) ? $strLinkMap : '') .
        ($self->synthetic() ? '' : ' --link-all') .
        (defined($strTargetAction) && $strTargetAction ne 'pause' ? " --target-action=${strTargetAction}" : '') .
        (defined($iRepo) ? " --repo=${iRepo}" : '') .
        " --stanza=" . $self->stanza() . ' restore',
        {strComment => $strComment, iExpectedExitStatus => $iExpectedExitStatus, bLogOutput => $self->synthetic()},
        $strUser);

    if (!defined($iExpectedExitStatus))
    {
        # Only compare restores in repo1. There is not a lot of value in comparing restores in other repos and it would require a
        # lot of changes to the Perl test harness.
        if ($iRepoDefault == 1)
        {
            $self->restoreCompare($strBackupExpected, dclone($rhExpectedManifest), $bTablespace);
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

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    # Load the last manifest if it exists
    my $oLastManifest = undef;

    if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR}))
    {
        my $oExpectedManifest =
            new pgBackRestTest::Env::Manifest(
                $self->repoBackupPath(
                    ($strBackup eq 'latest' ? $oHostBackup->backupLast() : $strBackup) . '/' . FILE_MANIFEST),
            {strCipherPass => $oHostBackup->cipherPassManifest()});

        # Get the --delta option from the backup manifest so the actual manifest can be built the same way for comparison
        $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA} =
            $oExpectedManifest->get(MANIFEST_SECTION_BACKUP_OPTION, &MANIFEST_KEY_DELTA);

        $oLastManifest =
            new pgBackRestTest::Env::Manifest(
                $self->repoBackupPath(
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} . qw{/} . FILE_MANIFEST),
            {strCipherPass => $oHostBackup->cipherPassManifest()});
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

                $$oTablespaceMap{$iTablespaceId} =
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

    my $oActualManifest = new pgBackRestTest::Env::Manifest(
        "${strTestPath}/" . FILE_MANIFEST,
        {bLoad => false, strDbVersion => $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION},
            iDbCatalogVersion => $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG},
            oStorage => storageTest()});

    # Build the actual manifest using the delta setting that was actually used by the latest backup if one exists
    $oActualManifest->build(storageTest(), $strDbClusterPath, $oLastManifest, false,
        $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA}, $oTablespaceMap);
    $oActualManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA, undef,
        $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS_TYPE, undef,
        $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_TYPE});

    my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strName ($oActualManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # When bundling zero-length files will not have a reference
        if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{'backup-bundle'} &&
            $oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_SIZE} == 0)
        {
            $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE);
        }

        # If synthetic match checksum errors since they can't be verified here
        if ($self->synthetic)
        {
            my $bChecksumPage = $oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE};

            if (defined($bChecksumPage))
            {
                $oActualManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE, $bChecksumPage);

                if (!$bChecksumPage &&
                    defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR}))
                {
                    $oActualManifest->set(
                        MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR,
                        $oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR});
                }
            }
        }
        # Else if page checksums are enabled make sure the correct files are being checksummed
        else
        {
            if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE})
            {
                if (defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE}) !=
                    isChecksumPage($strName))
                {
                    confess
                        "check-page actual for ${strName} is " .
                        ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName,
                            MANIFEST_SUBKEY_CHECKSUM_PAGE) ? 'set' : '[undef]') .
                        ' but isChecksumPage() says it should be ' .
                        (isChecksumPage($strName) ? 'set' : '[undef]') . '.';
                }

                # Because the page checksum flag is copied to incr and diff from the previous backup but further processing is not
                # done, they can't be expected to match so delete them.
                delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE});
                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE);
            }
        }

        if (!$self->synthetic())
        {
            $oActualManifest->set(
                MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE,
                ${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{size});
        }

        # Remove repo-size, bno, bni from the manifest
        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE);
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REPO_SIZE});
        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, "bni");
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{"bni"});
        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, "bno");
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{"bno"});

        if ($oActualManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) != 0)
        {
            my $oStat = lstat($oActualManifest->dbPathGet($strSectionPath, $strName));

            # When performing a selective restore, the files for the database(s) that are not restored are still copied but as empty
            # sparse files (blocks == 0). If the file is not a sparse file or is a link, then get the actual checksum for comparison
            if ($oStat->blocks > 0 || S_ISLNK($oStat->mode))
            {
                my ($strHash) = storageTest()->hashSize($oActualManifest->dbPathGet($strSectionPath, $strName));

                $oActualManifest->set(
                    MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM, $strHash);

                # If the delta option was set, it is possible that the checksum on the file changed from the last manifest. If so,
                # then the file was expected to be copied by the backup and therefore the reference would have been removed.
                if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA})
                {
                    # If the actual checksum and last manifest checksum don't match, remove the reference
                    if (defined($oLastManifest) &&
                        $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM) &&
                        $strHash ne $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM))
                    {
                        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE);
                    }
                }
            }
            else
            {
                # If there is a sparse file, remove the checksum and reference since they may or may not match. In this case, it is
                # not important to check them since it is known that the file was intentionally not restored.
                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM);
                delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM});

                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE);
                delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REFERENCE});
            }
        }
    }

    # If PostgreSQL >= 12 don't compare postgresql.auto.conf since it will have recovery settings written into it
    if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_12)
    {
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/postgresql.auto.conf'});
        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, 'pg_data/postgresql.auto.conf');
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
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BACKUP_STANDBY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BACKUP_STANDBY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BUFFER_SIZE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BUFFER_SIZE});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS_LEVEL, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_LEVEL});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS_LEVEL_NETWORK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_LEVEL_NETWORK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_PROCESS_MAX, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_PROCESS_MAX});
    $oActualManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA});

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

    # Copy passphrase if one exists
    if (defined($oExpectedManifestRef->{&INI_SECTION_CIPHER}) &&
        defined($oExpectedManifestRef->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS}))
    {
        $oActualManifest->set(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS, undef,
                              $oExpectedManifestRef->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS});
    }

    # This option won't be set in the actual manifest
    delete($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE});

    if ($self->synthetic())
    {
        $oActualManifest->remove(MANIFEST_SECTION_BACKUP);
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP});
    }
    else
    {
        $oActualManifest->set(
            INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef, $oExpectedManifestRef->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE});

        if (defined($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{'backup-bundle'}))
        {
            $oActualManifest->set(
                MANIFEST_SECTION_BACKUP, 'backup-bundle', undef,
                $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{'backup-bundle'});
        }

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

    # Check that archive status exists in the manifest for an online backup
    my $strArchiveStatusPath = MANIFEST_TARGET_PGDATA . qw{/} . $oActualManifest->walPath() . qw{/} . DB_PATH_ARCHIVESTATUS;

    if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE} &&
        !defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_PATH}{$strArchiveStatusPath}))
    {
        confess &log(ERROR, "${strArchiveStatusPath} expected for online backup", ERROR_ASSERT);
    }

    # Delete the list of DBs
    delete($$oExpectedManifestRef{&MANIFEST_SECTION_DB});

    # Only update defaults if the expect manifest is synthetic. If loaded from a file the defaults will already be correct.
    if ($self->synthetic())
    {
        $self->manifestDefault($oExpectedManifestRef);
    }

    # Newer Perls will change this variable to a number whenever a numeric comparison is performed. It is expected to be a string so
    # make sure it is one before saving.
    $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} .= '';

    storageTest()->put("${strTestPath}/actual.manifest", iniRender($oActualManifest->{oContent}));
    storageTest()->put("${strTestPath}/expected.manifest", iniRender($oExpectedManifestRef));

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    storageTest()->remove("${strTestPath}/expected.manifest");
    storageTest()->remove("${strTestPath}/actual.manifest");
}

####################################################################################################################################
# Get repo backup/archive path
####################################################################################################################################
sub repoSubPath
{
    my $self = shift;
    my $strSubPath = shift;
    my $strPath = shift;
    my $iRepo = shift;

    my $strRepoPath = $self->repoPath();

    if (defined($iRepo) && $iRepo == 2)
    {
        $strRepoPath = $self->repo2Path();
    }

    return
        ($strRepoPath eq '/' ? '' : $strRepoPath) . "/${strSubPath}/" . $self->stanza() .
        (defined($strPath) ? "/${strPath}" : '');
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub backrestConfig {return shift->{strBackRestConfig}}
sub backupDestination {return shift->{strBackupDestination}}
sub backrestExe {return testRunGet()->backrestExe()}
sub bogusHost {return shift->{bBogusHost}}
sub hardLink {return shift->{bHardLink}}
sub hasLink {storageRepo()->capability(STORAGE_CAPABILITY_LINK)}
sub isFS {storageRepo()->type() ne STORAGE_OBJECT}
sub isHostBackup {my $self = shift; return $self->backupDestination() eq $self->nameGet()}
sub isHostDbPrimary {return shift->nameGet() eq HOST_DB_PRIMARY}
sub isHostDbStandby {return shift->nameGet() eq HOST_DB_STANDBY}
sub isHostDb {my $self = shift; return $self->isHostDbPrimary() || $self->isHostDbStandby()}
sub lockPath {return shift->{strLockPath}}
sub logPath {return shift->{strLogPath}}
sub repoArchivePath {return shift->repoSubPath('archive', shift)}
sub repoBackupPath {return shift->repoSubPath('backup', shift, shift)}
sub repoPath {return shift->{strRepoPath}}
sub repo2Path {return shift->{strRepo2Path}}
sub repoEncrypt {return shift->{bRepoEncrypt}}
sub stanza {return testRunGet()->stanza()}
sub synthetic {return shift->{bSynthetic}}
sub cipherPassManifest {return shift->{strCipherPassManifest}}
sub cipherPassArchive {return shift->{strCipherPassArchive}}

1;
