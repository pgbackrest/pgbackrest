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
use File::Remove;
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
    
# Global constants
use constant
{
    true  => 1,
    false => 0
};

# Global command strings
my $strCommandChecksum;
my $strCommandCompress;
my $strCommandDecompress;
my $strCommandCopy;

my $strCompressExtension = "gz";
my $strDefaultPathPermission = "0750";

# Global variables
my $strDbClusterPath;          # Database cluster base path

my $strBackupPath;             # Backup base path
my $strBackupClusterPath;       # Backup cluster path

####################################################################################################################################
####################################################################################################################################
## UTILITY FUNCTIONS
####################################################################################################################################
####################################################################################################################################

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
    
    if (!defined($strMessage))
    {
        $strMessage = "(undefined)";
    }

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

    return $strOutput;
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
####################################################################################################################################
## FILE FUNCTIONS
####################################################################################################################################
####################################################################################################################################

####################################################################################################################################
# PATH_GET
####################################################################################################################################
use constant 
{
    PATH_DB_ABSOLUTE    => 'db:absolute',
#    PATH_DB_RELATIVE    => 'db:relative',
    PATH_BACKUP         => 'backup',
    PATH_BACKUP_CLUSTER => 'backup:cluster',
    PATH_BACKUP_TMP     => 'backup:tmp',
    PATH_BACKUP_ARCHIVE => 'backup:archive'
};

sub path_get
{
    my $strType = shift;
    my $strFile = shift;
    my $bTemp = shift;

    if (defined($bTemp) && $bTemp && !($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_TMP))
    {
        confess &log(ASSERT, "temp file not supported on path " . $strType);
    }

    # Parse paths on the db side
    if ($strType eq PATH_DB_ABSOLUTE)
    {
        return $strFile;
    }
#    elsif ($strType eq PATH_DB_RELATIVE)
#    {
#        if (!defined($strDbClusterPath))
#        {
#            confess &log(ASSERT, "\$strDbClusterPath not yet defined");
#        }
#
#        return $strDbClusterPath . "/" . $strPath;
#    }
    # Parse paths on the backup side
    elsif ($strType eq PATH_BACKUP)
    {
        if (!defined($strBackupPath))
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
            return $strBackupClusterPath . (defined($strFile) ? "/${strFile}" : "");
        }
        elsif ($strType eq PATH_BACKUP_TMP)
        {
            if (defined($bTemp) && $bTemp)
            {
                return $strBackupClusterPath . "/backup.tmp/file.tmp";
            }
            
            return $strBackupClusterPath . "/backup.tmp" . (defined($strFile) ? "/${strFile}" : "");
        }
        elsif ($strType eq PATH_BACKUP_ARCHIVE)
        {
            if (defined($bTemp) && $bTemp)
            {
                return $strBackupClusterPath . "/archive/archive.tmp";
            }
            else
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
    }

    # Error when path type not recognized
    confess &log(ASSERT, "no known path types in '${strType}'");
}

####################################################################################################################################
# LINK_CREATE
####################################################################################################################################
sub link_create
{
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bHard = shift;
    my $bRelative = shift;
    
    my $strSource = path_get($strSourcePathType, $strSourceFile);
    my $strDestination = path_get($strDestinationPathType, $strDestinationFile);

    # If the destination path does not exist, create it
    unless ($strDestinationPathType =~ /^backup\:.*/ and -e dirname($strDestination))
    {
        path_create(undef, dirname($strDestination), $strDefaultPathPermission);
    }

    unless (-e $strSource)
    {
        if (-e $strSource . ".${strCompressExtension}")
        {
            $strSource .= ".${strCompressExtension}";
            $strDestination .= ".${strCompressExtension}";
        }
        else
        {
            confess &log(ASSERT, "unable to find ${strSource}(.${strCompressExtension}) for checksum");
        }
    }
    
    # Create the link
    my $strCommand = "ln";
    
    if (!defined($bHard) || !$bHard)
    {
        $strCommand .= " -s";
    }
    
    $strCommand .= " ${strSource} ${strDestination}";
    system($strCommand) == 0 or confess &log("unable to create link from ${strSource} to ${strDestination}");
#    &log(DEBUG, "link command ($strSourcePathType): ${strCommand}");
}

####################################################################################################################################
# PATH_CREATE
####################################################################################################################################
sub path_create
{
    my $strPathType = shift;
    my $strPath = shift;
    my $strPermission = shift;
    
    # If no permission are given then use the default
    if (!defined($strPermission))
    {
        $strPermission = "0750";
    }

    # Get the path to create
    my $strPathCreate = $strPath;
    
    if (defined($strPathType))
    {
        $strPathCreate = path_get($strPathType, $strPath);
    }

    # Create the path
    system("mkdir -p -m ${strPermission} ${strPathCreate}") == 0 or confess &log(ERROR, "unable to create path ${strPathCreate}");
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
    my $bNoCompressionOverride = shift;

    my $strSource = path_get($strSourcePathType, $strSourceFile);
    my $strDestination = path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmp = path_get($strDestinationPathType, $strDestinationFile, true);
    
    # Is this already a compressed file?
    my $bAlreadyCompressed = $strSource =~ "^.*\.${strCompressExtension}\$";
    
    if ($bAlreadyCompressed && $strDestination !~ "^.*\.${strCompressExtension}\$")
    {
        $strDestination .= ".${strCompressExtension}";
    }
    
    # Generate the command
    my $strCommand;

    if ($bAlreadyCompressed ||
        (defined($bNoCompressionOverride) && $bNoCompressionOverride) ||
        (!defined($bNoCompressionOverride) && $bNoCompression))
    {
        $strCommand = $strCommandCopy;
        $strCommand =~ s/\%source\%/${strSource}/g;
        $strCommand =~ s/\%destination\%/${strDestinationTmp}/g;
    }
    else
    {
        $strCommand = $strCommandCompress;
        $strCommand =~ s/\%file\%/${strSourceFile}/g;
        $strCommand .= " > ${strDestinationTmp}";
        $strDestination .= ".${strCompressExtension}";
    }

    #&log(DEBUG, "copy command $strSource to $strDestination ($strDestinationTmp)");

    # If the destination path does not exist, create it
    unless ($strDestinationPathType =~ /^backup\:.*/ and -e dirname($strDestination))
    {
        path_create(undef, dirname($strDestination), $strDefaultPathPermission);
    }

    # Remove the temp file if it exists (indicates a previous failure)
    if (-e $strDestinationTmp)
    {
        &log(ASSERT, "temp file ${strDestinationTmp} found - should not exist");
        unlink($strDestinationTmp) or die &log(ERROR, "unable to remove temp file ${strDestinationTmp}");
    }
    
    # Copy the file to temp and then move to the final destination
    system($strCommand) == 0 or die &log(ERROR, "unable to copy $strSource to $strDestinationTmp");
    rename($strDestinationTmp, $strDestination) or die &log(ERROR, "unable to move $strDestinationTmp to $strDestination");
}

####################################################################################################################################
# FILE_HASH_GET
####################################################################################################################################
sub file_hash_get
{
    my $strPathType = shift;
    my $strFile = shift;
    
    if (!defined($strCommandChecksum))
    {
        confess &log(ASSERT, "\$strCommandChecksum not defined");
    }
    
    my $strPath = path_get($strPathType, $strFile);
    my $strCommand;
    
    if (-e $strPath)
    {
        $strCommand = $strCommandChecksum;
        $strCommand =~ s/\%file\%/$strFile/g;
    }
    elsif (-e $strPath . ".${strCompressExtension}")
    {
        $strCommand = $strCommandDecompress;
        $strCommand =~ s/\%file\%/${strPath}/g;
        $strCommand .= " | " . $strCommandChecksum;
        $strCommand =~ s/\%file\%//g;
    }
    else
    {
        confess &log(ASSERT, "unable to find $strPath(.${strCompressExtension}) for checksum");
    }
    
    return trim(capture($strCommand)) or confess &log(ERROR, "unable to checksum ${strPath}");
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
####################################################################################################################################
## BACKUP FUNCTIONS
####################################################################################################################################
####################################################################################################################################

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
    
#    &log(DEBUG, "backup_regexp_get($bFull, $bDifferential, $bIncremental): $strRegExp");
    
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
            confess &log(ASSERT, "unrecognized file type $cType for file $strName");
        }

        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{user} = $oManifestHash{name}{"${strName}"}{user};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{group} = $oManifestHash{name}{"${strName}"}{group};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{permission} = $oManifestHash{name}{"${strName}"}{permission};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} = 
            (split("\\.", $oManifestHash{name}{"${strName}"}{modification_time}))[0];

        if ($cType eq "f" || $cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{inode} = $oManifestHash{name}{"${strName}"}{inode};

            my $bSizeMatch = true;
            
            if ($cType eq "f")
            {
                $bSizeMatch = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} =
                              ${$oLastManifestRef}{"${strSection}"}{"$strName"}{size};
            }

            if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}))
            {
                if ($bSizeMatch &&
                    ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} ==
                        ${$oLastManifestRef}{"${strSection}"}{"$strName"}{modification_time})
                {
                    if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference}))
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} =
                            ${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference};
                    }
                    else
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} =
                            ${$oLastManifestRef}{common}{backup}{label};
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

        if ($cType eq "f")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};
        }

        if ($cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{link_destination} =
                $oManifestHash{name}{"${strName}"}{link_destination};

            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq "base")
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"$strTablespaceOid"}{name};
                #&log(DEBUG, "tablespace: ${strTablespace}");

                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{oid} = $strTablespaceOid;
                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{path} = $strLinkDestination;
                
                backup_manifest_build($strCommandManifest, $strLinkDestination, $oBackupManifestRef, $oLastManifestRef,
                                      $oTablespaceMapRef, "tablespace:${strTablespaceName}");
            }
        }
    }
}

####################################################################################################################################
# BACKUP - Perform the backup
#
# Uses the information in the manifest to determine which files need to be copied.  Directories and tablespace links are only
# created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub backup
{
    my $oBackupManifestRef = shift;    # Manifest for the current backup

    # Iterate through the path sections of the manifest to backup
    my $strSectionPath;

    foreach $strSectionPath (sort(keys $oBackupManifestRef))
    {
        # Skip non-path sections
        if ($strSectionPath !~ /\:path$/)
        {
            next;
        }

        # Determine the source and destination backup paths
        my $strBackupSourcePath;        # Absolute path to the database base directory or tablespace to backup
        my $strBackupDestinationPath;   # Relative path to the backup directory where the data will be stored
        my $strSectionFile;             # Manifest section that contains the file data

        # Process the base database directory
        if ($strSectionPath =~ /^base\:/)
        {
            $strBackupSourcePath = "${strDbClusterPath}";
            $strBackupDestinationPath = "base";
            $strSectionFile = "base:file";

            # Create the archive log directory
            path_create(PATH_BACKUP_TMP, "base/pg_xlog");
        }
        # Process each tablespace
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            my $strTablespaceName = (split(":", $strSectionPath))[1];
            $strBackupSourcePath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            $strBackupDestinationPath = "tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";

            # Create the tablespace directory and link
            if ($bHardLink || $strType eq "full")
            {
                path_create(PATH_BACKUP_TMP, $strBackupDestinationPath);

                link_create(PATH_BACKUP_TMP, $strBackupDestinationPath,
                            PATH_BACKUP_TMP, "pg_tblspc/" . ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{oid});
            }
        }
        else
        {
            confess &log(ASSERT, "cannot find type for section ${strSectionPath}");
        }

        # Create all the sub paths if this is a full backup or hardlinks are requested
        if ($bHardLink || $strType eq "full")
        {
            my $strPath;
            
            foreach $strPath (sort(keys ${$oBackupManifestRef}{"${strSectionPath}"}))
            {
                path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}",
                            ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{permission});
            }
        }

        # Possible for the path section to exist with no files (i.e. empty tablespace)
        if (!defined(${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            next;
        }

        # Iterate through the files for each backup source path
        my $strFile;
        
        foreach $strFile (sort(keys ${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            my $strBackupSourceFile = "${strBackupSourcePath}/${strFile}";

            # If the file is a has reference it does not need to be copied since it can be retrieved from the referenced backup.
            # However, if hard-linking is turned on the link will need to be created
            my $strReference = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference};

            if (defined($strReference))
            {
                # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
                if ($bHardLink)
                {
                    &log(DEBUG, "   hard-linking ${strBackupSourceFile} from ${strReference}");

                    link_create(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true);
                }
            }
            # Else copy/compress the file and generate a checksum
            else
            {
                # Copy the file from db to backup
                &log(DEBUG, "   backing up ${strBackupSourceFile}");
            
                my $lModificationTime = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time};
                file_copy(PATH_DB_ABSOLUTE, $strBackupSourceFile, PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");

                # Write the hash into the backup manifest (if not suppressed)
                if (!$bNoChecksum)
                {
                    ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} =
                        file_hash_get(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");
                }
            }
        }
    }
}

####################################################################################################################################
# ARCHIVE_LIST_GET
# 
# !!! Need to put code in here to cover pre-9.3 skipping log FF.
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
        # be consistent if the process dies
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
    
    # if no backups were found then preserve current archive logs - too scary to delete them!
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
$strCommandChecksum = config_load(\%oConfig, "command", "checksum", !$bNoChecksum);
$strCommandCompress = config_load(\%oConfig, "command", "compress", !$bNoCompression);
$strCommandDecompress = config_load(\%oConfig, "command", "decompress", !$bNoCompression);
$strCommandCopy = config_load(\%oConfig, "command", "copy", $bNoCompression);

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
    # !!! Need to think about locking here
    
    # archive-push command must have two arguments
    if (@ARGV != 2)
    {
        die "not enough arguments - show usage";
    }

    # Get the source/destination file
    my $strSourceFile = $ARGV[1];
    my $strDestinationFile = basename($strSourceFile);
    
    # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/;

    # Append the checksum (if requested)
    if ($bArchiveFile && !$bNoChecksum)
    {
        $strDestinationFile .= "-" . file_hash_get(PATH_DB_ABSOLUTE, $strSourceFile);
    }
    
    # Copy the archive file
    file_copy(PATH_DB_ABSOLUTE, $strSourceFile, PATH_BACKUP_ARCHIVE, $strDestinationFile, !$bArchiveFile);

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

        # !!! This is temporary until we can clean backup dirs
        rmtree($strBackupTmpPath) or confess &log(ERROR, "unable to delete backup.tmp");
        #if (-e $strBackupConfFile)
        #{
        #    unlink $strBackupConfFile or die &log(ERROR, "backup config ${strBackupConfFile} could not be deleted");
        #}
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
    backup(\%oBackupManifest);
           
    # Stop backup
    my $strArchiveStop = trim(execute($strCommandPsql .
        " -c \"set client_min_messages = 'warning'; copy (select pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout\" postgres"));

    ${oBackupManifest}{archive}{archive_location}{stop} = $strArchiveStop;

    &log(INFO, 'Backup archive stop: ' . $strArchiveStop);

    # After the backup has been stopped, need to 
    my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop);

    foreach my $strArchive (@stryArchive)
    {
        my $strArchivePath = dirname(path_get(PATH_BACKUP_ARCHIVE, $strArchive));
        my @stryArchiveFile = file_list_get($strArchivePath, "^${strArchive}(-[0-f]+){0,1}(\\.${strCompressExtension}){0,1}\$");
        
        if (scalar @stryArchiveFile != 1)
        {
            die &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}"); 
        }

        &log(DEBUG, "    archiving: ${strArchive} (${stryArchiveFile[0]})");

        file_copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0], PATH_BACKUP_TMP, "base/pg_xlog/${strArchive}");
    }
    
    # Save the backup conf file
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);

    # Rename the backup tmp path to complete the backup
    rename($strBackupTmpPath, "${strBackupClusterPath}/${strBackupPath}") or die &log(ERROR, "unable to ${strBackupTmpPath} rename to ${strBackupPath}"); 

    # Expire backups (!!! Need to read this from config file)
    backup_expire($strBackupClusterPath, 2, 2, "full", 2);
    
    exit 0;
}
