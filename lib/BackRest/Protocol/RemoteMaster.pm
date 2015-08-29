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
use BackRest::Common::Log;
use BackRest::Config;
use BackRest::Protocol::CommonMaster;

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
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,                                # Command to execute on local/remote
        $iBlockSize,                                # Buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $strHost,                                   # Host to connect to for remote (optional as this can also be used for local)
        $strUser                                    # User to connect to for remote (must be set if strHost is set)
    ) =
        logDebugParam
        (
            OP_PROTOCOL_REMOTE_MASTER_NEW, \@_,
            {name => 'strCommand'},
            {name => 'iBlockSize'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'strHost'},
            {name => 'strUser'}
        );

    # Create SSH command
    $strCommand = "ssh -o Compression=no -o PasswordAuthentication=no ${strUser}\@${strHost} '${strCommand}'";

    # Init object and store variables
    my $self = $class->SUPER::new('remote', $strCommand, $iBlockSize, $iCompressLevel, $iCompressLevelNetwork);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
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
