####################################################################################################################################
# C to Perl Interface
####################################################################################################################################
package pgBackRest::LibC;
use base 'Exporter';

use 5.010001;
use strict;
use warnings;
use Carp;

use pgBackRest::LibCAuto;

# Dynamically create constants
my $rhConstant = pgBackRest::LibCAuto::libcAutoConstant();

foreach my $strConstant (keys(%{$rhConstant}))
{
    eval "use constant ${strConstant} => '" . $rhConstant->{$strConstant} . "'";
}

# Export functions and constants
our %EXPORT_TAGS = %{pgBackRest::LibCAuto::libcAutoExportTag()};
our @EXPORT_OK;

foreach my $strSection (keys(%EXPORT_TAGS))
{
    # Assign values to serial constants like CFGCMD_* and CFGOPT_*.  New commands and options (especially options) renumber the list
    # and cause a lot of churn in the commits.  This takes care of the renumbering to cut down on that churn.
    my $strPrefixLast = 'XXXXXXXX';
    my $iConstantIdx = 0;

    foreach my $strConstant (@{$EXPORT_TAGS{$strSection}})
    {
        my $strPrefix = ($strConstant =~ m/^[A-Z0-9]+/g)[0];

        if (defined($strPrefix))
        {
            if ($strPrefix ne $strPrefixLast)
            {
                $iConstantIdx = 0;
            }
            else
            {
                $iConstantIdx++;
            }

            if ($strPrefix eq 'CFGCMD' || $strPrefix eq 'CFGOPT')
            {
                eval "use constant ${strConstant} => ${iConstantIdx}";
            }

            $strPrefixLast = $strPrefix;
        }
    }

    # OK to export everything in the tag
    push(@EXPORT_OK, @{$EXPORT_TAGS{$strSection}});
}

# Nothing is exported by default
our @EXPORT = ();

1;
