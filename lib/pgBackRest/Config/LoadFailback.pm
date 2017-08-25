####################################################################################################################################
# Load C or Perl Config Code
####################################################################################################################################
package pgBackRest::Config::LoadFailback;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::LibCLoad;

####################################################################################################################################
# Load the C library if present, else failback to the Perl code
####################################################################################################################################
if (libC())
{
    require pgBackRest::LibC;
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
