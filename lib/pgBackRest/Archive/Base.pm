####################################################################################################################################
# ARCHIVE MODULE
####################################################################################################################################
package pgBackRest::Archive::Base;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Archive::Info;
use pgBackRest::Archive::Common;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strBackRestBin},
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->new', \@_,
        {name => 'strBackRestBin', default => BACKREST_BIN, trace => true},
    );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# getCheck
####################################################################################################################################
sub getCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $strWalFile,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->getCheck', \@_,
        {name => 'strDbVersion', required => false},
        {name => 'ullDbSysId', required => false},
        {name => 'strWalFile', required => false},
    );

    my $strArchiveId;
    my $strArchiveFile;
    my $strCipherPass;

    # If the dbVersion/dbSysId are not passed, then we need to retrieve the database information
    if (!defined($strDbVersion) || !defined($ullDbSysId) )
    {
        # get DB info for comparison
        ($strDbVersion, my $iControlVersion, my $iCatalogVersion, $ullDbSysId) = dbMasterGet()->info();
    }

    # Get db info from the repo
    if (!isRepoLocal())
    {
        ($strArchiveId, $strArchiveFile, $strCipherPass) = protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP)->cmdExecute(
            OP_ARCHIVE_GET_CHECK, [$strDbVersion, $ullDbSysId, $strWalFile], true);
    }
    else
    {
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), true);

        # check that the archive info is compatible with the database
        $strArchiveId = $oArchiveInfo->check($strDbVersion, $ullDbSysId);

        if (defined($strWalFile))
        {
            $strArchiveFile = walSegmentFind(storageRepo(), ${strArchiveId}, $strWalFile);
        }

        # Get the encryption passphrase to read/write files (undefined if the repo is not encrypted)
        $strCipherPass = $oArchiveInfo->cipherPassSub();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId},
        {name => 'strArchiveFile', value => $strArchiveFile},
        {name => 'strCipherPass', value => $strCipherPass, redact => true}
    );
}

1;
