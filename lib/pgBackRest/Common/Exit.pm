####################################################################################################################################
# COMMON EXIT MODULE
####################################################################################################################################
package pgBackRest::Common::Exit;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Protocol;

####################################################################################################################################
# Signal constants
####################################################################################################################################
use constant SIGNAL_HUP                                             => 'HUP';
use constant SIGNAL_INT                                             => 'INT';
use constant SIGNAL_TERM                                            => 'TERM';

####################################################################################################################################
# Hook important signals into exitSafe function
####################################################################################################################################
$SIG{&SIGNAL_HUP} = sub {exitSafe(ERROR_TERM, undef, SIGNAL_HUP)};
$SIG{&SIGNAL_INT} = sub {exitSafe(ERROR_TERM, undef, SIGNAL_INT)};
$SIG{&SIGNAL_TERM} = sub {exitSafe(ERROR_TERM, undef, SIGNAL_TERM)};

####################################################################################################################################
# exitSafe
#
# Terminate all remotes and release locks.
####################################################################################################################################
sub exitSafe
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iExitCode,
        $oException,
        $strSignal,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::exitSafe', \@_,
            {name => 'iExitCode', required => false},
            {name => 'oException', required => false},
            {name => 'strSignal', required => false},
        );

    # Reset logging in case it was disabled when the exception/signal occurred
    configLogging();

    # Close the remote
    protocolDestroy(undef, undef, defined($iExitCode) && ($iExitCode == 0 || $iExitCode == 1));

    # Don't fail if the lock can't be released
    eval
    {
        lockRelease(false);
    }
    or do {};

    # If exit code is not defined then try to get it from the exception
    if (!defined($iExitCode))
    {
        # If a backrest exception
        if (isException($oException))
        {
            $iExitCode = $oException->code();
            logException($oException);
        }
        else
        {
            $iExitCode = ERROR_UNHANDLED;

            &log(
                ERROR,
                'process terminated due to an unhandled exception' .
                    (defined($oException) ? ":\n${oException}" : ': [exception not defined]'),
                $iExitCode);
        }
    }
    elsif ($iExitCode == ERROR_TERM)
    {
        &log(ERROR, "process terminated on a ${strSignal} signal", ERROR_TERM);
    }

    # Log command end
    commandEnd($iExitCode, $strSignal);

    # Log return values if any
    logDebugReturn
    (
        $strOperation,
        {name => 'iExitCode', value => $iExitCode}
    );

    exit $iExitCode;
}

push @EXPORT, qw(exitSafe);

1;
