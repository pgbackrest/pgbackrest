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

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

use BackRestDoc::Html::DocHtmlBuilder;
use BackRestDoc::Html::DocHtmlElement;

####################################################################################################################################
# Generate a C coverage report
####################################################################################################################################
sub coverageGenerate
{
    my $oStorage = shift;
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
            my $strFile = substr($strFileCov, 0, length($strFileCov) - 5) . '.c';
            $rhCoverage->{$strFile} = undef;

            my $iBranchLine = -1;
            my $iBranch = undef;
            my $iBranchIdx = -1;
            my $iBranchPart = undef;

            foreach my $strLine (split("\n", $strCoverage))
            {
                # Check branch coverage
                if ($strLine =~ /^BRDA\:/)
                {
                    my @stryData = split("\,", substr($strLine, 5));

                    if (@stryData < 4)
                    {
                        confess &log(ERROR, "'${strLine}' should have four fields");
                    }

                    my $strBranchLine = sprintf("%09d", $stryData[0]);

                    if ($iBranchLine != $stryData[0])
                    {
                        $iBranchLine = $stryData[0];
                        $iBranch = $stryData[1];
                        $iBranchIdx = 0;
                        $iBranchPart = 0;
                    }
                    elsif ($iBranch != $stryData[1])
                    {
                        if ($iBranchPart != 1)
                        {
                            confess &log(ERROR, "line ${iBranchLine}, branch ${iBranch} does not have two parts");
                        }

                        $iBranch = $stryData[1];
                        $iBranchIdx++;
                        $iBranchPart = 0;
                    }
                    else
                    {
                        $iBranchPart++;
                    }

                    $rhCoverage->{$strFile}{line}{$strBranchLine}{branch}{$iBranchIdx}{$iBranchPart} =
                        $stryData[3] eq '-' || $stryData[3] eq '0' ? false : true;
                }

                # Check line coverage
                if ($strLine =~ /^DA\:/)
                {
                    my @stryData = split("\,", substr($strLine, 3));

                    if (@stryData < 2)
                    {
                        confess &log(ERROR, "'${strLine}' should have two fields");
                    }

                    my $strBranchLine = sprintf("%09d", $stryData[0]);

                    if ($stryData[1] eq '0')
                    {
                        $rhCoverage->{$strFile}{line}{$strBranchLine}{statement} = 0;
                    }
                }
            }
        }
    }

    # Remove branch coverage on lines that are completely covered
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        foreach my $iLine (sort(keys(%{$rhCoverage->{$strFile}{line}})))
        {
            if (defined($rhCoverage->{$strFile}{line}{$iLine}{branch}))
            {
                # We'll assume the line is completely covered
                my $bCovered = true;

                foreach my $iBranch (sort(keys(%{$rhCoverage->{$strFile}{line}{$iLine}{branch}})))
                {
                    foreach my $iBranchPart (sort(keys(%{$rhCoverage->{$strFile}{line}{$iLine}{branch}{$iBranch}})))
                    {
                        if (!$rhCoverage->{$strFile}{line}{$iLine}{branch}{$iBranch}{$iBranchPart})
                        {
                            $bCovered = false;
                        }
                    }
                }

                if ($bCovered)
                {
                    # &log(WARN, "removed branch coverage for ${strFile} line ${iLine}");
                    if (defined($rhCoverage->{$strFile}{line}{$iLine}{statement}))
                    {
                        delete($rhCoverage->{$strFile}{line}{$iLine}{branch});
                    }
                    else
                    {
                        delete($rhCoverage->{$strFile}{line}{$iLine});
                    }
                }
            }
        }
    }

    # Remove line when no lines are uncovered
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        my $bCovered = true;

        foreach my $iLine (sort(keys(%{$rhCoverage->{$strFile}{line}})))
        {
            $bCovered = false;
            last;
        }

        if ($bCovered)
        {
            delete($rhCoverage->{$strFile}{line});
        }
    }

    # Report on the entire function if any lines in the function are uncovered
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        if (defined($rhCoverage->{$strFile}{line}))
        {
            my $strC = ${$oStorage->get("${strCoveragePath}/${strFile}")};
            my @stryC = split("\n", $strC);

            foreach my $iLine (sort(keys(%{$rhCoverage->{$strFile}{line}})))
            {
                # Run back to the beginning of the function comment
                for (my $iLineIdx = $iLine; $iLineIdx >= 0; $iLineIdx--)
                {
                    if (!defined($rhCoverage->{$strFile}{line}{sprintf("%09d", $iLineIdx)}))
                    {
                        $rhCoverage->{$strFile}{line}{sprintf("%09d", $iLineIdx)} = undef;
                    }

                    last if ($stryC[$iLineIdx - 1] =~ '^\/\*');
                }

                # Run forward to the end of the function
                for (my $iLineIdx = $iLine; $iLineIdx < @stryC; $iLineIdx++)
                {
                    if (!defined($rhCoverage->{$strFile}{line}{sprintf("%09d", $iLineIdx)}))
                    {
                        $rhCoverage->{$strFile}{line}{sprintf("%09d", $iLineIdx)} = undef;
                    }

                    last if ($stryC[$iLineIdx - 1] eq '}');
                }
            }
        }
    }

    # Build html
    my $strTitle = PROJECT_NAME . ' Coverage Report';
    my $strDarkRed = '#580000';
    my $strGray = '#555555';
    my $strDarkGray = '#333333';

    my $oHtml = new BackRestDoc::Html::DocHtmlBuilder(
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

    $oHtml->bodyGet()->addNew(HTML_DIV, 'title', {strContent => $strTitle});

    # Build the file list table
    my $oTable = $oHtml->bodyGet()->addNew(HTML_TABLE, 'list-table');
    $oTable->addNew(HTML_DIV, 'list-table-caption');

    my $oHeader = $oTable->addNew(HTML_TR, 'list-table-header');
    $oHeader->addNew(HTML_TH, 'list-table-header-file', {strContent => 'FILE'});

    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        my $oRow = $oTable->addNew(HTML_TR, 'list-table-row-' . (defined($rhCoverage->{$strFile}{line}) ? 'uncovered' : 'covered'));
        $oRow->addNew(HTML_TD, 'list-table-row-file', {strContent => $strFile});
    }

    # Report on files that are missing coverage
    foreach my $strFile (sort(keys(%{$rhCoverage})))
    {
        if (defined($rhCoverage->{$strFile}{line}))
        {
            # Build the file report table
            $oTable = $oHtml->bodyGet()->addNew(HTML_TABLE, 'report-table');

            $oTable->addNew(HTML_DIV, 'report-table-caption', {strContent => "${strFile}"});

            $oHeader = $oTable->addNew(HTML_TR, 'report-table-header');
            $oHeader->addNew(HTML_TH, 'report-table-header-line', {strContent => 'LINE'});
            $oHeader->addNew(HTML_TH, 'report-table-header-branch', {strContent => 'BRANCH'});
            $oHeader->addNew(HTML_TH, 'report-table-header-code', {strContent => 'CODE'});

            my $strC = ${$oStorage->get("${strCoveragePath}/${strFile}")};
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
                if (defined($rhCoverage->{$strFile}{line}{$strLine}{branch}) &&
                    !defined($rhCoverage->{$strFile}{line}{$strLine}{statement}))
                {
                    my $iBranchIdx = 0;

                    foreach my $iBranch (sort(keys(%{$rhCoverage->{$strFile}{line}{$strLine}{branch}})))
                    {
                        $strBranch .=
                            '[' .
                            ($rhCoverage->{$strFile}{line}{$strLine}{branch}{$iBranch}{0} ?
                                '+' : '-') .
                            ' ' .
                            ($rhCoverage->{$strFile}{line}{$strLine}{branch}{$iBranch}{1} ?
                                '+' : '-') .
                            ']';

                        if ($iBranchIdx == 1)
                        {
                            $strBranch .= '<br/>';
                            $iBranchIdx = 0;
                        }
                        else
                        {
                            $strBranch .= ' ';
                            $iBranchIdx++;
                        }
                    }
                }

                $oRow->addNew(
                    HTML_TD, 'report-table-row-branch' . (defined($strBranch) ? '-uncovered' : ''),
                    {strContent => $strBranch});

                # Color code based on coverage
                my $bUncovered =
                    defined($rhCoverage->{$strFile}{line}{$strLine}{branch}) ||
                    defined($rhCoverage->{$strFile}{line}{$strLine}{statement});

                $oRow->addNew(
                    HTML_TD, 'report-table-row-code' . ($bUncovered ? '-uncovered' : ''),
                    {bPre => true, strContent => $stryC[$strLine - 1]});
            }
        }
    }

    # Write coverage report
    $oStorage->put($strOutFile, $oHtml->htmlGet());
}

push @EXPORT, qw(coverageGenerate);

1;
