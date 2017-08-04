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
# cgenSwitchBuild - build switch statements to perform lookups
####################################################################################################################################
sub cgenSwitchBuild
{
    my $strName = shift;
    my $strType = shift;
    my $ryMatrix = dclone(shift);
    my $rstryParam = dclone(shift);
    my $rstryParamSwitch = dclone(shift);
    my $rhLabelMap = shift;
    my $rhValueLabelMap = shift;

    my $rhValueMatrix;
    my $lMostCommonTotal = 0;
    my $strMostCommonValue;

    foreach my $ryEntry (@{$ryMatrix})
    {
        my @ySubMatrix = @{$ryEntry};
        my $strValue = coalesce(shift(@ySubMatrix), CGEN_DATAVAL_NULL);

        if (!defined($rhValueMatrix))
        {
            $rhValueMatrix->{$strValue}{total} = 0;
            $rhValueMatrix->{$strValue}{matrix} = [];
        }

        $rhValueMatrix->{$strValue}{total}++;
        push(@{$rhValueMatrix->{$strValue}{matrix}}, \@ySubMatrix);

        if ($rhValueMatrix->{$strValue}{total} > $lMostCommonTotal)
        {
            $lMostCommonTotal = $rhValueMatrix->{$strValue}{total};
            $strMostCommonValue = $strValue;
        }
    }

    # print STDERR "MOST COMMON ${strMostCommonValue} (${lMostCommonTotal})\n";

    # Remove the most common value from switch statement -- it will be returned by default at the end of the function
    delete($rhValueMatrix->{$strMostCommonValue});

    my @stryInterimMatrix;

    foreach my $strValue (sort(keys(%{$rhValueMatrix})))
    {
        foreach my $ryEntry (@{$rhValueMatrix->{$strValue}{matrix}})
        {
            my $strEntry;

            foreach my $strEntrySub (@{$ryEntry})
            {
                $strEntry .= defined($strEntry) ? ',' : '';

                if (!defined($strEntrySub))
                {
                    $strEntry .= 'XXXXXX';
                }
                else
                {
                    $strEntry .= sprintf('%06d', $strEntrySub);
                }
            }

            $strEntry .= ",$strValue";

            push(@stryInterimMatrix, $strEntry);
        }
    }

    @stryInterimMatrix = sort(@stryInterimMatrix);

    return
        cgenTypeName($strType) . "\n" .
        "${strName}(uint32 " . join(', uint32 ', @{$rstryParam}) . ")\n" .
        "{\n" .
        cgenSwitchBuildSub($strType, \@stryInterimMatrix, 0, $rstryParamSwitch, $rhLabelMap, $rhValueLabelMap) .
        "\n" .
        "    return " .
            cgenTypeFormat(
                $strType,
                defined($rhValueLabelMap) && defined($rhValueLabelMap->{$strMostCommonValue}) ?
                    $rhValueLabelMap->{$strMostCommonValue} : $strMostCommonValue) .
            ";\n" .
        "}\n";
}

push @EXPORT, qw(cgenSwitchBuild);

####################################################################################################################################
# cgenSwitchBuildSub - build individual switch statements recursively
####################################################################################################################################
sub cgenSwitchBuildSub
{
    my $strType = shift;
    my $rstryMatrix = shift;
    my $iDepth = shift;
    my $rstryParam = dclone(shift);
    my $rhLabelMap = shift;
    my $rhValueLabelMap = shift;

    my $strIndent = ('    ' x (($iDepth * 2) + 1));

    my $strParam = shift(@{$rstryParam});

    my $strFunction =
        "${strIndent}switch (${strParam})\n" .
        "${strIndent}{\n";

    # print STDERR "DEPTH ${iDepth}\n";

    my $rhClauseHash;

    while (@{$rstryMatrix} > 0)
    {
        my @stryEntry = split(',', $rstryMatrix->[0]);
        my $strKeyTop = $stryEntry[0];
        my $strRetVal;
        my @stryEntrySub;

        # print STDERR "KEY $strKeyTop\n";

        while (@{$rstryMatrix} > 0 && $stryEntry[0] eq $strKeyTop)
        {
            my $strEntry = shift(@{$rstryMatrix});
            # print STDERR "ENTRY $strEntry\n";
            @stryEntry = split(',', $strEntry);

            if (@stryEntry <= 2 || $stryEntry[1] eq 'XXXXXX')
            {
                $strRetVal = $stryEntry[-1];
                # print STDERR "RETVAL ${strRetVal}\n";
            }
            else
            {
                shift(@stryEntry);
                # print STDERR "SUB " . join(',', @stryEntry) . "\n";
                push(@stryEntrySub, join(',', @stryEntry));
            }

            if (@{$rstryMatrix} > 0)
            {
                @stryEntry = split(',', $rstryMatrix->[0]);
            }
        };

        my $strClause = '';

        if (@stryEntrySub > 0)
        {
            $strClause .= cgenSwitchBuildSub(
                $strType, \@stryEntrySub, $iDepth + 1, $rstryParam, $rhLabelMap, $rhValueLabelMap) . "\n";
        }

        if (defined($strRetVal))
        {
            $strClause .=
                "${strIndent}        return " .
                cgenTypeFormat(
                    $strType,
                    defined($rhValueLabelMap) && defined($rhValueLabelMap->{$strRetVal}) ?
                        $rhValueLabelMap->{$strRetVal} : $strRetVal) .
                ";\n";
        }
        else
        {
            $strClause .= "${strIndent}        break;\n";
        }

        if (defined($rhClauseHash->{$strClause}))
        {
            push(@{$rhClauseHash->{$strClause}{key}}, int($strKeyTop));
        }
        else
        {
            $rhClauseHash->{$strClause}{key} = [int($strKeyTop)];
        }
    }

    my $bFirst = true;

    foreach my $strClause (sort(keys(%{$rhClauseHash})))
    {
        if (!$bFirst)
        {
            $strFunction .= "\n";
        }

        foreach my $strKey (@{$rhClauseHash->{$strClause}{key}})
        {
            $strFunction .=
                "${strIndent}    case " .
                (defined($rhLabelMap->{$strParam}) ? $rhLabelMap->{$strParam}{int($strKey)} : int($strKey)) . ":\n";
        }

        $strFunction .= $strClause;
        $bFirst = false;
    }

    $strFunction .= "${strIndent}}\n";

    return $strFunction;
}

1;
