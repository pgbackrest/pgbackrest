####################################################################################################################################
# C Storage Write Interface
####################################################################################################################################
package pgBackRest::Storage::StorageWrite;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;

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
        $self->{oStorage},
        $self->{oStorageCWrite},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorage'},
            {name => 'oStorageCWrite'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Open the file
####################################################################################################################################
sub open
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->open');

    $self->{oStorageCWrite}->open();

    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => true, trace => true},
    );
}

####################################################################################################################################
# Read data
####################################################################################################################################
sub write
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my (
        $strOperation,
        $rtBuffer,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->write', \@_,
            {name => 'rtBuffer'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iActualSize', value => $self->{oStorageCWrite}->write($$rtBuffer)}
    );
}

####################################################################################################################################
# Close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->close');

    $self->{oStorageCWrite}->close();

    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => true, trace => true},
    );
}

####################################################################################################################################
# Get a filter result
####################################################################################################################################
sub result
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strClass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->result', \@_,
            {name => 'strClass'},
        );

    my $xResult = $self->{oStorage}->{oJSON}->decode($self->{oStorageCWrite}->result($strClass));

    return logDebugReturn
    (
        $strOperation,
        {name => 'xResult', value => $xResult, trace => true},
    );
}

1;
