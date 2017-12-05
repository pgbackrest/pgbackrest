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
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Posix::Driver;

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
    my $oStorage = new pgBackRest::Storage::Local(
        $strBuildPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

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
            my $rhFileData = $rhFile->{&BLD_DATA};
            my $rhSource;

            # Build general banner
            #-------------------------------------------------------------------------------------------------------------------------------
            my $strBanner = bldBanner($rhFile->{&BLD_SUMMARY}, GENERATOR);

            # Build header file
            #-------------------------------------------------------------------------------------------------------------------------------
            if (defined($rhFileEnum) || defined($rhFileConstant))
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

                    # Iterate constants
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

                # Only save the file if the content has changed
                my $strFileName = "${strPath}/${strFile}.auto.${strExt}";
                my $strContent = trim($rhSource->{$strFileType}) . "\n";
                my $bChanged = true;

                if ($oStorage->exists($strFileName))
                {
                    my $strOldContent = ${$oStorage->get($strFileName)};

                    if ($strContent eq $strOldContent)
                    {
                        $bChanged = false;
                    }
                }

                if ($bChanged)
                {
                    $oStorage->put($strFileName, $strContent);
                }
            }
        }
    }

    # Return build hash to caller for further processing
    return $rhBuild;
}

push @EXPORT, qw(buildAll);

1;
