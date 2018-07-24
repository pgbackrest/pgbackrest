####################################################################################################################################
# Tests for Backup File module
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupFileUnitPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::File;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin('backupFile()'))
    {
# $strDbFile,                                 # Database file to backup
# $strRepoFile,                               # Location in the repository to copy to
# $lSizeFile,                                 # File size
# $strChecksum,                               # File checksum to be checked
# $bChecksumPage,                             # Should page checksums be calculated?
# $strBackupLabel,                            # Label of current backup
# $bCompress,                                 # Compress destination file
# $iCompressLevel,                            # Compress level
# $lModificationTime,                         # File modification time
# $bIgnoreMissing,                            # Is it OK if the file is missing?
# $hExtraParam,                               # Parameter to pass to the extra function
# $bDelta,                                    # Is the detla option on?
# $bHasReference,                             # Does a reference exist to the file in a prior backup in the set?
# $strCipherPass,                             # Passphrase to access the repo file (undefined if repo not encrypted). This
#                                             # parameter must always be last in the parameter list to this function.

        my $strRepoBackupPath = $self->testPath() . "/repobackup";
        my $strDbPath = $self->testPath() . "/db";
        my $strBackupLabel = '20180724-182750F';

        storageTest()->pathCreate($strRepoBackupPath . "/" $strBackupLabel, {bIgnoreExists => true, bCreateParent => true});
        storageTest()->pathCreate($strDbPath, {bIgnoreExists => true});

        my $strFileName = "12345";
        my $strFileDbPath = $strDbPath . "/$strFileName";
        my $strFileHash = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';

        executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strFileDbPath}");

        my $hManifest = storageTest()->manifest($strDbPath);
        # $hManifest->{$strName}{size} + 0

    }

    ################################################################################################################################
    # if ($self->begin('backupManifestUpdate()'))
    # {
    #
    # }
}

1;
