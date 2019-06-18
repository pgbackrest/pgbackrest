####################################################################################################################################
# I/O Performance Tests
####################################################################################################################################
package pgBackRestTest::Module::Performance::PerformanceIoTest;
        use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Storable qw(dclone);
use Time::HiRes qw(gettimeofday);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    # Load reference page data
    my $tPageBin = ${storageTest()->get($self->dataPath() . '/filecopy.table.bin')};

    # Create large test file
    $self->{iTableLargeSize} = 32;
    $self->{strTableLargeFile} = 'table-large.bin';

    my $oFileWrite = storageTest()->openWrite($self->{strTableLargeFile});
    $oFileWrite->open();

    for (my $iIndex = 0; $iIndex < $self->{iTableLargeSize}; $iIndex++)
    {
        $oFileWrite->write(\$tPageBin);
    }

    $oFileWrite->close();
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("copy"))
    {
        # Setup the remote for testing remote storage
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_PG_PATH, $self->testPath());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath());
        $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());
        $self->optionTestSet(CFGOPT_REPO_HOST, 'localhost');
        $self->optionTestSet(CFGOPT_REPO_HOST_USER, $self->backrestUser());
        $self->configTestLoad(CFGCMD_RESTORE);

        protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP, undef, {strBackRestBin => $self->backrestExe()});
        storageRepo();

        # Setup file info
        my $strFile = $self->{strTableLargeFile};
        my $strFileCopy = "${strFile}.copy";

        my $iRunTotal = 1;
        &log(INFO, "time is average of ${iRunTotal} run(s)");

        foreach my $bGzip (false, true)
        {
        foreach my $bRemote (false, true)
        {
            my $rhyFilter;

            push(@{$rhyFilter}, {strClass => STORAGE_FILTER_SHA});
            push(@{$rhyFilter}, {strClass => STORAGE_FILTER_GZIP, rxyParam => [STORAGE_COMPRESS, false, 6]}) if ($bGzip);

            my $lTimeTotal = 0;
            my $lTimeBegin;

            for (my $iIndex = 0; $iIndex < $iRunTotal; $iIndex++)
            {
                # Get the remote or local for writing
                my $oStorageWrite = $bRemote ? storageRepo() : storageTest();

                # Start the timer
                $lTimeBegin = gettimeofday();

                # Copy the file
                storageTest()->copy(
                    storageTest()->openRead($strFile, {rhyFilter => $rhyFilter}),
                    $oStorageWrite->openWrite($strFileCopy));

                # Record time
                $lTimeTotal += gettimeofday() - $lTimeBegin;

                # Remove file so it can be copied again
                executeTest("sudo rm " . $oStorageWrite->pathGet($strFileCopy));
            }

            # Calculate out output metrics
            my $fExecutionTime = int($lTimeTotal * 1000 / $iRunTotal) / 1000;
            my $fGbPerHour = int((60 * 60) * 1000 / ((1024 / $self->{iTableLargeSize}) * $fExecutionTime)) / 1000;

            &log(INFO, "sha1 1, gz ${bGzip}, rmt ${bRemote}: ${fExecutionTime}s, ${fGbPerHour} GB/hr");
        }
        }

        # Destroy protocol object
        protocolDestroy();
    }
}

1;
