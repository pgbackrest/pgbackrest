####################################################################################################################################
# Archive Performance Tests
####################################################################################################################################
package pgBackRestTest::Module::Performance::PerformanceArchivePerlTest;
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

use pgBackRestDoc::Common::Log;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strSpoolPath} = $self->testPath() . '/spool';
    $self->{strLogPath} = $self->testPath() . '/log';
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create spool path
    storageTest()->pathCreate($self->{strSpoolPath}, {bIgnoreExists => true, bCreateParent => true});
    storageTest()->pathCreate($self->{strLogPath}, {bIgnoreExists => true, bCreateParent => true});
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

        my $iRunTotal = 100;
        my $lTimeBegin = gettimeofday();

        for (my $iIndex = 0; $iIndex < $iRunTotal; $iIndex++)
        {
            my $iResult = system(
                $self->backrestExe() . ' --stanza=' . $self->stanza() . ' --archive-async --spool-path=' . $self->{strSpoolPath} .
                ' --log-level-file=detail --log-path=' . $self->{strLogPath} . ' --archive-timeout=1' .
                ' --pg1-path=/not/used archive-push /pg_xlog/000000010000000100000001');

            if ($iResult != 0)
            {
                confess "archive-push returned ${iResult}";
            }
        }

        &log(INFO, 'time per execution: ' . ((gettimeofday() - $lTimeBegin) / $iRunTotal));
    }
}

1;
