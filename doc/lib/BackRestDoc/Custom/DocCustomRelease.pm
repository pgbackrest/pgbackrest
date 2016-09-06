####################################################################################################################################
# DOC RELEASE MODULE
####################################################################################################################################
package BackRestDoc::Custom::DocCustomRelease;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Config::ConfigHelp;
use pgBackRest::FileCommon;

use BackRestDoc::Common::DocRender;

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
use constant XML_RELEASE_FEATURE_LIST                               => 'release-feature-list';
use constant XML_RELEASE_REFACTOR_LIST                              => 'release-refactor-list';

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
        $self->{oDoc}
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDoc'}
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
# docGet
#
# Get the xml for release.
####################################################################################################################################
sub docGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->docGet');

    # Create the doc
    my $oDoc = new BackRestDoc::Common::Doc();
    $oDoc->paramSet('title', $self->{oDoc}->paramGet('title'));

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

    foreach my $oRelease ($self->{oDoc}->nodeGet('release-list')->nodeList('release'))
    {
        # Get the release version
        my $strVersion = $oRelease->paramGet('version');

        # Display versions in TOC?
        my $bTOC = true;

        # Create a release section
        if ($strVersion =~ /dev$/)
        {
            if ($iDevReleaseTotal > 1)
            {
                confess &log(ERROR, 'only one development release is allowed');
            }

            $oSection = $oDoc->nodeAdd('section', undef, {id => 'development', keyword => 'dev'});
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
                $oSection->nodeAdd('title')->textSet("Supported Stable Releases");
            }

            $iStableReleaseTotal++;
        }
        else
        {
            if ($iUnsupportedReleaseTotal == 0)
            {
                $oSection = $oDoc->nodeAdd('section', undef, {id => 'unsupported'});
                $oSection->nodeAdd('title')->textSet("Unsupported Releases");
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
            "v${strVersion} " . ($strVersion =~ /dev$/ ? '' : 'Release ') . 'Notes');

        $oReleaseSection->nodeAdd('subtitle')->textSet($oRelease->paramGet('title'));
        $oReleaseSection->nodeAdd('subsubtitle')->textSet($strDateOut);

        # Add release sections
        my $bAdditionalNotes = false;

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
                # Create subsections for any release section other than core.  This breaks up the release items.
                my $oSubSection = $oReleaseSection;

                if ($strSectionType ne XML_RELEASE_CORE_LIST && !$bAdditionalNotes)
                {
                    $oReleaseSection->nodeAdd('subtitle')->textSet("Additional Notes");
                    $bAdditionalNotes = true;
                }

                # Add release note if present
                if ($oRelease->nodeGet($strSectionType)->nodeTest('p'))
                {
                    $oSubSection->nodeAdd('p')->textSet($oRelease->nodeGet($strSectionType)->nodeGet('p')->textGet());
                }

                # Add release item types
                my $hItemType =
                {
                    &XML_RELEASE_BUG_LIST => {title => 'Bug Fixes', type => 'bug'},
                    &XML_RELEASE_FEATURE_LIST => {title => 'Features', type=> 'feature'},
                    &XML_RELEASE_REFACTOR_LIST => {title => 'Refactoring', type=> 'refactor'},
                };

                foreach my $strItemType (XML_RELEASE_BUG_LIST, XML_RELEASE_FEATURE_LIST, XML_RELEASE_REFACTOR_LIST)
                {
                    if ($oRelease->nodeGet($strSectionType)->nodeTest($strItemType))
                    {
                        my $strTypeText =
                            ($strSectionType eq XML_RELEASE_CORE_LIST ? '' : $$hSectionType{$strSectionType}{title}) . ' ' .
                            $$hItemType{$strItemType}{title} . ':';

                        $oSubSection->
                            nodeAdd('p')->textSet(
                                {name => 'text', children=> [{name => 'b', value => $strTypeText}]});

                        my $oList = $oSubSection->nodeAdd('list');

                        # Add release items
                        foreach my $oReleaseFeature ($oRelease->nodeGet($strSectionType)->
                                                     nodeGet($strItemType)->nodeList('release-item'))
                        {
                            my $oReleaseItemText = $oReleaseFeature->nodeGet('p')->textGet();
                            my $strContributorText = $self->contributorTextGet($oReleaseFeature, $$hItemType{$strItemType}{type});

                            if (defined($strContributorText))
                            {
                                push(@{$oReleaseItemText->{oDoc}{children}}, ' (');
                                push(@{$oReleaseItemText->{oDoc}{children}},
                                     {name => 'i', value => $strContributorText});
                                push(@{$oReleaseItemText->{oDoc}{children}}, ')');
                            }

                            $oList->nodeAdd('list-item')->textSet($oReleaseItemText);
                        }
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oDoc}
    );
}

1;
