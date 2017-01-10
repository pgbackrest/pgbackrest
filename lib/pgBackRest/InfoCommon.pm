####################################################################################################################################
# INFO MODULE
####################################################################################################################################
package pgBackRest::InfoCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# History section constants
####################################################################################################################################
use constant INFO_HISTORY_ID                                     => 'id';
    push @EXPORT, qw(INFO_HISTORY_ID);
use constant INFO_DB_VERSION                                     => 'version';
    push @EXPORT, qw(INFO_DB_VERSION);
use constant INFO_SYSTEM_ID                                      => 'system-id';
    push @EXPORT, qw(INFO_SYSTEM_ID);

1;
