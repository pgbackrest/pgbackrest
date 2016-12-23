####################################################################################################################################
# FileUnitTest.pm - Unit tests for File module.
####################################################################################################################################
package pgBackRestTest::File::FileUnitTest;
use parent 'pgBackRestTest::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::File;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Increment the run, log, and decide whether this unit test should be run
    if (!$self->begin('unit')) {return}

    # Setup test directory and get file object
    my $oLocalFile = $self->setup(false, false);

    # Test File->pathTypeGet()
    #---------------------------------------------------------------------------------------------------------------------------
    {
        &log(INFO, "    File->pathTypeGet()");

        $self->testResult(sub {$oLocalFile->pathTypeGet(PATH_ABSOLUTE)}, PATH_ABSOLUTE);
        $self->testResult(sub {$oLocalFile->pathTypeGet(PATH_DB)}, PATH_DB);
        $self->testResult(sub {$oLocalFile->pathTypeGet(PATH_DB_ABSOLUTE)}, PATH_DB);
        $self->testResult(sub {$oLocalFile->pathTypeGet(PATH_BACKUP)}, PATH_BACKUP);
        $self->testResult(sub {$oLocalFile->pathTypeGet(PATH_BACKUP_ARCHIVE)}, PATH_BACKUP);
        $self->testException(sub {$oLocalFile->pathTypeGet('bogus')}, ERROR_ASSERT, "no known path types in 'bogus'");
    }

    # Test File->pathGet()
    #---------------------------------------------------------------------------------------------------------------------------
    {
        &log(INFO, "    File->pathGet()");

        # Test temp file errors
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_BACKUP, 'test', true)},
            ERROR_ASSERT, "temp file not supported for path type 'backup'");
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_ABSOLUTE, undef, true)},
            ERROR_ASSERT, "strFile must be defined when temp file specified");
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, undef, true)},
            ERROR_ASSERT, "strFile must be defined when temp file specified");
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE_OUT, undef, true)},
            ERROR_ASSERT, "strFile must be defined when temp file specified");
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_BACKUP_TMP, undef, true)},
            ERROR_ASSERT, "strFile must be defined when temp file specified");

        # Test absolute path
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_ABSOLUTE)}, ERROR_ASSERT, "strFile must be defined for absolute path");
        $self->testException(
            sub {$oLocalFile->pathGet(PATH_ABSOLUTE, 'file')}, ERROR_ASSERT, "absolute path absolute:file must start with /");
        $self->testResult(sub {$oLocalFile->pathGet(PATH_ABSOLUTE, '/file', true)}, "/file.pgbackrest.tmp");
        $self->testResult(sub {$oLocalFile->pathGet(PATH_ABSOLUTE, '/file')}, "/file");

        # Test backup path
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP, 'file')}, $self->testPath() . '/file');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP, undef)}, $self->testPath());

        # Error when stanza not defined
        $self->testException(
            sub {(new pgBackRest::File(undef, $self->testPath(), $self->local()))->pathGet(PATH_BACKUP_TMP)},
            ERROR_ASSERT, "strStanza not defined");

        # Test backup tmp path
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_TMP, 'file', true)}, $self->testPath() . '/temp/db.tmp/file.pgbackrest.tmp');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_TMP, 'file')}, $self->testPath() . '/temp/db.tmp/file');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_TMP, undef)}, $self->testPath() . '/temp/db.tmp');

        # Test archive path
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, undef)}, $self->testPath() . '/archive/db');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, '9.3-1')}, $self->testPath() . '/archive/db/9.3-1');
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, '9.3-1/000000010000000100000001')},
            $self->testPath() . '/archive/db/9.3-1/0000000100000001/000000010000000100000001');
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, '9.3-1/000000010000000100000001', true)},
            $self->testPath() . '/archive/db/9.3-1/0000000100000001/000000010000000100000001.pgbackrest.tmp');
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, '9.3-1/00000001.history')},
            $self->testPath() . '/archive/db/9.3-1/00000001.history');
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE, '9.3-1/00000001.history', true)},
            $self->testPath() . '/archive/db/9.3-1/00000001.history.pgbackrest.tmp');

        # Test archive out path
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE_OUT, '000000010000000100000001')},
            $self->testPath() . '/archive/db/out/000000010000000100000001');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE_OUT)}, $self->testPath() . '/archive/db/out');
        $self->testResult(
            sub {$oLocalFile->pathGet(PATH_BACKUP_ARCHIVE_OUT, '000000010000000100000001', true)},
            $self->testPath() . '/archive/db/out/000000010000000100000001.pgbackrest.tmp');

        # Test backup cluster path
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_CLUSTER, 'file')}, $self->testPath() . '/backup/db/file');
        $self->testResult(sub {$oLocalFile->pathGet(PATH_BACKUP_CLUSTER)}, $self->testPath() . '/backup/db');

        # Test invalid path type
        $self->testException(sub {$oLocalFile->pathGet('bogus')}, ERROR_ASSERT, "no known path types in 'bogus'");
    }
}

1;
