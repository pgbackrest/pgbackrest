####################################################################################################################################
# Configuration Definition Data
#
# The configuration is defined in src/build/config/config.yaml, which also contains the documentation.
####################################################################################################################################
package pgBackRestBuild::Config::Data;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Getopt::Long qw(GetOptions);
use Storable qw(dclone);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Command constants
####################################################################################################################################
use constant CFGCMD_BACKUP                                          => 'backup';
    push @EXPORT, qw(CFGCMD_BACKUP);
use constant CFGCMD_HELP                                            => 'help';
    push @EXPORT, qw(CFGCMD_HELP);
use constant CFGCMD_INFO                                            => 'info';
    push @EXPORT, qw(CFGCMD_INFO);
use constant CFGCMD_VERSION                                         => 'version';

####################################################################################################################################
# Command role constants - roles allowed for each command. Commands may have multiple processes that work together to implement
# their functionality.  These roles allow each process to know what it is supposed to do.
####################################################################################################################################
# Called directly by the user. This is the main part of the command that may or may not spawn other command roles.
use constant CFGCMD_ROLE_DEFAULT                                    => 'default';
    push @EXPORT, qw(CFGCMD_ROLE_DEFAULT);

# Async worker that is spawned so the main process can return a result while work continues. An async worker may spawn local or
# remote workers.
use constant CFGCMD_ROLE_ASYNC                                      => 'async';
    push @EXPORT, qw(CFGCMD_ROLE_ASYNC);

# Local worker for parallelizing jobs. A local work may spawn a remote worker.
use constant CFGCMD_ROLE_LOCAL                                      => 'local';
    push @EXPORT, qw(CFGCMD_ROLE_LOCAL);

# Remote worker for accessing resources on another host
use constant CFGCMD_ROLE_REMOTE                                     => 'remote';
    push @EXPORT, qw(CFGCMD_ROLE_REMOTE);

####################################################################################################################################
# Option constants - options that are allowed for commands
####################################################################################################################################

# Command-line only options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_CONFIG                                          => 'config';
    push @EXPORT, qw(CFGOPT_CONFIG);
use constant CFGOPT_STANZA                                          => 'stanza';
    push @EXPORT, qw(CFGOPT_STANZA);

# Command-line only local/remote options
#-----------------------------------------------------------------------------------------------------------------------------------
# Paths
use constant CFGOPT_LOCK_PATH                                       => 'lock-path';
    push @EXPORT, qw(CFGOPT_LOCK_PATH);
use constant CFGOPT_LOG_PATH                                        => 'log-path';
    push @EXPORT, qw(CFGOPT_LOG_PATH);
use constant CFGOPT_SPOOL_PATH                                      => 'spool-path';
    push @EXPORT, qw(CFGOPT_SPOOL_PATH);

# Logging
use constant CFGOPT_LOG_LEVEL_STDERR                                => 'log-level-stderr';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_STDERR);
use constant CFGOPT_LOG_TIMESTAMP                                   => 'log-timestamp';
    push @EXPORT, qw(CFGOPT_LOG_TIMESTAMP);

# Repository options
#-----------------------------------------------------------------------------------------------------------------------------------
# Prefix that must be used by all repo options that allow multiple configurations
use constant CFGDEF_PREFIX_REPO                                     => 'repo';

# Repository General
use constant CFGOPT_REPO_PATH                                       => CFGDEF_PREFIX_REPO . '-path';
    push @EXPORT, qw(CFGOPT_REPO_PATH);

# Repository Host
use constant CFGOPT_REPO_HOST                                       => CFGDEF_PREFIX_REPO . '-host';
use constant CFGOPT_REPO_HOST_CMD                                   => CFGOPT_REPO_HOST . '-cmd';
    push @EXPORT, qw(CFGOPT_REPO_HOST_CMD);

# Stanza options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many databases can be configured
use constant CFGDEF_INDEX_PG                                        => 8;
    push @EXPORT, qw(CFGDEF_INDEX_PG);

# Prefix that must be used by all db options that allow multiple configurations
use constant CFGDEF_PREFIX_PG                                       => 'pg';
    push @EXPORT, qw(CFGDEF_PREFIX_PG);

# Set default PostgreSQL cluster
use constant CFGOPT_PG_HOST                                         => CFGDEF_PREFIX_PG . '-host';
use constant CFGOPT_PG_HOST_CMD                                     => CFGOPT_PG_HOST . '-cmd';
    push @EXPORT, qw(CFGOPT_PG_HOST_CMD);

####################################################################################################################################
# Option definition constants - defines, types, sections, etc.
####################################################################################################################################

# Command defines
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_LOG_FILE                                        => 'log-file';
    push @EXPORT, qw(CFGDEF_LOG_FILE);
use constant CFGDEF_LOG_LEVEL_DEFAULT                               => 'log-level-default';
    push @EXPORT, qw(CFGDEF_LOG_LEVEL_DEFAULT);
use constant CFGDEF_LOCK_REQUIRED                                   => 'lock-required';
    push @EXPORT, qw(CFGDEF_LOCK_REQUIRED);
use constant CFGDEF_LOCK_REMOTE_REQUIRED                            => 'lock-remote-required';
    push @EXPORT, qw(CFGDEF_LOCK_REMOTE_REQUIRED);

use constant CFGDEF_LOCK_TYPE                                       => 'lock-type';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE);
use constant CFGDEF_LOCK_TYPE_NONE                                  => 'none';

use constant CFGDEF_PARAMETER_ALLOWED                               => 'parameter-allowed';
    push @EXPORT, qw(CFGDEF_PARAMETER_ALLOWED);

# Option defines
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_ALLOW_LIST                                      => 'allow-list';
    push @EXPORT, qw(CFGDEF_ALLOW_LIST);
use constant CFGDEF_ALLOW_RANGE                                     => 'allow-range';
    push @EXPORT, qw(CFGDEF_ALLOW_RANGE);
use constant CFGDEF_DEFAULT                                         => 'default';
    push @EXPORT, qw(CFGDEF_DEFAULT);
use constant CFGDEF_DEFAULT_LITERAL                                 => 'default-literal';
    push @EXPORT, qw(CFGDEF_DEFAULT_LITERAL);
use constant CFGDEF_DEPEND                                          => 'depend';
    push @EXPORT, qw(CFGDEF_DEPEND);
use constant CFGDEF_DEPEND_OPTION                                   => 'option';
    push @EXPORT, qw(CFGDEF_DEPEND_OPTION);
use constant CFGDEF_DEPEND_LIST                                     => 'list';
    push @EXPORT, qw(CFGDEF_DEPEND_LIST);

# Group options together to share common configuration
use constant CFGDEF_GROUP                                           => 'group';
    push @EXPORT, qw(CFGDEF_GROUP);

use constant CFGDEF_INDEX                                           => 'index';
    push @EXPORT, qw(CFGDEF_INDEX);
use constant CFGDEF_INDEX_TOTAL                                     => 'indexTotal';
    push @EXPORT, qw(CFGDEF_INDEX_TOTAL);
use constant CFGDEF_INHERIT                                         => 'inherit';
    push @EXPORT, qw(CFGDEF_INHERIT);
use constant CFGDEF_INTERNAL                                        => 'internal';
    push @EXPORT, qw(CFGDEF_INTERNAL);
use constant CFGDEF_DEPRECATE                                       => 'deprecate';
    push @EXPORT, qw(CFGDEF_DEPRECATE);
use constant CFGDEF_NEGATE                                          => 'negate';
    push @EXPORT, qw(CFGDEF_NEGATE);
use constant CFGDEF_PREFIX                                          => 'prefix';
    push @EXPORT, qw(CFGDEF_PREFIX);
use constant CFGDEF_COMMAND                                         => 'command';
    push @EXPORT, qw(CFGDEF_COMMAND);
use constant CFGDEF_COMMAND_ROLE                                    => 'command-role';
    push @EXPORT, qw(CFGDEF_COMMAND_ROLE);
use constant CFGDEF_REQUIRED                                        => 'required';
    push @EXPORT, qw(CFGDEF_REQUIRED);
use constant CFGDEF_RESET                                           => 'reset';
    push @EXPORT, qw(CFGDEF_RESET);
use constant CFGDEF_SECTION                                         => 'section';
    push @EXPORT, qw(CFGDEF_SECTION);
use constant CFGDEF_SECURE                                          => 'secure';
    push @EXPORT, qw(CFGDEF_SECURE);
use constant CFGDEF_TYPE                                            => 'type';
    push @EXPORT, qw(CFGDEF_TYPE);

# Option types
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_TYPE_BOOLEAN                                    => 'boolean';
    push @EXPORT, qw(CFGDEF_TYPE_BOOLEAN);
use constant CFGDEF_TYPE_HASH                                       => 'hash';
    push @EXPORT, qw(CFGDEF_TYPE_HASH);
use constant CFGDEF_TYPE_INTEGER                                    => 'integer';
    push @EXPORT, qw(CFGDEF_TYPE_INTEGER);
use constant CFGDEF_TYPE_LIST                                       => 'list';
    push @EXPORT, qw(CFGDEF_TYPE_LIST);
use constant CFGDEF_TYPE_PATH                                       => 'path';
    push @EXPORT, qw(CFGDEF_TYPE_PATH);
use constant CFGDEF_TYPE_STRING                                     => 'string';
    push @EXPORT, qw(CFGDEF_TYPE_STRING);
use constant CFGDEF_TYPE_SIZE                                       => 'size';
    push @EXPORT, qw(CFGDEF_TYPE_SIZE);
use constant CFGDEF_TYPE_TIME                                       => 'time';
    push @EXPORT, qw(CFGDEF_TYPE_TIME);

# Option config sections
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CFGDEF_SECTION_GLOBAL);
use constant CFGDEF_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CFGDEF_SECTION_STANZA);

####################################################################################################################################
# Load configuration
####################################################################################################################################
use YAML::XS qw(LoadFile);

# Required so booleans are not read-only
local $YAML::XS::Boolean = "JSON::PP";

my $rhConfig = LoadFile(dirname(dirname($0)) . '/src/build/config/config.yaml');
my $rhCommandDefine = $rhConfig->{'command'};
my $rhOptionGroupDefine = $rhConfig->{'optionGroup'};
my $rhConfigDefine = $rhConfig->{'option'};

####################################################################################################################################
# Fix errors introduced by YAML::XS::LoadFile. This is typically fixed by setting local $YAML::XS::Boolean = "JSON::PP", but older
# Debian/Ubuntu versions do not support this fix. Some booleans get set read only and others also end up as empty strings. There is
# no apparent pattern to what gets broken so it is important to be on the lookout for strange output when adding new options.
#
# ??? For now this code is commented out since packages for older Debians can be built using backports. It is being preserved just
# in case it is needed before the migration to C is complete.
####################################################################################################################################
# sub optionDefineFixup
# {
#     my $strKey = shift;
#     my $rhDefine = shift;
#
#     # Fix read-only required values so they are writable
#     if (defined($rhDefine->{&CFGDEF_REQUIRED}))
#     {
#         my $value = $rhDefine->{&CFGDEF_REQUIRED} ? true : false;
#         delete($rhDefine->{&CFGDEF_REQUIRED});
#         $rhDefine->{&CFGDEF_REQUIRED} = $value;
#     }
#
#     # If the default is an empty string set to false. This must be a mangled boolean since empty strings are not valid defaults.
#     if (defined($rhDefine->{&CFGDEF_DEFAULT}) && $rhDefine->{&CFGDEF_DEFAULT} eq '')
#     {
#         delete($rhDefine->{&CFGDEF_DEFAULT});
#         $rhDefine->{&CFGDEF_DEFAULT} = false;
#     }
#
#     # If a depend list value is an empty string set it to false. This must be a mangled boolean since empty strings are not valid
#     # depend list values.
#     if (ref($rhDefine->{&CFGDEF_DEPEND}) && defined($rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST}))
#     {
#         my @stryList;
#
#         for (my $iDependListIdx = 0; $iDependListIdx < @{$rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST}}; $iDependListIdx++)
#         {
#             if ($rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST}[$iDependListIdx] eq '')
#             {
#                 push(@stryList, false);
#             }
#             else
#             {
#                 push(@stryList, $rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST}[$iDependListIdx]);
#             }
#         }
#
#         delete($rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST});
#         $rhDefine->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST} = \@stryList;
#     }
# }
#
# # Fix all options
# foreach my $strKey (sort(keys(%{$rhConfigDefine})))
# {
#     my $rhOption = $rhConfigDefine->{$strKey};
#     optionDefineFixup($strKey, $rhOption);
#
#     # Fix all option commands
#     if (ref($rhOption->{&CFGDEF_COMMAND}))
#     {
#         foreach my $strCommand (sort(keys(%{$rhOption->{&CFGDEF_COMMAND}})))
#         {
#             optionDefineFixup("$strKey-$strCommand", $rhOption->{&CFGDEF_COMMAND}{$strCommand});
#         }
#     }
# }

####################################################################################################################################
# Process command define defaults
####################################################################################################################################
foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
{
    # Commands are external by default
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_INTERNAL}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_INTERNAL} = false;
    }

    # Log files are created by default
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOG_FILE}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOG_FILE} = true;
    }

    # Default log level is INFO
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOG_LEVEL_DEFAULT}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOG_LEVEL_DEFAULT} = INFO;
    }

    # Default lock required is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED} = false;
    }

    # Default lock remote required is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REMOTE_REQUIRED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REMOTE_REQUIRED} = false;
    }

    # Lock type must be set if a lock is required
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE}))
    {
        # Is a lock type required?
        if ($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED})
        {
            confess &log(ERROR, "lock type is required for command '${strCommand}'");
        }

        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE} = CFGDEF_LOCK_TYPE_NONE;
    }
    else
    {
        if ($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED} &&
            $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE} eq CFGDEF_LOCK_TYPE_NONE)
        {
            confess &log(ERROR, "lock type is required for command '${strCommand}' and cannot be 'none'");
        }
    }

    # Default parameter allowed is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_PARAMETER_ALLOWED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_PARAMETER_ALLOWED} = false;
    }

    # All commands have the default role
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_COMMAND_ROLE}{&CFGCMD_ROLE_DEFAULT}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_COMMAND_ROLE}{&CFGCMD_ROLE_DEFAULT} = {};
    }
}

####################################################################################################################################
# Process option group defaults
####################################################################################################################################
foreach my $strGroup (sort(keys(%{$rhOptionGroupDefine})))
{
    # Error if prefix and index total are not both defined
    if ((defined($rhOptionGroupDefine->{$strGroup}{&CFGDEF_PREFIX}) &&
            !defined($rhOptionGroupDefine->{$strGroup}{&CFGDEF_INDEX_TOTAL})) ||
        (!defined($rhOptionGroupDefine->{$strGroup}{&CFGDEF_PREFIX}) &&
            defined($rhOptionGroupDefine->{$strGroup}{&CFGDEF_INDEX_TOTAL})))
    {
        confess &log(
            ASSERT, "CFGDEF_PREFIX and CFGDEF_INDEX_TOTAL must both be defined (or neither) for option group '${strGroup}'");
    }
}

####################################################################################################################################
# Process option define defaults
####################################################################################################################################
foreach my $strKey (sort(keys(%{$rhConfigDefine})))
{
    my $rhOption = $rhConfigDefine->{$strKey};

    # Error on invalid configuration
    if (defined($rhOption->{&CFGDEF_INDEX_TOTAL}))
    {
        confess &log(ASSERT, "CFGDEF_INDEX_TOTAL cannot be defined for option '${strKey}'");
    }

    if (defined($rhOption->{&CFGDEF_PREFIX}))
    {
        confess &log(ASSERT, "CFGDEF_PREFIX cannot be defined for option '${strKey}'");
    }

    # If the define is a scalar then copy the entire define from the referenced option
    if (defined($rhConfigDefine->{$strKey}{&CFGDEF_INHERIT}))
    {
        # Make a copy in case there are overrides that need to be applied after inheriting
        my $hConfigDefineOverride = dclone($rhConfigDefine->{$strKey});

        # Copy the option being inherited from
        $rhConfigDefine->{$strKey} = dclone($rhConfigDefine->{$rhConfigDefine->{$strKey}{&CFGDEF_INHERIT}});

        # No need to copy the inheritance key
        delete($rhConfigDefine->{$strKey}{&CFGDEF_INHERIT});

        # It makes no sense to inherit deprecations - they must be specified for each option
        delete($rhConfigDefine->{$strKey}{&CFGDEF_DEPRECATE});

        # Apply overrides
        foreach my $strOptionDef (sort(keys(%{$hConfigDefineOverride})))
        {
            $rhConfigDefine->{$strKey}{$strOptionDef} = $hConfigDefineOverride->{$strOptionDef};
        }

        # Update option variable with new hash reference
        $rhOption = $rhConfigDefine->{$strKey}
    }

    # If the option group is defined then copy configuration from the group to the option
    if (defined($rhOption->{&CFGDEF_GROUP}))
    {
        my $rhGroup = $rhOptionGroupDefine->{$rhConfigDefine->{$strKey}{&CFGDEF_GROUP}};

        $rhOption->{&CFGDEF_INDEX_TOTAL} = $rhGroup->{&CFGDEF_INDEX_TOTAL};
        $rhOption->{&CFGDEF_PREFIX} = $rhGroup->{&CFGDEF_PREFIX};
    }

    # If command is not specified then the option is valid for all commands except version and help
    if (!defined($rhOption->{&CFGDEF_COMMAND}))
    {
        foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
        {
            next if $strCommand eq CFGCMD_HELP || $strCommand eq CFGCMD_VERSION;

            $rhOption->{&CFGDEF_COMMAND}{$strCommand} = {};
        }
    }
    # Else if the command section is a scalar then copy the section from the referenced option
    elsif (defined($rhConfigDefine->{$strKey}{&CFGDEF_COMMAND}) && !ref($rhConfigDefine->{$strKey}{&CFGDEF_COMMAND}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_COMMAND} =
            dclone($rhConfigDefine->{$rhConfigDefine->{$strKey}{&CFGDEF_COMMAND}}{&CFGDEF_COMMAND});
    }

    # If the required section is a scalar then copy the section from the referenced option
    if (defined($rhConfigDefine->{$strKey}{&CFGDEF_DEPEND}) && !ref($rhConfigDefine->{$strKey}{&CFGDEF_DEPEND}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_DEPEND} =
            dclone($rhConfigDefine->{$rhConfigDefine->{$strKey}{&CFGDEF_DEPEND}}{&CFGDEF_DEPEND});
    }

    # If the allow list is a scalar then copy the list from the referenced option
    if (defined($rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_LIST}) && !ref($rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_LIST}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_LIST} =
            dclone($rhConfigDefine->{$rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_LIST}}{&CFGDEF_ALLOW_LIST});
    }

    # Default type is string
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_TYPE}))
    {
        &log(ASSERT, "type is required for option '${strKey}'");
    }

    # Default required is true
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_REQUIRED}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_REQUIRED} = true;
    }

    # Default internal is false
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_INTERNAL}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_INTERNAL} = false;
    }

    # Set index total for any option where it has not been explicitly defined
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_INDEX_TOTAL}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_INDEX_TOTAL} = 1;
    }

    # All boolean config options can be negated.  Boolean command-line options must be marked for negation individually.
    if ($rhConfigDefine->{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_BOOLEAN && defined($rhConfigDefine->{$strKey}{&CFGDEF_SECTION}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_NEGATE} = true;
    }

    # Default for negation is false
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_NEGATE}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_NEGATE} = false;
    }

    # All config options can be reset
    if (defined($rhConfigDefine->{$strKey}{&CFGDEF_SECTION}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_RESET} = true;
    }
    elsif (!defined($rhConfigDefine->{$strKey}{&CFGDEF_RESET}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_RESET} = false;
    }

    # By default options are not secure
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_SECURE}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_SECURE} = false;
    }

    # Set all indices to 1 by default - this defines how many copies of any option there can be
    if (!defined($rhConfigDefine->{$strKey}{&CFGDEF_INDEX_TOTAL}))
    {
        $rhConfigDefine->{$strKey}{&CFGDEF_INDEX_TOTAL} = 1;
    }

    # All int, size and time options must have an allow range
    if (($rhConfigDefine->{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_INTEGER ||
         $rhConfigDefine->{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_TIME ||
         $rhConfigDefine->{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_SIZE) &&
         !(defined($rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_RANGE}) || defined($rhConfigDefine->{$strKey}{&CFGDEF_ALLOW_LIST})))
    {
        confess &log(ASSERT, "int/size/time option '${strKey}' must have allow range or list");
    }

    # Ensure all commands are valid
    foreach my $strCommand (sort(keys(%{$rhConfigDefine->{$strKey}{&CFGDEF_COMMAND}})))
    {
        if (!defined($rhCommandDefine->{$strCommand}))
        {
            confess &log(ASSERT, "invalid command '${strCommand}'");
        }
    }
}

# Generate valid command roles for each option
foreach my $strOption (sort(keys(%{$rhConfigDefine})))
{
    my $rhOption = $rhConfigDefine->{$strOption};

    # Generate valid command roles for each command in the option
    foreach my $strCommand (sort(keys(%{$rhOption->{&CFGDEF_COMMAND}})))
    {
        # If command roles are defined in the option command override then check that they are valid
        if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}))
        {
            foreach my $strCommandRole (sort(keys(%{$rhOption->{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}})))
            {
                if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_COMMAND_ROLE}{$strCommandRole}))
                {
                    confess &log(
                        ASSERT, "option '${strOption}', command '${strCommand}' has invalid command role '${strCommandRole}'");
                }
            }
        }
        # Else if the option has command roles defined then use the intersection of command roles with the command
        elsif (defined($rhOption->{&CFGDEF_COMMAND_ROLE}))
        {
            foreach my $strCommandRole (sort(keys(%{$rhOption->{&CFGDEF_COMMAND_ROLE}})))
            {
                if (defined($rhCommandDefine->{$strCommand}{&CFGDEF_COMMAND_ROLE}{$strCommandRole}))
                {
                    $rhOption->{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}{$strCommandRole} = {};
                }
            }
        }
        # Else copy the command roles from the command
        else
        {
            foreach my $strCommandRole (sort(keys(%{$rhCommandDefine->{$strCommand}{&CFGDEF_COMMAND_ROLE}})))
            {
                $rhOption->{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}{$strCommandRole} = {};
            }
        }
    }

    # Remove option command roles so they don't accidentally get used in processing (since they were copied to option commands)
    delete($rhOption->{&CFGDEF_COMMAND_ROLE});
}

####################################################################################################################################
# Get option definition
####################################################################################################################################
sub cfgDefine
{
    return dclone($rhConfigDefine);
}

push @EXPORT, qw(cfgDefine);

####################################################################################################################################
# Get command definition
####################################################################################################################################
sub cfgDefineCommand
{
    return dclone($rhCommandDefine);
}

push @EXPORT, qw(cfgDefineCommand);

####################################################################################################################################
# Get option group definition
####################################################################################################################################
sub cfgDefineOptionGroup
{
    return dclone($rhOptionGroupDefine);
}

push @EXPORT, qw(cfgDefineOptionGroup);

####################################################################################################################################
# Get list of all commands
####################################################################################################################################
sub cfgDefineCommandList
{
    # Return sorted list
    return (sort(keys(%{$rhCommandDefine})));
}

push @EXPORT, qw(cfgDefineCommandList);

####################################################################################################################################
# Get list of all option types
####################################################################################################################################
sub cfgDefineOptionTypeList
{
    my $rhOptionTypeMap;

    # Get unique list of types
    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $strOptionType = $rhConfigDefine->{$strOption}{&CFGDEF_TYPE};

        if (!defined($rhOptionTypeMap->{$strOptionType}))
        {
            $rhOptionTypeMap->{$strOptionType} = true;
        }
    };

    # Return sorted list
    return (sort(keys(%{$rhOptionTypeMap})));
}

push @EXPORT, qw(cfgDefineOptionTypeList);

1;
