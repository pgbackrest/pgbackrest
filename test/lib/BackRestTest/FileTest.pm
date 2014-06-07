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

use lib dirname($0) . "/..";
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_remote;

use Exporter qw(import);
our @EXPORT = qw(BackRestFileTest);

sub BackRestFileTest
{
    my $strTest = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup test paths
    my $strTestPath = dirname(abs_path($0)) . "/test";
    my $iRun;

    my $strStanza = "db";
    my $strCommand = "/Users/dsteele/pg_backrest/bin/pg_backrest_remote.pl";
    my $strHost = "127.0.0.1";
    my $strUser = getpwuid($<);
    my $strGroup = getgrgid($();

    # Print test parameters
    &log(INFO, "Testing with test_path = ${strTestPath}, host = ${strHost}, user = ${strUser}, group = ${strGroup}");

    &log(INFO, "FILE MODULE ********************************************************************");

    system("ssh backrest\@${strHost} 'rm -rf ${strTestPath}/private'");

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = pg_backrest_remote->new
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
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            # Loop through exists (does the paren path exist?)
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                # Loop through permission (permission will be set on true)
                for (my $bPermission = 0; $bPermission <= $bExists; $bPermission++)
                {
                    $iRun++;

                    &log(INFO, "run ${iRun} - " .
                               "remote $bRemote, exists $bExists, permission $bPermission");

                    # Drop the old test directory and create a new one
                    system("rm -rf test");
                    system("mkdir test") == 0 or confess "Unable to create test directory";

                    my $strPath = "${strTestPath}/path";
                    my $strPermission;

                    # If permission then set one (other than the default)
                    if ($bPermission)
                    {
                        $strPermission = "0700";

                        # Make sure that we are not testing with the default permission
                        if ($strPermission eq $oFile->{strDefaultPathPermission})
                        {
                            confess 'cannot set test permission ${strPermission} equal to default permission' .
                                    $oFile->{strDefaultPathPermission};
                        }
                    }

                    # If not exists then set the path to something bogus
                    if (!$bExists)
                    {
                        $strPath = "${strTestPath}/error/path";
                    }

                    # Execute in eval to catch errors
                    eval
                    {
                        $oFile->path_create(PATH_BACKUP_ABSOLUTE, $strPath, $strPermission);
                    };

                    # Check for errors
                    if ($@)
                    {
                        # Ignore errors if the path did not exist
                        if (!$bExists)
                        {
                            next;
                        }

                        confess "error raised: " . $@ . "\n";
                    }
                    else
                    {
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

                        my $strPermissionCompare = defined($strPermission) ? $strPermission : $oFile->{strDefaultPathPermission};

                        if ($strPermissionCompare ne sprintf("%04o", S_IMODE($oStat->mode)))
                        {
                            confess "permissions were not set to {$strPermissionCompare}";
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

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            # Loop through source exists
            for (my $bSourceExists = 0; $bSourceExists <= 1; $bSourceExists++)
            {
                # Loop through destination exists
                for (my $bDestinationExists = 0; $bDestinationExists <= 1; $bDestinationExists++)
                {
                    # Loop through create
                    for (my $bCreate = 0; $bCreate <= $bDestinationExists; $bCreate++)
                    {
                        $iRun++;

                        &log(INFO, "run ${iRun} - " .
                                   "remote $bRemote, src_exists $bSourceExists, dst_exists $bDestinationExists, create $bCreate");

                        # Drop the old test directory and create a new one
                        system("rm -rf test");
                        system("mkdir test") == 0 or confess "Unable to create test directory";

                        my $strSourceFile = "${strTestPath}/test.txt";
                        my $strDestinationFile = "${strTestPath}/test-dest.txt";

                        if ($bCreate)
                        {
                            $strDestinationFile = "${strTestPath}/sub/test-dest.txt"
                        }

                        if ($bSourceExists)
                        {
                            system("echo 'TESTDATA' > ${strSourceFile}");
                        }

                        if (!$bDestinationExists)
                        {
                            $strDestinationFile = "error" . $strDestinationFile;
                        }

                        # Execute in eval in case of error
                        eval
                        {
                            $oFile->move(PATH_BACKUP_ABSOLUTE, $strSourceFile, PATH_BACKUP_ABSOLUTE, $strDestinationFile, $bCreate);
                        };

                        if ($@)
                        {
                            if (!$bSourceExists || !$bDestinationExists)
                            {
                                next;
                            }

                            confess "error raised: " . $@ . "\n";
                        }

                        if (!$bSourceExists || !$bDestinationExists)
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

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test compress()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'compress')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->compress()\n");

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            # Loop through exists
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote, exists $bExists");

                # Drop the old test directory and create a new one
                system("rm -rf test");
                system("mkdir test") == 0 or confess "Unable to create test directory";

                my $strFile = "${strTestPath}/test.txt";

                if ($bExists)
                {
                    system("echo 'TESTDATA' > ${strFile}");
                }

                # Execute in eval in case of error
                eval
                {
                    $oFile->compress(PATH_BACKUP_ABSOLUTE, $strFile);
                };

                if ($@ && $bExists)
                {
                    confess "error raised: " . $@ . "\n";
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

        # Create the test data
        system("rm -rf test");

        system("mkdir -m 750 ${strTestPath}") == 0 or confess "Unable to create test directory";
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

        my $strManifestCompare =
            ".,d,${strUser},${strGroup},0750,,,,\n" .
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
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            for (my $bError = 0; $bError <= 1; $bError++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote, error $bError");

                my $strPath = $strTestPath;

                if ($bError)
                {
                    $strPath .= "-error";
                }

                # Execute in eval in case of error
                eval
                {
                    my %oManifestHash;
                    $oFile->manifest(PATH_BACKUP_ABSOLUTE, $strPath, \%oManifestHash);

                    my $strManifest;

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
                };

                if ($@ && !$bError)
                {
                    confess "error raised: " . $@ . "\n";
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
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
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
                        $iRun++;

                        &log(INFO, "run ${iRun} - " .
                                   "remote $bRemote, exists $bExists, " .
                                   "expression " . (defined($strExpression) ? $strExpression : "[undef]") . ", " .
                                   "sort " . (defined($strSort) ? $strSort : "[undef]"));

                        my $strPath = "${strTestPath}";

                        # Drop the old test directory and create a new one
                        system("rm -rf test");

                        if ($bExists)
                        {
                            system("mkdir test") == 0 or confess "Unable to create test directory";
                            system("echo 'TESTDATA' > ${strPath}/test.txt");
                            system("echo 'TESTDATA2' > ${strPath}/test2.txt");
                        }

                        my @stryFileCompare = split(/\n/, "test.txt\ntest2.txt");

                        # Execute in eval in case of error
                        eval
                        {
                            my @stryFileList = $oFile->list(PATH_BACKUP_ABSOLUTE, $strPath, $strExpression, $strSort);

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
                        };

                        if ($@ && $bExists)
                        {
                            confess "error raised: " . $@ . "\n";
                        }
                    }
                }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test remove()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'remove')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "Test File->remove()\n");

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            my $strHost = $bRemote ? "127.0.0.1" : undef;
            my $strUser = $bRemote ? "dsteele" : undef;

            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => ${strCommand},
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            # Loop through exists
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                # Loop through temp
                for (my $bTemp = 0; $bTemp <= 1; $bTemp++)
                {
                    # Loop through ignore missing
                    for (my $bIgnoreMissing = 0; $bIgnoreMissing <= 1; $bIgnoreMissing++)
                    {
                        $iRun++;

                        &log(INFO, "run ${iRun} - " .
                                   "remote ${bRemote}, exists ${bExists}, temp ${bTemp}, ignore missing ${bIgnoreMissing}");

                        # Drop the old test directory and create a new one
                        system("rm -rf test");
                        system("mkdir test") == 0 or confess "Unable to create test directory";

                        my $strFile = "${strTestPath}/test.txt";

                        if ($bExists)
                        {
                            system("echo 'TESTDATA' > ${strFile}" . ($bTemp ? ".backrest.tmp" : ""));
                        }

                        # Execute in eval in case of error
                        eval
                        {
                            if ($oFile->remove(PATH_BACKUP_ABSOLUTE, $strFile, $bTemp, $bIgnoreMissing) != $bExists)
                            {
                                confess "hash did not match expected";
                            }
                        };

                        if ($@ && $bExists)
                        {
                            confess "error raised: " . $@ . "\n";
                        }

                        if (-e ($strFile . ($bTemp ? ".backrest.tmp" : "")))
                        {
                            confess "file still exists";
                        }
                    }
                }
            }
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test hash()
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'hash')
    {
        $iRun = 0;

        &log(INFO, "--------------------------------------------------------------------------------");
        &log(INFO, "test File->hash()\n");

        for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
        {
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => true,
                strCommand => $strCommand,
                strBackupClusterPath => ${strTestPath},
                strBackupPath => ${strTestPath},
                strBackupHost => $bRemote ? $strHost : undef,
                strBackupUser => $bRemote ? $strUser : undef
            );

            # Loop through exists
            for (my $bExists = 0; $bExists <= 1; $bExists++)
            {
                $iRun++;

                &log(INFO, "run ${iRun} - " .
                           "remote $bRemote, exists $bExists");

                # Drop the old test directory and create a new one
                system("rm -rf test");
                system("mkdir test") == 0 or confess "Unable to create test directory";

                my $strFile = "${strTestPath}/test.txt";

                if ($bExists)
                {
                    system("echo 'TESTDATA' > ${strFile}");
                }

                # Execute in eval in case of error
                eval
                {
                    if ($oFile->hash(PATH_BACKUP_ABSOLUTE, $strFile) ne '06364afe79d801433188262478a76d19777ef351')
                    {
                        confess "bExists is set to ${bExists}, but exists() returned " . !$bExists;
                    }
                };

                if ($@)
                {
                    if ($bExists)
                    {
                        confess "error raised: " . $@ . "\n";
                    }
                }
            }
        }
    }

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
            my $oFile = pg_backrest_file->new
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

                    # Drop the old test directory and create a new one
                    system("rm -rf test");
                    system("mkdir test") == 0 or confess "Unable to create test directory";

                    my $strFile = "${strTestPath}/test.txt";

                    if ($bError)
                    {
                        system("ssh backrest\@${strHost} 'mkdir -m 700 ${strTestPath}/private'");
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

                    if ($bError)
                    {
                        system("ssh backrest\@${strHost} 'rm -rf ${strTestPath}/private'");
                    }

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

        for (my $bBackupRemote = 0; $bBackupRemote <= 1; $bBackupRemote++)
        {
            # Loop through source compression
            for (my $bDbRemote = 0; $bDbRemote <= 1; $bDbRemote++)
            {
                # Backup and db cannot both be remote
                if ($bBackupRemote && $bDbRemote)
                {
                    next;
                }

                # Loop through destination compression
                for (my $bDestinationCompressed = 0; $bDestinationCompressed <= 1; $bDestinationCompressed++)
                {
                    my $oFile = pg_backrest_file->new
                    (
                        strStanza => "db",
                        strCommand => $strCommand,
                        bCompress => $bDestinationCompressed,
                        strBackupClusterPath => undef,
                        strBackupPath => ${strTestPath},
                        strBackupHost => $bBackupRemote ? $strHost : undef,
                        strBackupUser => $bBackupRemote ? $strUser : undef,
                        strDbHost => $bDbRemote ? $strHost : undef,
                        strDbUser => $bDbRemote ? $strUser : undef
                    );

                    for (my $bSourceCompressed = 0; $bSourceCompressed <= 1; $bSourceCompressed++)
                    {
                        for (my $bSourcePathType = 0; $bSourcePathType <= 1; $bSourcePathType++)
                        {
                            my $strSourcePathType = $bSourcePathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;
                            my $strSourcePath = $bSourcePathType ? "db" : "backup";

                            for (my $bDestinationPathType = 0; $bDestinationPathType <= 1; $bDestinationPathType++)
                            {
                                my $strDestinationPathType = $bDestinationPathType ? PATH_DB_ABSOLUTE : PATH_BACKUP_ABSOLUTE;
                                my $strDestinationPath = $bDestinationPathType ? "db" : "backup";

                                $iRun++;

                                &log(INFO, "run ${iRun} - " .
                                           "srcpth ${strSourcePath}, bkprmt $bBackupRemote, srccmp $bSourceCompressed, " .
                                           "dstpth ${strDestinationPath}, dbrmt $bDbRemote, dstcmp $bDestinationCompressed");

                                # Drop the old test directory and create a new one
                                system("rm -rf test");
                                system("mkdir -p test/lock") == 0 or confess "Unable to create test/lock directory";
                                system("mkdir -p test/backup") == 0 or confess "Unable to create test/backup directory";
                                system("mkdir -p test/db") == 0 or confess "Unable to create test/db directory";

                                my $strSourceFile = "${strTestPath}/${strSourcePath}/test-source.txt";
                                my $strDestinationFile = "${strTestPath}/${strDestinationPath}/test-destination.txt";

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

                                # Run file copy in an eval block because some errors are expected
                                my $bReturn;

                                eval
                                {
                                    $bReturn = $oFile->copy($strSourcePathType, $strSourceFile,
                                                            $strDestinationPathType, $strDestinationFile);
                                };

                                # Check for errors after copy
                                if ($@)
                                {
                                    # Different remote and destination with different path types should error
                                    # if (($bBackupRemote || $bDbRemote) && ($strSourcePathType ne $strDestinationPathType))
                                    # {
                                    #     print "    different source and remote for same path not supported\n";
                                    #     next;
                                    # }
                                    # If the error was intentional, then also continue
                                    # elsif ($bError)
                                    # {
                                    #     my $strError = $oFile->error_get();
                                    #
                                    #     if (!defined($strError) || ($strError eq ''))
                                    #     {
                                    #         confess 'no error message returned';
                                    #     }
                                    #
                                    #     print "    error raised: ${strError}\n";
                                    #     next;
                                    # }
                                    # Else this is an unexpected error
                                    # else
                                    # {
                                        confess $@;
                                    # }
                                }
                                # elsif ($bError)
                                # {
                                #     if ($bConfessError)
                                #     {
                                #         confess "Value was returned instead of exception thrown when confess error is true";
                                #     }
                                #     else
                                #     {
                                #         if ($bReturn)
                                #         {
                                #             confess "true was returned when an error was generated";
                                #         }
                                #         else
                                #         {
                                #             my $strError = $oFile->error_get();
                                #
                                #             if (!defined($strError) || ($strError eq ''))
                                #             {
                                #                 confess 'no error message returned';
                                #             }
                                #
                                #             print "    error returned: ${strError}\n";
                                #             next;
                                #         }
                                #     }
                                # }
                                # else
                                # {
                                #     if (!$bReturn)
                                #     {
                                #         confess "error was returned when no error generated";
                                #     }
                                #
                                #     print "    true was returned\n";
                                # }

                                # Check for errors after copy
                                if ($bDestinationCompressed)
                                {
                                    $strDestinationFile .= ".gz";
                                }

                                if ($bReturn)
                                {
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
