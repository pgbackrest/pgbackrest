####################################################################################################################################
# S3 File Info
####################################################################################################################################
package pgBackRest::Storage::S3::Info;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Log;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{lSize},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'lSize'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub size {shift->{lSize}}

1;
