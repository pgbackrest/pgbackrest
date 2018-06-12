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
    eval                    ## no critic (BuiltinFunctions::ProhibitStringyEval, ErrorHandling::RequireCheckingReturnValueOfEval)
        "use constant ${strConstant} => '" . $rhConstant->{$strConstant} . "'";
}

# Export functions and constants
our %EXPORT_TAGS = %{pgBackRest::LibCAuto::libcAutoExportTag()};
our @EXPORT_OK;

foreach my $strSection (keys(%EXPORT_TAGS))
{
    push(@EXPORT_OK, @{$EXPORT_TAGS{$strSection}});
}

# Nothing is exported by default
our @EXPORT = ();

1;
