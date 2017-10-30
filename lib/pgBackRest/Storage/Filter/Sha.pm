####################################################################################################################################
# SHA Filter
####################################################################################################################################
package pgBackRest::Storage::Filter::Sha;
use parent 'pgBackRest::Common::Io::Filter';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant STORAGE_FILTER_SHA                                     => __PACKAGE__;
    push @EXPORT, qw(STORAGE_FILTER_SHA);

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
        $oParent,
        $strAlgorithm,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParent', trace => true},
            {name => 'strAlgorithm', optional => true, default => 'sha1', trace => true},
        );

    # Bless with new class
    my $self = $class->SUPER::new($oParent);
    bless $self, $class;

    # Set variables
    $self->{strAlgorithm} = $strAlgorithm;

    # Create SHA object
    $self->{oSha} = Digest::SHA->new($self->{strAlgorithm});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - calculate sha digest
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    # Call the io method
    my $tShaBuffer;
    my $iActualSize = $self->parent()->read(\$tShaBuffer, $iSize);

    # Calculate sha for the returned buffer
    if ($iActualSize > 0)
    {
        $self->{oSha}->add($tShaBuffer);
        $$rtBuffer .= $tShaBuffer;
    }

    # Return the actual size read
    return $iActualSize;
}

####################################################################################################################################
# write - calculate sha digest
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Calculate sha for the buffer
    $self->{oSha}->add($$rtBuffer);

    # Call the io method
    return $self->parent()->write($rtBuffer);
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->{oSha}))
    {
        # Set result
        $self->resultSet(STORAGE_FILTER_SHA, $self->{oSha}->hexdigest());

        # Delete the sha object
        delete($self->{oSha});

        # Close io
        return $self->parent->close();
    }

    return false;
}

1;
