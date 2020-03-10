####################################################################################################################################
# ARCHIVE COMMON MODULE
####################################################################################################################################
package pgBackRest::Archive::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Config;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY);
use File::Basename qw(dirname);

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# RegEx constants
####################################################################################################################################
use constant REGEX_ARCHIVE_DIR_DB_VERSION                           => '^[0-9]+(\.[0-9]+)*-[0-9]+$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_DB_VERSION);
use constant REGEX_ARCHIVE_DIR_WAL                                  => '^[0-F]{16}$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_WAL);

####################################################################################################################################
# PostgreSQL WAL system id offset
####################################################################################################################################
use constant PG_WAL_SYSTEM_ID_OFFSET_GTE_93                         => 12 + $Config{ptrsize};
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_GTE_93);
use constant PG_WAL_SYSTEM_ID_OFFSET_LT_93                          => 12;
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_LT_93);

####################################################################################################################################
# WAL segment size
####################################################################################################################################
use constant PG_WAL_SEGMENT_SIZE                                    => 16777216;
    push @EXPORT, qw(PG_WAL_SEGMENT_SIZE);

1;
