#!/usr/bin/perl

use strict;
use warnings;
use File::Basename;
use File::Path;
use Getopt::Long;
use Config::IniFiles;
use IPC::System::Simple qw(capture);
use JSON;
use File::Copy;
use Carp;

# Process flags
my $bNoCompression;
my $bNoChecksum;
my $bHardLink; # !!! Add hardlink option to make restores easier
my $strConfigFile;
my $strCluster;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)

GetOptions ("no-compression" => \$bNoCompression,
            "no-checksum" => \$bNoChecksum,
            "hardlink" => \$bHardLink,
            "config=s" => \$strConfigFile,
            "cluster=s" => \$strCluster,
            "type=s" => \$strType)
    or die("Error in command line arguments\n");
    

# Global variables
my $strDbClusterPath;          # Database cluster base path

my $strBackupPath;             # Backup base path
my $strBackupClusterPath;       # Backup cluster path

####################################################################################################################################
# TRIM - trim whitespace off strings
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

    $strBuffer =~ s/^\s+|\s+$//g;

    return $strBuffer;
}

####################################################################################################################################
# DATE_STRING_GET - Get the date and time string
####################################################################################################################################
sub date_string_get
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

    return(sprintf("%4d%02d%02d-%02d%02d%02d", $year+1900, $mon+1, $mday, $hour, $min, $sec));
}

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
use constant 
{
    DEBUG   => 'DEBUG',
    INFO    => 'INFO',
    WARNING => 'WARNING',
    ERROR   => 'ERROR',
    ASSERT  => 'ASSERT'
};

sub log
{
    my $strLevel = shift;
    my $strMessage = shift;

    print "${strLevel}: ${strMessage}\n";

    return $strMessage;
}

####################################################################################################################################
# EXECUTE - execute a command
####################################################################################################################################
sub execute
{
    my $strCommand = shift;
    my $strOutput;

#    print("$strCommand");
    $strOutput = capture($strCommand);

#    if ($strOutput eq "")
#    {  
#        print(" ... complete\n\n");
#    }
#    else
#    {
#        print(" ... complete\n$strOutput\n\n");
#    }

    return $strOutput;
}

####################################################################################################################################
# BACKUP_REGEXP_GET - Generate a regexp depending on the backups that need to be found
####################################################################################################################################
sub backup_regexp_get
{
    my $bFull = shift;
    my $bDifferential = shift;
    my $bIncremental = shift;
    
    if (!$bFull && !$bDifferential && !$bIncremental)
    {
        die &log(ERROR, 'one parameter must be true');
    }
    
    my $strDateTimeRegExp = "[0-9]{8}\\-[0-9]{6}";
    my $strRegExp = "^";
    
    if ($bFull || $bDifferential || $bIncremental)
    {
        $strRegExp .= $strDateTimeRegExp . "F";
    }
    
    if ($bDifferential || $bIncremental)
    {
        if ($bFull)
        {
            $strRegExp .= "(\\_";
        }
        
        $strRegExp .= $strDateTimeRegExp;
        
        if ($bDifferential && $bIncremental)
        {
            $strRegExp .= "(D|I)";
        }
        elsif ($bDifferential)
        {
            $strRegExp .= "D";
        }
        else
        {
            $strRegExp .= "I";
        }

        if ($bFull)
        {
            $strRegExp .= "){0,1}";
        }
    }
    
    $strRegExp .= "\$";
    
    &log(DEBUG, "backup_regexp_get($bFull, $bDifferential, $bIncremental): $strRegExp");
    
    return $strRegExp;
}

####################################################################################################################################
# BACKUP_TYPE_FIND - Find the last backup depending on the type
####################################################################################################################################
sub backup_type_find
{
    my $strType = shift;
    my $strBackupClusterPath = shift;
    my $strDirectory;

    if ($strType eq 'incremental')
    {
        $strDirectory = (file_list_get($strBackupClusterPath, backup_regexp_get(1, 1, 1), "reverse"))[0];
    }

    if (!defined($strDirectory) && $strType ne "full")
    {
        $strDirectory = (file_list_get($strBackupClusterPath, backup_regexp_get(1, 0, 0), "reverse"))[0];
    }
    
    return $strDirectory;
}

####################################################################################################################################
# CONFIG_LOAD - Get a value from the config and be sure that it is defined (unless bRequired is false)
####################################################################################################################################
sub config_load
{
    my $oConfigRef = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $bRequired = shift;
    
    if (!defined($bRequired))
    {
        $bRequired = 1;
    }
    
    my $strValue = ${$oConfigRef}{"${strSection}"}{"${strKey}"};
    
    if ($bRequired && !defined($strValue))
    {
        die &log(ERROR, 'config value ${strSection}->${strKey} is undefined');
    }
    
    return $strValue;
}

####################################################################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
####################################################################################################################################
sub file_hash_get
{
    my $strCommand = shift;
    my $strFile = shift;
    
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = trim(execute($strCommand));
    
    return($strHash);
}

####################################################################################################################################
# DATA_HASH_BUILD - Hash a delimited file with header
####################################################################################################################################
sub data_hash_build
{
    my $strData = shift;
    my $strDelimiter = shift;
    my $strUndefinedKey = shift;

    my @stryFile = split("\n", $strData);
    my @stryHeader = split($strDelimiter, $stryFile[0]);
    
    my %oHash;

    for (my $iLineIdx = 1; $iLineIdx < scalar @stryFile; $iLineIdx++)
    {
        my @stryLine = split($strDelimiter, $stryFile[$iLineIdx]);

        if (!defined($stryLine[0]) || $stryLine[0] eq "")
        {
            $stryLine[0] = $strUndefinedKey;
        }

        for (my $iColumnIdx = 1; $iColumnIdx < scalar @stryHeader; $iColumnIdx++)
        {
            if (defined($oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"}))
            {
                confess "the first column must be unique to build the hash";
            }
            
            $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} = $stryLine[$iColumnIdx];
        }
    }

    return %oHash;
}

####################################################################################################################################
# TABLESPACE_MAP_GET - Get the mapping between oid and tablespace name
####################################################################################################################################
sub tablespace_map_get
{
    my $strCommandPsql = shift;

    my %oTablespaceMap = data_hash_build("oid\tname\n" . execute($strCommandPsql .
                                         " -c 'copy (select oid, spcname from pg_tablespace) to stdout' postgres"), "\t");
    
    return %oTablespaceMap;
}

####################################################################################################################################
# MANIFEST_GET - Get a directory manifest
####################################################################################################################################
sub manifest_get
{
    my $strCommandManifest = shift;
    my $strPath = shift;

    my $strCommand = $strCommandManifest;
    $strCommand =~ s/\%path\%/$strPath/g;

    my %oManifest = data_hash_build("name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination\n" .
                                    execute($strCommand), "\t", ".");
    
    return %oManifest;
}

####################################################################################################################################
# BACKUP_MANIFEST_LOAD - Load the backup manifest
####################################################################################################################################
sub backup_manifest_load
{
    my $strBackupManifestFile = shift;
    
    my %oBackupManifestFile;
    tie %oBackupManifestFile, 'Config::IniFiles', (-file => $strBackupManifestFile) or die &log(ERROR, "backup manifest '${strBackupManifestFile}' could not be loaded");

    my %oBackupManifest;
    my $strSection;

    foreach $strSection (sort(keys %oBackupManifestFile))
    {
        my $strKey;
        
        #&log(DEBUG, "section: ${strSection}");

        foreach $strKey (sort(keys ${oBackupManifestFile}{"${strSection}"}))
        {
            my $strValue = ${oBackupManifestFile}{"${strSection}"}{"$strKey"};

            #&log(DEBUG, "    key: ${strKey}=${strValue}");
            $oBackupManifest{"${strSection}"}{"$strKey"} = decode_json($strValue);
        }
    }
    
    return %oBackupManifest;
}
####################################################################################################################################
# BACKUP_MANIFEST_SAVE - Save the backup manifest
####################################################################################################################################
sub backup_manifest_save
{
    my $strBackupManifestFile = shift;
    my $oBackupManifestRef = shift;
    
    my %oBackupManifest;
    tie %oBackupManifest, 'Config::IniFiles' or die &log(ERROR, "Unable to create backup config");

    my $strSection;

    foreach $strSection (sort(keys $oBackupManifestRef))
    {
        my $strKey;
        
        #&log(DEBUG, "section: ${strSection}");

        foreach $strKey (sort(keys ${$oBackupManifestRef}{"${strSection}"}))
        {
            my $strValue = encode_json(${$oBackupManifestRef}{"${strSection}"}{"$strKey"});

            #&log(DEBUG, "    key: ${strKey}=${strValue}");
            $oBackupManifest{"${strSection}"}{"$strKey"} = $strValue;
        }
    }
    
    tied(%oBackupManifest)->WriteConfig($strBackupManifestFile);
}

####################################################################################################################################
# BACKUP_MANIFEST_BUILD - Create the backup manifest
####################################################################################################################################
sub backup_manifest_build
{
    my $strCommandManifest = shift;
    my $strDbClusterPath = shift;
    my $oBackupManifestRef = shift;
    my $oLastManifestRef = shift;
    my $oTablespaceMapRef = shift;
    my $strLevel = shift;
    
    if (!defined($strLevel))
    {
        $strLevel = "base";
    }
    
    my %oManifestHash = manifest_get($strCommandManifest, $strDbClusterPath);
    my $strName;

    foreach $strName (sort(keys $oManifestHash{name}))
    {
        # Skip certain files during backup
        if ($strName =~ /^pg\_xlog\/.*/ ||    # pg_xlog/ - this will be reconstructed
            $strName =~ /^postmaster\.pid$/)  # postmaster.pid - to avoid confusing postgres when restoring
        {
            next;
        }
        
        my $cType = $oManifestHash{name}{"${strName}"}{type};
        my $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
        my $strSection = "${strLevel}:path";

        #&log(DEBUG, "${strDbClusterPath} ${cType}: $strName");

        if ($cType eq "f")
        {
            $strSection = "${strLevel}:file";
        }
        elsif ($cType eq "l")
        {
            $strSection = "${strLevel}:link";
        }
        elsif ($cType ne "d")
        {
            die &log(ERROR, "Unrecognized file type $cType for file $strName");
        }

        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{user} = $oManifestHash{name}{"${strName}"}{user};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{group} = $oManifestHash{name}{"${strName}"}{group};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{permission} = $oManifestHash{name}{"${strName}"}{permission};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} = 
            (split("\\.", $oManifestHash{name}{"${strName}"}{modification_time}))[0];

#                print("modification time:" . ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time});

        if ($cType eq "f" || $cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{inode} = $oManifestHash{name}{"${strName}"}{inode};
        }

        if ($cType eq "f")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};
            
            if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}))
            {
                if (${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} == ${$oLastManifestRef}{"${strSection}"}{"$strName"}{size} &&
                    ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} == ${$oLastManifestRef}{"${strSection}"}{"$strName"}{modification_time})
                {
                    if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference}))
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} = ${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference};
                    }
                    else
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} = ${$oLastManifestRef}{common}{backup}{label};
                    }
                    
                    my $strReference = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference};
                    
                    if (!defined(${$oBackupManifestRef}{common}{backup}{reference}))
                    {
                        ${$oBackupManifestRef}{common}{backup}{reference} = $strReference;
                    }
                    else
                    {
                        if (${$oBackupManifestRef}{common}{backup}{reference} !~ /$strReference/)
                        {
                            ${$oBackupManifestRef}{common}{backup}{reference} .= ",$strReference";
                        }
                    }
                }
            }
        }

        if ($cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{link_destination} = $oManifestHash{name}{"${strName}"}{link_destination};

            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq "base")
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"$strTablespaceOid"}{name};
                #&log(DEBUG, "tablespace: ${strTablespace}");

                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{oid} = $strTablespaceOid;
                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{path} = $strLinkDestination;
                
                backup_manifest_build($strCommandManifest, $strLinkDestination, $oBackupManifestRef, $oLastManifestRef, $oTablespaceMapRef, "tablespace:${strTablespaceName}");
            }
        }
    }
}

####################################################################################################################################
# BACKUP - Perform the backup
####################################################################################################################################
sub backup
{
    my $strCommandChecksum = shift;
    my $strCommandCompress = shift;
    my $strCommandDecompress = shift;
    my $strCommandCopy = shift;
    my $strDbClusterPath = shift;
    my $strBackupTmpPath = shift;
    my $oBackupManifestRef = shift;

    my $strSectionPath;
    my $strCommand;

    # Create the backup file dir
    my $strBackupDestinationTmpFile = "${strBackupTmpPath}/file.tmp";

    # Iterate through the path sections to backup
    foreach $strSectionPath (sort(keys $oBackupManifestRef))
    {
        # Skip non-path sections
        if ($strSectionPath !~ /\:path$/)
        {
            next;
        }

        # Determine the source and destination backup paths
        my $strBackupSourcePath;
        my $strBackupDestinationPath;
        my $strSectionFile;
        my $strFile;

        my $strTablespaceName;
        my $strTablespaceLink;
        my $strBackupTablespaceLink;

        if ($strSectionPath =~ /^base\:/)
        {
            $strBackupSourcePath = "${strDbClusterPath}";
            $strBackupDestinationPath = "${strBackupTmpPath}/base";
            $strSectionFile = "base:file";

            undef($strTablespaceName);
            undef($strTablespaceLink);
            undef($strBackupTablespaceLink);
        }
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            $strTablespaceName = (split(":", $strSectionPath))[1];
            $strBackupSourcePath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            $strBackupDestinationPath = "${strBackupTmpPath}/tablespace";

            # Create the tablespace path
            #unless (-e $strBackupDestinationPath)
            #{
            #    execute("mkdir -p -m 0750 ${strBackupDestinationPath}");
            #}
            
            $strBackupDestinationPath = "${strBackupTmpPath}/tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";
            #my $strSectionLink = "tablespace:${strTablespaceName}:link";
            
            # Create link to the tablespace
            $strTablespaceLink = "pg_tblspc/" . ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{oid};
            $strBackupTablespaceLink = "${strBackupTmpPath}/base/${strTablespaceLink}";
            #my $strBackupLinkPath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
        }
        else
        {
            die "Cannot find type for section ${strSectionPath}";
        }

        # Create the base or tablespace path
#        unless (-e $strBackupDestinationPath)
#        {
#            execute("mkdir -p -m 0750 ${strBackupDestinationPath}");
            #mkdir $strBackupDestinationPath, 0750 or die "Unable to create path ${strBackupDestinationPath}";
#        }

        # Create all the paths required to store the files
        my $strPath;

        foreach $strPath (sort(keys ${$oBackupManifestRef}{"${strSectionPath}"}))
        {
#            my $lModificationTime = ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{modification_time};
            my $strBackupDestinationSubPath = "${strBackupDestinationPath}/${strPath}";
            my $iFileTotal = 0;

            if (!$bHardLink && $strType ne "full" && !(!defined($strTablespaceName) && $strPath eq 'pg_xlog'))
            {
                &log(DEBUG, "skipping path: ${strPath}");

                if (!defined(${$oBackupManifestRef}{"${strSectionFile}"}))
                {
                    next;
                }
                
                foreach $strFile (sort(keys ${$oBackupManifestRef}{"${strSectionFile}"}))
                {
                    if (dirname($strFile) eq $strPath && !defined(${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference}))
                    {
                        $iFileTotal += 1;
                        last;
                    }
                }
                
                if ($iFileTotal == 0)
                {
                    next;
                }
            }
            
            if (defined($strTablespaceName))
            {
#                unless (-e $strBackupTablespaceLink)
#                {
#                    execute("mkdir -p -m 0750 ${strBackupTablespaceLink}");
#                    unlink $strBackupTablespaceLink or die &log(ERROR, "Unable to remove table link '${strBackupTablespaceLink}'");
#                }

                unless (-e $strBackupTablespaceLink)
                {
                    execute("ln -s ../../tablespace/${strTablespaceName} $strBackupTablespaceLink");
                }
                
                execute ("chmod " . ${$oBackupManifestRef}{"base:link"}{$strTablespaceLink}{permission} . " ${strBackupTablespaceLink}");
            }

            unless (-e $strBackupDestinationSubPath)
            {
                execute("mkdir -p ${strBackupDestinationSubPath}");
            }
            
            execute ("chmod ${$oBackupManifestRef}{$strSectionPath}{$strPath}{permission} ${strBackupDestinationSubPath}");
#            utime($lModificationTime, $lModificationTime, $strBackupDestinationTmpFile) or die "Unable to set time for ${strBackupDestinationTmpFile}";
        }

        #&log(DEBUG, "Backing up ${strBackupSourcePath}");

        # Possible for the path section to exist with no files (i.e. empty tablespace)
        if (!defined(${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            next;
        }

        # Iterate through the files for each backup source path
        foreach $strFile (sort(keys ${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            my $strBackupSourceFile = "${strBackupSourcePath}/${strFile}";
            my $iSize = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{size};
            my $lModificationTime = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time};

            # If the file is a reference it does not need to be copied
            my $strReference = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference};

            if (defined($strReference))
            {
                if ($bHardLink)
                {
                    my $strLinkSource = "${strBackupTmpPath}/../${strReference}";
                    my $strLinkDestination = "${strBackupTmpPath}";
                    
                    if (defined($strTablespaceName))
                    {
                        $strLinkSource .= "/tablespace/${strTablespaceName}/${strFile}";
                        $strLinkDestination .= "/tablespace/${strTablespaceName}/${strFile}";
                    }
                    else
                    {
                        $strLinkSource .= "/base/${strFile}";
                        $strLinkDestination .= "/base/${strFile}";
                    }
                    
                    if (!$bNoCompression && $iSize != 0)
                    {
                        $strLinkSource .= ".gz";
                        $strLinkDestination .= ".gz";
                    }
                    
                    if (-e $strLinkDestination)
                    {
                        unlink $strLinkDestination or die "Unable to unlink ${$strLinkDestination}";
                    }
                    
                    execute("ln ${strLinkSource} ${strLinkDestination}");
                }
                
                next;
            }

            #&log(DEBUG, "   Backing up ${strBackupSourceFile}");
            
            # Copy the file and calculate checksum
            my $strBackupDestinationFile = "${strBackupDestinationPath}/${strFile}";
            my $strHash;

            if ($iSize == 0)
            {
                execute("touch ${strBackupDestinationTmpFile}")
            }
            else
            {
                if ($bNoCompression || $iSize == 0)
                {
                    $strCommand = $strCommandCopy;
                    $strCommand =~ s/\%source\%/${strBackupSourceFile}/g;
                    $strCommand =~ s/\%destination\%/${strBackupDestinationTmpFile}/g;

                    execute($strCommand);
                
                    $strHash = file_hash_get($strCommandChecksum, $strBackupDestinationTmpFile);
                }
                else
                {
                    $strCommand = $strCommandCompress;
                    $strCommand =~ s/\%file\%/${strBackupSourceFile}/g;
                    $strCommand .= " > ${strBackupDestinationTmpFile}";

                    execute($strCommand);

                    $strCommand = $strCommandDecompress;
                    $strCommand =~ s/\%file\%/${strBackupDestinationTmpFile}/g;
                    $strCommand .= " | " . $strCommandChecksum;
                    $strCommand =~ s/\%file\%//g;

                    #&log(DEBUG, "       command ${strCommand}");

                    $strHash = trim(execute($strCommand));
                }
            
                #&log(DEBUG, "       Hash ${strHash}");
            
                if (!$bNoCompression)
                {
                    $strBackupDestinationFile .= ".gz";
                }
            }

            # Set permissions and move the file
            execute ("chmod ${$oBackupManifestRef}{$strSectionFile}{$strFile}{permission} ${strBackupDestinationTmpFile}");
            utime($lModificationTime, $lModificationTime, $strBackupDestinationTmpFile) or die "Unable to set time for ${strBackupDestinationTmpFile}";
            rename $strBackupDestinationTmpFile, $strBackupDestinationFile or die "Unable to move ${strBackupDestinationFile}";

            # Write the hash into the backup manifest
            if (defined($strHash))
            {
                ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} = $strHash;
            }
        }
    }
}

####################################################################################################################################
# ARCHIVE_LIST_GET
####################################################################################################################################
sub archive_list_get
{
    my $strArchiveStart = shift;
    my $strArchiveStop = shift;
    
    my $strTimeline = substr($strArchiveStart, 0, 8);
    my @stryArchive;
    my $iArchiveIdx = 0;

    if ($strTimeline ne substr($strArchiveStop, 0, 8))
    {
        die "Timelines between ${strArchiveStart} and ${strArchiveStop} differ";
    }
        
    my $iStartMajor = hex substr($strArchiveStart, 8, 8);
    my $iStartMinor = hex substr($strArchiveStart, 16, 8);

    my $iStopMajor = hex substr($strArchiveStop, 8, 8);
    my $iStopMinor = hex substr($strArchiveStop, 16, 8);

    do
    {
        $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));

        if ($strArchiveStart ne $strArchiveStop)
        {
            $iArchiveIdx += 1;
            $iStartMinor += 1;

            if ($iStartMinor == 256)
            {
                $iStartMajor += 1;
                $iStartMinor = 0;
            }
        }
    }
    while !($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor);    

    return @stryArchive;
}

####################################################################################################################################
# FILE_LIST_GET
####################################################################################################################################
sub file_list_get
{
    my $strPath = shift;
    my $strExpression = shift;
    my $strSortOrder = shift;
    
    my $hDir;
    
    opendir $hDir, $strPath or die &log(ERROR, "unable to open path ${strPath}");
    my @stryFileAll = readdir $hDir or die &log(ERROR, "unable to get files for path ${strPath}, expression ${strExpression}");
    close $hDir;
    
    my @stryFile;

    if (@stryFileAll)
    {
        @stryFile = grep(/$strExpression/i, @stryFileAll)
    }
    
    if (@stryFile)
    {
        if (defined($strSortOrder) && $strSortOrder eq "reverse")
        {
            return sort {$b cmp $a} @stryFile;
        }
        else
        {
            return sort @stryFile;
        }
    }
    
    return @stryFile;
}

####################################################################################################################################
# BACKUP_EXPIRE
####################################################################################################################################
sub backup_expire
{
    my $strBackupClusterPath = shift;       # Base path to cluster backup
    my $iFullRetention = shift;             # Number of full backups to keep
    my $iDifferentialRetention = shift;     # Number of differential backups to keep
    my $strArchiveRetentionType = shift;    # Type of backup to base archive retention on
    my $iArchiveRetention = shift;          # Number of backups worth of archive to keep
    
    # Find all the expired full backups
    my $iIndex = $iFullRetention;
    my $strPath;
    my @stryPath = file_list_get($strBackupClusterPath, backup_regexp_get(1, 0, 0), "reverse");

    while (defined($stryPath[$iIndex]))
    {
        &log(INFO, "removed expired full backup: " . $stryPath[$iIndex]);

        # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
        # be consistent if the backup dies.
        foreach $strPath (file_list_get($strBackupClusterPath, "^" . $stryPath[$iIndex] . ".*", "reverse"))
        {
            rmtree("$strBackupClusterPath/$strPath") or die &log(ERROR, "unable to delete backup ${strPath}");
        }
        
        $iIndex++;
    }
    
    # Find all the expired differential backups
    @stryPath = file_list_get($strBackupClusterPath, backup_regexp_get(0, 1, 0), "reverse");
     
    if (defined($stryPath[$iDifferentialRetention]))
    {
        # Get a list of all differential and incremental backups
        foreach $strPath (file_list_get($strBackupClusterPath, backup_regexp_get(0, 1, 1), "reverse"))
        {
            # Remove all differential and incremental backups before the oldest valid differential
            if (substr($strPath, 0, length($strPath) - 1) lt $stryPath[$iDifferentialRetention])
            {
                rmtree("$strBackupClusterPath/$strPath") or die &log(ERROR, "unable to delete backup ${strPath}");
                &log(INFO, "removed expired diff/incr backup ${strPath}");
            }
        }
    }
    
    # Determine which backup type to use for archive retention (full, differential, incremental)
    if ($strArchiveRetentionType eq "full")
    {
        @stryPath = file_list_get($strBackupClusterPath, backup_regexp_get(1, 0, 0), "reverse");
    }
    elsif ($strArchiveRetentionType eq "differential")
    {
        @stryPath = file_list_get($strBackupClusterPath, backup_regexp_get(1, 1, 0), "reverse");
    }
    else
    {
        @stryPath = file_list_get($strBackupClusterPath, backup_regexp_get(1, 1, 1), "reverse");
    }
    
    # if no backups were found then preserve current archive logs - too afraid to delete!
    my $iBackupTotal = scalar @stryPath;
    
    if ($iBackupTotal == 0)
    {
        return;
    }
    
    # See if enough backups exist for expiration to start
    my $strArchiveRetentionBackup = $stryPath[$iArchiveRetention - 1];
    
    if (!defined($strArchiveRetentionBackup))
    {
        return;
    }

    # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this backup
    # even though they are also in the pg_xlog directory (since they have been copied more than once).
    &log(INFO, "archive retention based on backup " . $strArchiveRetentionBackup);

    my %oManifest = backup_manifest_load("${strBackupClusterPath}/$strArchiveRetentionBackup/backup.manifest");
    my $strArchiveLast = ${oManifest}{archive}{archive_location}{start};
    
    if (!defined($strArchiveLast))
    {
        &log(INFO, "invalid archive location retrieved ${$strArchiveRetentionBackup}");
    }
    
    &log(INFO, "archive retention starts at " . $strArchiveLast);

    # Remove any archive directories or files that are out of date
    foreach $strPath (file_list_get($strBackupClusterPath . "/archive", "^[0-F]{16}\$"))
    {
        # If less than first 16 characters of current archive file, then remove the directory
        if ($strPath lt substr($strArchiveLast, 0, 16))
        {
            rmtree($strBackupClusterPath . "/archive/" . $strPath) or die &log(ERROR, "unable to remove " . $strPath);
            &log(DEBUG, "removed major archive directory " . $strPath);
        }
        # If equals the first 16 characters of the current archive file, then delete individual files instead
        elsif ($strPath eq substr($strArchiveLast, 0, 16))
        {
            my $strSubPath;
        
            # Look for archive files in the archive directory
            foreach $strSubPath (file_list_get($strBackupClusterPath . "/archive/" . $strPath, "^[0-F]{24}.*\$"))
            {
                # Delete if the first 24 characters less than the current archive file
                if ($strSubPath lt substr($strArchiveLast, 0, 24))
                {
                    unlink("${strBackupClusterPath}/archive/${strPath}/${strSubPath}") or die &log(ERROR, "unable to remove " . $strSubPath);
                    &log(DEBUG, "removed expired archive file " . $strSubPath);
                }
            }
        }
    }
}

####################################################################################################################################
# PATH_GET
####################################################################################################################################
use constant 
{
    PATH_DB_ABSOLUTE    => 'db:absolute',
    PATH_DB_RELATIVE    => 'db:relative',
    PATH_BACKUP         => 'backup',
    PATH_BACKUP_CLUSTER => 'backup:cluster',
    PATH_BACKUP_TMP     => 'backup:tmp',
    PATH_BACKUP_ARCHIVE => 'backup:archive'
};

sub path_get
{
    my $strType = shift;
    my $strPath = shift;
    my $strFile = shift;

    # Parse paths on the db side
    if ($strType eq PATH_DB_ABSOLUTE)
    {
        return (defined($strPath) ? $strPath : "") . (defined($strFile) ? (defined($strPath) ? "/" : "") . $strFile : "");
    }
    elsif ($strType eq PATH_DB_RELATIVE)
    {
        if (!defined($strDbClusterPath))
        {
            confess &log(ASSERT, "\$strDbClusterPath not yet defined");
        }

        return $strDbClusterPath . "/" . $strPath;
    }
    # Parse paths on the backup side
    elsif ($strType eq PATH_BACKUP)
    {
        if (!defined($strBackupPath))
        {
            confess &log(ASSERT, "\$strBackupPath not yet defined");
        }
        
        return $strBackupPath;
    }
    elsif ($strType eq PATH_BACKUP_CLUSTER)
    {
        if (!defined($strBackupClusterPath))
        {
            confess &log(ASSERT, "\$strBackupPath not yet defined");
        }
        
        return $strBackupPath;
    }
    elsif ($strType eq PATH_BACKUP_CLUSTER || $strType eq PATH_BACKUP_TMP || $strType eq PATH_BACKUP_ARCHIVE)
    {
        if (!defined($strBackupClusterPath))
        {
            confess &log(ASSERT, "\$strBackupClusterPath not yet defined");
        }
        
        if ($strType eq PATH_BACKUP_CLUSTER)
        {
            return $strBackupClusterPath;
        }
        elsif ($strType eq PATH_BACKUP_TMP)
        {
            return $strBackupClusterPath . "/backup.tmp" . (defined($strPath) ? "/${strPath}" : "");
        }
        elsif ($strType eq PATH_BACKUP_ARCHIVE)
        {
            my $strArchive;
            
            if (defined($strFile))
            {
                $strArchive = substr($strFile, 0, 24);
            
                if ($strArchive !~ /^([0-F]){24}$/)
                {
                    croak &log(ERROR, "$strFile not a valid archive file");
                }
            }
            
            return $strBackupClusterPath . "/archive" . (defined($strArchive) ? "/" . 
                   substr($strArchive, 0, 16) : "") . "/" . $strFile;
        }
        
    }

    # Error when path type not recognized
    confess &log(ASSERT, "no known path types in '${strType}'");
}

####################################################################################################################################
# FILE_COPY
####################################################################################################################################
sub file_copy
{
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
}

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Get the command
my $strOperation = $ARGV[0];
my $strLogFile = "";

# !!! Pick the log file name here (backup, restore, archive-YYYYMMDD)
# 
if ($strOperation eq "archive-push")
{
    
}

####################################################################################################################################
# LOAD CONFIG FILE
####################################################################################################################################
if (!defined($strConfigFile))
{
    $strConfigFile = "/etc/pg_backrest.conf";
}

my %oConfig;
tie %oConfig, 'Config::IniFiles', (-file => $strConfigFile) or die "Unable to find config file";

# Load commands required for archive-push
my $strCommandChecksum = config_load(\%oConfig, "command", "checksum", !$bNoChecksum);
my $strCommandCompress = config_load(\%oConfig, "command", "compress", !$bNoCompression);
my $strCommandDecompress = config_load(\%oConfig, "command", "decompress", !$bNoCompression);
my $strCommandCopy = config_load(\%oConfig, "command", "copy", $bNoCompression);

# Load and check the base backup path
$strBackupPath = $oConfig{common}{backup_path};

if (!defined($strBackupPath))
{
    die &log(ERROR, "common:backup_path undefined");
}

unless (-e $strBackupPath)
{
    die &log(ERROR, "base path ${strBackupPath} does not exist");
}

# Load and check the cluster
if (!defined($strCluster))
{
    $strCluster = "db"; #!!! Modify to load cluster from conf if there is only one, else error
}

$strBackupClusterPath = "${strBackupPath}/${strCluster}";

unless (-e $strBackupClusterPath)
{
    &log(INFO, "creating cluster path ${strBackupClusterPath}");
    mkdir $strBackupClusterPath or die &log(ERROR, "cluster backup path '${strBackupClusterPath}' create failed");
}

####################################################################################################################################
# ARCHIVE-PUSH Command
####################################################################################################################################
if ($strOperation eq "archive-push")
{
    # archive-push command must have three arguments
    if (@ARGV != 2)
    {
        die "not enough arguments - show usage";
    }

    my $strBackupClusterArchivePath = "${strBackupClusterPath}/archive";

    unless (-e $strBackupClusterArchivePath)
    {
        &log(INFO, "creating cluster archive path ${strBackupClusterArchivePath}");
        mkdir $strBackupClusterArchivePath or die &log(ERROR, "cluster backup archive path '${strBackupClusterArchivePath}' create failed");
    }

    # Get the source dir/file
    my $strSourceFile = $ARGV[1];
    
    unless (-e $strSourceFile)
    {
        die "source file does not exist - show usage";
    }

    # Get the destination dir/file
    my $strDestinationFile = basename($strSourceFile);
    my $strDestinationTmpFile = "${strBackupClusterArchivePath}/archive.tmp";
    my $strBackupClusterArchiveSubPath = "${strBackupClusterArchivePath}";

    if (-e $strDestinationTmpFile)
    {
        unlink($strDestinationTmpFile);
    }

    # Setup the copy command
    my $strCommand;

    # !!! Modify this to skip compression and checksum for any file that is not a log file
    if ($strDestinationFile =~ /^([0-F]){24}.*/)
    {
        $strBackupClusterArchiveSubPath = "${strBackupClusterArchivePath}/" . substr($strDestinationFile, 0, 16);

        if ($strDestinationFile =~ /^([0-F]){24}$/)
        {
            unless (-e $strBackupClusterArchiveSubPath)
            {
                &log(INFO, "creating cluster archive sub path ${strBackupClusterArchiveSubPath}");
                mkdir $strBackupClusterArchiveSubPath or die &log(ERROR, "cluster backup archive sub path '${strBackupClusterArchiveSubPath}' create failed");
            }

            # Make sure the destination file does NOT exist - ignore checksum and extension in case they (or options) have changed
            if (glob("${strBackupClusterArchiveSubPath}/${strDestinationFile}*"))
            {
                die "destination file already exists";
            }

            # Calculate sha1 hash for the file (unless disabled)
            if (!$bNoChecksum)
            {
                $strDestinationFile .= "-" . file_hash_get($strCommandChecksum, $strSourceFile);
            }

            if ($bNoCompression)
            {
                $strCommand = $strCommandCopy;
                $strCommand =~ s/\%source\%/${strSourceFile}/g;
                $strCommand =~ s/\%destination\%/${strDestinationTmpFile}/g;
            }
            else
            {
                $strCommand = $strCommandCompress;
                $strCommand =~ s/\%file\%/${strSourceFile}/g;
                $strCommand .= " > ${strDestinationTmpFile}";
                $strDestinationFile .= ".gz";
            }
        }
        else
        {
            $strCommand = $strCommandCopy;
            $strCommand =~ s/\%source\%/$strSourceFile/g;
            $strCommand =~ s/\%destination\%/${strDestinationTmpFile}/g;
        }
    }
    else
    {
        die &log(ERROR, "Unknown file type ${strDestinationFile}")
    }
    
    # Execute the copy
    execute($strCommand);
    rename($strDestinationTmpFile, "${strBackupClusterArchiveSubPath}/${strDestinationFile}")
        or die &log(ERROR, "unable to rename '${strBackupClusterArchiveSubPath}/${strDestinationFile}'");

    exit 0;
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Check the backup type
if ($strType eq "diff")
{
    $strType = "differential";
}

if ($strType eq "incr")
{
    $strType = "incremental";
}

if ($strType ne "full" && $strType ne "differential" && $strType ne "incremental")
{
    die &log(ERROR, "backup type must be full, differential (diff), incremental (incr)");
}

# Load commands required for backup
my $strCommandManifest = config_load(\%oConfig, "command", "manifest");
my $strCommandPsql = config_load(\%oConfig, "command", "psql");

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq "backup")
{
    # Make sure that the cluster data directory exists
    $strDbClusterPath = $oConfig{"cluster:$strCluster"}{path};

    if (!defined($strDbClusterPath))
    {
        die &log(ERROR, "cluster data path is not defined");
    }

    unless (-e $strDbClusterPath)
    {
        die &log(ERROR, "cluster data path '${strDbClusterPath}' does not exist");
    }

    # Find the previous backup based on the type
    my $strBackupLastPath = backup_type_find($strType, $strBackupClusterPath);

    my %oLastManifest;

    if (defined($strBackupLastPath))
    {
        %oLastManifest = backup_manifest_load("${strBackupClusterPath}/$strBackupLastPath/backup.manifest");
        &log(INFO, "Last backup label: $oLastManifest{common}{backup}{label}");
    }

    # Create the path for the new backup
    my $strBackupPath;

    if ($strType eq "full" || !defined($strBackupLastPath))
    {
        $strBackupPath = date_string_get() . "F";
        $strType = "full";
    }
    else
    {
        $strBackupPath = substr($strBackupLastPath, 0, 16);

        $strBackupPath .= "_" . date_string_get();
        
        if ($strType eq "differential")
        {
            $strBackupPath .= "D";
        }
        else
        {
            $strBackupPath .= "I";
        }
    }

    # Build backup tmp and config
    my $strBackupTmpPath = "${strBackupClusterPath}/backup.tmp";
    my $strBackupConfFile = "${strBackupTmpPath}/backup.manifest";

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupTmpPath)
    {
        &log(WARNING, "backup path $strBackupTmpPath already exists");

        if (-e $strBackupConfFile)
        {
            unlink $strBackupConfFile or die &log(ERROR, "backup config ${strBackupConfFile} could not be deleted");
        }
    }
    # Else create the backup tmp path
    else
    {
        &log(INFO, "creating backup path $strBackupTmpPath");
        mkdir $strBackupTmpPath or die &log(ERROR, "backup path ${strBackupTmpPath} could not be created");
    }

    # Create a new backup manifest hash
    my %oBackupManifest;

    # Start backup
    my $strLabel = $strBackupPath;
    ${oBackupManifest}{common}{backup}{label} = $strLabel;

    my $strArchiveStart = trim(execute($strCommandPsql .
        " -c \"set client_min_messages = 'warning';copy (select pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}') as xlog) to stdout\" postgres"));
        
    ${oBackupManifest}{archive}{archive_location}{start} = $strArchiveStart;

    &log(INFO, 'Backup archive start: ' . $strArchiveStart);

    # Build the backup manifest
    my %oTablespaceMap = tablespace_map_get($strCommandPsql);
    backup_manifest_build($strCommandManifest, $strDbClusterPath, \%oBackupManifest, \%oLastManifest, \%oTablespaceMap);

    # Delete files leftover from a partial backup
    # !!! do it

    # Perform the backup
    backup($strCommandChecksum, $strCommandCompress, $strCommandDecompress, $strCommandCopy, $strDbClusterPath,
           $strBackupTmpPath, \%oBackupManifest);
           
    #sleep(30);

    # Stop backup
    my $strArchiveStop = trim(execute($strCommandPsql .
        " -c \"set client_min_messages = 'warning'; copy (select pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout\" postgres"));

    ${oBackupManifest}{archive}{archive_location}{stop} = $strArchiveStop;

    &log(INFO, 'Backup archive stop: ' . $strArchiveStop);

    # Fetch the archive logs and put them in pg_xlog
    my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop);

    foreach my $strArchive (@stryArchive)
    {
        my $strArchivePath = "${strBackupClusterPath}/archive/" . substr($strArchive, 0, 16);
        my @stryArchiveFile = file_list_get($strArchivePath, "^${strArchive}(-[0-f]+){0,1}(\\.gz){0,1}\$");
        
        if (scalar @stryArchiveFile != 1)
        {
            die &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}"); 
        }

        my $strArchiveFile = $strArchivePath . "/" . $stryArchiveFile[0];
        my $strArchiveBackupFile = "$strBackupTmpPath/base/pg_xlog/${strArchive}";
        
        if ($strArchiveFile =~ /\.gz$/)
        {
            $strArchiveBackupFile .= ".gz";
        }
        
        copy($strArchiveFile, $strArchiveBackupFile) or die "Unable to copy archive file: ${strArchiveFile}";
    }
    
    # Save the backup conf file
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);
    backup_manifest_load($strBackupConfFile);

    # Rename the backup tmp path to complete the backup
    rename($strBackupTmpPath, "${strBackupClusterPath}/${strBackupPath}") or die &log(ERROR, "unable to ${strBackupTmpPath} rename to ${strBackupPath}"); 

    # Expire backups
    backup_expire($strBackupClusterPath, 2, 2, "full", 2);
    
    exit 0;
}
