####################################################################################################################################
# PROTOCOL REMOTE MASTER MODULE
####################################################################################################################################
package BackRest::Protocol::RemoteMaster;
use parent 'BackRest::Protocol::CommonMaster';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Config;
use BackRest::Protocol::CommonMaster;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_PROTOCOL_REMOTE_MASTER                              => 'Protocol::RemoteMaster';

use constant OP_PROTOCOL_REMOTE_MASTER_NEW                          => OP_PROTOCOL_REMOTE_MASTER . "->new";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $strCommand = shift;             # Command to execute on local/remote
    my $iBlockSize = shift;             # Buffer size
    my $iCompressLevel = shift;         # Set compression level
    my $iCompressLevelNetwork = shift;  # Set compression level for network only compression
    my $strHost = shift;                # Host to connect to for remote (optional as this can also be used for local)
    my $strUser = shift;                # User to connect to for remote (must be set if strHost is set)

    # Debug
    logDebug(OP_PROTOCOL_REMOTE_MASTER_NEW, DEBUG_CALL, undef,
             {command => \$strCommand, host => \$strHost, user => \$strUser, blockSize => $iBlockSize,
              compressLevel => $iCompressLevel, compressLevelNetwork => $iCompressLevelNetwork});

    # Host must be defined
    if (!defined($strHost))
    {
        confess &log(ASSERT, 'strHost must be defined');
    }

    # User must be defined
    if (!defined($strUser))
    {
        confess &log(ASSERT, 'strUser must be defined');
    }

    # Command must be defined
    if (!defined($strCommand))
    {
        confess &log(ASSERT, 'strCommand must be defined');
    }

    # Create SSH command
    $strCommand = "ssh -o Compression=no -o PasswordAuthentication=no ${strUser}\@${strHost} '${strCommand}'";

    # Init object and store variables
    my $self = $class->SUPER::new('remote', $strCommand, $iBlockSize, $iCompressLevel, $iCompressLevelNetwork);
    bless $self, $class;

    return $self;
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;

    return BackRest::Protocol::RemoteMaster->new
    (
        $self->{strCommand},
        $self->{iBlockSize},
        $self->{iCompressLevel},
        $self->{iCompressLevelNetwork},
        $self->{strHost},
        $self->{strUser}
    );
}

1;
