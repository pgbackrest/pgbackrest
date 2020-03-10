####################################################################################################################################
# INFO MODULE
# Constants, variables and functions common to the info files
####################################################################################################################################
package pgBackRestTest::Env::InfoCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# DB section constants
####################################################################################################################################
use constant INFO_BACKUP_SECTION_DB                                 => 'db';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB);
use constant INFO_BACKUP_SECTION_DB_HISTORY                         => INFO_BACKUP_SECTION_DB . ':history';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB_HISTORY);

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
