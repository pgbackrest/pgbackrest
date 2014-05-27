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
    my $strLockPath = dirname(abs_path($0)) . "/lock";
    my $strTestPath = dirname(abs_path($0)) . "/test";
    my $iRun = 0;
    
    log_level_set(OFF, OFF);

    # !!! NEED TO TEST WHERE LOCK PATH IS UNDEF

    # Test file_exists()
    for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    {
        my $strHost = $bRemote ? "127.0.0.1" : undef;
        my $strUser = $bRemote ? "dsteele" : undef;

        system("rm -rf lock");
        system("mkdir -p lock") == 0 or confess "Unable to create lock directory";

        my $oFile = pg_backrest_file->new
        (
            strStanza => "db",
            bNoCompression => true,
            strBackupClusterPath => ${strTestPath},
            strBackupPath => ${strTestPath},
            strBackupHost => $strHost,
            strBackupUser => $strUser,
            strLockPath => $strLockPath
        );

        # Loop through exists
        for (my $bExists = 0; $bExists <= 1; $bExists++)
        {
            # Loop through error
            for (my $bError = 0; $bError <= $bRemote ? 1 : 0; $bError++)
            {
                $iRun++;

                print "run ${iRun} - " .
                      "rmt $bRemote, exist $bExists, error $bError\n";

                # Drop the old test directory and create a new one
                system("rm -rf test");
                system("mkdir test") == 0 or confess "Unable to create test directory";

                my $strFile = "${strTestPath}/test.txt";

                if ($bExists)
                {
                    system("echo 'TESTDATA' > ${strFile}");
                }

                if ($bError)
                {
                    $strFile = "--backrest-error " . $strFile;
                }
                
                # Execute in eval in case of error
                eval
                {
                    if ($oFile->file_exists(PATH_BACKUP_ABSOLUTE, $strFile) != $bExists)
                    {
                        confess "bExists is set to $bExists, but file_exists() returned " . !$bExists;
                    }
                };
                
                if ($@)
                {
                    if (!$bError)
                    {
                        confess 'error was returned but no error generated';
                    }
                    
                    my $strError = $oFile->error_get();
                    
                    if (!defined($strError) || ($strError eq ''))
                    {
                        confess 'no error message returned';
                    }
                    
                    print "    error raised: ${strError}\n";
                    next;
                }
            }
        }
    }

    return;

    # Test copy()
    $iRun = 0;

    system("rm -rf lock");
    system("mkdir -p lock") == 0 or confess "Unable to create lock directory";

    for (my $bBackupRemote = 0; $bBackupRemote <= 1; $bBackupRemote++)
    {
        my $strBackupHost = $bBackupRemote ? "127.0.0.1" : undef;
        my $strBackupUser = $bBackupRemote ? "dsteele" : undef;

        # Loop through source compression
        for (my $bDbRemote = 0; $bDbRemote <= 1; $bDbRemote++)
        {
            my $strDbHost = $bDbRemote ? "127.0.0.1" : undef;
            my $strDbUser = $bDbRemote ? "dsteele" : undef;

            # Loop through destination compression
            for (my $bDestinationCompressed = 0; $bDestinationCompressed <= 1; $bDestinationCompressed++)
            {
                my $oFile = pg_backrest_file->new
                (
                    strStanza => "db",
                    bNoCompression => !$bDestinationCompressed,
                    strBackupClusterPath => undef,
                    strBackupPath => ${strTestPath},
                    strBackupHost => $strBackupHost,
                    strBackupUser => $strBackupUser,
                    strDbHost => $strDbHost,
                    strDbUser => $strDbUser,
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
                                          "srcpth ${strSourcePath}, bkprmt $bBackupRemote, srccmp $bSourceCompressed, " .
                                          "dstpth ${strDestinationPath}, dbrmt $bDbRemote, dstcmp $bDestinationCompressed, " .
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
                                        if ($bBackupRemote && $bDbRemote && ($strSourcePath ne $strDestinationPath))
                                        {
                                            print "    different source and remote for same path not supported\n";
                                            next;
                                        }
                                        # If the error was intentional, then also continue
                                        elsif ($bError)
                                        {
                                            my $strError = $oFile->error_get();
                                            
                                            if (!defined($strError) || ($strError eq ''))
                                            {
                                                confess 'no error message returned';
                                            }
                                            
                                            print "    error raised: ${strError}\n";
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
                                                my $strError = $oFile->error_get();
                                                
                                                if (!defined($strError) || ($strError eq ''))
                                                {
                                                    confess 'no error message returned';
                                                }
                                                
                                                print "    error returned: ${strError}\n";
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
                                        
                                        print "    true was returned\n";
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
