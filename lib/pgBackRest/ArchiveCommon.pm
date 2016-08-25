####################################################################################################################################
# ARCHIVE COMMON MODULE
####################################################################################################################################
package pgBackRest::ArchiveCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::DbVersion;
use pgBackRest::Common::Log;

####################################################################################################################################
# lsnNormalize
#
# Generates a normalized form from an LSN that can be used for comparison.
####################################################################################################################################
sub lsnNormalize
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLsn,
    ) =
        logDebugParam
        (
            __PACKAGE__ . 'lsnFile', \@_,
            {name => 'strLsn', trace => true},
        );

    # Split the LSN into major and minor parts
    my @stryLsnSplit = split('/', $strLsn);

    if (@stryLsnSplit != 2)
    {
        confess &log(ASSERT, "invalid lsn ${strLsn}");
    }

    my $strLsnNormal = uc(sprintf("%08s%08s", $stryLsnSplit[0], $stryLsnSplit[1]));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strLsnNormal', value => $strLsnNormal, trace => true}
    );

}

push @EXPORT, qw(lsnNormalize);

####################################################################################################################################
# lsnFileRange
#
# Generates a range of WAL filenames given the start and stop LSN.  For pre-9.3 databases, use bSkipFF to exclude the FF that
# prior versions did not generate.
####################################################################################################################################
sub lsnFileRange
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLsnStart,
        $strLsnStop,
        $strDbVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . 'lsnFileRange', \@_,
            {name => 'strLsnStart'},
            {name => 'strLsnStop'},
            {name => '$strDbVersion'},
        );

    # Working variables
    my @stryArchive;
    my $iArchiveIdx = 0;
    my $bSkipFF = $strDbVersion < PG_VERSION_93;

    # Iterate through all archive logs between start and stop
    my @stryArchiveSplit = split('/', $strLsnStart);
    my $iStartMajor = hex($stryArchiveSplit[0]);
    my $iStartMinor = hex(substr(sprintf("%08s", $stryArchiveSplit[1]), 0, 2));

    @stryArchiveSplit = split('/', $strLsnStop);
    my $iStopMajor = hex($stryArchiveSplit[0]);
    my $iStopMinor = hex(substr(sprintf("%08s", $stryArchiveSplit[1]), 0, 2));

    $stryArchive[$iArchiveIdx] = uc(sprintf("%08x%08x", $iStartMajor, $iStartMinor));
    $iArchiveIdx += 1;

    while (!($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor))
    {
        $iStartMinor += 1;

        if ($bSkipFF && $iStartMinor == 255 || !$bSkipFF && $iStartMinor == 256)
        {
            $iStartMajor += 1;
            $iStartMinor = 0;
        }

        $stryArchive[$iArchiveIdx] = uc(sprintf("%08x%08x", $iStartMajor, $iStartMinor));
        $iArchiveIdx += 1;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryWalFileName', value => \@stryArchive}
    );
}

push @EXPORT, qw(lsnFileRange);

1;
