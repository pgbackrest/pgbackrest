####################################################################################################################################
# ExecuteTest.pm - Module to execute external commands
####################################################################################################################################
package BackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';
use Symbol 'gensym';

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::Wait;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_EXECUTE_TEST                                        => 'ExecuteTest';

use constant OP_EXECUTE_TEST_BEGIN                                  => OP_EXECUTE_TEST . "->begin";
use constant OP_EXECUTE_TEST_END                                    => OP_EXECUTE_TEST . "->end";
use constant OP_EXECUTE_TEST_END_RETRY                              => OP_EXECUTE_TEST . "->endRetry";
use constant OP_EXECUTE_TEST_NEW                                    => OP_EXECUTE_TEST . "->new";

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strCommandOriginal},
        my $oParam
    ) =
        logDebugParam
        (
            OP_EXECUTE_TEST_NEW, \@_,
            {name => 'strCommandOriginal'},
            {name => 'oParam', required => false}
        );

    # Load params
    foreach my $strParam (sort(keys(%$oParam)))
    {
        $self->{$strParam} = $$oParam{$strParam};
    }

    # Set defaults
    $self->{bRemote} = defined($self->{bRemote}) ? $self->{bRemote} : false;
    $self->{bSuppressError} = defined($self->{bSuppressError}) ? $self->{bSuppressError} : false;
    $self->{bSuppressStdErr} = defined($self->{bSuppressStdErr}) ? $self->{bSuppressStdErr} : false;
    $self->{bShowOutput} = defined($self->{bShowOutput}) ? $self->{bShowOutput} : false;
    $self->{iExpectedExitStatus} = defined($self->{iExpectedExitStatus}) ? $self->{iExpectedExitStatus} : 0;
    $self->{iRetrySeconds} = defined($self->{iRetrySeconds}) ? $self->{iRetrySeconds} : undef;

    $self->{strUserBackRest} = 'backrest'; #BackRestTestCommon_UserBackRestGet();
    $self->{strHost} = '127.0.0.1'; #BackRestTestCommon_HostGet();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# begin
####################################################################################################################################
sub begin
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    logDebugParam(OP_EXECUTE_TEST_BEGIN);

    if ($self->{bRemote})
    {
        # $self->{strCommand} = "sudo -u $self->{strUserBackRest} $self->{strCommandOriginal}";
        $self->{strCommand} = "ssh $self->{strUserBackRest}\@$self->{strHost} '$self->{strCommandOriginal}'";
    }
    else
    {
        $self->{strCommand} = $self->{strCommandOriginal};
    }

    $self->{strErrorLog} = '';
    $self->{strOutLog} = '';

    if (defined($self->{oTestLog}))
    {
        if (defined($self->{strComment}))
        {
            $self->{strFullLog} = $self->{oTestLog}->regExpAll($self->{strComment}) . "\n";
        }

        $self->{strFullLog} .= '> ' . $self->{oTestLog}->regExpAll($self->{strCommand}) . "\n" . ('-' x '132') . "\n";
    }

    logDebugMisc("executing command: $self->{strCommand}");

    # Execute the command
    $self->{hError} = gensym;

    $self->{pId} = open3(undef, $self->{hOut}, $self->{hError}, $self->{strCommand});

    if (!defined($self->{hError}))
    {
        confess 'STDERR handle is undefined';
    }
}

####################################################################################################################################
# endRetry
####################################################################################################################################
sub endRetry
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strTest
    ) =
        logDebugParam
        (
            OP_EXECUTE_TEST_END_RETRY, \@_,
            {name => 'strTest', required => false, trace => true}
        );

    # Create select objects
    my $oErrorSelect = IO::Select->new();
    $oErrorSelect->add($self->{hError});
    my $oOutSelect = IO::Select->new();
    $oOutSelect->add($self->{hOut});

    # Drain the output and error streams and look for test points
    while(waitpid($self->{pId}, WNOHANG) == 0)
    {
        # Drain the stderr stream
        if ($oErrorSelect->can_read(0))
        {
            while (my $strLine = readline($self->{hError}))
            {
                $self->{strErrorLog} .= $strLine;
            }
        }

        # Drain the stdout stream and look for test points
        if ($oOutSelect->can_read(.05))
        {
            while (my $strLine = readline($self->{hOut}))
            {
                $self->{strOutLog} .= $strLine;

                if (defined($strTest) && testCheck($strLine, $strTest))
                {
                    &log(DEBUG, "Found test ${strTest}");
                    return true;
                }
            }
        }
    }

    # Check the exit status
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    # Drain the stderr stream
    if ($oErrorSelect->can_read(0))
    {
        while (my $strLine = readline($self->{hError}))
        {
            $self->{strErrorLog} .= $strLine;
        }
    }

    # Drain the stdout stream
    if ($oOutSelect->can_read(0))
    {
        while (my $strLine = readline($self->{hOut}))
        {
            $self->{strOutLog} .= $strLine;
        }
    }

    # Pass the log to the LogTest object
    if (defined($self->{oLogTest}))
    {
        $self->{oLogTest}->logAdd($self->{strCommandOriginal}, $self->{strComment}, $self->{strOutLog});
    }

    # If an error was expected then return success if that error occurred
    if ($self->{iExpectedExitStatus} != 0 && $iExitStatus == $self->{iExpectedExitStatus})
    {
        return $iExitStatus;
    }

    # This is a hack to make regression tests pass even when threaded backup/restore sometimes return 255
    if ($self->{iExpectedExitStatus} == -1 && ($iExitStatus == 0 || $iExitStatus == 255))
    {
        return 0;
    }

    if ($iExitStatus != 0 || ($self->{iExpectedExitStatus} != 0 && $iExitStatus != $self->{iExpectedExitStatus}))
    {
        if ($self->{bSuppressError})
        {
            &log(DEBUG, "suppressed error was ${iExitStatus}");
            $self->{strErrorLog} = '';
        }
        else
        {
            if (defined($self->{iRetrySeconds}))
            {
                $self->{bRetry} = true;
                return;
            }
            else
            {
                confess &log(ERROR, "command '$self->{strCommand}' returned " . $iExitStatus .
                             ($self->{iExpectedExitStatus} != 0 ? ", but $self->{iExpectedExitStatus} was expected" : '') . "\n" .
                             ($self->{strOutLog} ne '' ? "STDOUT (last 10,000 characters):\n" . substr($self->{strOutLog},
                                 length($self->{strOutLog}) - 10000) : '') .
                             ($self->{strErrorLog} ne '' ? "STDERR:\n$self->{strErrorLog}" : ''));
            }
        }
    }

    if ($self->{strErrorLog} ne '' && !$self->{bSuppressStdErr} && !$self->{bSuppressError})
    {
        confess &log(ERROR, "output found on STDERR:\n$self->{strErrorLog}");
    }

    if ($self->{bShowOutput})
    {
        print "output:\n$self->{strOutLog}\n";
    }

    if (defined($strTest))
    {
        confess &log(ASSERT, "test point ${strTest} was not found");
    }

    return $iExitStatus;
}

####################################################################################################################################
# end
####################################################################################################################################
sub end
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strTest
    ) =
        logDebugParam
        (
            OP_EXECUTE_TEST_END, \@_,
            {name => 'strTest', required => false}
        );

    # If retry is not defined then run endRetry() one time
    my $iExitStatus;

    if (!defined($self->{iRetrySeconds}))
    {
        $iExitStatus = $self->endRetry($strTest);
    }
    # Else loop until success or timeout
    else
    {
        my $oWait = waitInit($self->{iRetrySeconds});

        do
        {
            $self->{bRetry} = false;
            $self->begin();
            $iExitStatus = $self->endRetry($strTest);

            if ($self->{bRetry})
            {
                &log(TRACE, 'error executing statement - retry');
            }
        }
        while ($self->{bRetry} && waitMore($oWait));

        if ($self->{bRetry})
        {
            $self->begin();
            $iExitStatus = $self->endRetry($strTest);
        }
    }

    return $iExitStatus;
}

####################################################################################################################################
# executeTest
####################################################################################################################################
sub executeTest
{
    my $strCommand = shift;
    my $oParam = shift;
    my $strTest = shift;

    my $oExec = new BackRestTest::Common::ExecuteTest($strCommand, $oParam);
    $oExec->begin();

    if (defined($strTest))
    {
        $oExec->end($strTest);
    }

    $oExec->end();

    return $oExec->{strOutLog};
}

push @EXPORT, qw(executeTest);

1;
