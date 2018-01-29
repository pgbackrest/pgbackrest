####################################################################################################################################
# ConfigCommonTest.pm - Common code for Config unit tests
####################################################################################################################################
package pgBackRestTest::Env::ConfigEnvTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Getopt::Long qw(GetOptions);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Version;

use constant CONFIGENVTEST                                          => 'ConfigEnvTest';

sub optionTestSet
{
    my $self = shift;
    my $iOptionId = shift;
    my $strValue = shift;

    $self->{&CONFIGENVTEST}{option}{cfgOptionName($iOptionId)} = $strValue;
}

sub optionTestSetBool
{
    my $self = shift;
    my $iOptionId = shift;
    my $bValue = shift;

    $self->{&CONFIGENVTEST}{boolean}{cfgOptionName($iOptionId)} = defined($bValue) ? $bValue : true;
}

sub optionTestClear
{
    my $self = shift;
    my $iOptionId = shift;

    delete($self->{&CONFIGENVTEST}{option}{cfgOptionName($iOptionId)});
    delete($self->{&CONFIGENVTEST}{boolean}{cfgOptionName($iOptionId)});
}

sub configTestClear
{
    my $self = shift;

    my $rhConfig = $self->{&CONFIGENVTEST};

    delete($self->{&CONFIGENVTEST});

    return $rhConfig;
}

sub configTestSet
{
    my $self = shift;
    my $rhConfig = shift;

    $self->{&CONFIGENVTEST} = $rhConfig;
}

sub commandTestWrite
{
    my $self = shift;
    my $strCommand = shift;
    my $rhConfig = shift;

    my @szyParam = ();

    if (defined($rhConfig->{boolean}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{boolean}})))
        {
            if ($rhConfig->{boolean}{$strOption})
            {
                push(@szyParam, "--${strOption}");
            }
            else
            {
                push(@szyParam, "--no-${strOption}");
            }
        }
    }

    if (defined($rhConfig->{option}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{option}})))
        {
            push(@szyParam, "--${strOption}=$rhConfig->{option}{$strOption}");
        }
    }

    push(@szyParam, $strCommand);

    return @szyParam;
}

####################################################################################################################################
# testOptionValidate
#
# Make sure the command-line options are valid based on the command.
####################################################################################################################################
sub testOptionValidate
{
    my $iCommandId = shift;

    # Build hash with all valid command-line options
    my @stryOptionAllow;

    for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
    {
        my $strOption = cfgOptionName($iOptionId);

        if (cfgDefOptionType($iOptionId) == CFGDEF_TYPE_HASH || cfgDefOptionType($iOptionId) == CFGDEF_TYPE_LIST)
        {
            $strOption .= '=s@';
        }
        elsif (cfgDefOptionType($iOptionId) != CFGDEF_TYPE_BOOLEAN)
        {
            $strOption .= '=s';
        }

        push(@stryOptionAllow, $strOption);

        # Check if the option can be negated
        if (cfgDefOptionNegate($iOptionId))
        {
            push(@stryOptionAllow, 'no-' . cfgOptionName($iOptionId));
        }
    }

    # Get command-line options
    my $oOptionTest = {};

    # Parse command line options
    if (!GetOptions($oOptionTest, @stryOptionAllow))
    {
        confess &log(ASSERT, "error parsing command line");
    }

    # Store results of validation
    my $rhOption = {};

    # Keep track of unresolved dependencies
    my $bDependUnresolved = true;
    my %oOptionResolved;

    # Loop through all possible options
    while ($bDependUnresolved)
    {
        # Assume that all dependencies will be resolved in this loop
        $bDependUnresolved = false;

        for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
        {
            my $strOption = cfgOptionName($iOptionId);

            # Skip the option if it has been resolved in a prior loop
            if (defined($oOptionResolved{$strOption}))
            {
                next;
            }

            # Determine if an option is valid for a command
            $rhOption->{$strOption}{valid} = cfgDefOptionValid($iCommandId, $iOptionId);

            if (!$rhOption->{$strOption}{valid})
            {
                $oOptionResolved{$strOption} = true;
                next;
            }

            # Store the option value
            my $strValue = $oOptionTest->{$strOption};

            # Check to see if an option can be negated.  Make sure that it is not set and negated at the same time.
            $rhOption->{$strOption}{negate} = false;

            if (cfgDefOptionNegate($iOptionId))
            {
                $rhOption->{$strOption}{negate} = defined($$oOptionTest{'no-' . $strOption});

                if ($rhOption->{$strOption}{negate} && defined($strValue))
                {
                    confess &log(ERROR, "option '${strOption}' cannot be both set and negated", ERROR_OPTION_NEGATE);
                }

                if ($rhOption->{$strOption}{negate} && cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_BOOLEAN)
                {
                    $strValue = false;
                }
            }

            # Check dependency for the command then for the option
            my $bDependResolved = true;
            my $strDependOption;
            my $strDependValue;
            my $strDependType;

            if (cfgDefOptionDepend($iCommandId, $iOptionId))
            {
                # Check if the depend option has a value
                my $iDependOptionId = cfgDefOptionDependOption($iCommandId, $iOptionId);
                $strDependOption = cfgOptionName($iDependOptionId);
                $strDependValue = $rhOption->{$strDependOption}{value};

                # Make sure the depend option has been resolved, otherwise skip this option for now
                if (!defined($oOptionResolved{$strDependOption}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                if (!defined($strDependValue))
                {
                    $bDependResolved = false;
                    $strDependType = 'source';
                }

                # If a depend value exists, make sure the option value matches
                if ($bDependResolved && cfgDefOptionDependValueTotal($iCommandId, $iOptionId) == 1 &&
                    cfgDefOptionDependValue($iCommandId, $iOptionId, 0) ne $strDependValue)
                {
                    $bDependResolved = false;
                    $strDependType = 'value';
                }

                # If a depend list exists, make sure the value is in the list
                if ($bDependResolved && cfgDefOptionDependValueTotal($iCommandId, $iOptionId) > 1 &&
                    !cfgDefOptionDependValueValid($iCommandId, $iOptionId, $strDependValue))
                {
                    $bDependResolved = false;
                    $strDependType = 'list';
                }
            }

            if (cfgDefOptionDepend($iCommandId, $iOptionId) && !$bDependResolved && defined($strValue))
            {
                my $strError = "option '${strOption}' not valid without option ";
                my $iDependOptionId = cfgOptionId($strDependOption);

                if ($strDependType eq 'source')
                {
                    confess &log(ERROR, "${strError}'${strDependOption}'", ERROR_OPTION_INVALID);
                }

                # If a depend value exists, make sure the option value matches
                if ($strDependType eq 'value')
                {
                    if (cfgDefOptionType($iDependOptionId) eq CFGDEF_TYPE_BOOLEAN)
                    {
                        $strError .=
                            "'" . (cfgDefOptionDependValue($iCommandId, $iOptionId, 0) ? '' : 'no-') . "${strDependOption}'";
                    }
                    else
                    {
                        $strError .= "'${strDependOption}' = '" . cfgDefOptionDependValue($iCommandId, $iOptionId, 0) . "'";
                    }

                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                $strError .= "'${strDependOption}'";

                # If a depend list exists, make sure the value is in the list
                if ($strDependType eq 'list')
                {
                    my @oyValue;

                    for (my $iValueId = 0; $iValueId < cfgDefOptionDependValueTotal($iCommandId, $iOptionId); $iValueId++)
                    {
                        push(@oyValue, "'" . cfgDefOptionDependValue($iCommandId, $iOptionId, $iValueId) . "'");
                    }

                    $strError .= @oyValue == 1 ? " = $oyValue[0]" : " in (" . join(", ", @oyValue) . ")";
                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }
            }

            # Is the option defined?
            if (defined($strValue))
            {
                # Check that floats and integers are valid
                if (cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_INTEGER ||
                    cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_FLOAT)
                {
                    # Test that the string is a valid float or integer by adding 1 to it.  It's pretty hokey but it works and it
                    # beats requiring Scalar::Util::Numeric to do it properly.
                    my $bError = false;

                    eval
                    {
                        my $strTest = $strValue + 1;
                        return true;
                    }
                    or do
                    {
                        $bError = true;
                    };

                    # Check that integers are really integers
                    if (!$bError && cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_INTEGER &&
                        (int($strValue) . 'S') ne ($strValue . 'S'))
                    {
                        $bError = true;
                    }

                    # Error if the value did not pass tests
                    !$bError
                        or confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow list for the command then for the option
                if (cfgDefOptionAllowList($iCommandId, $iOptionId) &&
                    !cfgDefOptionAllowListValueValid($iCommandId, $iOptionId, $strValue))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow range for the command then for the option
                if (cfgDefOptionAllowRange($iCommandId, $iOptionId) &&
                    ($strValue < cfgDefOptionAllowRangeMin($iCommandId, $iOptionId) ||
                     $strValue > cfgDefOptionAllowRangeMax($iCommandId, $iOptionId)))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_RANGE);
                }

                # Set option value
                if (ref($strValue) eq 'ARRAY' &&
                    (cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_HASH || cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_LIST))
                {
                    foreach my $strItem (@{$strValue})
                    {
                        my $strKey;
                        my $strValue;

                        # If the keys are expected to have values
                        if (cfgDefOptionType($iOptionId) eq CFGDEF_TYPE_HASH)
                        {
                            # Check for = and make sure there is a least one character on each side
                            my $iEqualPos = index($strItem, '=');

                            if ($iEqualPos < 1 || length($strItem) <= $iEqualPos + 1)
                            {
                                confess &log(ERROR, "'${strItem}' not valid key/value for '${strOption}' option",
                                                    ERROR_OPTION_INVALID_PAIR);
                            }

                            $strKey = substr($strItem, 0, $iEqualPos);
                            $strValue = substr($strItem, $iEqualPos + 1);
                        }
                        # Else no values are expected so set value to true
                        else
                        {
                            $strKey = $strItem;
                            $strValue = true;
                        }

                        # Check that the key has not already been set
                        if (defined($rhOption->{$strOption}{$strKey}{value}))
                        {
                            confess &log(ERROR, "'${$strItem}' already defined for '${strOption}' option",
                                                ERROR_OPTION_DUPLICATE_KEY);
                        }

                        # Set key/value
                        $rhOption->{$strOption}{value}{$strKey} = $strValue;
                    }
                }
                else
                {
                    $rhOption->{$strOption}{value} = $strValue;
                }

                # If not config sourced then it must be a param
                if (!defined($rhOption->{$strOption}{source}))
                {
                    $rhOption->{$strOption}{source} = CFGDEF_SOURCE_PARAM;
                }
            }
            # Else try to set a default
            elsif ($bDependResolved)
            {
                # Source is default for this option
                $rhOption->{$strOption}{source} = CFGDEF_SOURCE_DEFAULT;

                # Check for default in command then option
                my $strDefault = cfgDefOptionDefault($iCommandId, $iOptionId);

                # If default is defined
                if (defined($strDefault))
                {
                    # Only set default if dependency is resolved
                    $rhOption->{$strOption}{value} = $strDefault if !$rhOption->{$strOption}{negate};
                }
                # Else check required
                elsif (cfgDefOptionRequired($iCommandId, $iOptionId))
                {
                    confess &log(ERROR,
                        cfgCommandName($iCommandId) . " command requires option: ${strOption}" .
                        ERROR_OPTION_REQUIRED);
                }
            }

            $oOptionResolved{$strOption} = true;
        }
    }

    # Make sure all options specified on the command line are valid
    foreach my $strOption (sort(keys(%{$oOptionTest})))
    {
        # Strip "no-" off the option
        $strOption = $strOption =~ /^no\-/ ? substr($strOption, 3) : $strOption;

        if (!$rhOption->{$strOption}{valid})
        {
            confess &log(
                ERROR, "option '${strOption}' not valid for command '" . cfgCommandName($iCommandId) . "'", ERROR_OPTION_COMMAND);
        }
    }

    return $rhOption;
}

####################################################################################################################################
# Load the configuration
####################################################################################################################################
sub configTestLoad
{
    my $self = shift;
    my $iCommandId = shift;

    @ARGV = $self->commandTestWrite(cfgCommandName($iCommandId), $self->{&CONFIGENVTEST});
    $self->testResult(
        sub {configLoad(
            false, backrestBin(), cfgCommandName($iCommandId),
            (JSON::PP->new()->allow_nonref())->encode(testOptionValidate($iCommandId)))},
        true, 'config load: ' . join(" ", @ARGV));
}

####################################################################################################################################
# optionTestSetByName - used only by config unit tests, general option set should be done with optionTestSet
####################################################################################################################################
sub optionTestSetByName
{
    my $self = shift;
    my $strOption = shift;
    my $strValue = shift;

    $self->{&CONFIGENVTEST}{option}{$strOption} = $strValue;
}

####################################################################################################################################
# configTestLoadExpect - used only by config unit tests, for general config load use configTestLoad()
####################################################################################################################################
sub configTestLoadExpect
{
    my $self = shift;
    my $strCommand = shift;
    my $iExpectedError = shift;
    my $strErrorParam1 = shift;
    my $strErrorParam2 = shift;
    my $strErrorParam3 = shift;

    @ARGV = $self->commandTestWrite($strCommand, $self->{&CONFIGENVTEST});
    $self->configTestClear();
    &log(INFO, "    command line: " . join(" ", @ARGV));

    my $bErrorFound = false;

    eval
    {
        configLoad(false);
        return true;
    }
    or do
    {
        my $oException = $EVAL_ERROR;

        if (!defined($iExpectedError))
        {
            confess $oException;
        }

        $bErrorFound = true;

        if (isException(\$oException))
        {
            if ($oException->code() != $iExpectedError)
            {
                confess "expected error ${iExpectedError} from configLoad but got [" . $oException->code() .
                        "] '" . $oException->message() . "'";
            }

            my $strError;

            if ($iExpectedError == ERROR_OPTION_REQUIRED)
            {
                $strError = "${strCommand} command requires option: ${strErrorParam1}" .
                            (defined($strErrorParam2) ? "\nHINT: ${strErrorParam2}" : '');
            }
            elsif ($iExpectedError == ERROR_COMMAND_REQUIRED)
            {
                $strError = "command must be specified";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID)
            {
                $strError = "option '${strErrorParam1}' not valid without option '${strErrorParam2}'";

                if (defined($strErrorParam3))
                {
                    $strError .= @{$strErrorParam3} == 1 ? " = '$$strErrorParam3[0]'" :
                                 " in ('" . join("', '",@{ $strErrorParam3}) . "')";
                }
            }
            elsif ($iExpectedError == ERROR_OPTION_COMMAND)
            {
                $strError = "option '${strErrorParam1}' not valid for command '${strErrorParam2}'";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID_VALUE)
            {
                $strError = "'${strErrorParam1}' is not valid for '${strErrorParam2}' option" .
                            (defined($strErrorParam3) ? "\nHINT: ${strErrorParam3}." : '');
            }
            elsif ($iExpectedError == ERROR_OPTION_MULTIPLE_VALUE)
            {
                $strError = "option '${strErrorParam1}' cannot be specified multiple times";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID_RANGE)
            {
                $strError = "'${strErrorParam1}' is not valid for '${strErrorParam2}' option";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID_PAIR)
            {
                $strError = "'${strErrorParam1}' not valid key/value for '${strErrorParam2}' option";
            }
            elsif ($iExpectedError == ERROR_OPTION_NEGATE)
            {
                $strError = "option '${strErrorParam1}' cannot be both set and negated";
            }
            elsif ($iExpectedError == ERROR_FILE_INVALID)
            {
                $strError = "'${strErrorParam1}' is not a file";
            }
            else
            {
                confess
                    "must construct message for error ${iExpectedError}, use this as an example: '" . $oException->message() . "'";
            }

            if ($oException->message() ne $strError)
            {
                confess "expected error message \"${strError}\" from configLoad but got \"" . $oException->message() . "\"";
            }
        }
        else
        {
            confess "configLoad should throw pgBackRest::Common::Exception:\n$oException";
        }
    };

    if (!$bErrorFound && defined($iExpectedError))
    {
        confess "expected error ${iExpectedError} from configLoad but got success";
    }
}

####################################################################################################################################
# configTestExpect - used only by config unit tests
####################################################################################################################################
sub optionTestExpect
{
    my $self = shift;
    my $iOptionId = shift;
    my $strExpectedValue = shift;
    my $strExpectedKey = shift;

    my $strOption = cfgOptionName($iOptionId);

    if (defined($strExpectedValue))
    {
        my $strActualValue = cfgOption($iOptionId);

        if (defined($strExpectedKey))
        {
            $strActualValue = $$strActualValue{$strExpectedKey};
        }

        if (!defined($strActualValue))
        {
            confess "expected option ${strOption} to have value ${strExpectedValue} but [undef] found instead";
        }

        $strActualValue eq $strExpectedValue
            or confess "expected option ${strOption} to have value ${strExpectedValue} but ${strActualValue} found instead";
    }
    elsif (cfgOptionTest(cfgOptionId($strOption)))
    {
        confess "expected option ${strOption} to be [undef], but " . cfgOption(cfgOptionId($strOption)) . ' found instead';
    }
}

1;
