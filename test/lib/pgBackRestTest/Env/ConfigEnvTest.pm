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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Config::Data;

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

sub configTestLoad
{
    my $self = shift;
    my $iCommandId = shift;

    @ARGV = $self->commandTestWrite(cfgCommandName($iCommandId), $self->{&CONFIGENVTEST});
    $self->testResult(sub {configLoad(false)}, true, 'config load: ' . join(" ", @ARGV));
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
