####################################################################################################################################
# DOC RELEASE MODULE
####################################################################################################################################
package BackRestDoc::Custom::DocCustomRelease;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Config::ConfigHelp;
use pgBackRest::FileCommon;

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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
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
    foreach my $oRelease ($self->{oDoc}->nodeGet('changelog')->nodeList('changelog-release'))
    {
        # Get the required release version and date
        my $strVersion = $oRelease->paramGet('version');
        my $strDate = $oRelease->paramGet('date');
        my $strDateOut = "";

        # Format the date
        my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                         'July', 'August', 'September', 'October', 'November', 'December');

        if ($strDate !~ /^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$/)
        {
            confess &log(ASSERT, "invalid date ${strDate} for release {$strVersion}");
        }

        if ($strDate =~ /^X/)
        {
            $strDateOut .= "\n__No Release Date Set__";
        }
        else
        {
            $strDateOut .= $stryMonth[(substr($strDate, 5, 2) - 1)] . ' ' .
                          (substr($strDate, 8, 2) + 0) . ', ' . substr($strDate, 0, 4);
        }

        # Add section and titles
        my $oSection = $oDoc->nodeAdd('section', undef, {id => $strVersion});
        $oSection->nodeAdd('title')->textSet("$strVersion Release");
        $oSection->nodeAdd('subtitle')->textSet($oRelease->paramGet('title'));
        $oSection->nodeAdd('subsubtitle')->textSet('Released ' . $strDateOut);

        # Add release features
        foreach my $oReleaseFeature ($oRelease->nodeGet('release-feature-bullet-list')->nodeList('release-feature'))
        {
            $oSection->nodeAdd('p')->textSet($oReleaseFeature->textGet());
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
