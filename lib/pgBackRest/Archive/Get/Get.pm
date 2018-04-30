####################################################################################################################################
# ARCHIVE GET MODULE
####################################################################################################################################
package pgBackRest::Archive::Get::Get;
use parent 'pgBackRest::Archive::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);
use Scalar::Util qw(blessed);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Archive::Common;
use pgBackRest::Archive::Get::File;
use pgBackRest::Archive::Info;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceArchive,
        $strDestinationFile
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->process', \@_,
            {name => 'strSourceArchive'},
            {name => 'strDestinationFile'}
        );

    # Make sure the command happens on the db side
    if (!isDbLocal())
    {
        confess &log(ERROR, cfgCommandName(CFGCMD_ARCHIVE_GET) . ' operation must run on db host', ERROR_HOST_INVALID);
    }

    # Make sure the archive file is defined
    if (!defined($strSourceArchive))
    {
        confess &log(ERROR, 'WAL segment not provided', ERROR_PARAM_REQUIRED);
    }

    # Make sure the destination file is defined
    if (!defined($strDestinationFile))
    {
        confess &log(ERROR, 'WAL segment destination not provided', ERROR_PARAM_REQUIRED);
    }

    # Info for the Postgres log
    &log(INFO, 'get WAL segment ' . $strSourceArchive);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => archiveGetFile($strSourceArchive, $strDestinationFile), trace => true},
    );
}

1;
