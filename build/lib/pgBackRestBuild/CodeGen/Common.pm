####################################################################################################################################
# C Code Generation Formatting Functions
####################################################################################################################################
package pgBackRestBuild::CodeGen::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;

####################################################################################################################################
# Generate functions that return config rule data
####################################################################################################################################
use constant CGEN_DATATYPE_BOOL                                     => 'bool';
    push @EXPORT, qw(CGEN_DATATYPE_BOOL);
use constant CGEN_DATATYPE_DOUBLE                                   => 'double';
    push @EXPORT, qw(CGEN_DATATYPE_DOUBLE);
use constant CGEN_DATATYPE_INT32                                    => 'int32';
    push @EXPORT, qw(CGEN_DATATYPE_INT32);
use constant CGEN_DATATYPE_INT64                                    => 'int64';
    push @EXPORT, qw(CGEN_DATATYPE_INT64);
use constant CGEN_DATATYPE_CONSTCHAR                                => 'const char *';
    push @EXPORT, qw(CGEN_DATATYPE_CONSTCHAR);

use constant CGEN_DATAVAL_NULL                                      => '^^{{[[NULL]]}}^^';
    push @EXPORT, qw(CGEN_DATAVAL_NULL);

####################################################################################################################################
# cgenFunction - format type names for C
####################################################################################################################################
sub cgenFunction
{
    my $strName = shift;
    my $strSummary = shift;
    my $strDescription = shift;
    my $strSource = shift;

    # Summary should always be defined
    if (!defined($strSummary))
    {
        confess &log(ASSERT, "${strName} must have a summary");
    }

    # Make sure summary will fit on a single line
    my $iLenMax = 132 - 3 - length($strName);

    if (length($strSummary) > $iLenMax)
    {
        &log(ASSERT, "summary for ${strName} must be <= ${iLenMax} characters");
    }

    my $strFunction =
        qw{/} . (qw{*} x 131) . "\n" .
        $strName . (defined($strSummary) ? " - ${strSummary}" : '') . "\n" .
        (qw{*} x 131) . qw{/} . "\n" .
        $strSource;

    return $strFunction;
}

push @EXPORT, qw(cgenFunction);

####################################################################################################################################
# cgenTypeFormat - format types values for C
####################################################################################################################################
sub cgenTypeFormat
{
    my $strType = shift;
    my $strValue = shift;

    if ($strType eq CGEN_DATATYPE_BOOL)
    {
        return ($strValue == 1 ? 'true' : 'false');
    }
    elsif ($strType eq CGEN_DATATYPE_INT32 || $strType eq CGEN_DATATYPE_INT64 || $strType eq CGEN_DATATYPE_DOUBLE)
    {
        return $strValue;
    }
    elsif ($strType eq CGEN_DATATYPE_CONSTCHAR)
    {
        return ($strValue eq CGEN_DATAVAL_NULL ? 'NULL' : "\"${strValue}\"");
    }

    confess "unknown type ${strType}";
}

push @EXPORT, qw(cgenTypeFormat);

####################################################################################################################################
# cgenTypeName - format type names for C
####################################################################################################################################
sub cgenTypeName
{
    my $strType = shift;

    if ($strType eq CGEN_DATATYPE_BOOL)
    {
        return 'bool';
    }
    elsif ($strType eq CGEN_DATATYPE_INT32)
    {
        return 'int32';
    }
    elsif ($strType eq CGEN_DATATYPE_INT64)
    {
        return 'int64';
    }
    elsif ($strType eq CGEN_DATATYPE_DOUBLE)
    {
        return 'double';
    }
    elsif ($strType eq CGEN_DATATYPE_CONSTCHAR)
    {
        return 'const char *';
    }

    confess "unknown type ${strType}";
}

push @EXPORT, qw(cgenTypeName);

####################################################################################################################################
# cgenPermute - return all permutations of an array
####################################################################################################################################
sub cgenPermute
{
    my $rxyList = shift;

    # If there are one or less elements then the existing array represents the only permutation
    return map [$_], @$rxyList if @{$rxyList} <= 1;

    # Calculate permutations
    my @xyPermutation;

    for my $iIndex (0 .. $#{$rxyList})
    {
        my @xyRemaining = @{$rxyList};
        my $xValue  = splice(@xyRemaining, $iIndex, 1);

        push @xyPermutation, [$xValue, @$_] for cgenPermute(\@xyRemaining, @{$rxyList} - 1);
    }

  return @xyPermutation;
}

push @EXPORT, qw(cgenPermute);

1;
