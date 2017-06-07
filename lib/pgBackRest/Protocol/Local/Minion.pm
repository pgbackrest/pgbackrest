####################################################################################################################################
# PROTOCOL LOCAL MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Local::Minion;
use parent 'pgBackRest::Protocol::Command::Minion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Archive::ArchivePushFile;
use pgBackRest::Backup::File;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Storage::Local;
use pgBackRest::Protocol::Base::Master;
use pgBackRest::Protocol::Command::Minion;
use pgBackRest::Protocol::Helper;
use pgBackRest::RestoreFile;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Init object and store variables
    my $self = $class->SUPER::new(CMD_LOCAL, optionGet(OPTION_BUFFER_SIZE), optionGet(OPTION_PROTOCOL_TIMEOUT));
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# init
####################################################################################################################################
sub init
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->init');

    # Create anonymous subs for each command
    my $hCommandMap =
    {
        &OP_ARCHIVE_PUSH_FILE => sub {archivePushFile(@{shift()})},
        &OP_BACKUP_FILE => sub {backupFile(@{shift()})},
        &OP_RESTORE_FILE => sub {restoreFile(@{shift()})},
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCommandMap', value => $hCommandMap}
    );
}

1;
