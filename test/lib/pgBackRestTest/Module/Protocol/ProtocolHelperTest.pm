####################################################################################################################################
# Protocol Helper Tests
####################################################################################################################################
package pgBackRestTest::Module::Protocol::ProtocolHelperTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

#CSHANG Instead of pgBackRestTest::Env::HostEnvTest can use pgBackRestTest::Env::ConfigEnvTest instead since unit tests are in docker and don't creater servers (e.g. would blow up if initServer

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
use pgBackRest::Archive::Push::Async;
use pgBackRest::Archive::Push::File;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

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
    $self->{strWalPath} = "$self->{strDbPath}/pg_xlog";
    $self->{strWalStatusPath} = "$self->{strWalPath}/archive_status";
    $self->{strWalHash} = "1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create WAL path
    storageTest()->pathCreate($self->{strWalStatusPath}, {bIgnoreExists => true, bCreateParent => true});

    # Create archive info
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    my $oOption = $self->initOption();
    logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
    my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});
    $oArchiveInfo->create(PG_VERSION_94, WAL_VERSION_94_SYS_ID, true);

    $self->{strArchiveId} = $oArchiveInfo->archiveId();
}

####################################################################################################################################
# initOption
####################################################################################################################################
sub initOption
{
    my $self = shift;

    my $oOption = {};

    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_DB_PATH, $self->{strDbPath});
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});
    $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());
    $self->optionBoolSetTest($oOption, OPTION_COMPRESS, false);

    $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
    $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);
    $self->optionSetTest($oOption, OPTION_ARCHIVE_TIMEOUT, 3);

    return $oOption;
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = $self->initOption();

    ################################################################################################################################
    if ($self->begin("Protocol::Helper"))
    {
        $self->optionSetTest($oOption, OPTION_BACKUP_HOST, 'localhost');
        $self->optionSetTest($oOption, OPTION_BACKUP_USER, $self->pgUser());
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testResult(sub {protocolGet(BACKUP, undef, {strBackRestBin => $self->backrestExe()})}, "[object]",
        'ssh default port');

        # Destroy protocol object
        protocolDestroy();

        $self->optionSetTest($oOption, OPTION_BACKUP_SSH_PORT, 25);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testException(sub {protocolGet(BACKUP, undef, {strBackRestBin => $self->backrestExe()})}, ERROR_FILE_READ,
        "process 'localhost remote' terminated unexpectedly: ssh: connect to host localhost port 25: Cannot assign requested " .
        "address");

        # Destroy protocol object
        protocolDestroy();

        $self->optionReset($oOption, OPTION_BACKUP_HOST);
        $self->optionReset($oOption, OPTION_BACKUP_USER);
        $self->optionReset($oOption, OPTION_BACKUP_SSH_PORT);
    }
}

1;
