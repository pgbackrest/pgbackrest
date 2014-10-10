#!/usr/bin/perl
####################################################################################################################################
# pg_backrest_remote.pl - Simple Postgres Backup and Restore Remote
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;

use File::Basename;
use Getopt::Long;
use Carp;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::File;
use BackRest::Remote;
use BackRest::Exception;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant
{
    OP_NOOP        => 'noop',
    OP_EXIT        => 'exit'
};

####################################################################################################################################
# PARAM_GET - helper function that returns the param or an error if required and it does not exist
####################################################################################################################################
sub param_get
{
    my $oParamHashRef = shift;
    my $strParam = shift;
    my $bRequired = shift;

    my $strValue = ${$oParamHashRef}{$strParam};

    if (!defined($strValue) && (!defined($bRequired) || $bRequired))
    {
        confess "${strParam} must be defined";
    }

    return $strValue;
}

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Turn off logging
log_level_set(OFF, OFF);

# Create the remote object
my $oRemote = new BackRest::Remote();

# Create the file object
my $oFile = new BackRest::File
(
    undef,
    undef,
    undef,
    $oRemote
);

# Write the greeting so remote process knows who we are
$oRemote->greeting_write();

# Command string
my $strCommand = OP_NOOP;

# Loop until the exit command is received
while ($strCommand ne OP_EXIT)
{
    my %oParamHash;

    $strCommand = $oRemote->command_read(\%oParamHash);

    eval
    {
        # Copy a file to STDOUT
        if ($strCommand eq OP_FILE_COPY_OUT)
        {
            $oFile->copy(PATH_ABSOLUTE, param_get(\%oParamHash, 'source_file'),
                         PIPE_STDOUT, undef,
                         param_get(\%oParamHash, 'source_compressed'), undef);

            $oRemote->output_write();
        }
        # Copy a file from STDIN
        elsif ($strCommand eq OP_FILE_COPY_IN)
        {
            $oFile->copy(PIPE_STDIN, undef,
                         PATH_ABSOLUTE, param_get(\%oParamHash, 'destination_file'),
                         undef, param_get(\%oParamHash, 'destination_compress'),
                         undef, undef,
                         param_get(\%oParamHash, 'permission', false),
                         param_get(\%oParamHash, 'destination_path_create'));

            $oRemote->output_write();
        }
        # List files in a path
        elsif ($strCommand eq OP_FILE_LIST)
        {
            my $strOutput;

            foreach my $strFile ($oFile->list(PATH_ABSOLUTE, param_get(\%oParamHash, 'path'),
                                              param_get(\%oParamHash, 'expression', false),
                                              param_get(\%oParamHash, 'sort_order'),
                                              param_get(\%oParamHash, 'ignore_missing')))
            {
                if (defined($strOutput))
                {
                    $strOutput .= "\n";
                }

                $strOutput .= $strFile;
            }

            $oRemote->output_write($strOutput);
        }
        # Create a path
        elsif ($strCommand eq OP_FILE_PATH_CREATE)
        {
            $oFile->path_create(PATH_ABSOLUTE, param_get(\%oParamHash, 'path'), param_get(\%oParamHash, 'permission', false));
            $oRemote->output_write();
        }
        # Check if a file/path exists
        elsif ($strCommand eq OP_FILE_EXISTS)
        {
            $oRemote->output_write($oFile->exists(PATH_ABSOLUTE, param_get(\%oParamHash, 'path')) ? 'Y' : 'N');
        }
        # Copy a file locally
        elsif ($strCommand eq OP_FILE_COPY)
        {
            $oRemote->output_write(
                $oFile->copy(PATH_ABSOLUTE, param_get(\%oParamHash, 'source_file'),
                             PATH_ABSOLUTE, param_get(\%oParamHash, 'destination_file'),
                             param_get(\%oParamHash, 'source_compressed'),
                             param_get(\%oParamHash, 'destination_compress'),
                             param_get(\%oParamHash, 'ignore_missing_source', false),
                             undef,
                             param_get(\%oParamHash, 'permission', false),
                             param_get(\%oParamHash, 'destination_path_create')) ? 'Y' : 'N');
        }
        # Generate a manifest
        elsif ($strCommand eq OP_FILE_MANIFEST)
        {
            my %oManifestHash;

            $oFile->manifest(PATH_ABSOLUTE, param_get(\%oParamHash, 'path'), \%oManifestHash);

            my $strOutput = "name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination";

            foreach my $strName (sort(keys $oManifestHash{name}))
            {
                $strOutput .= "\n${strName}\t" .
                    $oManifestHash{name}{"${strName}"}{type} . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{user}) ? $oManifestHash{name}{"${strName}"}{user} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{group}) ? $oManifestHash{name}{"${strName}"}{group} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{permission}) ? $oManifestHash{name}{"${strName}"}{permission} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{modification_time}) ?
                        $oManifestHash{name}{"${strName}"}{modification_time} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{inode}) ? $oManifestHash{name}{"${strName}"}{inode} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{size}) ? $oManifestHash{name}{"${strName}"}{size} : "") . "\t" .
                    (defined($oManifestHash{name}{"${strName}"}{link_destination}) ?
                        $oManifestHash{name}{"${strName}"}{link_destination} : "");
            }

            $oRemote->output_write($strOutput);
        }
        # Continue if noop or exit
        elsif ($strCommand ne OP_NOOP && $strCommand ne OP_EXIT)
        {
            confess "invalid command: ${strCommand}";
        }
    };

    # Process errors
    if ($@)
    {
        $oRemote->error_write($@);
    }
}
