####################################################################################################################################
# Base Storage Module
####################################################################################################################################
package pgBackRestTest::Common::StorageBase;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Common::Io::Base;

####################################################################################################################################
# Storage constants
####################################################################################################################################
use constant STORAGE_POSIX                                          => 'posix';
    push @EXPORT, qw(STORAGE_POSIX);

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
        $self->{lBufferMax},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'lBufferMax', optional => true, default => COMMON_IO_BUFFER_MAX, trace => true},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
