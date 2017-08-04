####################################################################################################################################
# Generate C Lookup Functions
####################################################################################################################################
package pgBackRestBuild::CodeGen::Lookup;

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
# cgenSwitchBuild - build C function that looks up strings by id
####################################################################################################################################
sub cgenLookupString
{
    my $strName = shift;
    my $strTotal = shift;
    my $rhValue = shift;
    my $strFilter = shift;

    # Generate list of command strings
    my $strFunction =
        "const char *szy${strName}Name[${strTotal}] = \n" .
        "{\n";

    my $bFirst = true;

    foreach my $strLookupName (sort(keys(%{$rhValue})))
    {
        next if defined($strFilter) && $strLookupName =~ $strFilter;

        $strFunction .= ($bFirst ? '' : ",\n") . "    \"${strLookupName}\"";
        $bFirst = false;
    }

    $strFunction .=
        "\n};\n\n";

    $strFunction .=
        "const char *\n" .
        "cfg${strName}Name(uint32 ui${strName}Id)\n" .
        "{\n" .
        "    return szy${strName}Name[ui${strName}Id];\n" .
        "}\n";

    return $strFunction;
}

push @EXPORT, qw(cgenLookupString);

####################################################################################################################################
# cgenLookupId - build C function that looks up ids by string
####################################################################################################################################
sub cgenLookupId
{
    my $strName = shift;
    my $strTotal = shift;

    my $strFunction =
        "int32\n" .
        "cfg${strName}Id(const char *sz${strName}Name)\n" .
        "{\n" .
        "    for (uint32 uiIndex = 0; uiIndex < ${strTotal}; uiIndex++)\n" .
        "        if (strcmp(sz${strName}Name, cfg${strName}Name(uiIndex)) == 0)\n" .
        "            return uiIndex;\n" .
        "\n" .
        "    return -1;\n" .
        "}\n";

    return $strFunction;
}

push @EXPORT, qw(cgenLookupId);

1;
