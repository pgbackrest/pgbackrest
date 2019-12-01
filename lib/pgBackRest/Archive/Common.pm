####################################################################################################################################
# ARCHIVE COMMON MODULE
####################################################################################################################################
package pgBackRest::Archive::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Config;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY);
use File::Basename qw(dirname);

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# RegEx constants
####################################################################################################################################
use constant REGEX_ARCHIVE_DIR_DB_VERSION                           => '^[0-9]+(\.[0-9]+)*-[0-9]+$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_DB_VERSION);
use constant REGEX_ARCHIVE_DIR_WAL                                  => '^[0-F]{16}$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_WAL);

####################################################################################################################################
# PostgreSQL WAL system id offset
####################################################################################################################################
use constant PG_WAL_SYSTEM_ID_OFFSET_GTE_93                         => 12 + $Config{ptrsize};
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_GTE_93);
use constant PG_WAL_SYSTEM_ID_OFFSET_LT_93                          => 12;
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_LT_93);

####################################################################################################################################
# WAL segment size
####################################################################################################################################
use constant PG_WAL_SEGMENT_SIZE                                    => 16777216;
    push @EXPORT, qw(PG_WAL_SEGMENT_SIZE);

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
            __PACKAGE__ . '::lsnFile', \@_,
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
        $iWalSegmentSize,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::lsnFileRange', \@_,
            {name => 'strLsnStart'},
            {name => 'strLsnStop'},
            {name => '$strDbVersion'},
            {name => '$iWalSegmentSize'},
        );

    # Working variables
    my @stryArchive;
    my $iArchiveIdx = 0;
    my $bSkipFF = $strDbVersion < PG_VERSION_93;

    # Iterate through all archive logs between start and stop
    my @stryArchiveSplit = split('/', $strLsnStart);
    my $iStartMajor = hex($stryArchiveSplit[0]);
    my $iStartMinor = int(hex($stryArchiveSplit[1]) / $iWalSegmentSize);

    @stryArchiveSplit = split('/', $strLsnStop);
    my $iStopMajor = hex($stryArchiveSplit[0]);
    my $iStopMinor = int(hex($stryArchiveSplit[1]) / $iWalSegmentSize);

    $stryArchive[$iArchiveIdx] = uc(sprintf("%08x%08x", $iStartMajor, $iStartMinor));
    $iArchiveIdx += 1;

    while (!($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor))
    {
        $iStartMinor += 1;

        if ($bSkipFF && $iStartMinor == 255 || !$bSkipFF && $iStartMinor > int(0xFFFFFFFF / $iWalSegmentSize))
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

####################################################################################################################################
# walSegmentFind
#
# Returns the filename of a WAL segment in the archive.  Optionally, a wait time can be specified.  In this case an error will be
# thrown when the WAL segment is not found.  If the same WAL segment with multiple checksums is found then error.
####################################################################################################################################
sub walSegmentFind
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorageRepo,
        $strArchiveId,
        $strWalSegment,
        $iWaitSeconds,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walSegmentFind', \@_,
            {name => 'oStorageRepo'},
            {name => 'strArchiveId'},
            {name => 'strWalSegment'},
            {name => 'iWaitSeconds', required => false},
        );

    # Error if not a segment
    if (!walIsSegment($strWalSegment))
    {
        confess &log(ERROR, "${strWalSegment} is not a WAL segment", ERROR_ASSERT);
    }

    # Loop and wait for file to appear
    my $oWait = waitInit($iWaitSeconds);
    my @stryWalFileName;

    do
    {
        # Get the name of the requested WAL segment (may have compression extension)
        push(@stryWalFileName, $oStorageRepo->list(
            STORAGE_REPO_ARCHIVE . "/${strArchiveId}/" . substr($strWalSegment, 0, 16),
            {strExpression =>
                '^' . substr($strWalSegment, 0, 24) . (walIsPartial($strWalSegment) ? "\\.partial" : '') .
                "-[0-f]{40}(\\." . COMPRESS_EXT . "){0,1}\$",
                bIgnoreMissing => true}));
    }
    while (@stryWalFileName == 0 && waitMore($oWait));

    # If there is more than one matching archive file then there is a serious issue - either a bug in the archiver or the user has
    # copied files around or removed archive.info.
    if (@stryWalFileName > 1)
    {
        confess &log(ERROR,
            "duplicates found in archive for WAL segment ${strWalSegment}: " . join(', ', @stryWalFileName) .
            "\nHINT: are multiple primaries archiving to this stanza?",
            ERROR_ARCHIVE_DUPLICATE);
    }

    # If waiting and no WAL segment was found then throw an error
    if (@stryWalFileName == 0 && defined($iWaitSeconds))
    {
        confess &log(
            ERROR,
            "could not find WAL segment ${strWalSegment} after ${iWaitSeconds} second(s)" .
                "\nHINT: is archive_command configured correctly?" .
                "\nHINT: use the check command to verify that PostgreSQL is archiving.",
            ERROR_ARCHIVE_TIMEOUT);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strWalFileName', value => $stryWalFileName[0]}
    );
}

push @EXPORT, qw(walSegmentFind);

####################################################################################################################################
# walIsSegment
#
# Is the file a segment or some other file (e.g. .history, .backup, etc).
####################################################################################################################################
sub walIsSegment
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walIsSegment', \@_,
            {name => 'strWalFile', trace => true},
        );

    return $strWalFile =~ /^[0-F]{24}(\.partial){0,1}$/ ? true : false;
}

push @EXPORT, qw(walIsSegment);

####################################################################################################################################
# walIsPartial
#
# Is the file a segment and partial.
####################################################################################################################################
sub walIsPartial
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walIsPartial', \@_,
            {name => 'strWalFile', trace => true},
        );

    return walIsSegment($strWalFile) && $strWalFile =~ /\.partial$/ ? true : false;
}

push @EXPORT, qw(walIsPartial);

1;
