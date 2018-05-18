####################################################################################################################################
# Parameters used by LibC builds
####################################################################################################################################
package pgBackRestLibC::BuildParam;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# All CC params used for a debug build
####################################################################################################################################
sub buildParamCCDebug
{
    return qw(
        -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -Wno-missing-field-initializers
        -o $@
        -std=c99
        -D_FILE_OFFSET_BITS=64
        $(CFLAGS));
}

push @EXPORT, qw(buildParamCCDebug);

####################################################################################################################################
# All CC params used for a production build
####################################################################################################################################
sub buildParamCCAll
{
    my @stryParams = buildParamCCDebug;

    push(@stryParams, qw(
        -DNDEBUG
        -funroll-loops
        -ftree-vectorize));

    return @stryParams;
}

push @EXPORT, qw(buildParamCCAll);

1;
