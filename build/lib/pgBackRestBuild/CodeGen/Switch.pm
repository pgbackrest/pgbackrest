####################################################################################################################################
# Generate C Switch Functions
####################################################################################################################################
package pgBackRestBuild::CodeGen::Switch;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;

use pgBackRestBuild::CodeGen::Common;

####################################################################################################################################
# Cost constants used by optimizer
####################################################################################################################################
# Cost for setting up a new switch statement
use constant COST_SWITCH                                            => 5;
# Cost for each case
use constant COST_SWITCH_CASE                                       => 1;
# Cost for a clause (which might be shared with multipe cases)
use constant COST_SWITCH_CLAUSE                                     => 2;

####################################################################################################################################
# cgenSwitchBuild - build switch statements to perform lookups
####################################################################################################################################
sub cgenSwitchBuild
{
    my $strName = shift;
    my $strType = shift;
    my $ryMatrix = dclone(shift);
    my $rstryParam = dclone(shift);
    my $rhLabelMap = shift;
    my $rhValueLabelMap = shift;

    # Build a hash with positions for the data in parameter order
    my $rhParamOrder = {};

    for (my $iIndex = 0; $iIndex < @{$rstryParam}; $iIndex++)
    {
        $rhParamOrder->{$rstryParam->[$iIndex]} = $iIndex;
    }

    # Try each permutation of param order to find the most efficient switch statement
    my $iBestCost;
    my $strBestSwitch;

    foreach my $rstryParamPermute (cgenPermute($rstryParam))
    {
        # Build a hash with positions for the data in permutation order
        my $rhPermuteOrder = {};

        for (my $iIndex = 0; $iIndex < @{$rstryParamPermute}; $iIndex++)
        {
            $rhPermuteOrder->{$rstryParamPermute->[$iIndex]} = $iIndex;
        }

        # Arrange data in permutation order so later functions don't have to remap for every operation
        my @yMatrixPermute;

        foreach my $rxyEntry (@{$ryMatrix})
        {
            my @xyEntryPermute;

            for (my $iSwapIdx = 0; $iSwapIdx < @{$rstryParam}; $iSwapIdx++)
            {
                $xyEntryPermute[$rhPermuteOrder->{$rstryParam->[$iSwapIdx]}] =
                    $rxyEntry->[$rhParamOrder->{$rstryParam->[$iSwapIdx]}];
            }

            # Convert the value to a special string if it is null to ease later prcessing
            $xyEntryPermute[@{$rstryParam}] = defined($rxyEntry->[-1]) ? $rxyEntry->[-1] : CGEN_DATAVAL_NULL;

            push(@yMatrixPermute, \@xyEntryPermute);
        }

        # Build switch based on current param permutation
        my ($strSwitch, $iCost) = cgenSwitchBuildSub(
            $strType, \@yMatrixPermute, 0, $rstryParamPermute, $rhLabelMap, $rhValueLabelMap);

        # If the switch has a lower cost than the existing one then use it instead
        if (!defined($iBestCost) || $iCost < $iBestCost)
        {
            $iBestCost = $iCost;
            $strBestSwitch = $strSwitch;
        }
    }

    # Construct the function based on the best switch statement
    return
        cgenTypeName($strType) . "\n" .
        "${strName}(uint32 " . join(', uint32 ', @{$rstryParam}) . ")\n" .
        "{\n" .
        "${strBestSwitch}\n" .
        "}\n";
}

push @EXPORT, qw(cgenSwitchBuild);

####################################################################################################################################
# cgenSwitchBuildSub - build individual switch statements recursively
####################################################################################################################################
sub cgenSwitchBuildSub
{
    my $strType = shift;
    my $rstryMatrix = dclone(shift);
    my $iDepth = shift;
    my $rstryParam = dclone(shift);
    my $rhLabelMap = shift;
    my $rhValueLabelMap = shift;
    my $xMostCommonParentValue = shift;

    # How much to indent the code is based on the current depth
    my $strIndent = ('    ' x (($iDepth * 2) + 1));

    # Set initial cost for setting up the switch statement
    my $iCost = COST_SWITCH;

    # Get the param to be used for the switch statement
    my $strParam = shift(@{$rstryParam});

    # Determine the most common value
    my $iMostCommonTotal = 0;
    my $xMostCommonValue;
    my $rhMostCommon = {};

    foreach my $rxyEntry (@{$rstryMatrix})
    {
        my $xValue = $rxyEntry->[-1];

        $rhMostCommon->{$xValue} = (defined($rhMostCommon->{$xValue}) ? $rhMostCommon->{$xValue} : 0) + 1;

        if ($rhMostCommon->{$xValue} > $iMostCommonTotal)
        {
            $iMostCommonTotal = $rhMostCommon->{$xValue};
            $xMostCommonValue = $xValue;
        }
    }

    # Keep going until all keys are exhausted
    my $rhClauseHash;

    while (@{$rstryMatrix} > 0)
    {
        # Start processing the first key in the list
        my $strKeyTop = $rstryMatrix->[0][0];

        # A list of keys values to be passed to the next switch statement
        my @stryEntrySub;

        # Find all instances of the key and build a hash representing the distinct list of values
        my $rhKeyValue = {};
        my $iEntryIdx = 0;

        while ($iEntryIdx < @{$rstryMatrix})
        {
            # If this key matches the top key then process
            if ($rstryMatrix->[$iEntryIdx][0] eq $strKeyTop)
            {
                # Add value to unique list
                $rhKeyValue->{$rstryMatrix->[$iEntryIdx][-1]} = true;

                # Get the key/value entry, remove the top key, and store for the next switch statement
                my @stryEntry = @{$rstryMatrix->[$iEntryIdx]};
                shift(@stryEntry);
                push(@stryEntrySub, \@stryEntry);

                # Remove the key from the list
                splice(@{$rstryMatrix}, $iEntryIdx, 1);
            }
            # else move on to the next key
            else
            {
                $iEntryIdx++;
            }
        };

        # Only need a switch if there is more than one value or the one value is not the most common value
        if (keys(%{$rhKeyValue}) > 1 || $stryEntrySub[0][-1] ne $xMostCommonValue)
        {
            my $strClause = '';

            $iCost += COST_SWITCH_CASE;

            # Process next switch
            if (keys(%{$rhKeyValue}) > 1 && @{$stryEntrySub[0]} > 1)
            {
                my ($strClauseSub, $iCostSub) = cgenSwitchBuildSub(
                    $strType, \@stryEntrySub, $iDepth + 1, $rstryParam, $rhLabelMap, $rhValueLabelMap, $xMostCommonValue);

                $strClause .= $strClauseSub . "\n";
                $iCost += $iCostSub;
            }
            # Return the value
            else
            {
                my $strRetVal = $stryEntrySub[0][-1];

                $strClause .=
                    "${strIndent}        return " .
                    cgenTypeFormat(
                        $strType,
                        defined($rhValueLabelMap) && defined($rhValueLabelMap->{$strRetVal}) ?
                            $rhValueLabelMap->{$strRetVal} : $strRetVal) .
                    ";\n";
            }

            # Store the key and the clause in a hash to deduplicate
            push(@{$rhClauseHash->{$strClause}{key}}, int($strKeyTop));
        }
    }

    # Reorder clause based on an numerical/alpha representation of the case statements. This is done for primarily for readability
    # but may have some optimization benefits since integer keys are ordered numerically.
    my $rhClauseOrderedHash;

    foreach my $strClause (sort(keys(%{$rhClauseHash})))
    {
        my $strKey;

        foreach my $iKey (@{$rhClauseHash->{$strClause}{key}})
        {
            $strKey .= (defined($strKey) ? '' : ',') . sprintf('%07d', $iKey);
        }

        $rhClauseOrderedHash->{$strKey}{clause} = $strClause;
        $rhClauseOrderedHash->{$strKey}{key} = $rhClauseHash->{$strClause}{key};
    }

    # Build the switch statement
    my $bFirst = true;
    my $strFunction =
        "${strIndent}switch (${strParam})\n" .
        "${strIndent}{\n";

    # Retrieve each unique clause and create a case for each key assocated with it
    foreach my $strKey (sort(keys(%{$rhClauseOrderedHash})))
    {
        if (!$bFirst)
        {
            $strFunction .= "\n";
        }

        foreach my $strKey (@{$rhClauseOrderedHash->{$strKey}{key}})
        {
            $iCost += COST_SWITCH_CLAUSE;

            $strFunction .=
                "${strIndent}    case " .
                (defined($rhLabelMap->{$strParam}) ? $rhLabelMap->{$strParam}{int($strKey)} : int($strKey)) . ":\n";
        }

        $strFunction .= $rhClauseOrderedHash->{$strKey}{clause};
        $bFirst = false;
    }

    $strFunction .=
        "${strIndent}}\n\n";

    # If the most common value is the same as the parent then break instead of returning the same value.  Returning the same value
    # again might be slightly more efficient but the break makes it easier to debug where values are coming from.
    if (defined($xMostCommonParentValue) && $xMostCommonValue eq $xMostCommonParentValue)
    {
        $strFunction .=
            "${strIndent}break;";
    }
    # Else return the most common value
    else
    {
        $strFunction .=
            "${strIndent}return " .
                cgenTypeFormat(
                    $strType,
                    defined($rhValueLabelMap) && defined($rhValueLabelMap->{$xMostCommonValue}) ?
                        $rhValueLabelMap->{$xMostCommonValue} : $xMostCommonValue) .
                ";";
    }

    return $strFunction, $iCost;
}

1;
