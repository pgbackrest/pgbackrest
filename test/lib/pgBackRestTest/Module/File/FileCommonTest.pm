####################################################################################################################################
# FileCommonTest.pm - Common code for File tests
####################################################################################################################################
package pgBackRestTest::Module::File::FileCommonTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::RemoteMaster;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# initModule
#
# Common objects and variables used by all tests.
####################################################################################################################################
sub initModule
{
    my $self = shift;

    # Create the repo path so the remote won't complain that it's missing
    my $strRepoPath =  $self->testPath() . '/repo';

    mkdir($strRepoPath, oct('0770'))
        or confess "Unable to create repo directory: ${strRepoPath}";

    # Create local
    $self->{oLocal} = new pgBackRest::Protocol::Common(
        262144,
        1,
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,
        HOST_PROTOCOL_TIMEOUT);

    # Create remote
    $self->{oRemote} = new pgBackRest::Protocol::RemoteMaster(
        BACKUP,
        OPTION_DEFAULT_CMD_SSH,
        $self->backrestExeOriginal() . ' --stanza=' . $self->stanza() .
            " --type=backup --repo-path=${strRepoPath} --no-config --command=test remote",
        262144,
        OPTION_DEFAULT_COMPRESS_LEVEL,
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,
        $self->host(),
        $self->backrestUser(),
        HOST_PROTOCOL_TIMEOUT);

    rmdir($strRepoPath)
        or confess "Unable to remove repo directory: ${strRepoPath}";
}

####################################################################################################################################
# cleanModule
#
# Close objects created for tests.
####################################################################################################################################
sub cleanModule
{
    my $self = shift;

    $self->remote()->close();
}

####################################################################################################################################
# setup
#
# Setup directories for file tests.
####################################################################################################################################
sub setup
{
    my $self = shift;
    my $bRemote = shift;
    my $bPrivate = shift;

    # Remove the backrest private directory
    if (fileExists($self->testPath() . '/private'))
    {
        executeTest(
            'ssh ' . $self->backrestUser() . '\@' . $self->host() . ' rm -rf ' . $self->testPath() . '/private',
            {bSuppressStdErr => true});
    }

    # Remove contents of the test directory
    executeTest('rm -rf ' . $self->testPath() . '/*');

    # Create the private directories
    if (defined($bPrivate) && $bPrivate)
    {
        executeTest(
            'ssh ' . $self->backrestUser() . '\@' . $self->host() . ' mkdir -m 700 ' . $self->testPath() . '/backrest_private',
            {bSuppressStdErr => true});

        executeTest('mkdir -m 700 ' . $self->testPath() . '/user_private');
    }

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        $self->stanza(),
        $self->testPath(),
        $bRemote ? $self->remote() : $self->local()
    );

    return $oFile;
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub host {return '127.0.0.1'}
sub local {return shift->{oLocal}}
sub remote {return shift->{oRemote}}

1;
