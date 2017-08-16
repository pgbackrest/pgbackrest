####################################################################################################################################
# Generate Truth Tables in Markdown Format
####################################################################################################################################
package pgBackRestBuild::CodeGen::Truth;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Log;

use pgBackRestBuild::CodeGen::Common;

####################################################################################################################################
# cgenTruthTable - Main builder
####################################################################################################################################
sub cgenTruthTable
{
    my $strFunction = shift;
    my $rstryParam = shift;
    my $strReturnType = shift;
    my $strReturnDefault = shift;
    my $rxyMatrix = shift;
    my $strRedundantParam = shift;
    my $rhParamValueMap = shift;
    my $rhReturnValueMap = shift;

    # Write table header
    my $strTruthTable = '| Function';

    for (my $iParamIdx = 0; $iParamIdx < @{$rstryParam}; $iParamIdx++)
    {
        $strTruthTable .= ' | ' . $rstryParam->[$iParamIdx];
    }

    $strTruthTable .=
        " | Result |\n" .
        '| --------';

    for (my $iParamIdx = 0; $iParamIdx < @{$rstryParam}; $iParamIdx++)
    {
        $strTruthTable .= ' | ' . ('-' x length($rstryParam->[$iParamIdx]));
    }

    $strTruthTable .=
        " | ------ |\n";

    # Format matrix data so that can be ordered (even by integer)
    my @stryOrderedMatrix;

    foreach my $rxyEntry (@{$rxyMatrix})
    {
        my $strEntry;

        for (my $iEntryIdx = 0; $iEntryIdx < @{$rxyEntry}; $iEntryIdx++)
        {
            my $strValue = defined($rxyEntry->[$iEntryIdx]) ? $rxyEntry->[$iEntryIdx] : CGEN_DATAVAL_NULL;

            if ($iEntryIdx != @{$rxyEntry} - 1)
            {
                $strEntry .= (defined($strEntry) ? '~' : '') . sprintf('%016d', $strValue);
            }
            else
            {
                $strEntry .= '~' . $strValue;
            }
        }

        push(@stryOrderedMatrix, $strEntry);
    }

    # Optimize away one parameter that is frequently redundant
    my $rhMatrixFilter;
    my @stryFilteredMatrix;

    if ($rstryParam->[0] eq $strRedundantParam)
    {
        foreach my $strEntry (sort(@stryOrderedMatrix))
        {
            my @xyEntry = split('\~', $strEntry);

            shift(@xyEntry);
            my $strValue = pop(@xyEntry);
            my $strKey = join('~', @xyEntry);

            push(@{$rhMatrixFilter->{$strKey}{entry}}, $strEntry);
            $rhMatrixFilter->{$strKey}{value}{$strValue} = true;
        }

        foreach my $strKey (sort(keys(%{$rhMatrixFilter})))
        {
            if (keys(%{$rhMatrixFilter->{$strKey}{value}}) == 1)
            {
                my $strEntry =

                push(
                    @stryFilteredMatrix,
                    CGEN_DATAVAL_NULL . "~${strKey}~" . (keys(%{$rhMatrixFilter->{$strKey}{value}}))[0]);
            }
            else
            {
                push(@stryFilteredMatrix, @{$rhMatrixFilter->{$strKey}{entry}});
            }
        }
    }
    else
    {
        @stryFilteredMatrix = @stryOrderedMatrix;
    }

    # Output function entry
    foreach my $strEntry (sort(@stryFilteredMatrix))
    {
        my @xyEntry = split('\~', $strEntry);
        my $strRow = '| ' . $strFunction;
        my $strValue;

        for (my $iEntryIdx = 0; $iEntryIdx < @xyEntry; $iEntryIdx++)
        {
            $strValue = $xyEntry[$iEntryIdx];
            $strRow .= ' | ';

            if ($iEntryIdx != @xyEntry - 1)
            {
                my $strParam = $rstryParam->[$iEntryIdx];

                if ($strValue eq CGEN_DATAVAL_NULL)
                {
                    $strRow .= '_\<ANY\>_';
                }
                else
                {
                    $strRow .=
                        '`' .
                        (defined($rhParamValueMap->{$strParam}) ?
                            $rhParamValueMap->{$strParam}{int($strValue)} : int($strValue)) .
                        '`';
                }
            }
            else
            {
                $strRow .=
                    '`' .
                    cgenTypeFormat(
                        $strReturnType,
                        defined($rhReturnValueMap) &&
                            defined($rhReturnValueMap->{$strValue}) ?
                            $rhReturnValueMap->{$strValue} : $strValue) .
                    '`';

            }
        }

        # If default default value is returned then skip this entry
        if (defined($strReturnDefault) && $strReturnDefault eq $strValue)
        {
            next;
        }

        $strTruthTable .= "${strRow} |\n";
    }

    return $strTruthTable;
}

push @EXPORT, qw(cgenTruthTable);

1;
