####################################################################################################################################
# Archive Performance Tests
####################################################################################################################################
package pgBackRestTest::Module::Performance::PerformanceArchiveTest;
use parent 'pgBackRestTest::Common::RunTest';

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
use pgBackRest::LibC qw(:config);

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strSpoolPath} = $self->testPath() . '/spool';
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create spool path
    storageTest()->pathCreate($self->{strSpoolPath}, {bIgnoreExists => true, bCreateParent => true});
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("archive-push async (detect ok file)"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->put(
            storageTest()->openWrite(
                'spool/archive/' . $self->stanza() . '/out/000000010000000100000001.ok', {bPathCreate => true}));

        my $iRunTotal = 1;
        my $lTimeBegin = gettimeofday();

        for (my $iIndex = 0; $iIndex < $iRunTotal; $iIndex++)
        {
            executeTest(
                $self->backrestExe() . ' --' . cfgOptionName(CFGOPT_STANZA) . '=' . $self->stanza() .
                ' --' . cfgOptionName(CFGOPT_ARCHIVE_ASYNC) .
                ' --' . cfgOptionName(CFGOPT_SPOOL_PATH) . '=' . $self->{strSpoolPath} .
                ' --' . cfgOptionName(CFGOPT_ARCHIVE_TIMEOUT) . '=1' .
                ' archive-push /pg_xlog/000000010000000100000001');
        }

        &log(INFO, 'time per execution: ' . ((gettimeofday() - $lTimeBegin) / $iRunTotal));
    }
}

1;
