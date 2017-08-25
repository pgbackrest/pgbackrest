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
use pgBackRestBuild::CodeGen::Common;
use pgBackRestBuild::CodeGen::Truth;
use pgBackRestBuild::Config::Build;

####################################################################################################################################
# buildAll - execute all build functions and generate C source code
####################################################################################################################################
sub buildAll
{
    my $strBuildPath = shift;

    # Storage object
    my $oStorage = new pgBackRest::Storage::Local(
        $strBuildPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

    # Build and output source code
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhBuild =
    {
        'src/config' => buildConfig(),
    };

    foreach my $strPath (sort(keys(%{$rhBuild})))
    {
        foreach my $strFile (sort(keys(%{$rhBuild->{$strPath}{&BLD_FILE}})))
        {
            my $rhFile = $rhBuild->{$strPath}{&BLD_FILE}{$strFile};
            my $rhFileFunction = $rhFile->{&BLD_FUNCTION};
            my $rhFileConstant = $rhFile->{&BLD_CONSTANT_GROUP};
            my $rhSource;

            # Build the markdown documentation
            #-------------------------------------------------------------------------------------------------------------------------------
            if (defined($rhFileFunction))
            {
                my $strTruthTable = '# ' . $rhFile->{&BLD_SUMMARY} . "\n";

                foreach my $strFunction (sort(keys(%{$rhFileFunction})))
                {
                    my $rhFunction = $rhFileFunction->{$strFunction};

                    next if !defined($rhFunction->{&BLD_MATRIX});

                    # Build function summary
                    my $strSummary = ucfirst($rhFunction->{&BLD_SUMMARY});
                    $strSummary .= $strSummary =~ /\?$/ ? '' : '.';

                    $strTruthTable .=
                        "\n## ${strFunction}\n\n" .
                        "${strSummary}\n\n" .
                        "### Truth Table:\n\n";

                    my $strTruthDefault;
                    my $strTruthSummary;

                    # Does this function depend on the result of another function
                    if (defined($rhFunction->{&BLD_FUNCTION_DEPEND}))
                    {
                        $strTruthSummary .=
                            'This function is valid when `' . $rhFunction->{&BLD_FUNCTION_DEPEND} . '()` = `' .
                            cgenTypeFormat(CGEN_DATATYPE_BOOL, $rhFunction->{&BLD_FUNCTION_DEPEND_RESULT}) . '`.';
                    }

                    # Are there permutations which are excluded?
                    if (exists($rhFunction->{&BLD_TRUTH_DEFAULT}))
                    {
                        $strTruthDefault =
                            defined($rhFunction->{&BLD_TRUTH_DEFAULT}) ? $rhFunction->{&BLD_TRUTH_DEFAULT} : CGEN_DATAVAL_NULL;

                        $strTruthSummary .=
                            (defined($strTruthSummary) ? ' ' : '') .
                            'Permutations that return `' .
                            cgenTypeFormat(
                                $rhFunction->{&BLD_RETURN_TYPE},
                                defined($rhFunction->{&BLD_RETURN_VALUE_MAP}) &&
                                    defined($rhFunction->{&BLD_RETURN_VALUE_MAP}->{$strTruthDefault}) ?
                                        $rhFunction->{&BLD_RETURN_VALUE_MAP}->{$strTruthDefault} : $strTruthDefault) .
                                "` are excluded for brevity.";
                    }

                    # Build the truth table
                    $strTruthTable .=
                        (defined($strTruthSummary) ? "${strTruthSummary}\n\n" : '') .
                        cgenTruthTable(
                            $strFunction, $rhFunction->{&BLD_PARAM}, $rhFunction->{&BLD_RETURN_TYPE}, $strTruthDefault,
                            $rhFunction->{&BLD_MATRIX}, BLDLCL_PARAM_COMMANDID, $rhBuild->{$strPath}{&BLD_PARAM_LABEL},
                            $rhFunction->{&BLD_RETURN_VALUE_MAP});
                }

                $rhSource->{&BLD_MD} = $strTruthTable;
            }

            # Build general banner
            #-------------------------------------------------------------------------------------------------------------------------------
            my $strBanner = cgenBanner($rhFile->{&BLD_SUMMARY});

            # Build header file
            #-------------------------------------------------------------------------------------------------------------------------------
            if (defined($rhFileConstant))
            {
                my $strHeaderDefine = uc($strFile) . '_AUTO_H';

                my $strHeader =
                    $strBanner .
                    "#ifndef ${strHeaderDefine}\n" .
                    "#define ${strHeaderDefine}\n";

                # Iterate constant groups
                foreach my $strConstantGroup (sort(keys(%{$rhFileConstant})))
                {
                    my $rhConstantGroup = $rhFileConstant->{$strConstantGroup};

                    $strHeader .= "\n" . cgenBanner($rhConstantGroup->{&BLD_SUMMARY} . ' constants');

                    # Iterate constants
                    foreach my $strConstant (sort(keys(%{$rhConstantGroup->{&BLD_CONSTANT}})))
                    {
                        my $rhConstant = $rhConstantGroup->{&BLD_CONSTANT}{$strConstant};

                        $strHeader .=
                            "#define ${strConstant} " . (' ' x (69 - length($strConstant) - 10)) .
                                $rhConstant->{&BLD_CONSTANT_VALUE} . "\n";
                    }
                }

                $strHeader .=
                    "\n#endif";

                $rhSource->{&BLD_HEADER} = $strHeader;
            }

            # Build C file
            #-----------------------------------------------------------------------------------------------------------------------
            my $strFunctionCode = $strBanner;

            foreach my $strFunction (sort(keys(%{$rhFileFunction})))
            {
                my $rhFunction = $rhFileFunction->{$strFunction};

                $strFunctionCode .=
                    "\n" . cgenFunction($strFunction, $rhFunction->{&BLD_SUMMARY}, undef, $rhFunction->{&BLD_SOURCE});
            }

            $rhSource->{&BLD_C} = $strFunctionCode;

            # Output files
            #-----------------------------------------------------------------------------------------------------------------------
            foreach my $strFileType (sort(keys(%{$rhSource})))
            {
                $oStorage->put("${strPath}/${strFile}.auto.${strFileType}", trim($rhSource->{$strFileType}) . "\n");
            }
        }
    }

    # Return build hash to caller for further processing
    return $rhBuild;
}

push @EXPORT, qw(buildAll);

1;
