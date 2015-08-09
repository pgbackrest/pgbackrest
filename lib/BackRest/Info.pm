####################################################################################################################################
# INFO MODULE
####################################################################################################################################
package BackRest::Info;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);

use lib dirname($0);
use BackRest::BackupCommon;
use BackRest::BackupInfo;
use BackRest::Config;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_INFO_LIST_STANZA                                    => 'Info->listStanza';
    our @EXPORT = qw(OP_INFO_LIST_STANZA);

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
    my $oFile = shift;       # Remote object

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Set variables
    $self->{oFile} = $oFile;

    return $self;
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;

    # Get stanza if specified
    my $strStanza = optionTest(OPTION_STANZA) ? optionGet(OPTION_STANZA) : undef;

    # Create the file object
    my $oFile = new BackRest::File
    (
        $strStanza,
        optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        optionRemoteTypeTest(BACKUP) ? BACKUP : NONE,
        protocolGet(!optionRemoteTypeTest(BACKUP))
    );

    # Get the stanza list with all info
    my $oStanzaList = $self->listStanza($oFile, $strStanza);

    if (optionTest(OPTION_OUTPUT, INFO_OUTPUT_TEXT))
    {
        my $strOutput;

        foreach my $oStanzaInfo (@{$oStanzaList})
        {
            $strOutput = defined($strOutput) ? $strOutput .= "\n" : '';

            $strOutput .= 'stanza ' . $$oStanzaInfo{&INFO_STANZA_NAME} . "\n";
            $strOutput .= '    status: ' . ($$oStanzaInfo{&INFO_SECTION_STATUS}{&INFO_KEY_CODE} == 0 ? INFO_STANZA_STATUS_OK :
                          INFO_STANZA_STATUS_ERROR . ' (' . $$oStanzaInfo{&INFO_SECTION_STATUS}{&INFO_KEY_MESSAGE} . ')') . "\n";

            if (@{$$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}} > 0)
            {
                my $oOldestBackup = $$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}[0];

                $strOutput .= '    oldest backup label: ' . $$oOldestBackup{&INFO_KEY_LABEL} . "\n";
                $strOutput .= '    oldest backup timestamp: ' .
                              timestamp_string_get(undef, $$oOldestBackup{&INFO_SECTION_TIMESTAMP}{&INFO_KEY_START}) . "\n";

                my $oLatestBackup = $$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}[@{$$oStanzaInfo{&INFO_BACKUP_SECTION_BACKUP}} - 1];

                $strOutput .= '    latest backup label: ' . $$oLatestBackup{&INFO_KEY_LABEL} . "\n";
                $strOutput .= '    latest backup timestamp: ' .
                              timestamp_string_get(undef, $$oLatestBackup{&INFO_SECTION_TIMESTAMP}{&INFO_KEY_START}) . "\n";
            }
        }

        syswrite(*STDOUT, $strOutput);
    }
    elsif (optionTest(OPTION_OUTPUT, INFO_OUTPUT_JSON))
    {
        my $oJSON = JSON::PP->new()->canonical()->pretty()->indent_length(4);
        my $strJSON = $oJSON->encode($oStanzaList);

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

    return 0;
}

####################################################################################################################################
# listStanza
####################################################################################################################################
sub listStanza
{
    my $self = shift;
    my $oFile = shift;
    my $strStanza = shift;

    # Set operation and debug strings
    my $strOperation = OP_INFO_LIST_STANZA;
    &log(DEBUG, "${strOperation}". (defined($strStanza) ? ": stanza = ${strStanza}" : ''));
    my @oStanzaList;

    if ($oFile->is_remote(PATH_BACKUP))
    {
        # Build param hash
        my $oParamHash = undef;

        if (defined($strStanza))
        {
            $$oParamHash{'stanza'} = $strStanza;
        }

        # Trace the remote parameters
        &log(TRACE, "${strOperation}: remote (" . $oFile->{oProtocol}->commandParamString($oParamHash) . ')');

        # Execute the command
        my $strStanzaList = $oFile->{oProtocol}->cmdExecute($strOperation, $oParamHash, true);

        # Trace the remote response
        &log(TRACE, "${strOperation}: remote json response (${strStanzaList})");

        my $oJSON = JSON::PP->new();
        return $oJSON->decode($strStanzaList);
    }
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
                $self->listBackup($oFile, $strStanzaFound);

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

            push @oStanzaList, $oStanzaInfo;
        }

        if (defined($strStanza) && @oStanzaList == 0)
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

            push @oStanzaList, $oStanzaInfo;
        }
    }

    return \@oStanzaList;
}

####################################################################################################################################
# listBackup
###################################################################################################################################
sub listBackup
{
    my $self = shift;
    my $oFile = shift;
    my $strStanza = shift;

    # Load or build backup.info
    my $oBackupInfo = new BackRest::BackupInfo($oFile->path_get(PATH_BACKUP, CMD_BACKUP . "/${strStanza}"));

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
                    $oBackupInfo->getNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INI_KEY_FORMAT),
                &INFO_KEY_VERSION =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INI_KEY_VERSION)
            },
            &INFO_SECTION_DB =>
            {
                &INFO_KEY_ID =>
                    $oBackupInfo->getNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID)
            },
            &INFO_SECTION_INFO =>
            {
                &INFO_SECTION_REPO =>
                {
                    &INFO_KEY_SIZE =>
                        $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_REPO_SIZE),
                    &INFO_KEY_DELTA =>
                        $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA),
                },
                &INFO_KEY_SIZE =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_SIZE),
                &INFO_KEY_DELTA =>
                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_BACKUP_SIZE_DELTA),
            },
            &INFO_SECTION_TIMESTAMP =>
            {
                &INFO_KEY_START =>
                    $oBackupInfo->getNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_TIMESTAMP_START),
                &INFO_KEY_STOP =>
                    $oBackupInfo->getNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_TIMESTAMP_STOP),
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

    return \@oyBackupList, \@oyDbList;
}

1;
