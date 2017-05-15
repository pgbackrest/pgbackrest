####################################################################################################################################
# PROTOCOL LOCAL MASTER MODULE
####################################################################################################################################
package pgBackRest::Protocol::Local::Master;
use parent 'pgBackRest::Protocol::Command::Master';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Backup::File;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Command::Master;
use pgBackRest::Protocol::Common::Common;

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
        $strCommand,
        $iProcessIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strCommand'},
            {name => 'iProcessIdx', default => 1},
        );

    # Init object and store variables
    my $strLocal = 'local-' . $iProcessIdx;

    my $self = $class->SUPER::new(
        NONE, 'local', $strLocal, $strCommand, optionGet(OPTION_BUFFER_SIZE), optionGet(OPTION_COMPRESS_LEVEL),
        optionGet(OPTION_COMPRESS_LEVEL_NETWORK), optionGet(OPTION_PROTOCOL_TIMEOUT));

    bless $self, $class;

    # Store the host
    $self->{strLocal} = $strLocal;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
