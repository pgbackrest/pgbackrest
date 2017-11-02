####################################################################################################################################
# C to Perl Interface
####################################################################################################################################
package pgBackRest::LibC;

use 5.010001;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;
our @ISA = qw(Exporter);

use pgBackRest::LibCAuto;

# Library version
our $VERSION = pgBackRest::LibCAuto::libcAutoVersion();

sub libcVersion
{
    return $VERSION;
}

# Dynamically create constants
my $rhConstant = pgBackRest::LibCAuto::libcAutoConstant();

foreach my $strConstant (keys(%{$rhConstant}))
{
    eval "use constant ${strConstant} => '" . $rhConstant->{$strConstant} . "'";
}

# Export function and constants
our %EXPORT_TAGS = %{pgBackRest::LibCAuto::libcAutoExportTag()};
our @EXPORT_OK;

foreach my $strSection (keys(%EXPORT_TAGS))
{
    push(@EXPORT_OK, @{$EXPORT_TAGS{$strSection}});
}

# Nothing is exported by default
our @EXPORT = ();

# Autoload constants from the constant() XS function
sub AUTOLOAD
{
    my $strConstantFunctionName;
    our $AUTOLOAD;

    ($strConstantFunctionName = $AUTOLOAD) =~ s/.*:://;

    croak "&pgBackRest::LibC::constant not defined"
        if $strConstantFunctionName eq 'constant';
    my ($error, $val) = constant($strConstantFunctionName);
    if ($error) {croak $error;}

    {
        no strict 'refs';
        *$AUTOLOAD = sub {$val};
    }

    goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('pgBackRest::LibC', $VERSION);

1;
__END__
