#!/usr/bin/perl
####################################################################################################################################
# pg_backrest_command.pl - Simple Postgres Backup and Restore Command Helper
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;

use File::Basename;
use Getopt::Long;
use Carp;

use lib dirname($0) . '/..';
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_remote;

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest command
####################################################################################################################################
use constant
{
    OP_LIST        => "list",
    OP_EXISTS      => "exists",
    OP_HASH        => "hash",
    OP_REMOVE      => "remove",
    OP_MANIFEST    => "manifest",
    OP_COMPRESS    => "compress",
    OP_MOVE        => "move",
    OP_COPY_OUT    => "copy_out",
    OP_COPY_IN     => "copy_in",
    OP_PATH_CREATE => "path_create"
};

####################################################################################################################################
# Command line parameters
####################################################################################################################################
# my $bIgnoreMissing = false;          # Ignore errors due to missing file
# my $bDestinationPathCreate = false;  # Create destination path if it does not exist
# my $bCompress = false;               # Compress output
# my $bUncompress = false;             # Uncompress output
# my $strExpression = undef;           # Expression to use for filtering (undef = no filtering)
# my $strPermission = undef;           # Permission when creating directory or file (undef = default)
# my $strSort = undef;                 # Sort order (undef = forward)
#
# if (!GetOptions ("ignore-missing" => \$bIgnoreMissing,
#             "dest-path-create" => \$bDestinationPathCreate,
#             "compress" => \$bCompress,
#             "uncompress" => \$bUncompress,
#             "expression=s" => \$strExpression,
#             "permission=s" => \$strPermission,
#             "sort=s" => \$strSort))
# {
#     print(STDERR "error in command line arguments");
#     exit COMMAND_ERR_PARAM;
# }

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Turn off logging
log_level_set(OFF, OFF);

# Creat the file object
my $oFile = pg_backrest_file->new();

# Create the remote object for writing to stdout
my $oRemote = pg_backrest_remote->new();

# Write the greeting so remote process knows who we are
$oRemote->greeting_write();

# Command string
my $strCommand = '';

# Loop until the exit command is received
while ($strCommand ne 'exit')
{
    my %oParamHash;

    $strCommand = $oRemote->command_read(\%oParamHash);

    eval
    {
        # File->exists
        if ($strCommand eq OP_EXISTS)
        {
            if (!defined($oParamHash{path}))
            {
                confess "path must be defined";
            }

            $oRemote->output_write($oFile->exists(PATH_ABSOLUTE, $oParamHash{path}) ? "Y" : "N");
        }
        else
        {
            if ($strCommand ne 'noop')
            {
                confess "invalid command: ${strCommand}";
            }
        }
    };

    if ($@)
    {
        $oRemote->error_write($@);
    }
}

# # Get the operation
# my $strOperation = $ARGV[0];
#
# # Validate the operation
# if (!defined($strOperation))
# {
#     print(STDERR "operation is not defined");
#     exit COMMAND_ERR_PARAM;
# }
#
# # Make sure compress and uncompress are not both set
# if ($bCompress && $bUncompress)
# {
#     print(STDERR "compress and uncompress options cannot both be set");
#     exit COMMAND_ERR_PARAM;
# }
#
# # Create the file object
# my $oFile = pg_backrest_file->new();
#
# ####################################################################################################################################
# # LIST Command
# ####################################################################################################################################
# if ($strOperation eq OP_LIST)
# {
#     my $strPath = $ARGV[1];
#
#     if (!defined($strPath))
#     {
#         confess "path must be specified for ${strOperation} operation";
#     }
#
#     my $bFirst = true;
#
#     foreach my $strFile ($oFile->list(PATH_ABSOLUTE, $strPath, $strExpression, $strSort))
#     {
#         $bFirst ? $bFirst = false : print "\n";
#
#         print $strFile;
#     }
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # EXISTS Command
# ####################################################################################################################################
# if ($strOperation eq OP_EXISTS)
# {
#     my $strFile = $ARGV[1];
#
#     if (!defined($strFile))
#     {
#         confess "filename must be specified for ${strOperation} operation";
#     }
#
#     print $oFile->exists(PATH_ABSOLUTE, $strFile) ? "Y" : "N";
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # HASH Command
# ####################################################################################################################################
# if ($strOperation eq OP_HASH)
# {
#     my $strFile = $ARGV[1];
#
#     if (!defined($strFile))
#     {
#         confess "filename must be specified for ${strOperation} operation";
#     }
#
#     print $oFile->hash(PATH_ABSOLUTE, $strFile);
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # REMOVE Command
# ####################################################################################################################################
# if ($strOperation eq OP_REMOVE)
# {
#     my $strFile = $ARGV[1];
#
#     if (!defined($strFile))
#     {
#         confess "filename must be specified for ${strOperation} operation";
#     }
#
#     print $oFile->remove(PATH_ABSOLUTE, $strFile, undef, $bIgnoreMissing) ? "Y" : "N";
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # MANIFEST Command
# ####################################################################################################################################
# if ($strOperation eq OP_MANIFEST)
# {
#     my $strPath = $ARGV[1];
#
#     if (!defined($strPath))
#     {
#         confess "path must be specified for ${strOperation} operation";
#     }
#
#     my %oManifestHash;
#     $oFile->manifest(PATH_ABSOLUTE, $strPath, \%oManifestHash);
#
#     print "name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination";
#
#     foreach my $strName (sort(keys $oManifestHash{name}))
#     {
#         print "\n${strName}\t" .
#             $oManifestHash{name}{"${strName}"}{type} . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{user}) ? $oManifestHash{name}{"${strName}"}{user} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{group}) ? $oManifestHash{name}{"${strName}"}{group} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{permission}) ? $oManifestHash{name}{"${strName}"}{permission} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{modification_time}) ?
#                 $oManifestHash{name}{"${strName}"}{modification_time} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{inode}) ? $oManifestHash{name}{"${strName}"}{inode} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{size}) ? $oManifestHash{name}{"${strName}"}{size} : "") . "\t" .
#             (defined($oManifestHash{name}{"${strName}"}{link_destination}) ?
#                 $oManifestHash{name}{"${strName}"}{link_destination} : "");
#     }
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # COMPRESS Command
# ####################################################################################################################################
# if ($strOperation eq OP_COMPRESS)
# {
#     my $strFile = $ARGV[1];
#
#     if (!defined($strFile))
#     {
#         confess "file must be specified for ${strOperation} operation";
#     }
#
#     $oFile->compress(PATH_ABSOLUTE, $strFile);
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # MOVE Command
# ####################################################################################################################################
# if ($strOperation eq OP_MOVE)
# {
#     my $strFileSource = $ARGV[1];
#
#     if (!defined($strFileSource))
#     {
#         confess "source file source must be specified for ${strOperation} operation";
#     }
#
#     my $strFileDestination = $ARGV[2];
#
#     if (!defined($strFileDestination))
#     {
#         confess "destination file must be specified for ${strOperation} operation";
#     }
#
#     $oFile->move(PATH_ABSOLUTE, $strFileSource, PATH_ABSOLUTE, $strFileDestination, $bDestinationPathCreate);
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # COPY_IN Command
# ####################################################################################################################################
# if ($strOperation eq OP_COPY_IN)
# {
#     my $strFileSource = $ARGV[1];
#
#     # Make sure the source file is defined
#     if (!defined($strFileSource))
#     {
#         confess "source file must be specified for ${strOperation} operation";
#     }
#
#     # Variable to hold errors
#     my $strError;
#
#     # Open the source file
#     my $hIn;
#
#     if (!open($hIn, "<", ${strFileSource}))
#     {
#         $strError = $!;
#
#         unless (-e $strFileSource)
#         {
#             print(STDERR "${strFileSource} does not exist");
#             exit COMMAND_ERR_FILE_MISSING;
#         }
#     }
#     else
#     {
#         # Determine if the file is already compressed
#         my $bAlreadyCompressed = ($strFileSource =~ "^.*\.$oFile->{strCompressExtension}\$");
#
#         # Copy the file to STDOUT
#         eval
#         {
#             $oFile->pipe($hIn, *STDOUT, $bCompress && !$bAlreadyCompressed, $bUncompress && $bAlreadyCompressed);
#         };
#
#         $strError = $@;
#
#         # Close the input file
#         close($hIn);
#     }
#
#     if ($strError)
#     {
#         print(STDERR "${strFileSource} could not be read: ${strError}");
#         exit COMMAND_ERR_FILE_READ;
#     }
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # COPY_OUT Command
# ####################################################################################################################################
# if ($strOperation eq OP_COPY_OUT)
# {
#     my $strFileDestination = $ARGV[1];
#
#     # Make sure the source file is defined
#     if (!defined($strFileDestination))
#     {
#         confess "destination file must be specified for ${strOperation} operation";
#     }
#
#     # Determine if the file needs compression extension
#     if ($bCompress && $strFileDestination !~ "^.*\.$oFile->{strCompressExtension}\$")
#     {
#         $strFileDestination .= "." . $oFile->{strCompressExtension};
#     }
#
#     # Variable to hold errors
#     my $strError;
#
#     # Open the destination file
#     my $hOut;
#
#     if (!open($hOut, ">", ${strFileDestination}))
#     {
#         $strError = $!;
#     }
#     else
#     {
#         # Copy the file from STDIN
#         eval
#         {
#             $strError = $oFile->pipe(*STDIN, $hOut, $bCompress, $bUncompress);
#         };
#
#         $strError = $@;
#
#         # Close the input file
#         close($hOut);
#     }
#
#     if ($strError)
#     {
#         print(STDERR "${strFileDestination} could not be written: ${strError}");
#         exit COMMAND_ERR_FILE_READ;
#     }
#
#     exit 0;
# }
#
# ####################################################################################################################################
# # PATH_CREATE Command
# ####################################################################################################################################
# if ($strOperation eq OP_PATH_CREATE)
# {
#     my $strPath = $ARGV[1];
#
#     if (!defined($strPath))
#     {
#         confess "path must be specified for ${strOperation} operation";
#     }
#
#     $oFile->path_create(PATH_ABSOLUTE, $strPath, $strPermission);
#
#     exit 0;
# }
#
# print(STDERR "invalid operation ${strOperation}");
# exit COMMAND_ERR_PARAM;
