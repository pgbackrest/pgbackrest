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
    my $self = $class->SUPER::new(
        'local', "local-${iProcessIdx} process", $strCommand, cfgOption(CFGOPT_BUFFER_SIZE),
        cfgOption(CFGOPT_COMPRESS_LEVEL), cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK), cfgOption(CFGOPT_PROTOCOL_TIMEOUT));

    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
