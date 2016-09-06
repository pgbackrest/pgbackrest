####################################################################################################################################
# COMMON EXIT MODULE
####################################################################################################################################
package pgBackRest::Common::Exit;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
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
$SIG{&SIGNAL_HUP} = sub {exitSafe(-1, SIGNAL_HUP)};
$SIG{&SIGNAL_INT} = sub {exitSafe(-1, SIGNAL_INT)};
$SIG{&SIGNAL_TERM} = sub {exitSafe(-1, SIGNAL_TERM)};

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
        $strSignal
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::exitSafe', \@_,
            {name => 'iExitCode'},
            {name => 'strSignal', required => false}
        );

    commandStop();

    # Close the remote
    protocolDestroy();

    # Don't fail if the lock can't be released
    eval
    {
        lockRelease(false);
    };

    # Exit with code when defined
    if ($iExitCode != -1)
    {
        exit $iExitCode;
    }

    # Log error based on where the signal came from
    &log(
        ERROR,
        'process terminated ' .
            (defined($strSignal) ? "on a ${strSignal} signal" :  'due to an unhandled exception'),
        defined($strSignal) ? ERROR_TERM : ERROR_UNHANDLED_EXCEPTION);

    # If terminated by a signal exit with ERROR_TERM
    exit ERROR_TERM if defined($strSignal);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(exitSafe);

1;
