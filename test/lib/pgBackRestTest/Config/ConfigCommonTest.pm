####################################################################################################################################
# ConfigCommonTest.pm - Common code for Config unit tests
####################################################################################################################################
package pgBackRestTest::Config::ConfigCommonTest;
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

sub optionSetTest
{
    my $self = shift;
    my $oOption = shift;
    my $strKey = shift;
    my $strValue = shift;

    $$oOption{option}{$strKey} = $strValue;
}

sub optionBoolSetTest
{
    my $self = shift;
    my $oOption = shift;
    my $strKey = shift;
    my $bValue = shift;

    $$oOption{boolean}{$strKey} = defined($bValue) ? $bValue : true;
}

sub commandSetTest
{
    my $self = shift;
    my $oOption = shift;
    my $strCommand = shift;

    $$oOption{command} = $strCommand;
}

# sub optionRemoveTest
# {
#     my $self = shift;
#     my $oOption = shift;
#     my $strKey = shift;
#
#     delete($$oOption{option}{$strKey});
#     delete($$oOption{boolean}{$strKey});
# }

sub argvWriteTest
{
    my $self = shift;
    my $oOption = shift;

    @ARGV = ();

    if (defined($$oOption{boolean}))
    {
        foreach my $strKey (keys(%{$$oOption{boolean}}))
        {
            if ($$oOption{boolean}{$strKey})
            {
                $ARGV[@ARGV] = "--${strKey}";
            }
            else
            {
                $ARGV[@ARGV] = "--no-${strKey}";
            }
        }
    }

    if (defined($$oOption{option}))
    {
        foreach my $strKey (keys(%{$$oOption{option}}))
        {
            $ARGV[@ARGV] = "--${strKey}=$$oOption{option}{$strKey}";
        }
    }

    $ARGV[@ARGV] = $$oOption{command};

    &log(INFO, "    command line: " . join(" ", @ARGV));

    %$oOption = ();
}

sub configLoadExpect
{
    my $self = shift;
    my $oOption = shift;
    my $strCommand = shift;
    my $iExpectedError = shift;
    my $strErrorParam1 = shift;
    my $strErrorParam2 = shift;
    my $strErrorParam3 = shift;

    my $oOptionRuleExpected = optionRuleGet();

    $self->commandSetTest($oOption, $strCommand);
    $self->argvWriteTest($oOption);

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

        if (isException($oException))
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

sub optionTestExpect
{
    my $self = shift;
    my $strOption = shift;
    my $strExpectedValue = shift;
    my $strExpectedKey = shift;

    if (defined($strExpectedValue))
    {
        my $strActualValue = optionGet($strOption);

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
    elsif (optionTest($strOption))
    {
        confess "expected option ${strOption} to be [undef], but " . optionGet($strOption) . ' found instead';
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'main'};

1;
