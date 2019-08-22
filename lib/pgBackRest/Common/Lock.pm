####################################################################################################################################
# COMMON LOCK MODULE
####################################################################################################################################
package pgBackRest::Common::Lock;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:DEFAULT :flock);
use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# lockStopFileName
#
# Get the stop file name.
####################################################################################################################################
sub lockStopFileName
{
    my $strStanza = shift;

    return cfgOption(CFGOPT_LOCK_PATH) . (defined($strStanza) ? "/${strStanza}" : '/all') . '.stop';
}

####################################################################################################################################
# lockStopTest
#
# Test if a stop file exists for the current stanza or all stanzas.
####################################################################################################################################
sub lockStopTest
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bStanzaStopRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::lockStopTest', \@_,
            {name => 'bStanzaStopRequired', optional => true, default => false}
        );

    my $bStopExists = false;

    # Check the stanza first if it is specified
    if (cfgOptionTest(CFGOPT_STANZA))
    {
        # Generate the stop file name
        my $strStopFile = lockStopFileName(cfgOption(CFGOPT_STANZA));

        if (-e $strStopFile)
        {
            # If the stop file exists and is required then set the flag to true
            if ($bStanzaStopRequired)
            {
                $bStopExists = true;
            }
            else
            {
                confess &log(ERROR, 'stop file exists for stanza ' . cfgOption(CFGOPT_STANZA), ERROR_STOP);
            }
        }
    }

    # If not looking for a specific stanza stop file, then check all stanzas
    if (!$bStanzaStopRequired)
    {
        # Now check all stanzas
        my $strStopFile = lockStopFileName();

        if (-e $strStopFile)
        {
            confess &log(ERROR, 'stop file exists for all stanzas', ERROR_STOP);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bStopExists', value => $bStopExists}
    );
}

push @EXPORT, qw(lockStopTest);

1;
