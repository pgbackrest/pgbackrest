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
# cgenLookupString - build C function that looks up strings by id
####################################################################################################################################
sub cgenLookupString
{
    my $strName = shift;
    my $strTotal = shift;
    my $rhValue = shift;
    my $strFilter = shift;

    my $strLowerName = lc($strName);

    # Generate list of command strings
    my $strFunction =
        "const char *${strLowerName}NameList[${strTotal}] = \n" .
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
        "cfg${strName}Name(int ${strLowerName}Id)\n" .
        "{\n" .
        "    if (${strLowerName}Id < 0 || ${strLowerName}Id >= ${strTotal})\n" .
        "        return NULL;\n" .
        "\n" .
        "    return ${strLowerName}NameList[${strLowerName}Id];\n" .
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

    my $strLowerName = lc($strName);

    my $strFunction =
        "int\n" .
        "cfg${strName}Id(const char *${strLowerName}Name)\n" .
        "{\n" .
        "    for (int nameIdx = 0; nameIdx < ${strTotal}; nameIdx++)\n" .
        "        if (strcmp(${strLowerName}Name, cfg${strName}Name(nameIdx)) == 0)\n" .
        "            return nameIdx;\n" .
        "\n" .
        "    return -1;\n" .
        "}\n";

    return $strFunction;
}

push @EXPORT, qw(cgenLookupId);

1;
