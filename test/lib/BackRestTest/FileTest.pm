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

use Carp;
use File::Basename;
 use Cwd 'abs_path';

use lib dirname($0) . "/..";
use pg_backrest_file;
use pg_backrest_utility;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestFile);

sub BackRestTestFile
{
    my $strTestPath = dirname(abs_path($0)) . "/test";
    my $iRun = 0;
    
    log_level_set(OFF, OFF);

    # Test copy()
    for (my $bSourceRemote = 0; $bSourceRemote <= 1; $bSourceRemote++)
    {
        my $strSourceHost = $bSourceRemote ? "127.0.0.1" : undef;
        my $strSourceUser = $bSourceRemote ? "dsteele" : undef;

        # Loop through source compression
        for (my $bDestinationRemote = 0; $bDestinationRemote <= 1; $bDestinationRemote++)
        {
            my $strDestinationHost = $bDestinationRemote ? "127.0.0.1" : undef;
            my $strDestinationUser = $bDestinationRemote ? "dsteele" : undef;

            # Loop through destination compression
            for (my $bDestinationCompressed = 0; $bDestinationCompressed <= 1; $bDestinationCompressed++)
            {
                my $oFile = pg_backrest_file->new
                (
                    strStanza => "db",
                    bNoCompression => !$bDestinationCompressed,
                    strBackupClusterPath => undef,
                    strBackupPath => ${strTestPath},
                    strBackupHost => $strSourceHost,
                    strBackupUser => $strSourceUser,
                    strDbHost => $strDestinationHost,
                    strDbUser => $strDestinationUser,
                    strCommandCompress => "gzip --stdout %file%",
                    strCommandDecompress => "gzip -dc %file%",
                    strLockPath => dirname($0) . "/test/lock"
                );

                for (my $bSourceCompressed = 0; $bSourceCompressed <= 1; $bSourceCompressed++)
                {
                    for (my $bSourcePathType = 0; $bSourcePathType <= 1; $bSourcePathType++)
                    {
                        my $strSourcePath = $bSourcePathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;

                        for (my $bDestinationPathType = 0; $bDestinationPathType <= 1; $bDestinationPathType++)
                        {
                            my $strDestinationPath = $bDestinationPathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;

                            for (my $bError = 0; $bError <= 1; $bError++)
                            {
                                for (my $bConfessError = 0; $bConfessError <= 1; $bConfessError++)
                                {
                                    $iRun++;
                                
                                    print "run ${iRun} - " .
                                          "srcpth ${strSourcePath}, srcrmt $bSourceRemote, srccmp $bSourceCompressed, " .
                                          "dstpth ${strDestinationPath}, dstrmt $bDestinationRemote, dstcmp $bDestinationCompressed, " .
                                          "error $bError, confess_error $bConfessError\n";

                                    # Drop the old test directory and create a new one
                                    system("rm -rf test");
                                    system("mkdir -p test/lock") == 0 or confess "Unable to create the test directory";

                                    my $strSourceFile = "${strTestPath}/test-source.txt";
                                    my $strDestinationFile = "${strTestPath}/test-destination.txt";

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

                                    if ($bError)
                                    {
                                        $strSourceFile .= ".error";
                                    }

                                    # Run file copy in an eval block because some errors are expected
                                    my $bReturn;

                                    eval
                                    {
                                        $bReturn = $oFile->file_copy($strSourcePath, $strSourceFile,
                                                                     $strDestinationPath, $strDestinationFile,
                                                                     undef, undef, undef, undef,
                                                                     $bConfessError);
                                    };

                                    # Check for errors after copy
                                    if ($@)
                                    {
                                        # Different remote and destination with different path types should error
                                        if ($bSourceRemote && $bDestinationRemote)
                                        {
                                            next;
                                        }
                                        # If the error was intentional, then also continue
                                        elsif ($bError)
                                        {
                                            next;
                                        }
                                        # Else this is an unexpected error
                                        else
                                        {
                                            confess $@;
                                        }
                                    }
                                    elsif ($bError)
                                    {
                                        if ($bConfessError)
                                        {
                                            confess "Value was returned instead of exception thrown when confess error is true";
                                        }
                                        else
                                        {
                                            if ($bReturn)
                                            {
                                                confess "true was returned when an error was generated";
                                            }
                                            else
                                            {
                                                next;
                                            }
                                        }
                                    
                                    }
                                    else
                                    {
                                        if (!$bReturn)
                                        {
                                            confess "error was returned when no error generated";
                                        }
                                    }
                                
                                    # Check for errors after copy
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
                    }
                }
            }
        }
    }
}
