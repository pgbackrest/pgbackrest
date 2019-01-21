####################################################################################################################################
# PROTOCOL REMOTE MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Remote::Minion;
use parent 'pgBackRest::Protocol::Command::Minion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Backup::File;
use pgBackRest::Common::Log;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Common::Wait;
use pgBackRest::Archive::Get::File;
use pgBackRest::Archive::Push::File;
use pgBackRest::Check::Check;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::Protocol::Command::Minion;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iBufferMax,                                # Maximum buffer size
        $iProtocolTimeout                           # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'iBufferMax'},
            {name => 'iProtocolTimeout'}
        );

    # Init object and store variables
    my $self = $class->SUPER::new(cfgCommandName(CFGCMD_REMOTE), $iBufferMax, $iProtocolTimeout);
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

    # Create objects
    my $oStorage = cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_DB) ? storageDb() : storageRepo();

    my $oCheck = cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_BACKUP) ? new pgBackRest::Check::Check() : undef;
    my $oDb = cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_DB) ? new pgBackRest::Db() : undef;

    # Create anonymous subs for each command
    my $hCommandMap =
    {
        # ArchiveGet commands
        &OP_ARCHIVE_GET_CHECK => sub {archiveGetCheck(@{shift()})},

        # ArchivePush commands
        &OP_ARCHIVE_PUSH_CHECK => sub {archivePushCheck(@{shift()})},

        # Check commands
        &OP_CHECK_BACKUP_INFO_CHECK => sub {$oCheck->backupInfoCheck(@{shift()})},

        # Db commands
        &OP_DB_CONNECT => sub {$oDb->connect()},
        &OP_DB_EXECUTE_SQL => sub {$oDb->executeSql(@{shift()})},
        &OP_DB_INFO => sub {$oDb->info(@{shift()})},

        # File commands
        &OP_STORAGE_OPEN_READ => sub
        {
            my $oSourceFileIo = $oStorage->openRead(@{shift()});

            # If the source file exists
            if (defined($oSourceFileIo))
            {
                $self->outputWrite(true);

                $oStorage->copy($oSourceFileIo, new pgBackRest::Protocol::Storage::File($self, $oSourceFileIo));

                return true;
            }

            return false;
        },
        &OP_STORAGE_OPEN_WRITE => sub
        {
            my $oDestinationFileIo = $oStorage->openWrite(@{shift()});
            $oStorage->copy(new pgBackRest::Protocol::Storage::File($self, $oDestinationFileIo), $oDestinationFileIo);
        },

        &OP_STORAGE_CIPHER_PASS_USER => sub {$oStorage->cipherPassUser()},
        &OP_STORAGE_EXISTS => sub {$oStorage->exists(@{shift()})},
        &OP_STORAGE_LIST => sub {$oStorage->list(@{shift()})},
        &OP_STORAGE_MANIFEST => sub {$oStorage->manifest(@{shift()})},
        &OP_STORAGE_MOVE => sub {$oStorage->move(@{shift()})},
        &OP_STORAGE_PATH_GET => sub {$oStorage->pathGet(@{shift()})},
        &OP_STORAGE_HASH_SIZE => sub {$oStorage->hashSize(@{shift()})},

        # Wait command
        &OP_WAIT => sub {waitRemainder(@{shift()})},
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCommandMap', value => $hCommandMap}
    );
}

1;
