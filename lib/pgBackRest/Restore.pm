####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package pgBackRest::Restore;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use File::Basename qw(basename dirname);
use File::stat qw(lstat);

use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::RestoreFile;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Local::Process;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;              # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Initialize protocol
    $self->{oProtocol} = !isRepoLocal() ? protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP) : undef;

    # Initialize variables
    $self->{strDbClusterPath} = cfgOption(CFGOPT_PG_PATH);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# recovery
#
# Creates the recovery.conf file.
####################################################################################################################################
sub recovery
{
    my $self = shift;           # Class hash
    my $strDbVersion = shift;   # Version to restore

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (__PACKAGE__ . '->recovery');

    # Create recovery.conf path/file
    my $strRecoveryConf = $self->{strDbClusterPath} . '/' . DB_FILE_RECOVERYCONF;

    # See if recovery.conf already exists
    my $bRecoveryConfExists = storageDb()->exists($strRecoveryConf);

    # If CFGOPTVAL_RESTORE_TYPE_PRESERVE then warn if recovery.conf does not exist and return
    if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_PRESERVE))
    {
        if (!$bRecoveryConfExists)
        {
            &log(WARN, "recovery type is " . cfgOption(CFGOPT_TYPE) . " but recovery file does not exist at ${strRecoveryConf}");
        }
    }
    else
    {
        # In all other cases the old recovery.conf should be removed if it exists
        if ($bRecoveryConfExists)
        {
            storageDb()->remove($strRecoveryConf);
        }

        # If CFGOPTVAL_RESTORE_TYPE_NONE then return
        if (!cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_NONE))
        {
            # Write recovery options read from the configuration file
            my $strRecovery = '';
            my $bRestoreCommandOverride = false;

            if (cfgOptionTest(CFGOPT_RECOVERY_OPTION))
            {
                my $oRecoveryRef = cfgOption(CFGOPT_RECOVERY_OPTION);

                foreach my $strKey (sort(keys(%$oRecoveryRef)))
                {
                    my $strPgKey = $strKey;
                    $strPgKey =~ s/\-/\_/g;

                    if ($strPgKey eq 'restore_command')
                    {
                        $bRestoreCommandOverride = true;
                    }

                    $strRecovery .= "${strPgKey} = '$$oRecoveryRef{$strKey}'\n";
                }
            }

            # Write the restore command
            if (!$bRestoreCommandOverride)
            {
                $strRecovery .=  "restore_command = '" . cfgCommandWrite(CFGCMD_ARCHIVE_GET) . " %f \"%p\"'\n";
            }

            # If type is CFGOPTVAL_RESTORE_TYPE_IMMEDIATE
            if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_IMMEDIATE))
            {
                $strRecovery .= "recovery_target = '" . CFGOPTVAL_RESTORE_TYPE_IMMEDIATE . "'\n";
            }
            # If type is not CFGOPTVAL_RESTORE_TYPE_DEFAULT write target options
            elsif (!cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_DEFAULT))
            {
                # Write the recovery target
                $strRecovery .= "recovery_target_" . cfgOption(CFGOPT_TYPE) . " = '" . cfgOption(CFGOPT_TARGET) . "'\n";

                # Write recovery_target_inclusive
                if (cfgOption(CFGOPT_TARGET_EXCLUSIVE, false))
                {
                    $strRecovery .= "recovery_target_inclusive = 'false'\n";
                }
            }

            # Write pause_at_recovery_target/recovery_target_action
            if (cfgOptionTest(CFGOPT_TARGET_ACTION))
            {
                my $strTargetAction = cfgOption(CFGOPT_TARGET_ACTION);

                if ($strTargetAction ne cfgOptionDefault(CFGOPT_TARGET_ACTION))
                {
                    if ($strDbVersion >= PG_VERSION_95)
                    {
                        $strRecovery .= "recovery_target_action = '${strTargetAction}'\n";
                    }
                    elsif  ($strDbVersion >= PG_VERSION_91)
                    {
                        if ($strTargetAction eq CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN)
                        {
                            confess &log(ERROR,
                                cfgOptionName(CFGOPT_TARGET_ACTION) .  '=' . CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN .
                                    ' is only available in PostgreSQL >= ' . PG_VERSION_95)
                        }

                        $strRecovery .= "pause_at_recovery_target = 'false'\n";
                    }
                    else
                    {
                        confess &log(ERROR,
                            cfgOptionName(CFGOPT_TARGET_ACTION) .  ' option is only available in PostgreSQL >= ' . PG_VERSION_91)
                    }
                }
            }

            # Write recovery_target_timeline
            if (cfgOptionTest(CFGOPT_TARGET_TIMELINE))
            {
                $strRecovery .= "recovery_target_timeline = '" . cfgOption(CFGOPT_TARGET_TIMELINE) . "'\n";
            }

            # Write recovery.conf
            my $hFile;

            open($hFile, '>', $strRecoveryConf)
                or confess &log(ERROR, "unable to open ${strRecoveryConf}: $!");

            syswrite($hFile, $strRecovery)
                or confess "unable to write section ${strRecoveryConf}: $!";

            close($hFile)
                or confess "unable to close ${strRecoveryConf}: $!";

            &log(INFO, "write $strRecoveryConf");
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# process
#
# Takes a backup and restores it back to the original or a remapped location.
####################################################################################################################################
sub process
{
    my $self = shift;       # Class hash

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (__PACKAGE__ . '->process');

    # Db storage
    my $oStorageDb = storageDb();

    # Load the manifest into a hash (already partially processed by C)
    my $oManifest = new pgBackRest::Manifest(
        storageDb()->pathGet($self->{strDbClusterPath} . '/' . FILE_MANIFEST), {oStorage => storageDb()});

    # Create recovery.conf file
    $self->recovery($oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION));

    # Sync db cluster paths
    # foreach my $strPath ($oManifest->keys(MANIFEST_SECTION_TARGET_PATH))
    # {
    #     # This is already synced as a subpath of pg_data
    #     next if $strPath eq MANIFEST_TARGET_PGTBLSPC;
    #
    #     $oStorageDb->pathSync($oManifest->dbPathGet($self->{strDbClusterPath}, $strPath));
    # }

    # Sync targets that don't have paths above
    # foreach my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    # {
    #     # Already synced
    #     next if $strTarget eq MANIFEST_TARGET_PGDATA;
    #
    #     $oStorageDb->pathSync(
    #         $oStorageDb->pathAbsolute(
    #             $self->{strDbClusterPath} . ($strTarget =~ ('^' . MANIFEST_TARGET_PGTBLSPC) ? '/' . MANIFEST_TARGET_PGTBLSPC : ''),
    #             $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_VALUE_PATH)));
    # }

    # Move pg_control last
    &log(INFO,
        'restore ' . $oManifest->dbPathGet(undef, MANIFEST_FILE_PGCONTROL) .
        ' (performed last to ensure aborted restores cannot be started)');

    $oStorageDb->move(
        $self->{strDbClusterPath} . '/' . DB_FILE_PGCONTROL . '.' . STORAGE_TEMP_EXT,
        $self->{strDbClusterPath} . '/' . DB_FILE_PGCONTROL);

    # Sync to be sure the rename of pg_control is persisted
    $oStorageDb->pathSync($self->{strDbClusterPath});

    # Finally remove the manifest to indicate the restore is complete
    $oStorageDb->remove($self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # Sync removal of the manifest
    $oStorageDb->pathSync($self->{strDbClusterPath});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
