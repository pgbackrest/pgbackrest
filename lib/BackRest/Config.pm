####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package BackRest::Config;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename;
use Getopt::Long;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

use Exporter qw(import);

our @EXPORT = qw(config_load config_key_load operation_get operation_set param_get

                 FILE_MANIFEST FILE_VERSION FILE_POSTMASTER_PID
                 PATH_LATEST

                 OP_ARCHIVE_GET OP_ARCHIVE_PUSH OP_BACKUP OP_RESTORE OP_EXPIRE

                 BACKUP_TYPE_FULL BACKUP_TYPE_DIFF BACKUP_TYPE_INCR

                 PARAM_CONFIG PARAM_STANZA PARAM_TYPE PARAM_REMAP PARAM_NO_START_STOP PARAM_FORCE PARAM_VERSION PARAM_HELP
                 PARAM_TEST PARAM_TEST_DELAY PARAM_TEST_NO_FORK

                 CONFIG_SECTION_COMMAND CONFIG_SECTION_COMMAND_OPTION CONFIG_SECTION_LOG CONFIG_SECTION_BACKUP
                 CONFIG_SECTION_ARCHIVE CONFIG_SECTION_RETENTION CONFIG_SECTION_STANZA

                 CONFIG_KEY_USER CONFIG_KEY_HOST CONFIG_KEY_PATH

                 CONFIG_KEY_THREAD_MAX CONFIG_KEY_THREAD_TIMEOUT CONFIG_KEY_HARDLINK CONFIG_KEY_ARCHIVE_REQUIRED
                 CONFIG_KEY_ARCHIVE_MAX_MB CONFIG_KEY_START_FAST CONFIG_KEY_COMPRESS_ASYNC

                 CONFIG_KEY_LEVEL_FILE CONFIG_KEY_LEVEL_CONSOLE

                 CONFIG_KEY_COMPRESS CONFIG_KEY_CHECKSUM CONFIG_KEY_PSQL CONFIG_KEY_REMOTE

                 CONFIG_KEY_FULL_RETENTION CONFIG_KEY_DIFFERENTIAL_RETENTION CONFIG_KEY_ARCHIVE_RETENTION_TYPE
                 CONFIG_KEY_ARCHIVE_RETENTION);

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant
{
    FILE_MANIFEST       => 'backup.manifest',
    FILE_VERSION        => 'version',
    FILE_POSTMASTER_PID => 'postmaster.pid',

    PATH_LATEST         => 'latest'
};

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest
####################################################################################################################################
use constant
{
    OP_ARCHIVE_GET   => 'archive-get',
    OP_ARCHIVE_PUSH  => 'archive-push',
    OP_BACKUP        => 'backup',
    OP_RESTORE       => 'restore',
    OP_EXPIRE        => 'expire'
};

####################################################################################################################################
# BACKUP Type Constants
####################################################################################################################################
use constant
{
    BACKUP_TYPE_FULL          => 'full',
    BACKUP_TYPE_DIFF          => 'diff',
    BACKUP_TYPE_INCR          => 'incr'
};

####################################################################################################################################
# Parameter constants
####################################################################################################################################
use constant
{
    PARAM_CONFIG          => 'config',
    PARAM_STANZA          => 'stanza',
    PARAM_TYPE            => 'type',
    PARAM_NO_START_STOP   => 'no-start-stop',
    PARAM_REMAP           => 'remap',
    PARAM_FORCE           => 'force',
    PARAM_VERSION         => 'version',
    PARAM_HELP            => 'help',

    PARAM_TEST            => 'test',
    PARAM_TEST_DELAY      => 'test-delay',
    PARAM_TEST_NO_FORK    => 'no-fork'
};

####################################################################################################################################
# Configuration constants
####################################################################################################################################
use constant
{
    CONFIG_SECTION_COMMAND             => 'command',
    CONFIG_SECTION_COMMAND_OPTION      => 'command:option',
    CONFIG_SECTION_LOG                 => 'log',
    CONFIG_SECTION_BACKUP              => 'backup',
    CONFIG_SECTION_ARCHIVE             => 'archive',
    CONFIG_SECTION_RETENTION           => 'retention',
    CONFIG_SECTION_STANZA              => 'stanza',

    CONFIG_KEY_USER                    => 'user',
    CONFIG_KEY_HOST                    => 'host',
    CONFIG_KEY_PATH                    => 'path',

    CONFIG_KEY_THREAD_MAX              => 'thread-max',
    CONFIG_KEY_THREAD_TIMEOUT          => 'thread-timeout',
    CONFIG_KEY_HARDLINK                => 'hardlink',
    CONFIG_KEY_ARCHIVE_REQUIRED        => 'archive-required',
    CONFIG_KEY_ARCHIVE_MAX_MB          => 'archive-max-mb',
    CONFIG_KEY_START_FAST              => 'start-fast',
    CONFIG_KEY_COMPRESS_ASYNC          => 'compress-async',

    CONFIG_KEY_LEVEL_FILE              => 'level-file',
    CONFIG_KEY_LEVEL_CONSOLE           => 'level-console',

    CONFIG_KEY_COMPRESS                => 'compress',
    CONFIG_KEY_CHECKSUM                => 'checksum',
    CONFIG_KEY_PSQL                    => 'psql',
    CONFIG_KEY_REMOTE                  => 'remote',

    CONFIG_KEY_FULL_RETENTION          => 'full-retention',
    CONFIG_KEY_DIFFERENTIAL_RETENTION  => 'differential-retention',
    CONFIG_KEY_ARCHIVE_RETENTION_TYPE  => 'archive-retention-type',
    CONFIG_KEY_ARCHIVE_RETENTION       => 'archive-retention'
};

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oConfig;            # Configuration hash
my %oParam = ();        # Parameter hash
my $strOperation;       # Operation (backup, archive-get, ...)

####################################################################################################################################
# CONFIG_LOAD
#
# Load config file.
####################################################################################################################################
sub config_load
{
    my $strFile = shift;    # Full path to ini file to load from

    # Default for general parameters
    param_set(PARAM_NO_START_STOP, false); # Do not perform start/stop backup (and archive-required gets set to false)
    param_set(PARAM_FORCE, false);         # Force an action that would not normally be allowed (varies by action)
    param_set(PARAM_VERSION, false);       # Display version and exit
    param_set(PARAM_HELP, false);          # Display help and exit

    # Defaults for test parameters - not for general use
    param_set(PARAM_TEST_NO_FORK, false);  # Prevents the archive process from forking when local archiving is enabled
    param_set(PARAM_TEST, false);          # Enters test mode - not harmful, but adds special logging and pauses for unit testing
    param_set(PARAM_TEST_DELAY, 5);        # Seconds to delay after a test point (default is not enough for manual tests)

    # Get command line parameters
    GetOptions (\%oParam, PARAM_CONFIG . '=s', PARAM_STANZA . '=s', PARAM_TYPE . '=s', PARAM_REMAP . '=s%', PARAM_NO_START_STOP,
                          PARAM_FORCE, PARAM_VERSION, PARAM_HELP,
                          PARAM_TEST, PARAM_TEST_DELAY . '=s', PARAM_TEST_NO_FORK)
        or pod2usage(2);

    # Get and validate the operation
    $strOperation = $ARGV[0];

    if (!defined($strOperation))
    {
        confess &log(ERROR, 'operation is not defined');
    }

    if ($strOperation ne OP_ARCHIVE_GET &&
        $strOperation ne OP_ARCHIVE_PUSH &&
        $strOperation ne OP_BACKUP &&
        $strOperation ne OP_RESTORE &&
        $strOperation ne OP_EXPIRE)
    {
        confess &log(ERROR, "invalid operation ${strOperation}");
    }

    # Type should only be specified for backups
    if (defined(param_get(PARAM_TYPE)) && $strOperation ne OP_BACKUP)
    {
        confess &log(ERROR, 'type can only be specified for the backup operation')
    }

    # Set the backup type
    if ($strOperation ne OP_BACKUP)
    {
        if (!defined(param_get(PARAM_TYPE)))
        {
            param_set(PARAM_TYPE, BACKUP_TYPE_INCR);
        }
        elsif (param_get(PARAM_TYPE) ne BACKUP_TYPE_FULL && param_get(PARAM_TYPE) ne BACKUP_TYPE_DIFF &&
               param_get(PARAM_TYPE) ne BACKUP_TYPE_INCR)
        {
            confess &log(ERROR, 'backup type must be full, diff (differential), incr (incremental)');
        }
    }

    if (!defined(param_get(PARAM_CONFIG)))
    {
        param_set(PARAM_CONFIG, '/etc/pg_backrest.conf');
    }

    ini_load(param_get(PARAM_CONFIG), \%oConfig);

    # Load and check the cluster
    if (!defined(param_get(PARAM_STANZA)))
    {
        confess 'a backup stanza must be specified';
    }

    # Set the log levels
    log_level_set(uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_FILE, true, INFO)),
                  uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_CONSOLE, true, ERROR)));

    # Set test parameters
    test_set(param_get(PARAM_TEST), param_get(PARAM_TEST_DELAY));
}

####################################################################################################################################
# CONFIG_KEY_LOAD - Get a value from the config and be sure that it is defined (unless bRequired is false)
####################################################################################################################################
sub config_key_load
{
    my $strSection = shift;
    my $strKey = shift;
    my $bRequired = shift;
    my $strDefault = shift;

    # Default is that the key is not required
    if (!defined($bRequired))
    {
        $bRequired = false;
    }

    my $strValue;

    # Look in the default stanza section
    if ($strSection eq CONFIG_SECTION_STANZA)
    {
        $strValue = $oConfig{param_get(PARAM_STANZA)}{"${strKey}"};
    }
    # Else look in the supplied section
    else
    {
        # First check the stanza section
        $strValue = $oConfig{param_get(PARAM_STANZA) . ":${strSection}"}{"${strKey}"};

        # If the stanza section value is undefined then check global
        if (!defined($strValue))
        {
            $strValue = $oConfig{"global:${strSection}"}{"${strKey}"};
        }
    }

    if (!defined($strValue) && $bRequired)
    {
        if (defined($strDefault))
        {
            return $strDefault;
        }

        confess &log(ERROR, 'config value ' . (defined($strSection) ? $strSection : '[stanza]') .  "->${strKey} is undefined");
    }

    if ($strSection eq CONFIG_SECTION_COMMAND)
    {
        my $strOption = config_key_load(CONFIG_SECTION_COMMAND_OPTION, $strKey);

        if (defined($strOption))
        {
            $strValue =~ s/\%option\%/${strOption}/g;
        }
    }

    return $strValue;
}

####################################################################################################################################
# OPERATION_GET
#
# Get the current operation.
####################################################################################################################################
sub operation_get
{
    return $strOperation;
}

####################################################################################################################################
# OPERATION_SET
#
# Set current operation (usually for triggering follow-on operations).
####################################################################################################################################
sub operation_set
{
    my $strValue = shift;

    $strOperation = $strValue;
}

####################################################################################################################################
# PARAM_GET
#
# Get param value.
####################################################################################################################################
sub param_get
{
    my $strParam = shift;

    return $oParam{$strParam};
}

####################################################################################################################################
# PARAM_SET
#
# Set param value.
####################################################################################################################################
sub param_set
{
    my $strParam = shift;
    my $strValue = shift;

    $oParam{$strParam} = $strValue;
}

1;
