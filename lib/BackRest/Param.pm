####################################################################################################################################
# PARAM MODULE
####################################################################################################################################
package BackRest::Param;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Pod::Usage;
use File::Basename;
use Getopt::Long qw(GetOptions);
use Storable qw(dclone);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

use Exporter qw(import);

our @EXPORT = qw(configLoad optionGet optionTest optionRuleGet operationGet operationTest operationSet

                 OP_ARCHIVE_GET OP_ARCHIVE_PUSH OP_BACKUP OP_RESTORE OP_EXPIRE

                 BACKUP_TYPE_FULL BACKUP_TYPE_DIFF BACKUP_TYPE_INCR

                 RECOVERY_TYPE_NAME RECOVERY_TYPE_TIME RECOVERY_TYPE_XID RECOVERY_TYPE_PRESERVE RECOVERY_TYPE_NONE
                 RECOVERY_TYPE_DEFAULT

                 OPTION_CONFIG OPTION_STANZA OPTION_TYPE OPTION_DELTA OPTION_SET OPTION_NO_START_STOP OPTION_FORCE OPTION_TARGET
                 OPTION_TARGET_EXCLUSIVE OPTION_TARGET_RESUME OPTION_TARGET_TIMELINE OPTION_THREAD_MAX

                 OPTION_VERSION OPTION_HELP OPTION_TEST OPTION_TEST_DELAY OPTION_TEST_NO_FORK

                 OPTION_DEFAULT_RESTORE_SET);

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
# RECOVERY Type Constants
####################################################################################################################################
use constant
{
    RECOVERY_TYPE_NAME          => 'name',
    RECOVERY_TYPE_TIME          => 'time',
    RECOVERY_TYPE_XID           => 'xid',
    RECOVERY_TYPE_PRESERVE      => 'preserve',
    RECOVERY_TYPE_NONE          => 'none',
    RECOVERY_TYPE_DEFAULT       => 'default'
};

####################################################################################################################################
# Option constants
####################################################################################################################################
use constant
{
    # Command-line-only options
    OPTION_CONFIG            => 'config',
    OPTION_DELTA             => 'delta',
    OPTION_FORCE             => 'force',
    OPTION_NO_START_STOP     => 'no-start-stop',
    OPTION_SET               => 'set',
    OPTION_STANZA            => 'stanza',
    OPTION_TARGET            => 'target',
    OPTION_TARGET_EXCLUSIVE  => 'target-exclusive',
    OPTION_TARGET_RESUME     => 'target-resume',
    OPTION_TARGET_TIMELINE   => 'target-timeline',
    OPTION_TYPE              => 'type',

    # Command-line-only/conf file options
    OPTION_THREAD_MAX        => 'thread-max',

    # Command-line-only help/version options
    OPTION_HELP              => 'help',
    OPTION_VERSION           => 'version',

    # Command-line-only test options
    OPTION_TEST              => 'test',
    OPTION_TEST_DELAY        => 'test-delay',
    OPTION_TEST_NO_FORK      => 'no-fork'
};

####################################################################################################################################
# Option Defaults
####################################################################################################################################
use constant
{
    OPTION_DEFAULT_CONFIG                   => '/etc/pg_backrest.conf',
    OPTION_DEFAULT_THREAD_MAX               => 1,

    OPTION_DEFAULT_BACKUP_FORCE             => false,
    OPTION_DEFAULT_BACKUP_NO_START_STOP     => false,
    OPTION_DEFAULT_BACKUP_TYPE              => BACKUP_TYPE_INCR,

    OPTION_DEFAULT_RESTORE_DELTA            => false,
    OPTION_DEFAULT_RESTORE_FORCE            => false,
    OPTION_DEFAULT_RESTORE_SET              => 'latest',
    OPTION_DEFAULT_RESTORE_TYPE             => RECOVERY_TYPE_DEFAULT,
    OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE => false,
    OPTION_DEFAULT_RESTORE_TARGET_RESUME    => false,

    OPTION_DEFAULT_HELP                     => false,
    OPTION_DEFAULT_VERSION                  => false,

    OPTION_DEFAULT_TEST                     => false,
    OPTION_DEFAULT_TEST_DELAY               => 5,
    OPTION_DEFAULT_TEST_NO_FORK             => false
};

####################################################################################################################################
# Option Rules
####################################################################################################################################
use constant
{
    OPTION_RULE_ALLOW_LIST       => 'allow-list',
    OPTION_RULE_DEFAULT          => 'default',
    OPTION_RULE_DEPEND           => 'depend',
    OPTION_RULE_DEPEND_OPTION    => 'depend-option',
    OPTION_RULE_DEPEND_LIST      => 'depend-list',
    OPTION_RULE_DEPEND_VALUE     => 'depend-value',
    OPTION_RULE_REQUIRED         => 'required',
    OPTION_RULE_OPERATION        => 'operation',
    OPTION_RULE_TYPE             => 'type'
};

####################################################################################################################################
# Option Types
####################################################################################################################################
use constant
{
    OPTION_TYPE_STRING       => 'string',
    OPTION_TYPE_BOOLEAN      => 'boolean',
    OPTION_TYPE_INTEGER      => 'integer',
    OPTION_TYPE_FLOAT        => 'float'
};

####################################################################################################################################
# Option Rule Hash
####################################################################################################################################
my %oOptionRule =
(
    &OPTION_CONFIG =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_CONFIG
    },

    &OPTION_DELTA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_DELTA,
            }
        }
    },

    &OPTION_FORCE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_FORCE,
            },

            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_FORCE,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_NO_START_STOP,
                    &OPTION_RULE_DEPEND_VALUE => true
                }
            }
        }
    },

    &OPTION_NO_START_STOP =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_NO_START_STOP
            }
        }
    },

    &OPTION_SET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TYPE,
            }
        }
    },

    &OPTION_STANZA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING
    },

    &OPTION_TARGET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_EXCLUSIVE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_RESUME =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TARGET_RESUME,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_TIMELINE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_REQUIRED => false,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_DEFAULT => true,
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID => true
                    }
                }
            }
        }
    },

    &OPTION_TYPE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &BACKUP_TYPE_FULL => true,
                    &BACKUP_TYPE_DIFF => true,
                    &BACKUP_TYPE_INCR => true,
                }
            },

            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &RECOVERY_TYPE_NAME          => true,
                    &RECOVERY_TYPE_TIME          => true,
                    &RECOVERY_TYPE_XID           => true,
                    &RECOVERY_TYPE_PRESERVE      => true,
                    &RECOVERY_TYPE_NONE          => true,
                    &RECOVERY_TYPE_DEFAULT       => true
                }
            }
        }
    },

    &OPTION_THREAD_MAX =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_THREAD_MAX,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_HELP =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_HELP
    },

    &OPTION_VERSION =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_VERSION
    },

    &OPTION_TEST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST
    },

    &OPTION_TEST_DELAY =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_FLOAT,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_DELAY,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST,
            &OPTION_RULE_DEPEND_VALUE => true
        }
    },

    &OPTION_TEST_NO_FORK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_NO_FORK,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST
        }
    }
);

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oOption;            # Option hash
my $strOperation;       # Operation (backup, archive-get, ...)

####################################################################################################################################
# configLoad
#
# Load configuration.
####################################################################################################################################
sub configLoad
{
    # Clear option in case it was loaded before
    %oOption = ();

    # Build hash with all valid command-line options
    my %oOption;

    foreach my $strKey (keys(%oOptionRule))
    {
        my $strOption = $strKey;

        if (!defined($oOptionRule{$strKey}{&OPTION_RULE_TYPE}))
        {
            confess  &log(ASSERT, "Option ${strKey} does not have a defined type", ERROR_ASSERT);
        }
        elsif ($oOptionRule{$strKey}{&OPTION_RULE_TYPE} ne OPTION_TYPE_BOOLEAN)
        {
            $strOption .= '=s';
        }

        $oOption{$strOption} = $strOption;
    }

    # Get command-line options
    my %oOptionTest;

    GetOptions(\%oOptionTest, %oOption)
        or pod2usage(2);

    # Validate and store options
    optionValid(\%oOptionTest);

    # Display version and exit if requested
    if (optionGet(OPTION_VERSION) || optionGet(OPTION_HELP))
    {
        print 'pg_backrest ' . version_get() . "\n";

        if (!OPTION_get(OPTION_HELP))
        {
            exit 0;
        }
    }

    # Display help and exit if requested
    if (optionGet(OPTION_HELP))
    {
        print "\n";
        pod2usage();
    }

    # Set test options
    !optionGet(OPTION_TEST) or test_set(optionGet(OPTION_TEST), optionGet(OPTION_TEST_DELAY));
}

####################################################################################################################################
# optionValid
#
# Make sure the command-line options are valid based on the operation.
####################################################################################################################################
sub optionValid
{
    my $oOptionTest = shift;

    # Check that the operation is present and valid
    $strOperation = $ARGV[0];

    if (!defined($strOperation))
    {
        confess &log(ERROR, "operation must be specified", ERROR_OPERATION_REQUIRED);
    }

    if ($strOperation ne OP_ARCHIVE_GET &&
        $strOperation ne OP_ARCHIVE_PUSH &&
        $strOperation ne OP_BACKUP &&
        $strOperation ne OP_RESTORE &&
        $strOperation ne OP_EXPIRE)
    {
        confess &log(ERROR, "invalid operation ${strOperation}");
    }

    # Keep track of unresolved dependencies
    my $bDependUnresolved = true;
    my %oOptionResolved;

    # Loop through all possible options
    while ($bDependUnresolved)
    {
        # Assume that all dependencies will be resolved in this loop
        $bDependUnresolved = false;

        foreach my $strOption (sort(keys(%oOptionRule)))
        {
            # Skip the option if it has been resolved in a prior loop
            if (defined($oOptionResolved{$strOption}))
            {
                next;
            }

            # Check dependency for the operation then for the option
            my $oDepend;
            my $bDependResolved = true;

            if (defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) &&
                defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) &&
                ref($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) eq 'HASH')
            {
                $oDepend = $oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}{&OPTION_RULE_DEPEND};
            }

            if (!defined($oDepend))
            {
                $oDepend = $oOptionRule{$strOption}{&OPTION_RULE_DEPEND};
            }

            if (defined($oDepend))
            {
                # Make sure the depend option has been resolved, otherwise skip this option for now
                my $strDependOption = $$oDepend{&OPTION_RULE_DEPEND_OPTION};

                if (!defined($oOptionResolved{$strDependOption}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                # Check if the depend option has a value
                my $strDependValue = $oOption{$strDependOption};
                my $strError = "option '${strOption}' not valid without option '${strDependOption}'";

                $bDependResolved = defined($strDependValue) ? true : false;

                if (!$bDependResolved && defined($$oOptionTest{$strOption}))
                {
                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                # If a depend value exists, make sure the option value matches
                if ($bDependResolved && defined($$oDepend{&OPTION_RULE_DEPEND_VALUE}) &&
                    $$oDepend{&OPTION_RULE_DEPEND_VALUE} ne $strDependValue)
                {
                    $bDependResolved = false;

                    if (defined($$oOptionTest{$strOption}))
                    {
                        if ($oOptionRule{$strDependOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
                        {
                            if (!$$oDepend{&OPTION_RULE_DEPEND_VALUE})
                            {
                                confess &log(ASSERT, "no error has been created for unused case where depend value = false");
                            }
                        }
                        else
                        {
                            $strError .= " = '$$oDepend{&OPTION_RULE_DEPEND_VALUE}'";
                        }

                        confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                    }
                }

                # If a depend list exists, make sure the value is in the list
                if ($bDependResolved && defined($$oDepend{&OPTION_RULE_DEPEND_LIST}) &&
                    !defined($$oDepend{&OPTION_RULE_DEPEND_LIST}{$strDependValue}))
                {
                    $bDependResolved = false;

                    if (defined($$oOptionTest{$strOption}))
                    {
                        my @oyValue;

                        foreach my $strValue (sort(keys($$oDepend{&OPTION_RULE_DEPEND_LIST})))
                        {
                            push(@oyValue, "'${strValue}'");
                        }

                        $strError .= @oyValue == 1 ? " = $oyValue[0]" : " in (" . join(", ", @oyValue) . ")";
                        confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                    }
                }
            }

            # Is the option defined?
            if (defined($$oOptionTest{$strOption}))
            {
                my $strValue = $$oOptionTest{$strOption};

                # Test option type
                if ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_INTEGER ||
                    $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_FLOAT)
                {
                    # Test that the string is a valid float or integer  by adding 1 to it.  It's pretty hokey but it works and it
                    # beats requiring Scalar::Util::Numeric to do it properly.
                    eval
                    {
                        my $strTest = $strValue + 1;
                    };

                    my $bError = $@ ? true : false;

                    # Check that integers are really integers
                    if (!$bError && $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_INTEGER &&
                        (int($strValue) . 'S') ne ($strValue . 'S'))
                    {
                        $bError = true;
                    }

                    # Error if the value did not pass tests
                    !$bError
                        or confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow list for the operation then for the option
                my $oAllow;

                if (defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) &&
                    defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) &&
                    ref($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) eq 'HASH')
                {
                    $oAllow = $oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}{&OPTION_RULE_ALLOW_LIST};
                }

                if (!defined($oAllow))
                {
                    $oAllow = $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_LIST};
                }

                if (defined($oAllow) && !defined($$oAllow{$$oOptionTest{$strOption}}))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Set option value
                $oOption{$strOption} = $strValue;
            }
            # Else set the default if required
            elsif (!defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) ||
                    defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}))
            {
                # Check for default in operation then option
                my $strDefault;

                if (defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) &&
                    defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) &&
                    ref($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) eq 'HASH')
                {
                    $strDefault = $oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}{&OPTION_RULE_DEFAULT}
                }

                if (!defined($strDefault))
                {
                    $strDefault = $oOptionRule{$strOption}{&OPTION_RULE_DEFAULT};
                }

                # If default is defined
                if (defined($strDefault))
                {
                    # Only set default if dependency is resolved
                    $oOption{$strOption} = $strDefault if $bDependResolved;
                }
                # Else error
                else
                {
                    # Check for required in operation then option
                    my $bRequired;

                    if (defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) &&
                        defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) &&
                        ref($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) eq 'HASH')
                    {
                        $bRequired = $oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}{&OPTION_RULE_REQUIRED};
                    }

                    if (!defined($bRequired))
                    {
                        $bRequired = $oOptionRule{$strOption}{&OPTION_RULE_REQUIRED};
                    }

                    if (!defined($bRequired) || $bRequired)
                    {
                        if ($bDependResolved)
                        {
                            confess &log(ERROR, "${strOperation} operation requires option: ${strOption}", ERROR_OPTION_REQUIRED);
                        }
                    }
                }
            }

            $oOptionResolved{$strOption} = true;
        }
    }
}

####################################################################################################################################
# operationGet
#
# Get the current operation.
####################################################################################################################################
sub operationGet
{
    return $strOperation;
}

####################################################################################################################################
# operationTest
#
# Test the current operation.
####################################################################################################################################
sub operationTest
{
    my $strOperationTest = shift;

    return $strOperationTest eq $strOperation;
}

####################################################################################################################################
# operationSet
#
# Set current operation (usually for triggering follow-on operations).
####################################################################################################################################
sub operationSet
{
    my $strValue = shift;

    $strOperation = $strValue;
}

####################################################################################################################################
# optionGet
#
# Get option value.
####################################################################################################################################
sub optionGet
{
    my $strOption = shift;
    my $bRequired = shift;

    if (!defined($oOption{$strOption}) && (!defined($bRequired) || $bRequired))
    {
        confess &log(ASSERT, "option ${strOption} is required");
    }

    return $oOption{$strOption};
}

####################################################################################################################################
# optionTest
#
# Test a option value.
####################################################################################################################################
sub optionTest
{
    my $strOption = shift;
    my $strValue = shift;

    if (defined($strValue))
    {
        return optionGet($strOption) eq $strValue;
    }

    return defined($oOption{$strOption});
}

####################################################################################################################################
# optionRuleGet
#
# Get the option rules.
####################################################################################################################################
sub optionRuleGet
{
    return dclone(\%oOptionRule);
}

1;
