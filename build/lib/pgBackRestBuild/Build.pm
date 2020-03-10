####################################################################################################################################
# Auto-Generate C Files Required for Build
####################################################################################################################################
package pgBackRestBuild::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);
use Storable qw(dclone);

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;

use pgBackRestBuild::Build::Common;

####################################################################################################################################
# Define generator used for auto generated warning messages
####################################################################################################################################
use constant GENERATOR                                              => 'Build.pm';

####################################################################################################################################
# buildAll - execute all build functions and generate C source code
####################################################################################################################################
sub buildAll
{
    my $strBuildPath = shift;
    my $rhBuild = shift;
    my $strFileExt = shift;

    # Storage object
    my $oStorage = new pgBackRestTest::Common::Storage(
        $strBuildPath, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    # List of files actually built
    my @stryBuilt;

    # Build and output source code
    #-------------------------------------------------------------------------------------------------------------------------------
    foreach my $strBuild (sort(keys(%{$rhBuild})))
    {
        my $strPath = $rhBuild->{$strBuild}{&BLD_PATH};

        foreach my $strFile (sort(keys(%{$rhBuild->{$strBuild}{&BLD_DATA}{&BLD_FILE}})))
        {
            my $rhFile = $rhBuild->{$strBuild}{&BLD_DATA}{&BLD_FILE}{$strFile};
            my $rhFileConstant = $rhFile->{&BLD_CONSTANT_GROUP};
            my $rhFileEnum = $rhFile->{&BLD_ENUM};
            my $rhFileDeclare = $rhFile->{&BLD_DECLARE};
            my $rhFileData = $rhFile->{&BLD_DATA};
            my $rhSource;

            # Build general banner
            #-------------------------------------------------------------------------------------------------------------------------------
            my $strBanner = bldBanner($rhFile->{&BLD_SUMMARY}, GENERATOR);

            # Build header file
            #-------------------------------------------------------------------------------------------------------------------------------
            if (defined($rhFileEnum) || defined($rhFileConstant) || defined($rhFileDeclare))
            {
                my $strHeaderDefine = uc("${strPath}/${strFile}") . '_AUTO_H';
                $strHeaderDefine =~ s/\//_/g;

                my $strHeader =
                    $strBanner .
                    "#ifndef ${strHeaderDefine}\n" .
                    "#define ${strHeaderDefine}\n";

                # Iterate constant groups
                foreach my $strConstantGroup (sort(keys(%{$rhFileConstant})))
                {
                    my $rhConstantGroup = $rhFileConstant->{$strConstantGroup};

                    $strHeader .= "\n" . bldBanner($rhConstantGroup->{&BLD_SUMMARY} . ' constants');

                    # Iterate constants
                    foreach my $strConstant (sort(keys(%{$rhConstantGroup->{&BLD_CONSTANT}})))
                    {
                        my $rhConstant = $rhConstantGroup->{&BLD_CONSTANT}{$strConstant};

                        $strHeader .=
                            "#define ${strConstant} " . (' ' x (69 - length($strConstant) - 10)) .
                                $rhConstant->{&BLD_CONSTANT_VALUE} . "\n";
                    }
                }

                # Iterate enum groups
                foreach my $strEnum (sort(keys(%{$rhFileEnum})))
                {
                    my $rhEnum = $rhFileEnum->{$strEnum};

                    $strHeader .=
                        "\n" . bldBanner($rhEnum->{&BLD_SUMMARY} . ' enum');

                    $strHeader .=
                        "typedef enum\n" .
                        "{\n";

                    my $iExpectedValue = 0;

                    # Iterate enums
                    foreach my $strEnumItem (@{$rhEnum->{&BLD_LIST}})
                    {
                        $strHeader .=
                            "    ${strEnumItem}";

                        if (defined($rhEnum->{&BLD_VALUE}{$strEnumItem}) && $rhEnum->{&BLD_VALUE}{$strEnumItem} != $iExpectedValue)
                        {
                            $strHeader .= ' = ' . (' ' x (61 - length($strEnumItem)))  . $rhEnum->{&BLD_VALUE}{$strEnumItem};
                            $iExpectedValue = $rhEnum->{&BLD_VALUE}{$strEnumItem};
                        }

                        $strHeader .= ",\n";
                            $iExpectedValue++;
                    }

                    $strHeader .=
                        "} " . $rhEnum->{&BLD_NAME} . ";\n";
                }

                foreach my $strDeclare (sort(keys(%{$rhFileDeclare})))
                {
                    my $rhDeclare = $rhFileDeclare->{$strDeclare};

                    $strHeader .= "\n" . bldBanner($rhDeclare->{&BLD_SUMMARY});
                    $strHeader .= $rhDeclare->{&BLD_SOURCE};
                }

                $strHeader .=
                    "\n#endif";

                $rhSource->{&BLD_HEADER} = $strHeader;
            }

            # Build C file
            #-----------------------------------------------------------------------------------------------------------------------
            if (defined($rhFileData))
            {
                my $strCode;

                foreach my $strData (sort(keys(%{$rhFileData})))
                {
                    my $rhData = $rhFileData->{$strData};

                    $strCode .= "\n" . bldBanner($rhData->{&BLD_SUMMARY});
                    $strCode .= $rhData->{&BLD_SOURCE};
                }

                $rhSource->{&BLD_C} = "${strBanner}${strCode}";
            }

            # Output files
            #-----------------------------------------------------------------------------------------------------------------------
            foreach my $strFileType (sort(keys(%{$rhSource})))
            {
                my $strExt = $strFileType;

                if (defined($strFileExt))
                {
                    $strExt = $strFileType eq BLD_C ? $strFileExt : "${strFileExt}h";
                }

                # Save the file if it has not changed
                my $strBuilt = "${strPath}/${strFile}.auto.${strExt}";
                my $bSave = true;
                my $oFile = $oStorage->openRead($strBuilt, {bIgnoreMissing => true});

                if (defined($oFile) && ${$oStorage->get($oFile)} eq (trim($rhSource->{$strFileType}) . "\n"))
                {
                    $bSave = false;
                }

                if ($bSave)
                {
                    $oStorage->put($strBuilt, trim($rhSource->{$strFileType}) . "\n");
                    push(@stryBuilt, basename($strBuildPath) . "/${strBuilt}");
                }
            }
        }
    }

    # Return list of files built
    return @stryBuilt;
}

push @EXPORT, qw(buildAll);

1;
