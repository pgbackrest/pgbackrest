####################################################################################################################################
# Load C or Perl Config Code
####################################################################################################################################
package pgBackRest::Config::Load;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;

####################################################################################################################################
# Load the C library if present, else failback to the Perl ode
####################################################################################################################################
my $bLibC = false;

eval
{
    # Attempt to load the C Library
    require pgBackRest::LibC;
    $bLibC = true;

    return 1;
} or do {};

if ($bLibC)
{
    pgBackRest::LibC->import(qw(:config :configRule));
    push(@EXPORT, @{$pgBackRest::LibC::EXPORT_TAGS{config}});
    push(@EXPORT, @{$pgBackRest::LibC::EXPORT_TAGS{configRule}});
}
else
{
    require pgBackRest::Config::Data;
    pgBackRest::Config::Data->import();
    push(@EXPORT, @pgBackRest::Config::Data::EXPORT);

    require pgBackRest::Config::Rule;
    pgBackRest::Config::Rule->import();
    push(@EXPORT, @pgBackRest::Config::Rule::EXPORT);
}

1;
