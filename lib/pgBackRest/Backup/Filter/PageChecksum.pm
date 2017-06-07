####################################################################################################################################
# BACKUP FILTER PAGE CHECKSUM MODULE
####################################################################################################################################
package pgBackRest::Backup::Filter::PageChecksum;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::DbVersion qw(PG_PAGE_SIZE);

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant BACKUP_FILTER_PAGECHECKSUM                             => __PACKAGE__;
    push @EXPORT, qw(BACKUP_FILTER_PAGECHECKSUM);

####################################################################################################################################
# Load the C library if present
####################################################################################################################################
my $bLibC = false;

eval
{
    # Load the C library only if page checksums are required
    require pgBackRest::LibC;
    pgBackRest::LibC->import(qw(:checksum));

    $bLibC = true;

    return 1;
} or do {};

####################################################################################################################################
# isLibC
#
# Does the C library exist?
####################################################################################################################################
sub isLibC
{
    return $bLibC;
}

push @EXPORT, qw(isLibC);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
our @ISA = ();                                                      ## no critic (ClassHierarchies::ProhibitExplicitISA)

sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $self,
        $iSegmentNo,
        $iWalId,
        $iWalOffset,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'self', trace => true},
            {name => 'iSegmentNo', trace => true},
            {name => 'iWalId', trace => true},
            {name => 'iWalOffset', trace => true},
        );

    # Bless with new class
    @ISA = $self->isA();                                            ## no critic (ClassHierarchies::ProhibitExplicitISA)
    bless $self, $class;

    # Set variables
    $self->{iSegmentNo} = $iSegmentNo;
    $self->{iWalId} = $iWalId;
    $self->{iWalOffset} = $iWalOffset;

    # Create the result object
    $self->{hResult}{bValid} = true;
    $self->{hResult}{bAlign} = true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - validate page checksums
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    # Call the io method
    my $iActualSize = $self->SUPER::read($rtBuffer, $iSize);

    # Validate page checksums for the read block
    if ($iActualSize > 0)
    {
        # If the buffer is not divisible by 0 then it's not valid
        if (!$self->{hResult}{bAlign} || ($iActualSize % PG_PAGE_SIZE != 0))
        {
            if (!$self->{hResult}{bAlign})
            {
                confess &log(ASSERT, "should not be possible to see two misaligned blocks in a row");
            }

            $self->{hResult}{bValid} = false;
            $self->{hResult}{bAlign} = false;
            delete($self->{hResult}{iyPageError});
        }
        else
        {
            # Calculate offset to the first block in the buffer
            my $iBlockOffset = int(($self->size() - $iActualSize) / PG_PAGE_SIZE) + ($self->{iSegmentNo} * 131072);

            if (!pageChecksumBufferTest(
                    $$rtBuffer, $iActualSize, $iBlockOffset, PG_PAGE_SIZE, $self->{iWalId},
                    $self->{iWalOffset}))
            {
                $self->{hResult}{bValid} = false;

                # Now figure out exactly where the errors occurred.  It would be more efficient if the checksum function returned an
                # array, but we're hoping there won't be that many errors to scan so this should work fine.
                for (my $iBlockNo = 0; $iBlockNo < int($iActualSize / PG_PAGE_SIZE); $iBlockNo++)
                {
                    my $iBlockNoStart = $iBlockOffset + $iBlockNo;

                    if (!pageChecksumTest(
                            substr($$rtBuffer, $iBlockNo * PG_PAGE_SIZE, PG_PAGE_SIZE), $iBlockNoStart, PG_PAGE_SIZE,
                            $self->{iWalId}, $self->{iWalOffset}))
                    {
                        my $iLastIdx = defined($self->{hResult}{iyPageError}) ? @{$self->{hResult}{iyPageError}} - 1 : 0;
                        my $iyLast = defined($self->{hResult}{iyPageError}) ? $self->{hResult}{iyPageError}[$iLastIdx] : undef;

                        if (!defined($iyLast) || (!ref($iyLast) && $iyLast != $iBlockNoStart - 1) ||
                            (ref($iyLast) && $iyLast->[1] != $iBlockNoStart - 1))
                        {
                            push(@{$self->{hResult}{iyPageError}}, $iBlockNoStart);
                        }
                        elsif (!ref($iyLast))
                        {
                            $self->{hResult}{iyPageError}[$iLastIdx] = undef;
                            push(@{$self->{hResult}{iyPageError}[$iLastIdx]}, $iyLast);
                            push(@{$self->{hResult}{iyPageError}[$iLastIdx]}, $iBlockNoStart);
                        }
                        else
                        {
                            $self->{hResult}{iyPageError}[$iLastIdx][1] = $iBlockNoStart;
                        }
                    }
                }
            }
        }
    }

    # Return the actual size read
    return $iActualSize;
}

####################################################################################################################################
# close - close and set the result
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->{hResult}))
    {
        # Set result
        $self->resultSet(BACKUP_FILTER_PAGECHECKSUM, $self->{hResult});

        # Delete the sha object
        undef($self->{hResult});

        # Close io
        return $self->SUPER::close();
    }
}

1;
