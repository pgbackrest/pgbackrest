####################################################################################################################################
# Generate C Coverage Report
####################################################################################################################################
package pgBackRestTest::Common::CoverageTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Html::DocHtmlBuilder;
use pgBackRestDoc::Html::DocHtmlElement;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Generate an lcov configuration file
####################################################################################################################################
sub coverageLCovConfigGenerate
{
    my $oStorage = shift;
    my $strOutFile = shift;
    my $bCoverageSummary = shift;

    my $strBranchFilter =
        'OBJECT_DEFINE_[A-Z0-9_]+\(|\s{4}[A-Z][A-Z0-9_]+\([^\?]*\)|\s{4}(ASSERT|assert|switch\s)\(|\{\+{0,1}' .
        ($bCoverageSummary ? 'uncoverable_branch' : 'uncover(ed|able)_branch');
    my $strLineFilter = '\{\+{0,1}uncover' . ($bCoverageSummary ? 'able' : '(ed|able)') . '[^_]';

    my $strConfig =
        "# LCOV Settings\n" .
        "\n" .
        "# Specify if branch coverage data should be collected and processed\n" .
        "lcov_branch_coverage=1\n" .
        "\n" .
        "# Specify the regular expression of lines to exclude from branch coverage\n" .
        "#\n" .
        '# OBJECT_DEFINE_[A-Z0-9_]+\( - exclude object definitions' . "\n" .
        '# \s{4}[A-Z][A-Z0-9_]+\([^\?]*\) - exclude macros that do not take a conditional parameter and are not themselves a parameter' . "\n" .
        '# ASSERT/(|assert\( - exclude asserts since it usually not possible to trigger both branches' . "\n" .
        '# switch \( - lcov requires default: to show complete coverage but --Wswitch-enum enforces all enum values be present' . "\n" .
        "lcov_excl_br_line=${strBranchFilter}\n" .
        "\n" .
        "# Specify the regular expression of lines to exclude\n" .
        "lcov_excl_line=${strLineFilter}\n" .
        "\n" .
        "# Coverage rate limits\n" .
        "genhtml_hi_limit = 100\n" .
        "genhtml_med_limit = 90\n" .
        "\n" .
        "# Width of line coverage field in source code view\n" .
        "genhtml_line_field_width = 9\n";

    # Write configuration file
    $oStorage->put($strOutFile, $strConfig);
}

push @EXPORT, qw(coverageLCovConfigGenerate);

####################################################################################################################################
# Extract coverage using gcov
####################################################################################################################################
sub coverageExtract
{
    my $oStorage = shift;
    my $strModule = shift;
    my $strTest = shift;
    my $bSummary = shift;
    my $strContainerImage = shift;
    my $strWorkPath = shift;
    my $strWorkTmpPath = shift;
    my $strWorkUnitPath = shift;
    my $strTestResultCoveragePath = shift . '/coverage';

    # Generate a list of files to cover
    my $hTestCoverage = (testDefModuleTest($strModule, $strTest))->{&TESTDEF_COVERAGE};

    my @stryCoveredModule;

    foreach my $strModule (sort(keys(%{$hTestCoverage})))
    {
        push (@stryCoveredModule, $strModule);
    }

    push(@stryCoveredModule, "module/${strModule}/" . testRunName($strTest, false) . 'Test');

    # Generate coverage reports for the modules
    my $strLCovConf = "${strTestResultCoveragePath}/raw/lcov.conf";
    coverageLCovConfigGenerate($oStorage, $strLCovConf, $bSummary);

    my $strLCovExe = "lcov --config-file=${strLCovConf}";
    my $strLCovOut = "${strWorkUnitPath}/test.lcov";
    my $strLCovOutTmp = "${strWorkUnitPath}/test.tmp.lcov";

    executeTest(
        (defined($strContainerImage) ? 'docker exec -i -u ' . TEST_USER . " ${strContainerImage} " : '') .
        "${strLCovExe} --capture --directory=${strWorkUnitPath} --o=${strLCovOut}");

    # Generate coverage report for each module
    foreach my $strCoveredModule (@stryCoveredModule)
    {
        my $strModuleName = testRunName($strCoveredModule, false);
        my $strModuleOutName = $strModuleName;
        my $bTest = false;

        if ($strModuleOutName =~ /^module/mg)
        {
            $strModuleOutName =~ s/^module/test/mg;
            $bTest = true;
        }

        # Generate lcov reports
        my $strModulePath =
            "${strWorkPath}/repo/" .
            (${strModuleOutName} =~ /^test\// ?
                'test/src/module/' . substr(${strModuleOutName}, 5) : "src/${strModuleOutName}");
        my $strLCovFile = "${strTestResultCoveragePath}/raw/${strModuleOutName}.lcov";
        my $strLCovTotal = "${strWorkTmpPath}/all.lcov";

        executeTest(
            "${strLCovExe}" . ($bTest ? ' --rc lcov_branch_coverage=0' : '') . " --extract=${strLCovOut} */${strModuleName}.c" .
                " --o=${strLCovOutTmp}");

        # Combine with prior run if there was one
        if ($oStorage->exists($strLCovFile))
        {
            my $strCoverage = ${$oStorage->get($strLCovOutTmp)};
            $strCoverage =~ s/^SF\:.*$/SF:$strModulePath\.c/mg;
            $oStorage->put($strLCovOutTmp, $strCoverage);

            executeTest("${strLCovExe} --add-tracefile=${strLCovOutTmp} --add-tracefile=${strLCovFile} --o=${strLCovOutTmp}");
        }

        # Update source file
        my $strCoverage = ${$oStorage->get($strLCovOutTmp)};

        if (defined($strCoverage))
        {
            if (!$bTest && $hTestCoverage->{$strCoveredModule} eq TESTDEF_COVERAGE_NOCODE)
            {
                confess &log(ERROR, "module '${strCoveredModule}' is marked 'no code' but has code");
            }

            # Get coverage info
            my $iTotalLines = (split(':', ($strCoverage =~ m/^LF:.*$/mg)[0]))[1] + 0;
            my $iCoveredLines = (split(':', ($strCoverage =~ m/^LH:.*$/mg)[0]))[1] + 0;

            my $iTotalBranches = 0;
            my $iCoveredBranches = 0;

            if ($strCoverage =~ /^BRF\:/mg && $strCoverage =~ /^BRH\:/mg)
            {
                # If this isn't here the statements below fail -- huh?
                my @match = $strCoverage =~ m/^BRF\:.*$/mg;

                $iTotalBranches = (split(':', ($strCoverage =~ m/^BRF:.*$/mg)[0]))[1] + 0;
                $iCoveredBranches = (split(':', ($strCoverage =~ m/^BRH:.*$/mg)[0]))[1] + 0;
            }

            # Report coverage if this is not a test or if the test does not have complete coverage
            if (!$bTest || $iTotalLines != $iCoveredLines || $iTotalBranches != $iCoveredBranches)
            {
                # Fix source file name
                $strCoverage =~ s/^SF\:.*$/SF:$strModulePath\.c/mg;

                $oStorage->put($oStorage->openWrite($strLCovFile, {bPathCreate => true}), $strCoverage);

                if ($oStorage->exists($strLCovTotal))
                {
                    executeTest("${strLCovExe} --add-tracefile=${strLCovFile} --add-tracefile=${strLCovTotal} --o=${strLCovTotal}");
                }
                else
                {
                    $oStorage->copy($strLCovFile, $strLCovTotal)
                }
            }
            else
            {
                $oStorage->remove($strLCovFile);
            }
        }
        else
        {
            if ($hTestCoverage->{$strCoveredModule} ne TESTDEF_COVERAGE_NOCODE)
            {
                confess &log(ERROR, "module '${strCoveredModule}' is marked 'code' but has no code");
            }
        }
    }
}

push @EXPORT, qw(coverageExtract);

####################################################################################################################################
# Validate converage and generate reports
####################################################################################################################################
sub coverageValidateAndGenerate
{
    my $oyTestRun = shift;
    my $oStorage = shift;
    my $bCoverageSummary = shift;
    my $strWorkPath = shift;
    my $strWorkTmpPath = shift;
    my $strTestResultCoveragePath = shift . '/coverage';
    my $strTestResultSummaryPath = shift;

    my $result = 0;

    # Determine which modules were covered (only check coverage if all tests were successful)
    #-----------------------------------------------------------------------------------------------------------------------
    my $hModuleTest;                                        # Everything that was run

    # Build a hash of all modules, tests, and runs that were executed
    foreach my $hTestRun (@{$oyTestRun})
    {
        # Get coverage for the module
        my $strModule = $hTestRun->{&TEST_MODULE};
        my $hModule = testDefModule($strModule);

        # Get coverage for the test
        my $strTest = $hTestRun->{&TEST_NAME};
        my $hTest = testDefModuleTest($strModule, $strTest);

        # If no tests are listed it means all of them were run
        if (@{$hTestRun->{&TEST_RUN}} == 0)
        {
            $hModuleTest->{$strModule}{$strTest} = true;
        }
    }

    # Now compare against code modules that should have full coverage
    my $hCoverageList = testDefCoverageList();
    my $hCoverageType = testDefCoverageType();
    my $hCoverageActual;

    foreach my $strCodeModule (sort(keys(%{$hCoverageList})))
    {
        if (@{$hCoverageList->{$strCodeModule}} > 0)
        {
            my $iCoverageTotal = 0;

            foreach my $hTest (@{$hCoverageList->{$strCodeModule}})
            {
                if (!defined($hModuleTest->{$hTest->{strModule}}{$hTest->{strTest}}))
                {
                    next;
                }

                $iCoverageTotal++;
            }

            if (@{$hCoverageList->{$strCodeModule}} == $iCoverageTotal)
            {
                $hCoverageActual->{testRunName($strCodeModule, false)} = $hCoverageType->{$strCodeModule};
            }
        }
    }

    if (keys(%{$hCoverageActual}) == 0)
    {
        &log(INFO, 'no code modules had all tests run required for coverage');
    }

    # Generate C coverage report
    #---------------------------------------------------------------------------------------------------------------------------
    &log(INFO, 'writing C coverage report');

    my $strLCovFile = "${strWorkTmpPath}/all.lcov";

    if ($oStorage->exists($strLCovFile))
    {
        executeTest(
            "genhtml ${strLCovFile} --config-file=${strTestResultCoveragePath}/raw/lcov.conf" .
                " --prefix=${strWorkPath}/repo" .
                " --output-directory=${strTestResultCoveragePath}/lcov");

        foreach my $strCodeModule (sort(keys(%{$hCoverageActual})))
        {
            my $strCoverageFile = $strCodeModule;
            $strCoverageFile =~ s/^module/test/mg;
            $strCoverageFile = "${strTestResultCoveragePath}/raw/${strCoverageFile}.lcov";

            my $strCoverage = $oStorage->get($oStorage->openRead($strCoverageFile, {bIgnoreMissing => true}));

            if (defined($strCoverage) && defined($$strCoverage))
            {
                my $iTotalLines = (split(':', ($$strCoverage =~ m/^LF\:.*$/mg)[0]))[1] + 0;
                my $iCoveredLines = (split(':', ($$strCoverage =~ m/^LH\:.*$/mg)[0]))[1] + 0;

                my $iTotalBranches = 0;
                my $iCoveredBranches = 0;

                if ($$strCoverage =~ /^BRF\:/mg && $$strCoverage =~ /^BRH\:/mg)
                {
                    # If this isn't here the statements below fail -- huh?
                    my @match = $$strCoverage =~ m/^BRF\:.*$/mg;

                    $iTotalBranches = (split(':', ($$strCoverage =~ m/^BRF\:.*$/mg)[0]))[1] + 0;
                    $iCoveredBranches = (split(':', ($$strCoverage =~ m/^BRH\:.*$/mg)[0]))[1] + 0;
                }

                # Generate detail if there is missing coverage
                my $strDetail = undef;

                if ($iCoveredLines != $iTotalLines)
                {
                    $strDetail .= "$iCoveredLines/$iTotalLines lines";
                }

                if ($iTotalBranches != $iCoveredBranches)
                {
                    $strDetail .= (defined($strDetail) ? ', ' : '') . "$iCoveredBranches/$iTotalBranches branches";
                }

                if (defined($strDetail))
                {
                    &log(ERROR, "c module ${strCodeModule} is not fully covered ($strDetail)");
                    $result++;
                }
            }
        }

        coverageGenerate(
            $oStorage, "${strWorkPath}/repo", "${strTestResultCoveragePath}/raw", "${strTestResultCoveragePath}/coverage.html");

        if ($bCoverageSummary)
        {
            &log(INFO, 'writing C coverage summary report');

            coverageDocSummaryGenerate(
                $oStorage, "${strTestResultCoveragePath}/raw", "${strTestResultSummaryPath}/metric-coverage-report.auto.xml");
        }
    }
    else
    {
        executeTest("rm -rf ${strTestResultCoveragePath}/test/tesult/coverage");
    }

    return $result;
}

push @EXPORT, qw(coverageValidateAndGenerate);

####################################################################################################################################
# Generate a C coverage report
####################################################################################################################################
# Helper to get the function for the current line
sub coverageGenerateFunction
{
    my $rhFile = shift;
    my $iCurrentLine = shift;

    my $rhFunction;

    foreach my $iFunctionLine (sort(keys(%{$rhFile->{function}})))
    {
        last if $iCurrentLine < $iFunctionLine;

        $rhFunction = $rhFile->{function}{$iFunctionLine};
    }

    if (!defined($rhFunction))
    {
        confess &log(ERROR, "function not found at line ${iCurrentLine}");
    }

    return $rhFunction;
}

sub coverageGenerate
{
    my $oStorage = shift;
    my $strBasePath = shift;
    my $strCoveragePath = shift;
    my $strOutFile = shift;

    # Track missing coverage
    my $rhCoverage = {};

    # Find all lcov files in the coverage path
    my $rhManifest = $oStorage->manifest($strCoveragePath);

    foreach my $strFileCov (sort(keys(%{$rhManifest})))
    {
        if ($strFileCov =~ /\.lcov$/)
        {
            my $strCoverage = ${$oStorage->get("${strCoveragePath}/${strFileCov}")};

            # Show that the file is part of the coverage report even if there is no missing coverage
            my $strFile;

            my $iBranchLine = -1;
            my $iBranch = undef;
            my $iBranchIdx = -1;
            my $iBranchPart = undef;

            foreach my $strLine (split("\n", $strCoverage))
            {
                # Get source file name
                if ($strLine =~ /^SF\:/)
                {
                    $strFile = substr($strLine, 3);
                    $rhCoverage->{$strFile} = undef;

                    # Generate a random anchor so new reports will not show links as already followed.  This is also an easy way
                    # to create valid, disambiguos links.
                    $rhCoverage->{$strFile}{anchor} = sha1_hex(rand(16));
                }
                # Mark functions as initially covered
                if ($strLine =~ /^FN\:/)
                {
                    my ($strLineBegin) = split("\,", substr($strLine, 3));
                    $rhCoverage->{$strFile}{function}{sprintf("%09d", $strLineBegin - 1)}{covered} = true;
                }
                # Check branch coverage
                elsif ($strLine =~ /^BRDA\:/)
                {
                    my @stryData = split("\,", substr($strLine, 5));

                    if (@stryData < 4)
                    {
                        confess &log(ERROR, "'${strLine}' should have four fields");
                    }

                    my $strBranchLine = sprintf("%09d", $stryData[0]);

                    if ($iBranchLine != $stryData[0])
                    {
                        $iBranchLine = $stryData[0] + 0;
                        $iBranch = $stryData[1] + 0;
                        $iBranchIdx = 0;
                        $iBranchPart = 0;
                    }
                    elsif ($iBranch != $stryData[1])
                    {
                        if ($iBranchPart != 1)
                        {
                            confess &log(ERROR, "line ${iBranchLine}, branch ${iBranch} does not have at least two parts");
                        }

                        $iBranch = $stryData[1] + 0;
                        $iBranchIdx++;
                        $iBranchPart = 0;
                    }
                    else
                    {
                        $iBranchPart++;
                    }

                    $rhCoverage->{$strFile}{line}{$strBranchLine}{branch}{$iBranchIdx}{$iBranchPart} =
                        $stryData[3] eq '-' || $stryData[3] eq '0' ? false : true;

                    # If the branch is uncovered then the function is uncovered
                    if (!$rhCoverage->{$strFile}{line}{$strBranchLine}{branch}{$iBranchIdx}{$iBranchPart})
                    {
                        coverageGenerateFunction($rhCoverage->{$strFile}, $strBranchLine)->{covered} = false;
                    }
                }

                # Check line coverage
                if ($strLine =~ /^DA\:/)
                {
                    my @stryData = split("\,", substr($strLine, 3));

                    if (@stryData < 2)
                    {
                        confess &log(ERROR, "'${strLine}' should have two fields");
                    }

                    my $strStatementLine = sprintf("%09d", $stryData[0]);

                    # If the statement is uncovered then the function is uncovered
                    if ($stryData[1] eq '0')
                    {
                        $rhCoverage->{$strFile}{line}{$strStatementLine}{statement} = 0;
                        coverageGenerateFunction($rhCoverage->{$strFile}, $strStatementLine)->{covered} = false;
                    }
                }
            }
        }
    }

    # Report on the entire function if any branches/lines in the function are uncovered
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        my $bFileCovered = true;

        # Proceed if there is some coverage data
        if (defined($rhCoverage->{$strFile}{line}))
        {
            my @stryC = split("\n", ${$oStorage->get($strFile)});
            my $bInUncoveredFunction = false;

            # Iterate every line in the C file
            for (my $iLineIdx = 0; $iLineIdx < @stryC; $iLineIdx++)
            {
                my $iLine = sprintf("%09d", $iLineIdx + 1);

                # If not in an uncovered function see if this line is the start of an uncovered function
                if (!$bInUncoveredFunction)
                {
                    $bInUncoveredFunction =
                        defined($rhCoverage->{$strFile}{function}{$iLine}) && !$rhCoverage->{$strFile}{function}{$iLine}{covered};

                    # If any function is uncovered then the file is uncovered
                    if ($bInUncoveredFunction)
                    {
                        $bFileCovered = false;
                    }
                }

                # If not in an uncovered function remove coverage
                if (!$bInUncoveredFunction)
                {
                    delete($rhCoverage->{$strFile}{line}{$iLine});
                }
                # Else in an uncovered function
                else
                {
                    # If there is no coverage for this line define it so it will show up on the report
                    if (!defined($rhCoverage->{$strFile}{line}{$iLine}))
                    {
                        $rhCoverage->{$strFile}{line}{$iLine} = undef;
                    }

                    # Stop processing at the function end brace. This depends on the file being formated correctly, but worst case
                    # is we run on a display the entire file rather than just uncovered functions.
                    if ($stryC[$iLineIdx] =~ '^\}')
                    {
                        $bInUncoveredFunction = false;
                    }
                }
            }
        }

        # Remove coverage info when file is fully covered
        if ($bFileCovered)
        {
            delete($rhCoverage->{$strFile}{line});
        }
    }

    # Build html
    my $strTitle = PROJECT_NAME . ' Coverage Report';
    my $strDarkRed = '#580000';
    my $strGray = '#555555';
    my $strDarkGray = '#333333';

    my $oHtml = new pgBackRestDoc::Html::DocHtmlBuilder(
        PROJECT_NAME, $strTitle,
        undef, undef, undef,
        true, true,

        "html\n" .
        "{\n" .
        "    background-color: ${strGray};\n" .
        "    font-family: Avenir, Corbel, sans-serif;\n" .
        "    color: white;\n" .
        "    font-size: 12pt;\n" .
        "    margin-top: 8px;\n" .
        "    margin-left: 1\%;\n" .
        "    margin-right: 1\%;\n" .
        "    width: 98\%;\n" .
        "}\n" .
        "\n" .
        "body\n" .
        "{\n" .
        "    margin: 0px auto;\n" .
        "    padding: 0px;\n" .
        "    width: 100\%;\n" .
        "    text-align: justify;\n" .
        "}\n" .

        ".title\n" .
        "{\n" .
        "    width: 100\%;\n" .
        "    text-align: center;\n" .
        "    font-size: 200\%;\n" .
        "}\n" .

        "\n" .
        ".list-table\n" .
        "{\n" .
        "    width: 100\%;\n" .
        "}\n" .

        "\n" .
        ".list-table-caption\n" .
        "{\n" .
        "    margin-top: 1em;\n" .
        "    font-size: 130\%;\n" .
        "    margin-bottom: .25em;\n" .
        "}\n" .

        "\n" .
        ".list-table-caption::after\n" .
        "{\n" .
        "    content: \"Modules Tested for Coverage:\";\n" .
        "}\n" .

        "\n" .
        ".list-table-header-file\n" .
        "{\n" .
        "    padding-left: .5em;\n" .
        "    padding-right: .5em;\n" .
        "    background-color: ${strDarkGray};\n" .
        "    width: 100\%;\n" .
        "}\n" .

        "\n" .
        ".list-table-row-uncovered\n" .
        "{\n" .
        "    background-color: ${strDarkRed};\n" .
        "    color: white;\n" .
        "    width: 100\%;\n" .
        "}\n" .

        "\n" .
        ".list-table-row-file\n" .
        "{\n" .
        "    padding-left: .5em;\n" .
        "    padding-right: .5em;\n" .
        "}\n" .

        "\n" .
        ".report-table\n" .
        "{\n" .
        "    width: 100\%;\n" .
        "}\n" .

        "\n" .
        ".report-table-caption\n" .
        "{\n" .
        "    margin-top: 1em;\n" .
        "    font-size: 130\%;\n" .
        "    margin-bottom: .25em;\n" .
        "}\n" .

        "\n" .
        ".report-table-caption::after\n" .
        "{\n" .
        "    content: \" report:\";\n" .
        "}\n" .

        "\n" .
        ".report-table-header\n" .
        "{\n" .
        "}\n" .

        "\n" .
        ".report-table-header-line, .report-table-header-branch, .report-table-header-code\n" .
        "{\n" .
        "    padding-left: .5em;\n" .
        "    padding-right: .5em;\n" .
        "    background-color: ${strDarkGray};\n" .
        "}\n" .

        "\n" .
        ".report-table-header-code\n" .
        "{\n" .
        "    width: 100\%;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-dot-tr, .report-table-row\n" .
        "{\n" .
        "    font-family: \"Courier New\", Courier, monospace;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-dot-skip\n" .
        "{\n" .
        "    height: 1em;\n" .
        "    padding-top: .25em;\n" .
        "    padding-bottom: .25em;\n" .
        "    text-align: center;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-line, .report-table-row-branch, .report-table-row-branch-uncovered," .
            " .report-table-row-code, .report-table-row-code-uncovered\n" .
        "{\n" .
        "    padding-left: .5em;\n" .
        "    padding-right: .5em;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-line\n" .
        "{\n" .
        "    text-align: right;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-branch, .report-table-row-branch-uncovered\n" .
        "{\n" .
        "    text-align: right;\n" .
        "    white-space: nowrap;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-branch-uncovered\n" .
        "{\n" .
        "    background-color: ${strDarkRed};\n" .
        "    color: white;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-code, .report-table-row-code-uncovered\n" .
        "{\n" .
        "    white-space: pre;\n" .
        "}\n" .

        "\n" .
        ".report-table-row-code-uncovered\n" .
        "{\n" .
        "    background-color: ${strDarkRed};\n" .
        "    color: white;\n" .
        "}\n");

    # File list title
    $oHtml->bodyGet()->addNew(HTML_DIV, 'title', {strContent => $strTitle});

    # Build the file list table
    $oHtml->bodyGet()->addNew(HTML_DIV, 'list-table-caption');

    my $oTable = $oHtml->bodyGet()->addNew(HTML_TABLE, 'list-table');

    my $oHeader = $oTable->addNew(HTML_TR, 'list-table-header');
    $oHeader->addNew(HTML_TH, 'list-table-header-file', {strContent => 'FILE'});

    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        my $oRow = $oTable->addNew(HTML_TR, 'list-table-row-' . (defined($rhCoverage->{$strFile}{line}) ? 'uncovered' : 'covered'));
        my $strFileDisplay = substr($strFile, length($strBasePath) + 1);

        # Link only created when file is uncovered
        if (defined($rhCoverage->{$strFile}{line}))
        {
            $oRow->addNew(HTML_TD, 'list-table-row-file')->addNew(
                HTML_A, undef, {strContent => $strFileDisplay, strRef => '#' . $rhCoverage->{$strFile}{anchor}});
        }
        # Else just show the file name
        else
        {
            $oRow->addNew(HTML_TD, 'list-table-row-file', {strContent => $strFileDisplay});
        }
    }

    # Report on files that are missing coverage
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        my $strFileDisplay = substr($strFile, length($strBasePath) + 1);

        if (defined($rhCoverage->{$strFile}{line}))
        {
            # Anchor only created when file is uncovered
            $oHtml->bodyGet()->addNew(HTML_A, undef, {strId => $rhCoverage->{$strFile}{anchor}});

            # Report table caption, i.e. the uncovered file name
            $oHtml->bodyGet()->addNew(HTML_DIV, 'report-table-caption', {strContent => $strFileDisplay});

            # Build the file report table
            $oTable = $oHtml->bodyGet()->addNew(HTML_TABLE, 'report-table');

            $oHeader = $oTable->addNew(HTML_TR, 'report-table-header');
            $oHeader->addNew(HTML_TH, 'report-table-header-line', {strContent => 'LINE'});
            $oHeader->addNew(HTML_TH, 'report-table-header-branch', {strContent => 'BRANCH'});
            $oHeader->addNew(HTML_TH, 'report-table-header-code', {strContent => 'CODE'});

            my $strC = ${$oStorage->get($strFile)};
            my @stryC = split("\n", $strC);
            my $iLastLine = undef;

            foreach my $strLine (sort(keys(%{$rhCoverage->{$strFile}{line}})))
            {
                if (defined($iLastLine) && $strLine != $iLastLine + 1)
                {
                    my $oRow = $oTable->addNew(HTML_TR, 'report-table-row-dot');
                    $oRow->addNew(HTML_TD, 'report-table-row-dot-skip', {strExtra => 'colspan="3"'});
                }

                $iLastLine = $strLine;
                my $iLine = int($strLine);

                my $oRow = $oTable->addNew(HTML_TR, 'report-table-row');
                $oRow->addNew(HTML_TD, 'report-table-row-line', {strContent => $iLine});
                my $strBranch;

                # Show missing branch coverage
                my $bBranchCovered = true;

                if (defined($rhCoverage->{$strFile}{line}{$strLine}{branch}))
                {
                    foreach my $iBranch (sort(keys(%{$rhCoverage->{$strFile}{line}{$strLine}{branch}})))
                    {
                        $strBranch .= '[';

                        my $bBranchPartFirst = true;

                        foreach my $iBranchPart (sort(keys(%{$rhCoverage->{$strFile}{line}{$strLine}{branch}{$iBranch}})))
                        {
                            if (!$bBranchPartFirst)
                            {
                                $strBranch .= ' ';
                            }

                            if ($rhCoverage->{$strFile}{line}{$strLine}{branch}{$iBranch}{$iBranchPart})
                            {
                                $strBranch .= '+';
                            }
                            else
                            {
                                $strBranch .= '-';
                                $bBranchCovered = false;
                            }

                            $bBranchPartFirst = false;
                        }

                        $strBranch .= ']';
                    }
                }

                $oRow->addNew(
                    HTML_TD, 'report-table-row-branch' . (!$bBranchCovered ? '-uncovered' : ''), {strContent => $strBranch});

                # Color code based on coverage
                $oRow->addNew(
                    HTML_TD,
                    'report-table-row-code' . (defined($rhCoverage->{$strFile}{line}{$strLine}{statement}) ? '-uncovered' : ''),
                    {bPre => true, strContent => $stryC[$strLine - 1]});
            }
        }
    }

    # Write coverage report
    $oStorage->put($strOutFile, $oHtml->htmlGet());
}

push @EXPORT, qw(coverageGenerate);

####################################################################################################################################
# Generate a C coverage summary for the documentation
####################################################################################################################################
sub coverageDocSummaryGenerateValue
{
    my $iHit = shift;
    my $iFound = shift;

    if (!defined($iFound) || !defined($iHit) || $iFound == 0)
    {
        return "---";
    }

    my $fPercent = $iHit * 100 / $iFound;
    my $strPercent;

    if ($fPercent == 100)
    {
        $strPercent = '100.0';
    }
    elsif ($fPercent > 99.99)
    {
        $strPercent = '99.99';
    }
    else
    {
        $strPercent = sprintf("%.2f", $fPercent);
    }

    return "${iHit}/${iFound} (${strPercent}%)";
}

sub coverageDocSummaryGenerate
{
    my $oStorage = shift;
    my $strCoveragePath = shift;
    my $strOutFile = shift;

    # Track coverage summary
    my $rhSummary;

    # Find all lcov files in the coverage path
    my $rhManifest = $oStorage->manifest($strCoveragePath);

    foreach my $strFileCov (sort(keys(%{$rhManifest})))
    {
        next if $strFileCov =~ /^test\//;

        if ($strFileCov =~ /\.lcov$/)
        {
            my $strCoverage = ${$oStorage->get("${strCoveragePath}/${strFileCov}")};
            my $strModule = dirname($strFileCov);

            foreach my $strLine (split("\n", $strCoverage))
            {
                # Get Line Coverage
                if ($strLine =~ /^LF\:/)
                {
                    $rhSummary->{$strModule}{line}{found} += substr($strLine, 3) + 0;
                    $rhSummary->{zzztotal}{line}{found} += substr($strLine, 3) + 0;
                }

                if ($strLine =~ /^LH\:/)
                {
                    $rhSummary->{$strModule}{line}{hit} += substr($strLine, 3) + 0;
                    $rhSummary->{zzztotal}{line}{hit} += substr($strLine, 3) + 0;
                }

                # Get Function Coverage
                if ($strLine =~ /^FNF\:/)
                {
                    $rhSummary->{$strModule}{function}{found} += substr($strLine, 4) + 0;
                    $rhSummary->{zzztotal}{function}{found} += substr($strLine, 4) + 0;
                }

                if ($strLine =~ /^FNH\:/)
                {
                    $rhSummary->{$strModule}{function}{hit} += substr($strLine, 4) + 0;
                    $rhSummary->{zzztotal}{function}{hit} += substr($strLine, 4) + 0;
                }

                # Get Branch Coverage
                if ($strLine =~ /^BRF\:/)
                {
                    $rhSummary->{$strModule}{branch}{found} += substr($strLine, 4) + 0;
                    $rhSummary->{zzztotal}{branch}{found} += substr($strLine, 4) + 0;
                }

                if ($strLine =~ /^BRH\:/)
                {
                    $rhSummary->{$strModule}{branch}{hit} += substr($strLine, 4) + 0;
                    $rhSummary->{zzztotal}{branch}{hit} += substr($strLine, 4) + 0;
                }
            }
        }
    }

    # use Data::Dumper;confess Dumper($rhSummary);

    my $strSummary;

    foreach my $strModule (sort(keys(%{$rhSummary})))
    {
        my $rhModuleData = $rhSummary->{$strModule};

        $strSummary .=
            (defined($strSummary) ? "\n\n" : '') .
            "<table-row>\n" .
            "    <table-cell>" . ($strModule eq 'zzztotal' ? 'TOTAL' : $strModule) . "</table-cell>\n" .
            "    <table-cell>" .
                coverageDocSummaryGenerateValue($rhModuleData->{function}{hit}, $rhModuleData->{function}{found}) .
                "</table-cell>\n" .
            "    <table-cell>" .
                coverageDocSummaryGenerateValue($rhModuleData->{branch}{hit}, $rhModuleData->{branch}{found}) .
                "</table-cell>\n" .
            "    <table-cell>" .
                coverageDocSummaryGenerateValue($rhModuleData->{line}{hit}, $rhModuleData->{line}{found}) .
                "</table-cell>\n" .
            "</table-row>";
    }


    # Write coverage report
    $oStorage->put($strOutFile, $strSummary);
}

push @EXPORT, qw(coverageDocSummaryGenerate);

1;
