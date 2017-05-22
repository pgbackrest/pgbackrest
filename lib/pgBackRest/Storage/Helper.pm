####################################################################################################################################
# Helper functions for creating local storage objects.
####################################################################################################################################
package pgBackRest::Storage::Helper;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Storage::Local;
use pgBackRest::Version;

####################################################################################################################################
# Storage constants
####################################################################################################################################
use constant STORAGE_LOCAL                                          => '<LOCAL>';
    push @EXPORT, qw(STORAGE_LOCAL);

use constant STORAGE_SPOOL                                          => '<SPOOL>';
    push @EXPORT, qw(STORAGE_SPOOL);
use constant STORAGE_SPOOL_ARCHIVE_OUT                              => '<SPOOL:ARCHIVE:OUT>';
    push @EXPORT, qw(STORAGE_SPOOL_ARCHIVE_OUT);

####################################################################################################################################
# Compression extension
####################################################################################################################################
use constant COMPRESS_EXT                                           => 'gz';
    push @EXPORT, qw(COMPRESS_EXT);

####################################################################################################################################
# Temp file extension
####################################################################################################################################
use constant STORAGE_TEMP_EXT                                       => BACKREST_EXE . '.tmp';
    push @EXPORT, qw(STORAGE_TEMP_EXT);

####################################################################################################################################
# Cache storage so it can be retrieved quickly
####################################################################################################################################
my $hStorage;

####################################################################################################################################
# storageLocal - get local storage
#
# Local storage is always read-only and can never reference a remote path.  Used for adhoc activities like reading pgbackrest.conf.
####################################################################################################################################
sub storageLocal
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageLocal', \@_,
            {name => 'strPath', default => '/', trace => true},
        );

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_LOCAL}{$strPath}))
    {
        # Create local storage
        $hStorage->{&STORAGE_LOCAL}{$strPath} = new pgBackRest::Storage::Local(
            $strPath, new pgBackRest::Storage::Posix::Driver(),
            {lBufferMax => optionValid(OPTION_BUFFER_SIZE, false) ? optionGet(OPTION_BUFFER_SIZE, false) : undef});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageLocal', value => $hStorage->{&STORAGE_LOCAL}{$strPath}, trace => true},
    );
}

push @EXPORT, qw(storageLocal);

####################################################################################################################################
# storageSpool - get spool storage
####################################################################################################################################
sub storageSpool
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageSpool', \@_,
            {name => 'strStanza', default => optionGet(OPTION_STANZA), trace => true},
        );

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_SPOOL}{$strStanza}))
    {
        # Path rules
        my $hRule =
        {
            &STORAGE_SPOOL_ARCHIVE_OUT => "archive/${strStanza}/out",
        };

        # Create local storage
        $hStorage->{&STORAGE_SPOOL}{$strStanza} = new pgBackRest::Storage::Local(
            optionGet(OPTION_SPOOL_PATH), new pgBackRest::Storage::Posix::Driver(),
            {hRule => $hRule, bAllowTemp => true, strTempExtension => STORAGE_TEMP_EXT,
                lBufferMax => optionGet(OPTION_BUFFER_SIZE)});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageSpool', value => $hStorage->{&STORAGE_SPOOL}{$strStanza}, trace => true},
    );
}

push @EXPORT, qw(storageSpool);

1;
