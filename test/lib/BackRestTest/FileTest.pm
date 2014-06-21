#!/usr/bin/perl
####################################################################################################################################
# FileTest.pl - Unit Tests for BackRest::File
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
use File::stat;
use Fcntl ':mode';
use Scalar::Util 'blessed';

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;
use BackRest::Remote;

use Exporter qw(import);
our @EXPORT = qw(BackRestFileTest);

my $strTestPath;
my $strHost;
my $strUserBackRest;

####################################################################################################################################
# BackRestFileTestSetup
####################################################################################################################################
sub BackRestFileTestSetup
{
    my $bPrivate = shift;
    my $bDropOnly = shift;
    
    # Remove the backrest private directory
    if (-e "${strTestPath}/private")
    {
        system("ssh ${strUserBackRest}\@${strHost} 'rm -rf ${strTestPath}/private'");
    }
    
    # Remove the test directory
    system("rm -rf ${strTestPath}") == 0 or die 'unable to drop test path';
    
    if (defined($bDropOnly) || !$bDropOnly)
    {
        # Create the test directory
        mkdir($strTestPath, oct("0770")) or confess "Unable to create test directory";
        
        # Create the backrest private directory
        if (defined($bPrivate) && $bPrivate)
        {
            system("ssh backrest\@${strHost} 'mkdir -m 700 ${strTestPath}/private'") == 0 or die 'unable to create test/private path';
        }
    }
}

####################################################################################################################################
# BackRestFileTest
####################################################################################################################################
sub BackRestFileTest
{
    my $strTest = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup test paths
    $strTestPath = dirname(abs_path($0)) . "/test";
    my $iRun;

    my $strStanza = "db";
    my $strCommand = "/Users/dsteele/pg_backrest/bin/pg_backrest_remote.pl";
    $strHost = "127.0.0.1";
    my $strUser = getpwuid($<);
    my $strGroup = getgrgid($();
    $strUserBackRest = 'backrest';

    # Print test parameters
    &log(INFO, "Testing with test_path = ${strTestPath}, host = ${strHost}, user = ${strUser}, group = ${strGroup}");

    &log(INFO, "FILE MODULE ********************************************************************");

    system("ssh backrest\@${strHost} 'rm -rf ${strTestPath}/private'");

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = BackRest::Remote->new
    (
        strHost => $strHost,
        strUser => $strUser,
        strCommand => $strCommand,
    );

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test path_create()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'path_create')
    {
        $iRun = 0;

        &log(INFO, "Test File->path_create()\n");

        # Loop through local/remote
        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            # Loop through exists (does the paren path exist?)
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
            # Loop through exists (does the paren path exist?)
            for (my $bError = 0; $bError <= 1; $bError++)
            {
            # Loop through permission (permission will be set on true)
            for (my $bPermission = 0; $bPermission <= $bExists; $bPermission++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote ${bRemote}, exists ${bExists}, error ${bError}, permission ${bPermission}");

                # Setup test directory
                BackRestFileTestSetup($bError);

                my $strPath = "${strTestPath}/path";
                my $strPermission;

                # If permission then set one (other than the default)
                if ($bPermission)
                {
                    $strPermission = "0700";

                    # # Make sure that we are not testing with the default permission
                    # if ($strPermission eq $oFile->{strDefaultPathPermission})
                    # {
                    #     confess 'cannot set test permission ${strPermission} equal to default permission' .
                    #             $oFile->{strDefaultPathPermission};
                    # }
                }

                # If not exists then set the path to something bogus
                if ($bError)
                {
                    $strPath = "${strTestPath}/private/path";
                }
                elsif (!$bExists)
                {
                    $strPath = "${strTestPath}/error/path";
                }

                # Execute in eval to catch errors
                my $bErrorExpected = !$bExists || $bError;

                eval
                {
                    $oFile->path_create(PATH_BACKUP_ABSOLUTE, $strPath, $strPermission);
                };

                # Check for errors
                if ($@)
                {
                    # Ignore errors if the path did not exist
                    if ($bErrorExpected)
                    {
                        next;
                    }

                    confess "error raised: " . $@ . "\n";
                }

                if ($bErrorExpected)
                {
                    confess 'error was expected';
                }

                # Make sure the path was actually created
                unless (-e $strPath)
                {
                    confess "path was not created";
                }

                # Check that the permissions were set correctly
                my $oStat = lstat($strPath);

                if (!defined($oStat))
                {
                    confess "unable to stat ${strPath}";
                }

                if ($bPermission)
                {
                    if ($strPermission ne sprintf("%04o", S_IMODE($oStat->mode)))
                    {
                        confess "permissions were not set to {$strPermission}";
                    }
                }
            }
            }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test move()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'move')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->move()\n");

        for (my $bRemote = 0; $bRemote <= 0; $bRemote++)
        {
            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            # Loop through source exists
            for (my $bSourceExists = 0; $bSourceExists <= 1; $bSourceExists++)
            {
            # Loop through source errors
            for (my $bSourceError = 0; $bSourceError <= 1; $bSourceError++)
            {
            # Loop through destination exists
            for (my $bDestinationExists = 0; $bDestinationExists <= 1; $bDestinationExists++)
            {
            # Loop through source errors
            for (my $bDestinationError = 0; $bDestinationError <= 1; $bDestinationError++)
            {
            # Loop through create
            for (my $bCreate = 0; $bCreate <= $bDestinationExists; $bCreate++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote" .
                           ", src_exists $bSourceExists, src_error $bSourceError" .
                           ", dst_exists $bDestinationExists, dst_error $bDestinationError, dst_create $bCreate");

                # Setup test directory
                BackRestFileTestSetup($bSourceError || $bDestinationError);

                my $strSourceFile = "${strTestPath}/test.txt";
                my $strDestinationFile = "${strTestPath}/test-dest.txt";

                if ($bSourceError)
                {
                    $strSourceFile = "${strTestPath}/private/test.txt";
                }
                elsif ($bSourceExists)
                {
                    system("echo 'TESTDATA' > ${strSourceFile}");
                }

                if ($bDestinationError)
                {
                    $strSourceFile = "${strTestPath}/private/test.txt";
                }
                elsif (!$bDestinationExists)
                {
                    $strDestinationFile = "${strTestPath}/sub/test-dest.txt";
                }

                # Execute in eval in case of error
                eval
                {
                    $oFile->move(PATH_BACKUP_ABSOLUTE, $strSourceFile, PATH_BACKUP_ABSOLUTE, $strDestinationFile, $bCreate);
                };

                if ($@)
                {
                    if (!$bSourceExists || (!$bDestinationExists && !$bCreate) || $bSourceError || $bDestinationError)
                    {
                        next;
                    }

                    confess "error raised: " . $@ . "\n";
                }

                if (!$bSourceExists || (!$bDestinationExists && !$bCreate) || $bSourceError || $bDestinationError)
                {
                    confess "error should have been raised";
                }

                unless (-e $strDestinationFile)
                {
                    confess "file was not moved";
                }
            }
            }
            }
            }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test compress()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'compress')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->compress()\n");

        for (my $bRemote = 0; $bRemote <= 0; $bRemote++)
        {
            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            # Loop through exists
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
            for (my $bError = 0; $bError <= 1; $bError++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote, exists $bExists, error $bError");

                # Setup test directory
                BackRestFileTestSetup($bError);

                my $strFile = "${strTestPath}/test.txt";
                my $strSourceHash;

                if ($bError)
                {
                    $strFile = "${strTestPath}/private/test.txt";
                }
                elsif ($bExists)
                {
                    system("echo 'TESTDATA' > ${strFile}");
                    $strSourceHash = $oFile->hash(PATH_BACKUP_ABSOLUTE, $strFile);
                }

                # Execute in eval in case of error
                eval
                {
                    $oFile->compress(PATH_BACKUP_ABSOLUTE, $strFile);
                };

                if ($@)
                {
                    if (!$bExists || $bError)
                    {
                        next;
                    }
                    
                    confess "error raised: " . $@ . "\n";
                }
                
                if (!$bExists || $bError)
                {
                    confess "expected error";
                }

                my $strDestinationFile = $strFile . ".gz";

                if (-e $strFile)
                {
                    confess "source file still exists";
                }

                unless (-e $strDestinationFile)
                {
                    confess "file was not compressed";
                }
                
                system("gzip -d ${strDestinationFile}") == 0 or die "could not decompress ${strDestinationFile}";
        
                my $strDestinationHash = $oFile->hash(PATH_BACKUP_ABSOLUTE, $strFile);
                
                if ($strSourceHash ne $strDestinationHash)
                {
                    confess "source ${strSourceHash} and destination ${strDestinationHash} file hashes do not match";
                }
            }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test manifest()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'manifest')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->manifest()\n");

        my $strManifestCompare =
            ".,d,${strUser},${strGroup},0770,,,,\n" .
            "sub1,d,${strUser},${strGroup},0750,,,,\n" .
            "sub1/sub2,d,${strUser},${strGroup},0750,,,,\n" .
            "sub1/sub2/test,l,${strUser},${strGroup},,,,,../..\n" .
            "sub1/sub2/test-hardlink.txt,f,${strUser},${strGroup},1640,1111111111,0,9,\n" .
            "sub1/sub2/test-sub2.txt,f,${strUser},${strGroup},0666,1111111113,0,11,\n" .
            "sub1/test,l,${strUser},${strGroup},,,,,..\n" .
            "sub1/test-hardlink.txt,f,${strUser},${strGroup},1640,1111111111,0,9,\n" .
            "sub1/test-sub1.txt,f,${strUser},${strGroup},0646,1111111112,0,10,\n" .
            "test.txt,f,${strUser},${strGroup},1640,1111111111,0,9,";

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            for (my $bError = 0; $bError <= 1; $bError++)
            {
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote, error $bError, exists $bExists");

                # Setup test directory
                BackRestFileTestSetup($bError);

                # Setup test data
                system("mkdir -m 750 ${strTestPath}/sub1") == 0 or confess "Unable to create test directory";
                system("mkdir -m 750 ${strTestPath}/sub1/sub2") == 0 or confess "Unable to create test directory";

                system("echo 'TESTDATA' > ${strTestPath}/test.txt");
                utime(1111111111, 1111111111, "${strTestPath}/test.txt");
                system("chmod 1640 ${strTestPath}/test.txt");

                system("echo 'TESTDATA_' > ${strTestPath}/sub1/test-sub1.txt");
                utime(1111111112, 1111111112, "${strTestPath}/sub1/test-sub1.txt");
                system("chmod 0640 ${strTestPath}/sub1/test-sub1.txt");

                system("echo 'TESTDATA__' > ${strTestPath}/sub1/sub2/test-sub2.txt");
                utime(1111111113, 1111111113, "${strTestPath}/sub1/sub2/test-sub2.txt");
                system("chmod 0646 ${strTestPath}/sub1/test-sub1.txt");

                system("ln ${strTestPath}/test.txt ${strTestPath}/sub1/test-hardlink.txt");
                system("ln ${strTestPath}/test.txt ${strTestPath}/sub1/sub2/test-hardlink.txt");

                system("ln -s .. ${strTestPath}/sub1/test");
                system("chmod 0700 ${strTestPath}/sub1/test");
                system("ln -s ../.. ${strTestPath}/sub1/sub2/test");
                system("chmod 0750 ${strTestPath}/sub1/sub2/test");

                system("chmod 0770 ${strTestPath}");

                # Create path
                my $strPath = $strTestPath;

                if ($bError)
                {
                    $strPath = $strTestPath . "/private";
                }
                elsif (!$bExists)
                {
                    $strPath = $strTestPath . "/error";
                }

                # Execute in eval in case of error
                my %oManifestHash;
                my $bErrorExpected = !$bExists || $bError || $bRemote;

                eval
                {
                    $oFile->manifest(PATH_BACKUP_ABSOLUTE, $strPath, \%oManifestHash);
                };

                # Check for an error
                if ($@)
                {
                    if ($bErrorExpected)
                    {
                        next;
                    }
                    
                    confess "error raised: " . $@ . "\n";
                }
                
                # Check for an expected error
                if ($bErrorExpected)
                {
                    confess 'error was expected';
                }

                my $strManifest;

                # Validate the manifest
                foreach my $strName (sort(keys $oManifestHash{name}))
                {
                    if (!defined($strManifest))
                    {
                        $strManifest = "";
                    }
                    else
                    {
                        $strManifest .= "\n";
                    }

                    if (defined($oManifestHash{name}{"${strName}"}{inode}))
                    {
                        $oManifestHash{name}{"${strName}"}{inode} = 0;
                    }

                    $strManifest .=
                        "${strName}," .
                        $oManifestHash{name}{"${strName}"}{type} . "," .
                        (defined($oManifestHash{name}{"${strName}"}{user}) ?
                            $oManifestHash{name}{"${strName}"}{user} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{group}) ?
                            $oManifestHash{name}{"${strName}"}{group} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{permission}) ?
                            $oManifestHash{name}{"${strName}"}{permission} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{modification_time}) ?
                            $oManifestHash{name}{"${strName}"}{modification_time} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{inode}) ?
                            $oManifestHash{name}{"${strName}"}{inode} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{size}) ?
                            $oManifestHash{name}{"${strName}"}{size} : "") . "," .
                        (defined($oManifestHash{name}{"${strName}"}{link_destination}) ?
                            $oManifestHash{name}{"${strName}"}{link_destination} : "");
                }

                if ($strManifest ne $strManifestCompare)
                {
                    confess "manifest is not equal:\n\n${strManifest}\n\ncompare:\n\n${strManifestCompare}\n\n";
                }
            }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test list()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'list')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->list()\n");

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            # Loop through exists
            for (my $bSort = 0; $bSort <= 1; $bSort++)
            {
                my $strSort = $bSort ? undef : "reverse";

                # Loop through expression
                for (my $iExpression = 0; $iExpression <= 2; $iExpression++)
                {
                    my $strExpression;

                    # Expression tha returns results
                    if ($iExpression == 1)
                    {
                        $strExpression = "^test2\\..*\$";
                    }
                    # Expression that does not return results
                    elsif ($iExpression == 2)
                    {
                        $strExpression = "^du\$";
                    }

                    # Loop through exists
                    for (my $bExists = 0; $bExists <= 1; $bExists++)
                    {
                    
                    # Loop through error
                    for (my $bError = 0; $bError <= 1; $bError++)
                    {
                        $iRun++;

                        &log(INFO, "run ${iRun} - " .
                                   "remote $bRemote, error $bError, exists $bExists, " .
                                   "expression " . (defined($strExpression) ? $strExpression : "[undef]") . ", " .
                                   "sort " . (defined($strSort) ? $strSort : "[undef]"));

                        # Setup test directory
                        BackRestFileTestSetup($bError);

                        my $strPath = "${strTestPath}";

                        if ($bError)
                        {
                            $strPath = "${strTestPath}/private";
                        }
                        elsif (!$bExists)
                        {
                            $strPath = "${strTestPath}/error";
                        }
                        else
                        {
                            system("echo 'TESTDATA' > ${strPath}/test.txt");
                            system("echo 'TESTDATA2' > ${strPath}/test2.txt");
                        }

                        my @stryFileCompare = split(/\n/, "test.txt\ntest2.txt");

                        # Execute in eval in case of error
                        my @stryFileList;
                        my $bErrorExpected = !$bExists || $bError;

                        eval
                        {
                            @stryFileList = $oFile->list(PATH_BACKUP_ABSOLUTE, $strPath, $strExpression, $strSort);
                        };

                        if ($@)
                        {
                            if ($bErrorExpected)
                            {
                                next;
                            }

                            confess "error raised: " . $@ . "\n";
                        }
                        
                        if ($bErrorExpected)
                        {
                            confess 'error was expected';
                        }

                        # Validate the list
                        if (defined($strExpression))
                        {
                            @stryFileCompare = grep(/$strExpression/i, @stryFileCompare);
                        }

                        if (defined($strSort))
                        {
                            @stryFileCompare = sort {$b cmp $a} @stryFileCompare;
                        }

                        my $strFileList = sprintf("@stryFileList");
                        my $strFileCompare = sprintf("@stryFileCompare");

                        if ($strFileList ne $strFileCompare)
                        {
                            confess "list (${strFileList})[" . @stryFileList .
                                    "] does not match compare (${strFileCompare})[" . @stryFileCompare . "]";
                        }
                    }
                    }
                }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test remove()
    #-------------------------------------------------------------------------------------------------------------------------------
    # if ($strTest eq 'all' || $strTest eq 'remove')
    # {
    #     $iRun = 0;
    #
    #     &log(INFO, "--------------------------------------------------------------------------------");
    #     &log(INFO, "Test File->remove()\n");
    #
    #     for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    #     {
    #         my $strHost = $bRemote ? "127.0.0.1" : undef;
    #         my $strUser = $bRemote ? "dsteele" : undef;
    #
    #         my $oFile = BackRest::File->new
    #         (
    #             strStanza => $strStanza,
    #             bNoCompression => true,
    #             strCommand => ${strCommand},
    #             strBackupClusterPath => ${strTestPath},
    #             strBackupPath => ${strTestPath},
    #             strBackupHost => $bRemote ? $strHost : undef,
    #             strBackupUser => $bRemote ? $strUser : undef
    #         );
    #
    #         # Loop through exists
    #         for (my $bExists = 0; $bExists <= 1; $bExists++)
    #         {
    #             # Loop through temp
    #             for (my $bTemp = 0; $bTemp <= 1; $bTemp++)
    #             {
    #                 # Loop through ignore missing
    #                 for (my $bIgnoreMissing = 0; $bIgnoreMissing <= 1; $bIgnoreMissing++)
    #                 {
    #                     $iRun++;
    #
    #                     &log(INFO, "run ${iRun} - " .
    #                                "remote ${bRemote}, exists ${bExists}, temp ${bTemp}, ignore missing ${bIgnoreMissing}");
    #
    #                     # Drop the old test directory and create a new one
    #                     system("rm -rf test");
    #                     system("mkdir test") == 0 or confess "Unable to create test directory";
    #
    #                     my $strFile = "${strTestPath}/test.txt";
    #
    #                     if ($bExists)
    #                     {
    #                         system("echo 'TESTDATA' > ${strFile}" . ($bTemp ? ".backrest.tmp" : ""));
    #                     }
    #
    #                     # Execute in eval in case of error
    #                     eval
    #                     {
    #                         if ($oFile->remove(PATH_BACKUP_ABSOLUTE, $strFile, $bTemp, $bIgnoreMissing) != $bExists)
    #                         {
    #                             confess "hash did not match expected";
    #                         }
    #                     };
    #
    #                     if ($@ && $bExists)
    #                     {
    #                         confess "error raised: " . $@ . "\n";
    #                     }
    #
    #                     if (-e ($strFile . ($bTemp ? ".backrest.tmp" : "")))
    #                     {
    #                         confess "file still exists";
    #                     }
    #                 }
    #             }
    #         }
    #     }
    # }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test hash()
    #-------------------------------------------------------------------------------------------------------------------------------
    # if ($strTest eq 'all' || $strTest eq 'hash')
    # {
    #     $iRun = 0;
    #
    #     &log(INFO, "--------------------------------------------------------------------------------");
    #     &log(INFO, "test File->hash()\n");
    #
    #     for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    #     {
    #         my $oFile = BackRest::File->new
    #         (
    #             strStanza => $strStanza,
    #             bNoCompression => true,
    #             strCommand => $strCommand,
    #             strBackupClusterPath => ${strTestPath},
    #             strBackupPath => ${strTestPath},
    #             strBackupHost => $bRemote ? $strHost : undef,
    #             strBackupUser => $bRemote ? $strUser : undef
    #         );
    #
    #         # Loop through exists
    #         for (my $bExists = 0; $bExists <= 1; $bExists++)
    #         {
    #             $iRun++;
    #
    #             &log(INFO, "run ${iRun} - " .
    #                        "remote $bRemote, exists $bExists");
    #
    #             # Drop the old test directory and create a new one
    #             system("rm -rf test");
    #             system("mkdir test") == 0 or confess "Unable to create test directory";
    #
    #             my $strFile = "${strTestPath}/test.txt";
    #
    #             if ($bExists)
    #             {
    #                 system("echo 'TESTDATA' > ${strFile}");
    #             }
    #
    #             # Execute in eval in case of error
    #             eval
    #             {
    #                 if ($oFile->hash(PATH_BACKUP_ABSOLUTE, $strFile) ne '06364afe79d801433188262478a76d19777ef351')
    #                 {
    #                     confess "incorrect hash returned";
    #                 }
    #             };
    #
    #             if ($@)
    #             {
    #                 if ($bExists)
    #                 {
    #                     confess "error raised: " . $@ . "\n";
    #                 }
    #             }
    #         }
    #     }
    # }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test exists()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'exists')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "test File->exists()\n");

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            my $oFile = BackRest::File->new
            (
                strStanza => $strStanza,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strRemote => $bRemote ? 'backup' : undef,
                oRemote => $bRemote ? $oRemote : undef
            );

            # Loop through exists
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                # Loop through exists
                for (my $bError = 0; $bError <= $bExists; $bError++)
                {
                    $iRun++;

                    &log(INFO, "run ${iRun} - " .
                               "remote $bRemote, exists $bExists, error ${bError}");

                    # Setup test directory
                    BackRestFileTestSetup($bError);

                    my $strFile = "${strTestPath}/test.txt";

                    if ($bError)
                    {
                        $strFile = "${strTestPath}/private/test.txt";
                    }
                    elsif ($bExists)
                    {
                        system("echo 'TESTDATA' > ${strFile}");
                    }

                    # Execute in eval in case of error
                    eval
                    {
                        if ($oFile->exists(PATH_BACKUP_ABSOLUTE, $strFile) != $bExists)
                        {
                            confess "bExists is set to ${bExists}, but exists() returned " . !$bExists;
                        }
                    };

                    if ($@)
                    {
                        my $oMessage = $@;
                        my $iCode;
                        my $strMessage;

                        if (blessed($oMessage))
                        {
                            if ($oMessage->isa("BackRest::Exception"))
                            {
                                $iCode = $oMessage->code();
                                $strMessage = $oMessage->message();
                            }
                            else
                            {
                                confess 'unknown error object';
                            }
                        }
                        else
                        {
                            $strMessage = $oMessage;
                        }

                        if ($bError && defined($iCode) && $iCode == COMMAND_ERR_FILE_READ)
                        {
                            next;
                        }

                        confess "error raised: " . $strMessage . "\n";
                    }
                }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test copy()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'copy')
    {
        $iRun = 0;

        # Loop through backup local vs remote
        for (my $bBackupRemote = 0; $bBackupRemote <= 1; $bBackupRemote++)
        {
        # Loop through db local vs remote
        for (my $bDbRemote = 0; $bDbRemote <= 1; $bDbRemote++)
        {
            # Backup and db cannot both be remote
            if ($bBackupRemote && $bDbRemote)
            {
                next;
            }

            # Determine side is remote
            my $strRemote = $bBackupRemote ? 'backup' : $bDbRemote ? 'db' : undef;

            # Create the file object
            my $oFile = BackRest::File->new
            (
                strStanza => "db",
                strBackupClusterPath => undef,
                strBackupPath => ${strTestPath},
                strRemote => $strRemote,
                oRemote => $bBackupRemote || $bDbRemote ? $oRemote : undef
            );

            # Loop through source compression
            for (my $bSourceCompressed = 0; $bSourceCompressed <= 1; $bSourceCompressed++)
            {
            # Loop through destination compression
            for (my $bDestinationCompress = 0; $bDestinationCompress <= 1; $bDestinationCompress++)
            {
            # Loop through source path types
            for (my $bSourcePathType = 0; $bSourcePathType <= 1; $bSourcePathType++)
            {
            # Loop through destination path types
            for (my $bDestinationPathType = 0; $bDestinationPathType <= 1; $bDestinationPathType++)
            {
            # Loop through source missing/present
            for (my $bSourceMissing = 0; $bSourceMissing <= 1; $bSourceMissing++)
            {
            # Loop through source ignore/require
            for (my $bSourceIgnoreMissing = 0; $bSourceIgnoreMissing <= 1; $bSourceIgnoreMissing++)
            {
                my $strSourcePathType = $bSourcePathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;
                my $strSourcePath = $bSourcePathType ? "db" : "backup";

                my $strDestinationPathType = $bDestinationPathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;
                my $strDestinationPath = $bDestinationPathType ? "db" : "backup";

                $iRun++;
                
                &log(INFO, "run ${iRun} - " .
                           "srcpth " . (defined($strRemote) && $strRemote eq $strSourcePath ? "remote" : "local") .
                               ":${strSourcePath}, srccmp $bSourceCompressed, srcmiss ${bSourceMissing}, " .
                               "srcignmiss ${bSourceIgnoreMissing}, " .
                           "dstpth " . (defined($strRemote) && $strRemote eq $strDestinationPath ? "remote" : "local") .
                               ":${strDestinationPath}, dstcmp $bDestinationCompress");

                # Setup test directory
                BackRestFileTestSetup(false);
                system("mkdir ${strTestPath}/backup") == 0 or confess "Unable to create test/backup directory";
                system("mkdir ${strTestPath}/db") == 0 or confess "Unable to create test/db directory";

                my $strSourceFile = "${strTestPath}/${strSourcePath}/test-source.txt";
                my $strDestinationFile = "${strTestPath}/${strDestinationPath}/test-destination.txt";

                # Create the compressed or uncompressed test file
                my $strSourceHash;
                
                if (!$bSourceMissing)
                {
                    system("echo 'TESTDATA' > ${strSourceFile}");
                    $strSourceHash = $oFile->hash(PATH_ABSOLUTE, $strSourceFile);

                    if ($bSourceCompressed)
                    {
                        system("gzip ${strSourceFile}");
                        $strSourceFile .= ".gz";
                    }
                }

                if ($bDestinationCompress)
                {
                    $strDestinationFile .= ".gz";
                }

                # Run file copy in an eval block because some errors are expected
                my $bReturn;

                eval
                {
                    $bReturn = $oFile->copy($strSourcePathType, $strSourceFile,
                                            $strDestinationPathType, $strDestinationFile,
                                            $bSourceCompressed, $bDestinationCompress,
                                            $bSourceIgnoreMissing);
                };

                # Check for errors after copy
                if ($@)
                {
                    my $oMessage = $@;
                    
                    if (blessed($oMessage))
                    {
                        if ($oMessage->isa("BackRest::Exception"))
                        {
                            if ($bSourceMissing && !$bSourceIgnoreMissing)
                            {
                                next;
                            }
                    
                            confess $oMessage->message();
                        }
                        else
                        {
                            confess 'unknown error object: ' . $oMessage;
                        }
                    }
                    else
                    {
                        confess $oMessage;
                    }
                }

                if ($bSourceMissing)
                {
                    if ($bSourceIgnoreMissing)
                    {
                         if ($bReturn)
                         {
                             confess 'copy() returned ' . $bReturn .  ' when ignore missing set';
                         }
                         
                         next;
                    }
                    
                    confess "expected source file missing error";
                }
            
                unless (-e $strDestinationFile)
                {
                    confess "could not find destination file ${strDestinationFile}";
                }

                if ($bDestinationCompress)
                {
                    system("gzip -d ${strDestinationFile}") == 0 or die "could not decompress ${strDestinationFile}";
                    $strDestinationFile = substr($strDestinationFile, 0, length($strDestinationFile) - 3);
                }
            
                my $strDestinationHash = $oFile->hash(PATH_ABSOLUTE, $strDestinationFile);
            
                if ($strSourceHash ne $strDestinationHash)
                {
                    confess "source ${strSourceHash} and destination ${strDestinationHash} file hashes do not match";
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
}

1;