####################################################################################################################################
# Protocol Helper Tests
####################################################################################################################################
package pgBackRestTest::Module::Protocol::ProtocolHelperPerlTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Push::Push;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create archive info
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});
}

####################################################################################################################################
# initOption
####################################################################################################################################
sub initOption
{
    my $self = shift;

    $self->configTestClear();

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_PG_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());
    $self->optionTestSetBool(CFGOPT_COMPRESS, false);

    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);
    $self->optionTestSet(CFGOPT_ARCHIVE_TIMEOUT, 3);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin('protocolParam()'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST, 1), 'pg-host-1');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_PATH, 1), '/db1');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_PORT, 1), '1111');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, 1), 'pgbackrest1');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST, 2), 'pg-host-2');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_PATH, 2), '/db2');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_PORT, 2), '2222');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, 2), 'pgbackrest2');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG, 2), '/config2');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH, 2), '/config-include2');
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_PATH, 2), '/config-path2');
        $self->configTestLoad(CFGCMD_BACKUP);

        $self->testResult(
            sub {pgBackRest::Protocol::Helper::protocolParam(cfgCommandName(CFGCMD_BACKUP), CFGOPTVAL_REMOTE_TYPE_DB, 2)},
            '(pg-host-2, postgres, [undef], pgbackrest2 --buffer-size=4194304 --command=backup --compress-level=6' .
                ' --compress-level-network=3 --config=/config2 --config-include-path=/config-include2 --config-path=/config-path2' .
                ' --log-level-file=off --pg1-path=/db2 --pg1-port=2222 --process=0 --protocol-timeout=1830 --stanza=db --type=db' .
                ' remote)',
            'more than one backup db host');

        # --------------------------------------------------------------------------------------------------------------------------
        $self->configTestClear();
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(cfgOptionIdFromIndex(CFGOPT_PG_PATH, 1), '/db1');
        $self->optionTestSet(CFGOPT_REPO_HOST, 'repo-host');
        $self->optionTestSet(CFGOPT_REPO_PATH, '/repo');
        $self->optionTestSet(CFGOPT_REPO_HOST_CMD, 'pgbackrest-repo');
        $self->optionTestSet(CFGOPT_REPO_HOST_CONFIG, '/config-repo');
        $self->optionTestSet(CFGOPT_REPO_HOST_CONFIG_INCLUDE_PATH, '/config-include-repo');
        $self->optionTestSet(CFGOPT_REPO_HOST_CONFIG_PATH, '/config-path-repo');
        $self->configTestLoad(CFGCMD_RESTORE);

        $self->testResult(
            sub {pgBackRest::Protocol::Helper::protocolParam(cfgCommandName(CFGCMD_RESTORE), CFGOPTVAL_REMOTE_TYPE_BACKUP)},
            '(repo-host, pgbackrest, [undef], pgbackrest-repo --buffer-size=4194304 --command=restore --compress-level=6' .
                ' --compress-level-network=3 --config=/config-repo --config-include-path=/config-include-repo' .
                ' --config-path=/config-path-repo --log-level-file=off --pg1-path=/db1 --process=0 --protocol-timeout=1830' .
                ' --repo1-path=/repo --stanza=db --type=backup remote)',
            'config params to repo host');
    }

    ################################################################################################################################
    if ($self->begin("Protocol::Helper"))
    {
        $self->initOption();
        $self->optionTestSet(CFGOPT_REPO_HOST, 'localhost');
        $self->optionTestSet(CFGOPT_REPO_HOST_USER, $self->pgUser());
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        $self->testResult(
            sub {protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP, undef, {strBackRestBin => $self->backrestExe()})}, "[object]",
            'ssh default port');

        # Destroy protocol object
        protocolDestroy();

        $self->optionTestSet(CFGOPT_REPO_HOST_PORT, 25);
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        $self->testException(
            sub {protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP, undef, {strBackRestBin => $self->backrestExe()})}, ERROR_FILE_READ,
            "remote process on 'localhost' terminated unexpectedly: ssh: connect to host localhost port 25:");

        # Destroy protocol object
        protocolDestroy();
    }
}

1;
