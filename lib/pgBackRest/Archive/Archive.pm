####################################################################################################################################
# ARCHIVE MODULE
####################################################################################################################################
package pgBackRest::Archive::Archive;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::Common;
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
        $oFile,
        $strDbVersion,
        $ullDbSysId,
        $strWalFile,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->getCheck', \@_,
        {name => 'oFile'},
        {name => 'strDbVersion', required => false},
        {name => 'ullDbSysId', required => false},
        {name => 'strWalFile', required => false},
    );

    my $strArchiveId;
    my $strArchiveFile;

    # If the dbVersion/dbSysId are not passed, then we need to retrieve the database information
    if (!defined($strDbVersion) || !defined($ullDbSysId) )
    {
        # get DB info for comparison
        ($strDbVersion, my $iControlVersion, my $iCatalogVersion, $ullDbSysId) = dbMasterGet()->info();
    }

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        ($strArchiveId, $strArchiveFile) = $oFile->{oProtocol}->cmdExecute(
            OP_ARCHIVE_GET_CHECK, [$strDbVersion, $ullDbSysId, $strWalFile], true);
    }
    else
    {
        # check that the archive info is compatible with the database
        $strArchiveId =
            (new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->check($strDbVersion, $ullDbSysId);

        if (defined($strWalFile))
        {
            $strArchiveFile = walSegmentFind($oFile, ${strArchiveId}, $strWalFile);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId},
        {name => 'strArchiveFile', value => $strArchiveFile}
    );
}

1;
