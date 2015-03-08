####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package BackRest::Config;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;
use Getopt::Long;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Param;

use Exporter qw(import);

our @EXPORT = qw(config_load config_key_load config_section_load

                 FILE_MANIFEST FILE_VERSION FILE_POSTMASTER_PID FILE_RECOVERY_CONF
                 PATH_LATEST

                 CONFIG_SECTION_COMMAND CONFIG_SECTION_GENERAL CONFIG_SECTION_COMMAND_OPTION CONFIG_SECTION_LOG CONFIG_SECTION_BACKUP
                 CONFIG_SECTION_RESTORE CONFIG_SECTION_RECOVERY CONFIG_SECTION_RECOVERY_OPTION CONFIG_SECTION_TABLESPACE_MAP
                 CONFIG_SECTION_ARCHIVE CONFIG_SECTION_RETENTION CONFIG_SECTION_STANZA

                 CONFIG_KEY_USER CONFIG_KEY_HOST CONFIG_KEY_PATH

                 CONFIG_KEY_THREAD_MAX CONFIG_KEY_THREAD_TIMEOUT CONFIG_KEY_HARDLINK CONFIG_KEY_ARCHIVE_REQUIRED
                 CONFIG_KEY_ARCHIVE_MAX_MB CONFIG_KEY_START_FAST CONFIG_KEY_COMPRESS_ASYNC

                 CONFIG_KEY_LEVEL_FILE CONFIG_KEY_LEVEL_CONSOLE

                 CONFIG_KEY_BUFFER_SIZE CONFIG_KEY_COMPRESS CONFIG_KEY_COMPRESS_LEVEL CONFIG_KEY_COMPRESS_LEVEL_NETWORK
                 CONFIG_KEY_PSQL CONFIG_KEY_REMOTE

                 CONFIG_KEY_FULL_RETENTION CONFIG_KEY_DIFFERENTIAL_RETENTION CONFIG_KEY_ARCHIVE_RETENTION_TYPE
                 CONFIG_KEY_ARCHIVE_RETENTION

                 CONFIG_KEY_STANDBY_MODE CONFIG_KEY_PRIMARY_CONNINFO CONFIG_KEY_TRIGGER_FILE CONFIG_KEY_RESTORE_COMMAND
                 CONFIG_KEY_ARCHIVE_CLEANUP_COMMAND CONFIG_KEY_RECOVERY_END_COMMAND

                 CONFIG_DEFAULT_BUFFER_SIZE CONFIG_DEFAULT_COMPRESS_LEVEL CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK);

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant
{
    FILE_MANIFEST       => 'backup.manifest',
    FILE_VERSION        => 'version',
    FILE_POSTMASTER_PID => 'postmaster.pid',
    FILE_RECOVERY_CONF  => 'recovery.conf',
};

####################################################################################################################################
# Configuration constants
####################################################################################################################################
use constant
{
    CONFIG_SECTION_COMMAND             => 'command',
    CONFIG_SECTION_COMMAND_OPTION      => 'command:option',
    CONFIG_SECTION_GENERAL             => 'general',
    CONFIG_SECTION_LOG                 => 'log',
    CONFIG_SECTION_BACKUP              => 'backup',
    CONFIG_SECTION_RESTORE             => 'restore',
    CONFIG_SECTION_RECOVERY            => 'recovery',
    CONFIG_SECTION_RECOVERY_OPTION     => 'recovery:option',
    CONFIG_SECTION_TABLESPACE_MAP      => 'tablespace:map',
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

    CONFIG_KEY_BUFFER_SIZE             => 'buffer-size',
    CONFIG_KEY_COMPRESS                => 'compress',
    CONFIG_KEY_COMPRESS_LEVEL          => 'compress-level',
    CONFIG_KEY_COMPRESS_LEVEL_NETWORK  => 'compress-level-network',
    CONFIG_KEY_PSQL                    => 'psql',
    CONFIG_KEY_REMOTE                  => 'remote',

    CONFIG_KEY_FULL_RETENTION          => 'full-retention',
    CONFIG_KEY_DIFFERENTIAL_RETENTION  => 'differential-retention',
    CONFIG_KEY_ARCHIVE_RETENTION_TYPE  => 'archive-retention-type',
    CONFIG_KEY_ARCHIVE_RETENTION       => 'archive-retention',

    CONFIG_KEY_STANDBY_MODE            => 'standby-mode',
    CONFIG_KEY_PRIMARY_CONNINFO        => 'primary-conninfo',
    CONFIG_KEY_TRIGGER_FILE            => 'trigger-file',
    CONFIG_KEY_RESTORE_COMMAND         => 'restore-command',
    CONFIG_KEY_ARCHIVE_CLEANUP_COMMAND => 'archive-cleanup-command',
    CONFIG_KEY_RECOVERY_END_COMMAND    => 'recovery-end-command'
};

####################################################################################################################################
# Configuration defaults
####################################################################################################################################
use constant
{
    CONFIG_DEFAULT_BUFFER_SIZE                  => 1048576,
    CONFIG_DEFAULT_BUFFER_SIZE_MIN              => 4096,
    CONFIG_DEFAULT_BUFFER_SIZE_MAX              => 8388608,

    CONFIG_DEFAULT_COMPRESS_LEVEL               => 6,
    CONFIG_DEFAULT_COMPRESS_LEVEL_MIN           => 0,
    CONFIG_DEFAULT_COMPRESS_LEVEL_MAX           => 9,

    CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK       => 3,
    CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN   => 0,
    CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX   => 9,

    CONFIG_DEFAULT_THREAD_MAX                   => 1,
    CONFIG_DEFAULT_THREAD_MAX_MIN               => 1,
    CONFIG_DEFAULT_THREAD_MAX_MAX               => 64
};

####################################################################################################################################
# Validation constants
####################################################################################################################################
use constant
{
    VALID_RANGE             => 'range'
};

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oConfig;            # Configuration hash

####################################################################################################################################
# CONFIG_LOAD
#
# Load config file.
####################################################################################################################################
sub config_load
{
    my $strFile = shift;    # Full path to ini file to load from

    # Load parameters
    configLoad();
    ini_load(optionGet(OPTION_CONFIG), \%oConfig);

    # If this is a restore, then try to default config
    if (!defined(config_key_load(CONFIG_SECTION_RESTORE, CONFIG_KEY_PATH)))
    {
        if (!defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
        {
            $oConfig{'global:restore'}{path} = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH);
        }

        if (!defined(config_key_load(CONFIG_SECTION_RESTORE, CONFIG_KEY_PATH)))
        {
            $oConfig{'global:restore'}{path} = config_key_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH);
        }
    }

    # Set the log levels
    log_level_set(uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_FILE, true, INFO)),
                  uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_CONSOLE, true, ERROR)));

    # Validate config
    config_valid();
}

####################################################################################################################################
# CONFIG_STANZA_SECTION_LOAD - Get an entire stanza section
####################################################################################################################################
sub config_section_load
{
    my $strSection = shift;

    $strSection = optionGet(OPTION_STANZA) . ':' . $strSection;

    return $oConfig{$strSection};
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
        $strValue = $oConfig{optionGet(OPTION_STANZA)}{"${strKey}"};
    }
    # Else look in the supplied section
    else
    {
        # First check the stanza section
        $strValue = $oConfig{optionGet(OPTION_STANZA) . ":${strSection}"}{"${strKey}"};

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
# CONFIG_KEY_SET
####################################################################################################################################
sub config_key_set
{
    my $strSection = shift;
    my $strKey = shift;
    my $strValue = shift;

    # Make sure all parameters are defined
    if (!defined($strSection) || !defined($strKey) || !defined($strValue))
    {
        confess &log(ASSERT, 'section, key and value must all be defined');
    }

    # Set the value
    $strSection = optionGet(OPTION_STANZA) . ':' . $strSection;

    $oConfig{$strSection}{$strKey} = $strValue;
}

####################################################################################################################################
# CONFIG_VALID
#
# Make sure the configuration is valid.
####################################################################################################################################
sub config_valid
{
    # Local variables
    my $strSection;
    my $oSectionHashRef;

    # Check [stanza]:recovery:option section
    $strSection = optionGet(OPTION_STANZA) . ':' . CONFIG_SECTION_RECOVERY_OPTION;
    $oSectionHashRef = $oConfig{$strSection};

    if (defined($oSectionHashRef) && keys($oSectionHashRef) != 0)
    {
        foreach my $strKey (sort(keys($oSectionHashRef)))
        {
            if ($strKey ne CONFIG_KEY_STANDBY_MODE &&
                $strKey ne CONFIG_KEY_PRIMARY_CONNINFO &&
                $strKey ne CONFIG_KEY_TRIGGER_FILE &&
                $strKey ne CONFIG_KEY_RESTORE_COMMAND &&
                $strKey ne CONFIG_KEY_ARCHIVE_CLEANUP_COMMAND &&
                $strKey ne CONFIG_KEY_RECOVERY_END_COMMAND)
            {
                confess &log(ERROR, "invalid key '${strKey}' for section '${strSection}', must be: '" .
                             CONFIG_KEY_STANDBY_MODE . "', '" . CONFIG_KEY_PRIMARY_CONNINFO . "', '" .
                             CONFIG_KEY_TRIGGER_FILE . "', '" . CONFIG_KEY_RESTORE_COMMAND . "', '" .
                             CONFIG_KEY_ARCHIVE_CLEANUP_COMMAND . "', '" . CONFIG_KEY_RECOVERY_END_COMMAND . "'", ERROR_CONFIG);
            }
        }
    }

    # Validate buffer_size
    my @iyRange = [CONFIG_DEFAULT_BUFFER_SIZE_MIN, CONFIG_DEFAULT_BUFFER_SIZE_MAX];

    config_key_valid(CONFIG_SECTION_GENERAL, CONFIG_KEY_BUFFER_SIZE, CONFIG_DEFAULT_BUFFER_SIZE, VALID_RANGE, @iyRange);

    # Validate compress-level
    @iyRange = [CONFIG_DEFAULT_COMPRESS_LEVEL_MIN, CONFIG_DEFAULT_COMPRESS_LEVEL_MAX];

    config_key_valid(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL,
                     CONFIG_DEFAULT_COMPRESS_LEVEL, VALID_RANGE, @iyRange);

    config_key_valid(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS_LEVEL,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL), VALID_RANGE, @iyRange);
    config_key_valid(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_COMPRESS_LEVEL,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL), VALID_RANGE, @iyRange);

    # Validate compress-level-network
    @iyRange = [CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN, CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX];

    config_key_valid(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL_NETWORK,
                     CONFIG_DEFAULT_COMPRESS_LEVEL_NETWORK, VALID_RANGE, @iyRange);

    config_key_valid(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS_LEVEL_NETWORK,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL_NETWORK), VALID_RANGE, @iyRange);
    config_key_valid(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_COMPRESS_LEVEL_NETWORK,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL_NETWORK), VALID_RANGE, @iyRange);
    config_key_valid(CONFIG_SECTION_RESTORE, CONFIG_KEY_COMPRESS_LEVEL_NETWORK,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_COMPRESS_LEVEL_NETWORK), VALID_RANGE, @iyRange);

    # Validate thread-max
    @iyRange = [CONFIG_DEFAULT_THREAD_MAX_MIN, CONFIG_DEFAULT_THREAD_MAX_MAX];

    config_key_valid(CONFIG_SECTION_GENERAL, CONFIG_KEY_THREAD_MAX,
                     CONFIG_DEFAULT_THREAD_MAX, VALID_RANGE, @iyRange);

    config_key_valid(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_THREAD_MAX), VALID_RANGE, @iyRange);
    config_key_valid(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_THREAD_MAX,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_THREAD_MAX), VALID_RANGE, @iyRange);
    config_key_valid(CONFIG_SECTION_RESTORE, CONFIG_KEY_THREAD_MAX,
        config_key_load(CONFIG_SECTION_GENERAL, CONFIG_KEY_THREAD_MAX), VALID_RANGE, @iyRange);
}

####################################################################################################################################
# CONFIG_KEY_VALID
#
# Validate that the config matches the specified range, and default if undefined
####################################################################################################################################
sub config_key_valid
{
    my $strSection = shift;
    my $strKey = shift;
    my $strDefault = shift;
    my $strValidType = shift;
    my $stryValidDataRef = shift;

    # Get the key value or set it to a default
    my $strValue = defined($oConfig{$strSection}{$strKey}) ? $oConfig{$strSection}{$strKey} : $strDefault;

    # Validate the value
    if ($strValidType eq VALID_RANGE)
    {
        if (!defined($strValue) || $strValue < $$stryValidDataRef[0] || $strValue > $$stryValidDataRef[1])
        {
            confess &log(ERROR, "${strSection}::${strKey} is " . (defined($strValue) ? $strValue : 'not set') .
                                ', but should be between ' . $$stryValidDataRef[0] . ' and ' . $$stryValidDataRef[1]);
        }
    }
    else
    {
        confess &log(ASSERT, "invalid validation type ${strValidType}");
    }

    $oConfig{$strSection}{$strKey} = $strValue;

    # Also do validation for the stanza section
    my $strStanza = optionGet(OPTION_STANZA);

    if (substr($strSection, 0, length($strStanza) + 1) ne "${strStanza}:")
    {
        config_key_valid("${strStanza}:${strSection}", $strKey, $strValue, $strValidType, $stryValidDataRef);
    }
}

1;
