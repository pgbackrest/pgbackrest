####################################################################################################################################
# DOC CHANGE LOG MODULE
####################################################################################################################################
package BackRestDoc::Common::DocChangeLog;

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
# Operation constants
####################################################################################################################################
use constant OP_DOC_CHANGE_LOG                                      => 'DocChangeLog';

use constant OP_DOC_CHANGE_LOG_NEW                                  => OP_DOC_CHANGE_LOG . '->new';
use constant OP_DOC_CHANGE_LOG_DOC_GET                              => OP_DOC_CHANGE_LOG . '->docGet';

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
        $self->{oDocRender}
    ) =
        logDebugParam
        (
            OP_DOC_CHANGE_LOG_NEW, \@_,
            {name => 'oDoc'},
            {name => 'oDocRender'}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# helpConfigDocGet
#
# Get the xml for change log.
####################################################################################################################################
sub docGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_CHANGE_LOG_DOC_GET);

    my $oDoc = new BackRestDoc::Common::Doc();

    my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                     'July', 'August', 'September', 'October', 'November', 'December');

    $oDoc->paramSet('title', $self->{oDoc}->paramGet('title'));

    # set the description for use as a meta tag
    $oDoc->fieldSet('description', $self->{oDoc}->fieldGet('description'));

    # # Output the introduction
    my $oIntroSectionDoc = $oDoc->nodeAdd('section', undef, {id => 'introduction'});
    $oIntroSectionDoc->nodeAdd('title')->textSet('Introduction');
    $oIntroSectionDoc->textSet($self->{oDoc}->nodeGet('intro')->textGet());

    # build each release section
    foreach my $oChangeLogRelease ($self->{oDoc}->nodeGet('changelog')->nodeList('changelog-release'))
    {
        # Get the required release version and date
        my $strVersion = $oChangeLogRelease->paramGet('version');
        my $strDate = $oChangeLogRelease->paramGet('date');
        my $strDateOut = "";

        # format the date
        if ($strDate !~ /^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$/)
        {
            confess &log(ASSERT, "invalid date ${strDate} for change-log release {$strVersion}");
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

        my $oSection = $oDoc->nodeAdd('section', undef, {id => "release-${strVersion}"});
        $oSection->nodeAdd('title')->textSet("Release $strVersion ");
        # $oSection->nodeAdd('p')->textSet($oChangeLogRelease->paramGet('title'));
        $oSection->nodeAdd('release-title')->textSet($oChangeLogRelease->paramGet('title'));
        $oSection->nodeAdd('release-date')->textSet('Released ' . $strDateOut);

        foreach my $oReleaseFeature ($oChangeLogRelease->nodeGet('release-feature-bullet-list')->nodeList('release-feature'))
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
