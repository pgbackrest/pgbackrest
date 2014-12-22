####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package BackRest::Restore;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings;
use Carp;

use File::Basename;
#use File::Path qw(remove_tree);
#use Scalar::Util qw(looks_like_number);
#use Thread::Queue;

use lib dirname($0);
use BackRest::Utility;
use BackRest::ThreadGroup;
use BackRest::Config;
use BackRest::File;

####################################################################################################################################
# Global variables
####################################################################################################################################
# !!! Passing this to the threads does not work if it is created in the object - what's up with that?
my @oyThreadQueue;
my %oManifest;
my $oFile;
my $strBackupPath;
my $bSourceCompressed = false;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;              # Class name
    my $strDbClusterPath = shift;   # Database cluster path
    my $strBackupPathParam = shift;      # Backup to restore
    my $oFileParam = shift;         # Default file object
    my $iThreadTotal = shift;       # Total threads to run for restore
    my $bForce = shift;             # Force the restore even if files are present

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize variables
    $self->{strDbClusterPath} = $strDbClusterPath;
    $oFile = $oFileParam;
    $self->{thread_total} = $iThreadTotal;
    $self->{bForce} = $bForce;

    # If backup path is not specified then default to latest
    if (defined($strBackupPathParam))
    {
        $strBackupPath = $strBackupPathParam;
    }
    else
    {
        $strBackupPath = PATH_LATEST;
    }

    return $self;
}

####################################################################################################################################
# RESTORE
####################################################################################################################################
sub restore
{
    my $self = shift;       # Class hash

    # Make sure that Postgres is not running
    if ($oFile->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_POSTMASTER_PID))
    {
        confess &log(ERROR, 'unable to restore while Postgres is running');
    }

    # Make sure the backup path is valid and load the manifest
    if ($oFile->exists(PATH_BACKUP_CLUSTER, $strBackupPath))
    {
        # Copy the backup manifest to the db cluster path
        $oFile->copy(PATH_BACKUP_CLUSTER, $strBackupPath . '/' . FILE_MANIFEST,
                             PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

         # Load the manifest into a hash
         ini_load($oFile->path_get(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST), \%oManifest);

         # Remove the manifest now that it is in memory
         $oFile->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);
    }
    else
    {
        confess &log(ERROR, 'backup ' . $strBackupPath . ' does not exist');
    }

    # Set source compression
    if ($oManifest{'backup:option'}{compress} eq 'y')
    {
        $bSourceCompressed = true;
    }

    # Declare thread queues that will be used to process files
    my $iThreadQueueTotal = 0;

    # Check each restore directory in the manifest and make sure that it exists and is empty.
    # The --force option can be used to override the empty requirement.
    foreach my $strPathKey (sort(keys $oManifest{'backup:path'}))
    {
        my $strPath = $oManifest{'backup:path'}{$strPathKey};

        &log(INFO, "processing db path ${strPath}");

        if (!$oFile->exists(PATH_DB_ABSOLUTE,  $strPath))
        {
            confess &log(ERROR, "required db path '${strPath}' does not exist");
        }

        # Load path manifest so it can be compared to delete files/paths/links that are not in the backup
        my %oPathManifest;
        $oFile->manifest(PATH_DB_ABSOLUTE, $strPath, \%oPathManifest);

        foreach my $strName (sort {$b cmp $a} (keys $oPathManifest{name}))
        {
            # Skip the root path
            if ($strName eq '.')
            {
                next;
            }

            # If force was not specified then error if any file is found
            if (!$self->{bForce})
            {
                confess &log(ERROR, "db path '${strPath}' contains files");
            }

            my $strFile = "${strPath}/${strName}";

            # Determine the file/path/link type
            my $strType = 'file';

            if ($oPathManifest{name}{$strName}{type} eq 'd')
            {
                $strType = 'path';
            }
            elsif ($oPathManifest{name}{$strName}{type} eq 'l')
            {
                $strType = 'link';
            }

            # Check to see if the file/path/link exists in the manifest
            if (defined($oManifest{"${strPathKey}:${strType}"}{$strName}))
            {
                my $strMode = $oManifest{"${strPathKey}:${strType}"}{$strName}{permission};

                # If file/path mode does not match, fix it
                if ($strType ne 'link' && $strMode ne $oPathManifest{name}{$strName}{permission})
                {
                    &log(DEBUG, "setting ${strFile} mode to ${strMode}");

                    chmod(oct($strMode), $strFile)
                        or confess 'unable to set mode ${strMode} for ${strFile}';
                }

                my $strUser = $oManifest{"${strPathKey}:${strType}"}{$strName}{user};
                my $strGroup = $oManifest{"${strPathKey}:${strType}"}{$strName}{group};

                # If ownership does not match, fix it
                if ($strUser ne $oPathManifest{name}{$strName}{user} ||
                    $strGroup ne $oPathManifest{name}{$strName}{group})
                {
                    &log(DEBUG, "setting ${strFile} ownership to ${strUser}:${strGroup}");

                    # !!! Need to decide if it makes sense to set the user to anything other than the db owner
                }

                # If a link does not have the same destination, then delete it (it will be recreated later)
                if ($strType eq 'link' && $oManifest{"${strPathKey}:${strType}"}{$strName}{link_destination} ne
                    $oPathManifest{name}{$strName}{link_destination})
                {
                    &log(DEBUG, "removing link ${strFile} - destination changed");
                    unlink($strFile) or confess &log(ERROR, "unable to delete file ${strFile}");
                }
            }
            # If it does not then remove it
            else
            {
                # If a path then remove it, all the files should have already been deleted since we are going in reverse order
                if ($strType eq 'path')
                {
                    &log(DEBUG, "removing path ${strFile}");
                    rmdir($strFile) or confess &log(ERROR, "unable to delete path ${strFile}, is it empty?");
                }
                # Else delete a file/link
                else
                {
                    &log(DEBUG, "removing file ${strFile}");
                    unlink($strFile) or confess &log(ERROR, "unable to delete file ${strFile}");
                }
            }
        }

        # Create all paths in the manifest that do not already exist
        foreach my $strName (sort (keys $oManifest{"${strPathKey}:path"}))
        {
            # Skip the root path
            if ($strName eq '.')
            {
                next;
            }

            # Create the Path
            $oFile->path_create(PATH_DB_ABSOLUTE, "${strPath}/${strName}",
                                        $oManifest{"${strPathKey}:path"}{$strName}{permission});
        }

        # Create all links in the manifest that do not already exist
        if (defined($oManifest{"${strPathKey}:link"}))
        {
            foreach my $strName (sort (keys $oManifest{"${strPathKey}:link"}))
            {
                if (!$oFile->exists(PATH_DB_ABSOLUTE, "${strPath}/${strName}"))
                {
                    $oFile->link_create(PATH_DB_ABSOLUTE, $oManifest{"${strPathKey}:link"}{$strName}{link_destination},
                                                PATH_DB_ABSOLUTE, "${strPath}/${strName}");
                }
            }
        }

        # Assign the files in each path to a thread queue
        if (defined($oManifest{"${strPathKey}:file"}))
        {
            $oyThreadQueue[$iThreadQueueTotal] = Thread::Queue->new();

            foreach my $strName (sort (keys $oManifest{"${strPathKey}:file"}))
            {
                $oyThreadQueue[$iThreadQueueTotal]->enqueue("${strPathKey}|${strName}");
            }

            $oyThreadQueue[$iThreadQueueTotal]->end();
            $iThreadQueueTotal++;
        }
    }

    # Create threads to process the thread queues
    my $oThreadGroup = new BackRest::ThreadGroup();

    for (my $iThreadIdx = 0; $iThreadIdx < $self->{thread_total}; $iThreadIdx++)
    {
#        &log(INFO, "creating thread $iThreadIdx");
        $oThreadGroup->add(threads->create(\&restore_thread, $iThreadIdx, $self->{thread_total}));
    }

    $oThreadGroup->complete();
}

sub restore_thread
{
    my @args = @_;

    my $iThreadIdx = $args[0];          # Defines the index of this thread
    my $iThreadTotal = $args[1];        # Total threads running

    my $iDirection = $iThreadIdx % 2 == 0 ? 1 : -1; # Size of files currently copied by this thread
    my $oFileThread = $oFile->clone($iThreadIdx);   # Thread local file object

    # Initialize the starting and current queue index
    my $iQueueStartIdx = int((@oyThreadQueue / $iThreadTotal) * $iThreadIdx);
    my $iQueueIdx = $iQueueStartIdx;

    # When a KILL signal is received, immediately abort
    $SIG{'KILL'} = sub {threads->exit();};

    # Loop through all the queues to restore files
    do
    {
        while (my $strMessage = $oyThreadQueue[$iQueueIdx]->dequeue())
        {
            my $strSourcePath = (split(/\|/, $strMessage))[0];                   # Source path from backup
            my $strSection = "${strSourcePath}:file";                            # Backup section with file info
            my $strDestinationPath = $oManifest{'backup:path'}{$strSourcePath};  # Destination path stored in manifest
            $strSourcePath =~ s/\:/\//g;                                         # Replace : with / in source path
            my $strName = (split(/\|/, $strMessage))[1];                         # Name of file to be restored

            # Generate destination file name
            my $strDestinationFile = $oFileThread->path_get(PATH_DB_ABSOLUTE, "${strDestinationPath}/${strName}");

            # If checksum is set the destination file already exists, try a checksum before copying
            my $strChecksum = $oManifest{$strSection}{$strName}{checksum};

            if ($oFileThread->exists(PATH_DB_ABSOLUTE, $strDestinationFile))
            {
                if (defined($strChecksum) && $oFileThread->hash(PATH_DB_ABSOLUTE, $strDestinationFile) eq $strChecksum)
                {
                    &log(DEBUG, "${strDestinationFile} exists and matches backup checksum ${strChecksum}");
                    next;
                }

                $oFileThread->remove(PATH_DB_ABSOLUTE, $strDestinationFile);
            }

            # Copy the file from the backup to the database
            $oFileThread->copy(PATH_BACKUP_CLUSTER, "${strBackupPath}/${strSourcePath}/${strName}" .
                               ($bSourceCompressed ? '.' . $oFileThread->{strCompressExtension} : ''),
                               PATH_DB_ABSOLUTE, $strDestinationFile,
                               $bSourceCompressed,   # Source is compressed based on backup settings
                               false, false,
                               $oManifest{$strSection}{$strName}{modification_time},
                               $oManifest{$strSection}{$strName}{permission});
        }

        $iQueueIdx += $iDirection;

        if ($iQueueIdx < 0)
        {
            $iQueueIdx = @oyThreadQueue - 1;
        }
        elsif ($iQueueIdx >= @oyThreadQueue)
        {
            $iQueueIdx = 0;
        }
    }
    while ($iQueueIdx != $iQueueStartIdx);

    &log(DEBUG, "thread ${iThreadIdx} exiting");
}

1;
