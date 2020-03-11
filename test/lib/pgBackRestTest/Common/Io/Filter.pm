####################################################################################################################################
# Base Filter Module
####################################################################################################################################
package pgBackRestTest::Common::Io::Filter;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Scalar::Util qw(blessed);

use pgBackRestDoc::Common::Log;

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
        $self->{oParent},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParent', trace => true},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Pass through for unimplemented methods
####################################################################################################################################
sub bufferMax {shift->{oParent}->bufferMax()};
sub className {shift->{oParent}->className()};
sub close {shift->{oParent}->close()};
sub eof {shift->{oParent}->eof()};
sub eofSet {shift->{oParent}->eofSet(@_)};
sub error {shift->{oParent}->error(@_)};
sub id {shift->{oParent}->id()};
sub handleRead {shift->{oParent}->handleRead()};
sub handleReadSet {shift->{oParent}->handleReadSet(@_)};
sub handleWrite {shift->{oParent}->handleWrite()};
sub handleWriteSet {shift->{oParent}->handleWriteSet(@_)};
sub name {shift->{oParent}->name()};
sub read {shift->{oParent}->read(@_)};
sub readLine {shift->{oParent}->readLine(@_)};
sub size {shift->{oParent}->size()};
sub timeout {shift->{oParent}->timeout()};
sub write {shift->{oParent}->write(@_)};
sub writeLine {shift->{oParent}->writeLine(@_)};

####################################################################################################################################
# Getters
####################################################################################################################################
sub parent {shift->{oParent}}

1;
