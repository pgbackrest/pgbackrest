####################################################################################################################################
# ARCHIVE COMMON MODULE
####################################################################################################################################
package pgBackRest::Archive::ArchiveCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY); # !!! Only needed until read from buffer
use File::Basename qw(dirname);

use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;

####################################################################################################################################
# RegEx constants
####################################################################################################################################
use constant REGEX_ARCHIVE_DIR_DB_VERSION                           => '^[0-9]+\.[0-9]+-[0-9]+$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_DB_VERSION);
use constant REGEX_ARCHIVE_DIR_WAL                                  => '^[0-F]{16}$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR_WAL);

####################################################################################################################################
# PostgreSQL WAL system id offset
####################################################################################################################################
use constant PG_WAL_SYSTEM_ID_OFFSET_GTE_93                         => 20;
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_GTE_93);
use constant PG_WAL_SYSTEM_ID_OFFSET_LT_93                          => 12;
    push @EXPORT, qw(PG_WAL_SYSTEM_ID_OFFSET_LT_93);

####################################################################################################################################
# PostgreSQL WAL magic
####################################################################################################################################
my $oWalMagicHash =
{
    hex('0xD062') => PG_VERSION_83,
    hex('0xD063') => PG_VERSION_84,
    hex('0xD064') => PG_VERSION_90,
    hex('0xD066') => PG_VERSION_91,
    hex('0xD071') => PG_VERSION_92,
    hex('0xD075') => PG_VERSION_93,
    hex('0xD07E') => PG_VERSION_94,
    hex('0xD087') => PG_VERSION_95,
    hex('0xD093') => PG_VERSION_96,
};

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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::lsnFileRange', \@_,
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

####################################################################################################################################
# walInfo
#
# Retrieve information such as db version and system identifier from a WAL segment.
####################################################################################################################################
sub walInfo
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walInfo', \@_,
            {name => 'strWalFile'}
        );

    # Open the WAL segment and read magic number
    #-------------------------------------------------------------------------------------------------------------------------------
    my $hFile;
    my $tBlock;

    sysopen($hFile, $strWalFile, O_RDONLY)
        or confess &log(ERROR, "unable to open ${strWalFile}", ERROR_FILE_OPEN);

    # Read magic
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog magic");

    my $iMagic = unpack('S', $tBlock);

    # Map the WAL magic number to the version of PostgreSQL.
    #
    # The magic number can be found in src/include/access/xlog_internal.h The offset can be determined by counting bytes in the
    # XLogPageHeaderData struct, though this value rarely changes.
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strDbVersion = $$oWalMagicHash{$iMagic};

    if (!defined($strDbVersion))
    {
        confess &log(ERROR, "unexpected WAL magic 0x" . sprintf("%X", $iMagic) . "\n" .
                     'HINT: is this version of PostgreSQL supported?',
                     ERROR_VERSION_NOT_SUPPORTED);
    }

    # Map the WAL PostgreSQL version to the system identifier offset.  The offset can be determined by counting bytes in the
    # XLogPageHeaderData struct, though this value rarely changes.
    #-------------------------------------------------------------------------------------------------------------------------------
    my $iSysIdOffset = $strDbVersion >= PG_VERSION_93 ? PG_WAL_SYSTEM_ID_OFFSET_GTE_93 : PG_WAL_SYSTEM_ID_OFFSET_LT_93;

    # Check flags to be sure the long header is present (this is an extra check to be sure the system id exists)
    #-------------------------------------------------------------------------------------------------------------------------------
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog info");

    my $iFlag = unpack('S', $tBlock);

    # Make sure that the long header is present or there won't be a system id
    $iFlag & 2
        or confess &log(ERROR, "expected long header in flags " . sprintf("%x", $iFlag));

    # Get the system id
    #-------------------------------------------------------------------------------------------------------------------------------
    sysseek($hFile, $iSysIdOffset, SEEK_CUR)
        or confess &log(ERROR, "unable to read padding");

    sysread($hFile, $tBlock, 8) == 8
        or confess &log(ERROR, "unable to read database system identifier");

    length($tBlock) == 8
        or confess &log(ERROR, "block is incorrect length");

    close($hFile);

    my $ullDbSysId = unpack('Q', $tBlock);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strDbVersion', value => $strDbVersion},
        {name => 'ullDbSysId', value => $ullDbSysId}
    );
}

push @EXPORT, qw(walInfo);

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
        $oFile,
        $strArchiveId,
        $strWalSegment,
        $iWaitSeconds,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walSegmentFind', \@_,
            {name => 'oFile'},
            {name => 'strArchiveId'},
            {name => 'strWalSegment'},
            {name => 'iWaitSeconds', required => false},
        );

    # Error if not a segment
    my $bTimeline = $strWalSegment =~ /^[0-F]{16}$/ ? false : true;

    if ($bTimeline && !walIsSegment($strWalSegment))
    {
        confess &log(ERROR, "${strWalSegment} is not a WAL segment", ERROR_ASSERT);
    }

    # Loop and wait for file to appear
    my $oWait = waitInit($iWaitSeconds);
    my @stryWalFileName;

    do
    {
        # If the WAL segment includes the timeline then use it, otherwise contruct a regexp with the major WAL part to find paths
        # where the wal could be found.
        my @stryTimelineMajor;

        if ($bTimeline)
        {
            @stryTimelineMajor = (substr($strWalSegment, 0, 16));
        }
        else
        {
            @stryTimelineMajor = $oFile->list(
                PATH_BACKUP_ARCHIVE, $strArchiveId, '[0-F]{8}' . substr($strWalSegment, 0, 8), undef, true);
        }

        # Search each timelin/major path
        foreach my $strTimelineMajor (@stryTimelineMajor)
        {
            # Construct the name of the WAL segment to find
            my $strWalSegmentFind = $bTimeline ? substr($strWalSegment, 0, 24) : $strTimelineMajor . substr($strWalSegment, 8, 16);

            # Get the name of the requested WAL segment (may have hash info and compression extension)
            push(@stryWalFileName, $oFile->list(
                PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strTimelineMajor}",
                "^${strWalSegmentFind}" . (walIsPartial($strWalSegment) ? "\\.partial" : '') .
                    "-[0-f]{40}(\\." . COMPRESS_EXT . "){0,1}\$",
                undef, true));
        }
    }
    while (@stryWalFileName == 0 && waitMore($oWait));

    # If there is more than one matching archive file then there is a serious issue - either a bug in the archiver or the user has
    # copied files around or removed archive.info.
    if (@stryWalFileName > 1)
    {
        confess &log(ERROR,
            "duplicates found in archive for WAL segment " . ($bTimeline ? $strWalSegment : "XXXXXXXX${strWalSegment}") . ': ' .
            join(', ', @stryWalFileName), ERROR_ARCHIVE_DUPLICATE);
    }

    # If waiting and no WAL segment was found then throw an error
    if (@stryWalFileName == 0 && defined($iWaitSeconds))
    {
        confess &log(ERROR, "could not find WAL segment ${strWalSegment} after ${iWaitSeconds} second(s)", ERROR_ARCHIVE_TIMEOUT);
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
# walPath
#
# Generates the location of the pg_xlog directory using a relative xlog path and the supplied db path.
####################################################################################################################################
sub walPath
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalFile,
        $strDbPath,
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::walPath', \@_,
            {name => 'strWalFile', trace => true},
            {name => 'strDbPath', trace => true, required => false},
            {name => 'strCommand', trace => true},
        );

    if (index($strWalFile, '/') != 0)
    {
        if (!defined($strDbPath))
        {
            confess &log(ERROR,
                "option 'db-path' must be specified when relative xlog paths are used\n" .
                "HINT: Is \%f passed to ${strCommand} instead of \%p?\n" .
                "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.",
                ERROR_OPTION_REQUIRED);
        }

        $strWalFile = "${strDbPath}/${strWalFile}";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strWalFile', value => $strWalFile, trace => true}
    );

}

push @EXPORT, qw(walPath);

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
