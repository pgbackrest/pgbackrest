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
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::Common;

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Create the class hash
    my $self = {};
    bless $self, $class;

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
        $ullDbSysId
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->getCheck', \@_,
        {name => 'oFile'},
        {name => 'strDbVersion', required => false},
        {name => 'ullDbSysId', required => false}
    );

    my $strArchiveId;

    # If the dbVersion/dbSysId are not passed, then we need to retrieve the database information
    if (!defined($strDbVersion) || !defined($ullDbSysId) )
    {
        # get DB info for comparison
        ($strDbVersion, my $iControlVersion, my $iCatalogVersion, $ullDbSysId) = dbMasterGet()->info();
    }

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        $strArchiveId = $oFile->{oProtocol}->cmdExecute(OP_ARCHIVE_GET_CHECK, [$strDbVersion, $ullDbSysId], true);
    }
    else
    {
        # check that the archive info is compatible with the database
        $strArchiveId =
            (new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->check($strDbVersion, $ullDbSysId);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId, trace => true}
    );
}

1;
