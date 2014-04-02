#!/usr/bin/perl
####################################################################################################################################
# File.pl - Unit Tests for BackRest::File
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;

use File::Basename;
use Carp;

use lib dirname($0) . "/..";
use pg_backrest_file;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestFile);

sub BackRestTestFile
{
    # Test copy()
    for (my $bSourceCompressed = 0; $bSourceCompressed <= 1; $bSourceCompressed++)
    {
        # Loop through destination compression
        for (my $bDestinationCompressed = 0; $bDestinationCompressed <= 1; $bDestinationCompressed++)
        {
            print "srccmp $bSourceCompressed, dstcmp $bDestinationCompressed\n";

            # Drop the old test directory and create a new one
            system("rm -rf test");
            system("mkdir test") == 0 or confess "Unable to create the test directory";
#            system("mkdir test/backup") == 0 or confess "Unable to create the test/backup directory";
#            system("mkdir test/db") == 0 or confess "Unable to create the test/db directory";

            my $strSourceFile = "test/test-source.txt";
            my $strDestinationFile = "test/test-destination.txt";

            # Create the compressed or uncompressed test file
            if ($bSourceCompressed)
            {
                $strSourceFile .= ".gz";
                system("echo 'TESTDATA' | gzip > ${strSourceFile}");
            }
            else
            {
                system("echo 'TESTDATA' > ${strSourceFile}");
            }

            # Create the file object based on current values
            my $oFile = pg_backrest_file->new
            (
                strStanza => "db",
                bNoCompression => !$bDestinationCompressed,
                strBackupPath => "test/backup",
                strBackupUser => undef,
                strBackupHost => undef,
                strBackupPath => "test/backup",
                strBackupClusterPath => "test/db",
                strCommandCompress => "gzip --stdout %file%",
                strCommandDecompress => "gzip -dc %file%"
            );

            $oFile->file_copy(PATH_DB_ABSOLUTE, $strSourceFile, PATH_DB_ABSOLUTE, $strDestinationFile);

            if ($bDestinationCompressed)
            {
                $strDestinationFile .= ".gz";
            }

            unless (-e $strDestinationFile)
            {
                confess "could not find destination file ${strDestinationFile}";
            }
        }
    }
}
