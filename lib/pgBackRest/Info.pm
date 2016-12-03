####################################################################################################################################
# INFO MODULE
####################################################################################################################################
package pgBackRest::Info;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Common::Log;
use pgBackRest::Common::Ini;
use pgBackRest::Common::String;
use pgBackRest::BackupCommon;
use pgBackRest::BackupInfo;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

####################################################################################################################################
# Info constants
####################################################################################################################################
use constant INFO_SECTION_BACKREST                                  => 'backrest';
use constant INFO_SECTION_ARCHIVE                                   => 'archive';
use constant INFO_SECTION_DB                                        => 'database';
use constant INFO_SECTION_INFO                                      => 'info';
use constant INFO_SECTION_REPO                                      => 'repository';
use constant INFO_SECTION_TIMESTAMP                                 => 'timestamp';
use constant INFO_SECTION_STATUS                                    => 'status';

use constant INFO_STANZA_NAME                                       => 'name';

use constant INFO_STANZA_STATUS_OK                                  => 'ok';
use constant INFO_STANZA_STATUS_ERROR                               => 'error';

use constant INFO_STANZA_STATUS_OK_CODE                             => 0;
use constant INFO_STANZA_STATUS_OK_MESSAGE                          => INFO_STANZA_STATUS_OK;
use constant INFO_STANZA_STATUS_MISSING_STANZA_CODE                 => 1;
use constant INFO_STANZA_STATUS_MISSING_STANZA_MESSAGE              => 'missing stanza path';
use constant INFO_STANZA_STATUS_NO_BACKUP_CODE                      => 2;
use constant INFO_STANZA_STATUS_NO_BACKUP_MESSAGE                   => 'no valid backups';

use constant INFO_KEY_CODE                                          => 'code';
use constant INFO_KEY_DELTA                                         => 'delta';
use constant INFO_KEY_FORMAT                                        => 'format';
use constant INFO_KEY_ID                                            => 'id';
use constant INFO_KEY_LABEL                                         => 'label';
use constant INFO_KEY_MESSAGE                                       => 'message';
use constant INFO_KEY_PRIOR                                         => 'prior';
use constant INFO_KEY_REFERENCE                                     => 'reference';
use constant INFO_KEY_SIZE                                          => 'size';
use constant INFO_KEY_START                                         => 'start';
use constant INFO_KEY_STOP                                          => 'stop';
use constant INFO_KEY_SYSTEM_ID                                     => 'system-id';
use constant INFO_KEY_TYPE                                          => 'type';
use constant INFO_KEY_VERSION                                       => 'version';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Get stanza if specified
    my $strStanza = optionTest(OPTION_STANZA) ? optionGet(OPTION_STANZA) : undef;

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        $strStanza,
        optionGet(OPTION_REPO_PATH),
        protocolGet(BACKUP)
    );

    # Get the stanza list with all info
    my $oyStanzaList = $self->stanzaList($oFile, $strStanza);

    if (optionTest(OPTION_OUTPUT, INFO_OUTPUT_TEXT))
    {
        my $strOutput;

        foreach my $oStanzaInfo (@{$oyStanzaList})
        {
            # Add an LF between stanzas
            $strOutput .= defined($strOutput) ? "\n" : '';

            # Output stanza name and status
            $strOutput .=
                'stanza: ' . $$oStanzaInfo{&INFO_STANZA_NAME} . "\n" .
                '    status: ' . ($$oStanzaInfo{&INFO_SECTION_STATUS}{&INFO_KEY_CODE} == 0 ? INFO_STANZA_STATUS_OK :
                INFO_STANZA_STATUS_ERROR . ' (' . $$oStanzaInfo{&INFO_SECTION_STATUS}{&INFO_KEY_MESSAGE} . ')') . "\n";

            # Output information for each stanza backup, from oldest to newest
            foreach my $oBackupInfo (@{$$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}})
            {
                $strOutput .=
                    "\n".
                    '    ' . $$oBackupInfo{&INFO_KEY_TYPE} . ' backup: ' . $$oBackupInfo{&INFO_KEY_LABEL} . "\n" .

                    '        start / stop timestamp: ' .
                    timestampFormat(undef, $$oBackupInfo{&INFO_SECTION_TIMESTAMP}{&INFO_KEY_START}) .
                    ' / ' .
                    timestampFormat(undef, $$oBackupInfo{&INFO_SECTION_TIMESTAMP}{&INFO_KEY_STOP}) . "\n" .

                    '        database size: ' .
                    (defined($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_KEY_SIZE}) ?
                        fileSizeFormat($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_KEY_SIZE}) : '') .
                    ', backup size: ' .
                    (defined($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_KEY_DELTA}) ?
                        fileSizeFormat($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_KEY_DELTA}) : '') . "\n" .

                    '        repository size: ' .
                    (defined($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_SECTION_REPO}{&INFO_KEY_SIZE}) ?
                        fileSizeFormat($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_SECTION_REPO}{&INFO_KEY_SIZE}) : '') .
                    ', repository backup size: ' .
                    (defined($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_SECTION_REPO}{&INFO_KEY_DELTA}) ?
                        fileSizeFormat($$oBackupInfo{&INFO_SECTION_INFO}{&INFO_SECTION_REPO}{&INFO_KEY_DELTA}) : '') . "\n";

                # List the backup reference chain, if any, for this backup
                if (defined($$oBackupInfo{&INFO_KEY_REFERENCE}))
                {
                    $strOutput .= '        backup reference list: ' . (join(', ', @{$$oBackupInfo{&INFO_KEY_REFERENCE}})) . "\n";
                }
            }
        }

        if (defined($strOutput))
        {
            syswrite(*STDOUT, $strOutput);
        }
        else
        {
            syswrite(*STDOUT, 'No stanzas exist in ' . $oFile->pathGet(PATH_BACKUP) . ".\n");
        }
    }
    elsif (optionTest(OPTION_OUTPUT, INFO_OUTPUT_JSON))
    {
        my $oJSON = JSON::PP->new()->canonical()->pretty()->indent_length(4);
        my $strJSON = $oJSON->encode($oyStanzaList);

        syswrite(*STDOUT, $strJSON);

        # On some systems a linefeed will be appended by encode() but others will not have it.  In our case there should always
        # be a terminating linefeed.
        if ($strJSON !~ /\n$/)
        {
            syswrite(*STDOUT, "\n");
        }
    }
    else
    {
        confess &log(ASSERT, "invalid info output option '" . optionGet(OPTION_OUTPUT) . "'");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => 0, trace => true}
    );
}

####################################################################################################################################
# stanzaList
####################################################################################################################################
sub stanzaList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strStanza
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaList', \@_,
            {name => 'oFile'},
            {name => 'strStanza', required => false}
        );

    my @oyStanzaList;

    # Run remotely
    if ($oFile->isRemote(PATH_BACKUP))
    {
        @oyStanzaList = @{$oFile->{oProtocol}->cmdExecute(OP_INFO_STANZA_LIST, [$strStanza], true)};
    }
    # Run locally
    else
    {
        my @stryStanza = $oFile->list(PATH_BACKUP, CMD_BACKUP, undef, undef, true);

        foreach my $strStanzaFound (@stryStanza)
        {
            if (defined($strStanza) && $strStanza ne $strStanzaFound)
            {
                next;
            }

            my $oStanzaInfo = {};
            $$oStanzaInfo{&INFO_STANZA_NAME} = $strStanzaFound;
            ($$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}, $$oStanzaInfo{&INFO_BACKUP_SECTION_DB}) =
                $self->backupList($oFile, $strStanzaFound);

            if (@{$$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}} == 0)
            {
                $$oStanzaInfo{&INFO_SECTION_STATUS} =
                {
                    &INFO_KEY_CODE => INFO_STANZA_STATUS_NO_BACKUP_CODE,
                    &INFO_KEY_MESSAGE => INFO_STANZA_STATUS_NO_BACKUP_MESSAGE
                };
            }
            else
            {
                $$oStanzaInfo{&INFO_SECTION_STATUS} =
                {
                    &INFO_KEY_CODE => INFO_STANZA_STATUS_OK_CODE,
                    &INFO_KEY_MESSAGE => INFO_STANZA_STATUS_OK_MESSAGE
                };
            }

            push @oyStanzaList, $oStanzaInfo;
        }

        if (defined($strStanza) && @oyStanzaList == 0)
        {
            my $oStanzaInfo = {};

            $$oStanzaInfo{&INFO_STANZA_NAME} = $strStanza;

            $$oStanzaInfo{&INFO_SECTION_STATUS} =
            {
                &INFO_KEY_CODE => INFO_STANZA_STATUS_MISSING_STANZA_CODE,
                &INFO_KEY_MESSAGE => INFO_STANZA_STATUS_MISSING_STANZA_MESSAGE
            };

            $$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP} = [];
            $$oStanzaInfo{&INFO_BACKUP_SECTION_DB} = [];

            push @oyStanzaList, $oStanzaInfo;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oyStanzaList', value => \@oyStanzaList, log => false, ref => true}
    );
}

####################################################################################################################################
# backupList
###################################################################################################################################
sub backupList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strStanza
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupList', \@_,
            {name => 'oFile'},
            {name => 'strStanza'}
        );

    # Load or build backup.info
    my $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP, CMD_BACKUP . "/${strStanza}"));

    # Build the db list
    my @oyDbList;

    foreach my $iHistoryId ($oBackupInfo->keys(INFO_BACKUP_SECTION_DB_HISTORY))
    {
        my $oDbHash =
        {
            &INFO_KEY_ID => $iHistoryId,
            &INFO_KEY_VERSION =>
                $oBackupInfo->get(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_DB_VERSION),
            &INFO_KEY_SYSTEM_ID =>
                $oBackupInfo->get(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_SYSTEM_ID)
        };

        push(@oyDbList, $oDbHash);
    }

    # Build the backup list
    my @oyBackupList;

    foreach my $strBackup ($oBackupInfo->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        my $oBackupHash =
        {
            &INFO_SECTION_ARCHIVE =>
            {
                &INFO_KEY_START =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_ARCHIVE_START, false),
                &INFO_KEY_STOP =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_ARCHIVE_STOP, false),
            },
            &INFO_SECTION_BACKREST =>
            {
                &INFO_KEY_FORMAT =>
                    $oBackupInfo->numericGet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INI_KEY_FORMAT),
                &INFO_KEY_VERSION =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INI_KEY_VERSION)
            },
            &INFO_SECTION_DB =>
            {
                &INFO_KEY_ID =>
                    $oBackupInfo->numericGet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID)
            },
            &INFO_SECTION_INFO =>
            {
                &INFO_SECTION_REPO =>
                {
                    # Size of the backup in the repository, taking compression into account
                    &INFO_KEY_SIZE =>
                        $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_REPO_SIZE),
                    # Size of this backup only (does not include referenced backups like repo->size)
                    &INFO_KEY_DELTA =>
                        $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA),
                },
                # Original database size
                &INFO_KEY_SIZE =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_SIZE),
                # Amount of database backed up (will be equal for full backups)
                &INFO_KEY_DELTA =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_SIZE_DELTA),
            },
            &INFO_SECTION_TIMESTAMP =>
            {
                &INFO_KEY_START =>
                    $oBackupInfo->numericGet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_TIMESTAMP_START),
                &INFO_KEY_STOP =>
                    $oBackupInfo->numericGet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_TIMESTAMP_STOP),
            },
            &INFO_KEY_LABEL => $strBackup,
            &INFO_KEY_PRIOR =>
                $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_PRIOR, false),
            &INFO_KEY_REFERENCE =>
                $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_REFERENCE, false),
            &INFO_KEY_TYPE =>
                $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_TYPE)
        };

        push(@oyBackupList, $oBackupHash);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oyBackupList', value => \@oyBackupList, log => false, ref => true},
        {name => 'oyDbList', value => \@oyDbList, log => false, ref => true}
    );
}

1;
