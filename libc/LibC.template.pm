####################################################################################################################################
# C to Perl Interface
#
# {[LIBC_AUTO_WARNING]}
####################################################################################################################################
package pgBackRest::LibC;

use 5.010001;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Library version (.999 indicates development version)
our $VERSION = '{[LIBC_VERSION]}';

sub libCVersion {return $VERSION};

# Configuration option value constants
use constant
{
    {[LIBC_CONSTANT]}
};

# Export function and constants
our %EXPORT_TAGS =
(
    {[LIBC_EXPORT_TAGS]}
);

our @EXPORT_OK = (
    {[LIBC_EXPORT_OK]}
);

# Nothing is exported by default
our @EXPORT = qw();

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
