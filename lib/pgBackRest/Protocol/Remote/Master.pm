####################################################################################################################################
# PROTOCOL REMOTE MASTER MODULE
####################################################################################################################################
package pgBackRest::Protocol::Remote::Master;
use parent 'pgBackRest::Protocol::Command::Master';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Command::Master;

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
        $strRemoteType,                             # Type of remote (DB or BACKUP)
        $strCommandSSH,                             # SSH client
        $strCommand,                                # Command to execute on local/remote
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $strHost,                                   # Host to connect to for remote (optional as this can also be used for local)
        $strUser,                                   # User to connect to for remote (must be set if strHost is set)
        $iProtocolTimeout                           # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strRemoteType'},
            {name => 'strCommandSSH'},
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'strHost'},
            {name => 'strUser'},
            {name => 'iProtocolTimeout'}
        );

    # Create SSH command
    $strCommand =
        "${strCommandSSH} -o LogLevel=error -o Compression=no -o PasswordAuthentication=no ${strUser}\@${strHost} '${strCommand}'";

    # Init object and store variables
    my $self = $class->SUPER::new(
        $strRemoteType, 'remote', "'$strHost remote'", $strCommand, $iBufferMax, $iCompressLevel, $iCompressLevelNetwork,
        $iProtocolTimeout);
    bless $self, $class;

    # Store the host
    $self->{strHost} = $strHost;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
