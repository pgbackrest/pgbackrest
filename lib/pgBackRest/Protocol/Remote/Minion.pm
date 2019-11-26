####################################################################################################################################
# PROTOCOL REMOTE MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Remote::Minion;
use parent 'pgBackRest::Protocol::Command::Minion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Common::Log;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Common::Wait;
use pgBackRest::Archive::Get::File;
use pgBackRest::Config::Config;
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

    my $oDb = cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_DB) ? new pgBackRest::Db() : undef;

    # Create anonymous subs for each command
    my $hCommandMap =
    {
        # ArchiveGet commands
        &OP_ARCHIVE_GET_CHECK => sub {archiveGetCheck(@{shift()})},

        # File commands
        &OP_STORAGE_OPEN_READ => sub
        {
            my $oSourceFileIo = $oStorage->openRead(@{shift()});

            # If the source file exists
            if (defined($oSourceFileIo) && (!defined($oSourceFileIo->{oStorageCRead}) || $oSourceFileIo->open()))
            {
                $self->outputWrite(true);

                $oStorage->copy(
                    $oSourceFileIo, new pgBackRest::Protocol::Storage::File($self, $oSourceFileIo), {bSourceOpen => true});

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
        &OP_STORAGE_MANIFEST => sub {$oStorage->manifestJson(@{shift()})},
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
