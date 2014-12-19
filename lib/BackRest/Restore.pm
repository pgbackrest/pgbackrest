####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package BackRest::Restore;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename;
#use File::Path qw(remove_tree);
#use Scalar::Util qw(looks_like_number);
#use Thread::Queue;

use lib dirname($0);
use BackRest::Utility;
use BackRest::Config;
use BackRest::File;
use BackRest::Db;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;              # Class name
    my $strDbClusterPath = shift;   # Database cluster path
    my $strBackupPath = shift;      # Backup to restore
    my $oFile = shift;              # Default file object
    my $bForce = shift;             # Force the restore even if files are present

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize variables
    $self->{strDbClusterPath} = $strDbClusterPath;
    $self->{oFile} = $oFile;
    $self->{bForce} = $bForce;

    # If backup path is not specified then default to latest
    if (defined($strBackupPath))
    {
        $self->{strBackupPath} = $strBackupPath;
    }
    else
    {
        $self->{strBackupPath} = PATH_LATEST;
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
    if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_POSTMASTER_PID))
    {
        confess &log(ERROR, 'unable to restore while Postgres is running');
    }

    # Make sure the backup path is valid and load the manifest
    my %oManifest;

    if ($self->{oFile}->exists(PATH_BACKUP_CLUSTER, $self->{strBackupPath}))
    {
        # Copy the backup manifest to the db cluster path
        $self->{oFile}->copy(PATH_BACKUP_CLUSTER, $self->{strBackupPath} . '/' . FILE_MANIFEST,
                             PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

         # Load the manifest into a hash
         ini_load($self->{oFile}->path_get(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST), \%oManifest);

         # Remove the manifest now that it is in memory
         $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);
    }
    else
    {
        confess &log(ERROR, 'backup ' . $self->{strBackupPath} . ' does not exist');
    }

    # Check each restore directory in the manifest and make sure that it exists and is empty.
    # The --force option can be used to override the empty requirement.
    foreach my $strPathKey (sort(keys $oManifest{'backup:path'}))
    {
        my $strPath = $oManifest{'backup:path'}{$strPathKey};

        if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE,  $strPath))
        {
            confess &log(ERROR, "required db path '${strPath}' does not exist");
        }

        # Load path manifest so it can be compared to delete files/paths/links that are not in the backup
        my %oPathManifest;
        $self->{oFile}->manifest(PATH_DB_ABSOLUTE, $strPath, \%oPathManifest);

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

            # Check to see if the file/path/link exists in the manifest, if not remove it
            if (!defined($oManifest{"${strPathKey}:${strType}"}{$strName}))
            {
                my $strDelete = "${strPath}/${strName}";

                # If a path then remove it, all the files should have already been deleted since we are going in reverse order
                if ($strType eq 'path')
                {
                    &log(DEBUG, "remove path ${strDelete}");
                    rmdir($strDelete) or confess &log(ERROR, "unable to delete path ${strDelete}, is it empty?");
                }
                # Else delete a file
                else
                {
                    &log(DEBUG, "remove file ${strDelete}");
                    unlink($strDelete) or confess &log(ERROR, "unable to delete file ${strDelete}");
                }
            }
        }
    }
}

1;
