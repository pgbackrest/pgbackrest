####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package BackRest::Config;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

use Exporter qw(import);

our @EXPORT = qw(config_load config_key_load

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
# Configuration constants - configuration sections and keys
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

my $strStanza;          # Config stanza

####################################################################################################################################
# CONFIG_LOAD
#
# Load config file.
####################################################################################################################################
sub config_load
{
    my $strFile = shift;    # Full path to ini file to load from
    my $strStanzaParam = shift;  # Stanza specified on command line

    if (!defined($strFile))
    {
        $strFile = '/etc/pg_backrest.conf';
    }

    ini_load($strFile, \%oConfig);

    # Load and check the cluster
    if (!defined($strStanzaParam))
    {
        confess 'a backup stanza must be specified';
    }

    # Set the log levels
    log_level_set(uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_FILE, true, INFO)),
                  uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_CONSOLE, true, ERROR)));

    # Set globals
    $strStanza = $strStanzaParam;
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
        $strValue = $oConfig{"${strStanza}"}{"${strKey}"};
    }
    # Else look in the supplied section
    else
    {
        # First check the stanza section
        $strValue = $oConfig{"${strStanza}:${strSection}"}{"${strKey}"};

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

1;
