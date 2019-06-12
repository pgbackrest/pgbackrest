####################################################################################################################################
# C Storage Read Interface
####################################################################################################################################
package pgBackRest::Storage::StorageRead;

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
        $self->{oStorageCRead},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorage'},
            {name => 'oStorageCRead'},
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

    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $self->{oStorageCRead}->open() ? true : false, trace => true},
    );
}

####################################################################################################################################
# Read data
####################################################################################################################################
sub read
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my (
        $strOperation,
        $rtBuffer,
        $iSize,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->read', \@_,
            {name => 'rtBuffer'},
            {name => 'iSize'},
        );

    # Read the block
    my $iActualSize = 0;

    if (!$self->eof())
    {
        my $tBuffer = $self->{oStorageCRead}->read($iSize);
        $iActualSize = length($tBuffer);
        $$rtBuffer .= $tBuffer;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iActualSize', value => $iActualSize}
    );
}

####################################################################################################################################
# Is the file at eof?
####################################################################################################################################
sub eof
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->eof');

    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $self->{oStorageCRead}->eof() ? true : false, trace => true},
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

    $self->{oStorageCRead}->close();

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

    my $xResult = $self->{oStorage}->{oJSON}->decode($self->{oStorageCRead}->result($strClass));

    return logDebugReturn
    (
        $strOperation,
        {name => 'xResult', value => $xResult, trace => true},
    );
}

####################################################################################################################################
# Get all filter results
####################################################################################################################################
sub resultAll
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->resultAll');

    my $xResult = $self->{oStorage}->{oJSON}->decode($self->{oStorageCRead}->resultAll());

    return logDebugReturn
    (
        $strOperation,
        {name => 'xResultAll', value => $xResult, trace => true},
    );
}

1;
