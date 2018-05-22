####################################################################################################################################
# Auto-Generate Embedded Perl Modules
####################################################################################################################################
package pgBackRestBuild::Embed::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestBuild::Build::Common;

use pgBackRest::Common::Log;
use pgBackRest::Common::String;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'embed';

use constant BLDLCL_DATA_EMBED                                      => '01-dataEmbed';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Embed Perl modules';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_DATA =>
            {
                &BLDLCL_DATA_EMBED =>
                {
                    &BLD_SUMMARY => 'Embedded Perl modules',
                },
            },
        },
    },
};

####################################################################################################################################
# Embed Perl modules
####################################################################################################################################
sub buildEmbed
{
    my $oStorage = shift;

    # Output embedded modules array
    my $strBuildSource =
        "static const EmbeddedModule embeddedModule[] =\n" .
        "{\n";

    foreach my $strModule (sort(keys(%{$oStorage->manifest('lib')})))
    {
        # Ignore non-Perl modules
        if ($strModule =~ /\.pm$/)
        {
            # Load data
            my $strData = trim(${$oStorage->get("lib/${strModule}")});

            # Escape \ and quotes
            $strData =~ s/\\/\\\\/g;
            $strData =~ s/\"/\\\"/g;

            $strBuildSource .=
                "    {\n" .
                "        .name = \"${strModule}\",\n" .
                "        .data =\n";

            # Process each line
            foreach my $strLine (split("\n", $strData))
            {
                # Remove comments
                $strLine =~ s/^\s*\#.*$//;
                $strLine =~ s/\#[^\'\"]*$//;

                # Remove spacing in constant declarations
                if ($strLine =~ /use constant [A-Z0-9_]+/)
                {
                    $strLine =~ s/\s{2,}\=\> / \=\> /g;
                }

                # Remove leading/trailing spaces
                $strLine = trim($strLine);

                # Output line
                $strBuildSource .=
                    "            \"$strLine\\n\"\n";
            }

            $strBuildSource .=
                "    },\n";
        }
    }

    $strBuildSource .=
        "};";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_EMBED}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildEmbed);

1;
