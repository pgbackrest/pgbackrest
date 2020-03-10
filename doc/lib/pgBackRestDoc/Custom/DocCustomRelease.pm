####################################################################################################################################
# DOC RELEASE MODULE
####################################################################################################################################
package pgBackRestDoc::Custom::DocCustomRelease;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRestBuild::Config::Data;

use pgBackRestDoc::Common::DocRender;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

####################################################################################################################################
# XML node constants
####################################################################################################################################
use constant XML_PARAM_ID                                           => 'id';

use constant XML_CONTRIBUTOR_LIST                                   => 'contributor-list';
use constant XML_CONTRIBUTOR                                        => 'contributor';
use constant XML_CONTRIBUTOR_NAME_DISPLAY                           => 'contributor-name-display';

use constant XML_RELEASE_CORE_LIST                                  => 'release-core-list';
use constant XML_RELEASE_DOC_LIST                                   => 'release-doc-list';
use constant XML_RELEASE_TEST_LIST                                  => 'release-test-list';

use constant XML_RELEASE_BUG_LIST                                   => 'release-bug-list';
use constant XML_RELEASE_DEVELOPMENT_LIST                           => 'release-development-list';
use constant XML_RELEASE_FEATURE_LIST                               => 'release-feature-list';
use constant XML_RELEASE_IMPROVEMENT_LIST                           => 'release-improvement-list';

use constant XML_RELEASE_ITEM_CONTRIBUTOR_LIST                      => 'release-item-contributor-list';

use constant XML_RELEASE_ITEM_CONTRIBUTOR                           => 'release-item-contributor';
use constant XML_RELEASE_ITEM_IDEATOR                               => 'release-item-ideator';
use constant XML_RELEASE_ITEM_REVIEWER                              => 'release-item-reviewer';

####################################################################################################################################
# Contributor text constants
####################################################################################################################################
use constant TEXT_CONTRIBUTED                                       => 'Contributed';
use constant TEXT_FIXED                                             => 'Fixed';
use constant TEXT_FOUND                                             => 'Reported';
use constant TEXT_REVIEWED                                          => 'Reviewed';
use constant TEXT_SUGGESTED                                         => 'Suggested';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oDoc},
        $self->{bDev},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDoc'},
            {name => 'bDev', required => false, default => false},
        );

    # Get contributor list
    foreach my $oContributor ($self->{oDoc}->nodeGet(XML_CONTRIBUTOR_LIST)->nodeList(XML_CONTRIBUTOR))
    {
        my $strContributorId = $oContributor->paramGet(XML_PARAM_ID);

        if (!defined($self->{hContributor}))
        {
            $self->{hContributor} = {};
            $self->{strContributorDefault} = $strContributorId;
        }

        ${$self->{hContributor}}{$strContributorId}{name} = $oContributor->fieldGet(XML_CONTRIBUTOR_NAME_DISPLAY);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# currentStableVersion
#
# Return the current stable version.
####################################################################################################################################
sub currentStableVersion
{
    my $self = shift;

    my $oDoc = $self->{oDoc};

    foreach my $oRelease ($oDoc->nodeGet('release-list')->nodeList('release'))
    {
        my $strVersion = $oRelease->paramGet('version');

        if ($strVersion !~ /dev$/)
        {
            return $strVersion;
        }
    }

    confess &log(ERROR, "unable to find non-development version");
}


####################################################################################################################################
# releaseLast
#
# Get the last release.
####################################################################################################################################
sub releaseLast
{
    my $self = shift;

    my $oDoc = $self->{oDoc};

    foreach my $oRelease ($oDoc->nodeGet('release-list')->nodeList('release'))
    {
        return $oRelease;
    }
}

####################################################################################################################################
# contributorTextGet
#
# Get a list of contributors for an item in text format.
####################################################################################################################################
sub contributorTextGet
{
    my $self = shift;
    my $oReleaseItem = shift;
    my $strItemType = shift;

    my $strContributorText;
    my $hItemContributorType = {};

    # Create a the list of contributors
    foreach my $strContributorType (XML_RELEASE_ITEM_IDEATOR, XML_RELEASE_ITEM_CONTRIBUTOR, XML_RELEASE_ITEM_REVIEWER)
    {
        my $stryItemContributor = [];

        if ($oReleaseItem->nodeTest(XML_RELEASE_ITEM_CONTRIBUTOR_LIST))
        {
            foreach my $oContributor ($oReleaseItem->nodeGet(XML_RELEASE_ITEM_CONTRIBUTOR_LIST)->
                                      nodeList($strContributorType, false))
            {
                push @{$stryItemContributor}, $oContributor->paramGet(XML_PARAM_ID);
            }
        }

        if (@$stryItemContributor == 0 && $strContributorType eq XML_RELEASE_ITEM_CONTRIBUTOR)
        {
            push @{$stryItemContributor}, $self->{strContributorDefault}
        }

        $$hItemContributorType{$strContributorType} = $stryItemContributor;
    }

    # Error if a reviewer is also a contributor
    foreach my $strReviewer (@{$$hItemContributorType{&XML_RELEASE_ITEM_REVIEWER}})
    {
        foreach my $strContributor (@{$$hItemContributorType{&XML_RELEASE_ITEM_CONTRIBUTOR}})
        {
            if ($strReviewer eq $strContributor)
            {
                confess &log(ERROR, "${strReviewer} cannot be both a contributor and a reviewer");
            }
        }
    }

    # Error if the ideator list is the same as the contributor list
    if (join(',', @{$$hItemContributorType{&XML_RELEASE_ITEM_IDEATOR}}) eq
        join(',', @{$$hItemContributorType{&XML_RELEASE_ITEM_CONTRIBUTOR}}))
    {
        confess &log(ERROR, 'cannot have same contributor and ideator list: ' .
                     join(', ', @{$$hItemContributorType{&XML_RELEASE_ITEM_CONTRIBUTOR}}));
    }

    # Remove the default user if they are the only one in a group (to prevent the entire page from being splattered with one name)
    foreach my $strContributorType (XML_RELEASE_ITEM_IDEATOR, XML_RELEASE_ITEM_CONTRIBUTOR, XML_RELEASE_ITEM_REVIEWER)
    {
        if (@{$$hItemContributorType{$strContributorType}} == 1 &&
            @{$$hItemContributorType{$strContributorType}}[0] eq $self->{strContributorDefault})
        {
            $$hItemContributorType{$strContributorType} = [];
        }
    }

    # Render the string
    foreach my $strContributorType (XML_RELEASE_ITEM_CONTRIBUTOR, XML_RELEASE_ITEM_REVIEWER, XML_RELEASE_ITEM_IDEATOR)
    {
        my $stryItemContributor = $$hItemContributorType{$strContributorType};
        my $strContributorTypeText;

        foreach my $strContributor (@{$stryItemContributor})
        {
            my $hContributor = ${$self->{hContributor}}{$strContributor};

            if (!defined($hContributor))
            {
                confess &log(ERROR, "contributor ${strContributor} does not exist");
            }

            $strContributorTypeText .= (defined($strContributorTypeText) ? ', ' : '') . $$hContributor{name};
        }

        if (defined($strContributorTypeText))
        {
            $strContributorTypeText = ' by ' . $strContributorTypeText . '.';

            if ($strContributorType eq XML_RELEASE_ITEM_CONTRIBUTOR)
            {
                $strContributorTypeText = ($strItemType eq 'bug' ? TEXT_FIXED : TEXT_CONTRIBUTED) . $strContributorTypeText;
            }
            elsif ($strContributorType eq XML_RELEASE_ITEM_IDEATOR)
            {
                $strContributorTypeText = ($strItemType eq 'bug' ? TEXT_FOUND : TEXT_SUGGESTED) . $strContributorTypeText;
            }
            elsif ($strContributorType eq XML_RELEASE_ITEM_REVIEWER)
            {
                $strContributorTypeText = TEXT_REVIEWED . $strContributorTypeText;
            }

            $strContributorText .= (defined($strContributorText) ? ' ' : '') . $strContributorTypeText;
        }
    }

    return $strContributorText;
}

####################################################################################################################################
# Find a commit by subject prefix.  Error if the prefix appears more than once.
####################################################################################################################################
sub commitFindSubject
{
    my $self = shift;
    my $rhyCommit = shift;
    my $strSubjectPrefix = shift;
    my $bRegExp = shift;

    $bRegExp = defined($bRegExp) ? $bRegExp : true;
    my $rhResult = undef;

    foreach my $rhCommit (@{$rhyCommit})
    {
        if (($bRegExp && $rhCommit->{subject} =~ /^$strSubjectPrefix/) ||
            (!$bRegExp && length($rhCommit->{subject}) >= length($strSubjectPrefix) &&
                substr($rhCommit->{subject}, 0, length($strSubjectPrefix)) eq $strSubjectPrefix))
        {
            if (defined($rhResult))
            {
                confess &log(ERROR, "subject prefix '${strSubjectPrefix}' already found in commit " . $rhCommit->{commit});
            }

            $rhResult = $rhCommit;
        }
    }

    return $rhResult;
}

####################################################################################################################################
# Throw an error that includes a list of release commits
####################################################################################################################################
sub commitError
{
    my $self = shift;
    my $strMessage = shift;
    my $rstryCommitRemaining = shift;
    my $rhyCommit = shift;

    my $strList;

    foreach my $strCommit (@{$rstryCommitRemaining})
    {
        $strList .=
            (defined($strList) ? "\n" : '') .
            substr($rhyCommit->{$strCommit}{date}, 0, length($rhyCommit->{$strCommit}{date}) - 15) . " $strCommit: " .
            $rhyCommit->{$strCommit}{subject};
    }

    confess &log(ERROR, "${strMessage}:\n${strList}");
}

####################################################################################################################################
# docGet
#
# Get the xml for release.
####################################################################################################################################
sub docGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->docGet');

    # Load the git history
    my $oStorageDoc = new pgBackRestTest::Common::Storage(
        dirname(abs_path($0)), new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));
    my @hyGitLog = @{(JSON::PP->new()->allow_nonref())->decode(${$oStorageDoc->get("resource/git-history.cache")})};

    # Get renderer
    my $oRender = new pgBackRestDoc::Common::DocRender('text');
    $oRender->tagSet('backrest', PROJECT_NAME);

    # Create the doc
    my $oDoc = new pgBackRestDoc::Common::Doc();
    $oDoc->paramSet('title', $self->{oDoc}->paramGet('title'));
    $oDoc->paramSet('toc-number', $self->{oDoc}->paramGet('toc-number'));

    # Set the description for use as a meta tag
    $oDoc->fieldSet('description', $self->{oDoc}->fieldGet('description'));

    # Add the introduction
    my $oIntroSectionDoc = $oDoc->nodeAdd('section', undef, {id => 'introduction'});
    $oIntroSectionDoc->nodeAdd('title')->textSet('Introduction');
    $oIntroSectionDoc->textSet($self->{oDoc}->nodeGet('intro')->textGet());

    # Add each release section
    my $oSection;
    my $iDevReleaseTotal = 0;
    my $iCurrentReleaseTotal = 0;
    my $iStableReleaseTotal = 0;
    my $iUnsupportedReleaseTotal = 0;

    my @oyRelease = $self->{oDoc}->nodeGet('release-list')->nodeList('release');

    for (my $iReleaseIdx = 0; $iReleaseIdx < @oyRelease; $iReleaseIdx++)
    {
        my $oRelease = $oyRelease[$iReleaseIdx];

        # Get the release version and dev flag
        my $strVersion = $oRelease->paramGet('version');
        my $bReleaseDev = $strVersion =~ /dev$/ ? true : false;

        # Get a list of commits that apply to this release
        my @rhyReleaseCommit;
        my $rhReleaseCommitRemaining;
        my @stryReleaseCommitRemaining;
        my $bReleaseCheckCommit = false;

        if ($strVersion ge '2.01')
        {
            # Should commits in the release be checked?
            $bReleaseCheckCommit = !$bReleaseDev ? true : false;

            # Get the begin commit
            my $rhReleaseCommitBegin = $self->commitFindSubject(\@hyGitLog, "Begin v${strVersion} development\\.");
            my $strReleaseCommitBegin = defined($rhReleaseCommitBegin) ? $rhReleaseCommitBegin->{commit} : undef;

            # Get the end commit of the last release
            my $strReleaseLastVersion = $oyRelease[$iReleaseIdx + 1]->paramGet('version');
            my $rhReleaseLastCommitEnd =  $self->commitFindSubject(\@hyGitLog, "v${strReleaseLastVersion}\\: .+");

            if (!defined($rhReleaseLastCommitEnd))
            {
                confess &log(ERROR, "release ${strReleaseLastVersion} must have an end commit");
            }

            my $strReleaseLastCommitEnd = $rhReleaseLastCommitEnd->{commit};

            # Get the end commit
            my $rhReleaseCommitEnd = $self->commitFindSubject(\@hyGitLog, "v${strVersion}\\: .+");
            my $strReleaseCommitEnd = defined($rhReleaseCommitEnd) ? $rhReleaseCommitEnd->{commit} : undef;

            if ($bReleaseCheckCommit && !defined($rhReleaseCommitEnd) && $iReleaseIdx != 0)
            {
                confess &log(ERROR, "release ${strVersion} must have an end commit");
            }

            # Make a list of commits for this release
            while ($hyGitLog[0]->{commit} ne $strReleaseLastCommitEnd)
            {
                # Don't add begin/end commits to the list since they are already accounted for
                if ((defined($strReleaseCommitEnd) && $hyGitLog[0]->{commit} eq $strReleaseCommitEnd) ||
                    (defined($strReleaseCommitBegin) && $hyGitLog[0]->{commit} eq $strReleaseCommitBegin))
                {
                    shift(@hyGitLog);
                }
                # Else add the commit to this releases' list
                else
                {
                    push(@stryReleaseCommitRemaining, $hyGitLog[0]->{commit});
                    push(@rhyReleaseCommit, $hyGitLog[0]);

                    $rhReleaseCommitRemaining->{$hyGitLog[0]->{commit}}{date} = $hyGitLog[0]->{date};
                    $rhReleaseCommitRemaining->{$hyGitLog[0]->{commit}}{subject} = $hyGitLog[0]->{subject};

                    shift(@hyGitLog);
                }
            }

            # At least one commit is required for non-dev releases
            if ($bReleaseCheckCommit && @stryReleaseCommitRemaining == 0)
            {
                confess &log(ERROR, "no commits found for release ${strVersion}");
            }
        }

        # Display versions in TOC?
        my $bTOC = true;

        # Create a release section
        if ($bReleaseDev)
        {
            if ($iDevReleaseTotal > 1)
            {
                confess &log(ERROR, 'only one development release is allowed');
            }

            $oSection = $oDoc->nodeAdd('section', undef, {id => 'development', if => "'{[dev]}' eq 'y'"});
            $oSection->nodeAdd('title')->textSet("Development Notes");

            $iDevReleaseTotal++;
        }
        elsif ($iCurrentReleaseTotal == 0)
        {
            $oSection = $oDoc->nodeAdd('section', undef, {id => 'current'});
            $oSection->nodeAdd('title')->textSet("Current Stable Release");
            $iCurrentReleaseTotal++;
        }
        elsif ($strVersion ge '1.00')
        {
            if ($iStableReleaseTotal == 0)
            {
                $oSection = $oDoc->nodeAdd('section', undef, {id => 'supported'});
                $oSection->nodeAdd('title')->textSet("Stable Releases");
            }

            $iStableReleaseTotal++;
            $bTOC = false;
        }
        else
        {
            if ($iUnsupportedReleaseTotal == 0)
            {
                $oSection = $oDoc->nodeAdd('section', undef, {id => 'unsupported'});
                $oSection->nodeAdd('title')->textSet("Pre-Stable Releases");
            }

            $iUnsupportedReleaseTotal++;
            $bTOC = false;
        }

        # Format the date
        my $strDate = $oRelease->paramGet('date');
        my $strDateOut = "";

        my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                         'July', 'August', 'September', 'October', 'November', 'December');

        if ($strDate =~ /^X/)
        {
            $strDateOut .= 'No Release Date Set';
        }
        else
        {
            if ($strDate !~ /^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$/)
            {
                confess &log(ASSERT, "invalid date ${strDate} for release {$strVersion}");
            }

            $strDateOut .= 'Released ' . $stryMonth[(substr($strDate, 5, 2) - 1)] . ' ' .
                          (substr($strDate, 8, 2) + 0) . ', ' . substr($strDate, 0, 4);
        }

        # Add section and titles
        my $oReleaseSection = $oSection->nodeAdd('section', undef, {id => $strVersion, toc => !$bTOC ? 'n' : undef});
        $oReleaseSection->paramSet(XML_SECTION_PARAM_ANCHOR, XML_SECTION_PARAM_ANCHOR_VALUE_NOINHERIT);

        $oReleaseSection->nodeAdd('title')->textSet(
            "v${strVersion} " . ($bReleaseDev ? '' : 'Release ') . 'Notes');

        $oReleaseSection->nodeAdd('subtitle')->textSet($oRelease->paramGet('title'));
        $oReleaseSection->nodeAdd('subsubtitle')->textSet($strDateOut);

        # Add release sections
        my $bAdditionalNotes = false;
        my $bReleaseNote = false;

        my $hSectionType =
        {
            &XML_RELEASE_CORE_LIST => {title => 'Core', type => 'core'},
            &XML_RELEASE_DOC_LIST => {title => 'Documentation', type => 'doc'},
            &XML_RELEASE_TEST_LIST => {title => 'Test Suite', type => 'test'},
        };

        foreach my $strSectionType (XML_RELEASE_CORE_LIST, XML_RELEASE_DOC_LIST, XML_RELEASE_TEST_LIST)
        {
            if ($oRelease->nodeTest($strSectionType))
            {
                # Add release item types
                my $hItemType =
                {
                    &XML_RELEASE_BUG_LIST => {title => 'Bug Fixes', type => 'bug'},
                    &XML_RELEASE_FEATURE_LIST => {title => 'Features', type => 'feature'},
                    &XML_RELEASE_IMPROVEMENT_LIST => {title => 'Improvements', type => 'improvement'},
                    &XML_RELEASE_DEVELOPMENT_LIST => {title => 'Development', type => 'development'},
                };

                foreach my $strItemType (
                    XML_RELEASE_BUG_LIST, XML_RELEASE_FEATURE_LIST, XML_RELEASE_IMPROVEMENT_LIST, XML_RELEASE_DEVELOPMENT_LIST)
                {
                    next if (!$self->{bDev} && $strItemType eq XML_RELEASE_DEVELOPMENT_LIST);

                    if ($oRelease->nodeGet($strSectionType)->nodeTest($strItemType))
                    {
                        if ($strSectionType ne XML_RELEASE_CORE_LIST && !$bAdditionalNotes)
                        {
                            $oReleaseSection->nodeAdd('subtitle')->textSet('Additional Notes');
                            $bAdditionalNotes = true;
                        }

                        # Add release note if present
                        if (!$bReleaseNote && $oRelease->nodeGet($strSectionType)->nodeTest('p'))
                        {
                            $oReleaseSection->nodeAdd('p')->textSet($oRelease->nodeGet($strSectionType)->nodeGet('p')->textGet());
                            $bReleaseNote = true;
                        }

                        my $strTypeText =
                            ($strSectionType eq XML_RELEASE_CORE_LIST ? '' : $$hSectionType{$strSectionType}{title}) . ' ' .
                            $$hItemType{$strItemType}{title} . ':';

                        $oReleaseSection->
                            nodeAdd('p')->textSet(
                                {name => 'text', children=> [{name => 'b', value => $strTypeText}]});

                        my $oList = $oReleaseSection->nodeAdd('list');

                        # Add release items
                        foreach my $oReleaseFeature ($oRelease->nodeGet($strSectionType)->
                                                     nodeGet($strItemType)->nodeList('release-item'))
                        {
                            my @rhyReleaseItemP = $oReleaseFeature->nodeList('p');
                            my $oReleaseItemText = $rhyReleaseItemP[0]->textGet();

                            # Check release item commits
                            if ($bReleaseCheckCommit && $strItemType ne XML_RELEASE_DEVELOPMENT_LIST)
                            {
                                my @oyCommit = $oReleaseFeature->nodeList('commit', false);

                                # If no commits found then try to use the description as the commit subject
                                if (@oyCommit == 0)
                                {
                                    my $strSubject = $oRender->processText($oReleaseItemText);
                                    my $rhCommit = $self->commitFindSubject(\@rhyReleaseCommit, $strSubject, false);

                                    if (!defined($rhCommit))
                                    {
                                        $self->commitError(
                                            "unable to find commit or no subject match for release ${strVersion} item" .
                                                " '${strSubject}'",
                                            \@stryReleaseCommitRemaining, $rhReleaseCommitRemaining);

                                        my $strCommit = $rhCommit->{commit};
                                        @stryReleaseCommitRemaining = grep(!/$strCommit/, @stryReleaseCommitRemaining);
                                    }
                                }

                                # Check the rest of the commits to ensure they exist
                                foreach my $oCommit (@oyCommit)
                                {
                                    my $strSubject = $oCommit->paramGet('subject');
                                    my $rhCommit = $self->commitFindSubject(\@rhyReleaseCommit, $strSubject, false);

                                    if (defined($rhCommit))
                                    {
                                        my $strCommit = $rhCommit->{commit};
                                        @stryReleaseCommitRemaining = grep(!/$strCommit/, @stryReleaseCommitRemaining);
                                    }
                                    else
                                    {
                                        $self->commitError(
                                            "unable to find release ${strVersion} commit subject '${strSubject}' in list",
                                            \@stryReleaseCommitRemaining, $rhReleaseCommitRemaining);
                                    }
                                }
                            }

                            # Append the rest of the text
                            if (@rhyReleaseItemP > 1)
                            {
                                shift(@rhyReleaseItemP);

                                push(@{$oReleaseItemText->{oDoc}{children}}, ' ');

                                foreach my $rhReleaseItemP (@rhyReleaseItemP)
                                {
                                    push(@{$oReleaseItemText->{oDoc}{children}}, @{$rhReleaseItemP->textGet()->{oDoc}{children}});
                                }
                            }

                            # Append contributor info
                            my $strContributorText = $self->contributorTextGet($oReleaseFeature, $$hItemType{$strItemType}{type});

                            if (defined($strContributorText))
                            {
                                push(@{$oReleaseItemText->{oDoc}{children}}, ' (');
                                push(@{$oReleaseItemText->{oDoc}{children}},
                                     {name => 'i', value => $strContributorText});
                                push(@{$oReleaseItemText->{oDoc}{children}}, ')');
                            }

                            # Add the list item
                            $oList->nodeAdd('list-item')->textSet($oReleaseItemText);
                        }
                    }
                }
            }
        }

        # Error if there are commits left over
        # if ($bReleaseCheckCommit && @stryReleaseCommitRemaining != 0)
        # {
        #     $self->commitError(
        #         "unassigned commits for release ${strVersion}", \@stryReleaseCommitRemaining, $rhReleaseCommitRemaining);
        # }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oDoc}
    );
}

1;
