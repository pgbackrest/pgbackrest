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

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize variables
    $self->{strDbClusterPath} = $strDbClusterPath;

    if (defined($strBackupPath))
    {
        $self->{strBackupPath} = $strBackupPath;
    }
    else
    {
        $self->{strBackupPath} = 'latest';
    }

    $self->{oFile} = $oFile;

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
    if ($self->{oFile}->exists(PATH_BACKUP_CLUSTER, $self->{strBackupPath}))
    {
        # Copy the backup manifest to the db cluster path
        $self->{oFile}->copy(PATH_BACKUP_CLUSTER, $self->{strBackupPath} . '/' . FILE_MANIFEST,
                             PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

         # Load the manifest into a hash
         ini_load($self->{oFile}->path_get(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST));

         # Remove the manifest now that it is in memory
         $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);
    }
    else
    {
        confess &log(ERROR, 'backup ' . $self->{strBackupPath} . ' does not exist');
    }
}

1;
