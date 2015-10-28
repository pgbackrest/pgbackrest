####################################################################################################################################
# DOC HTML SITE MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlSite;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use POSIX qw(strftime);
use Storable qw(dclone);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::FileCommon;
use BackRest::Version;

use lib dirname($0) . '/../test/lib';
use BackRestTest::Common::ExecuteTest;

use BackRestDoc::Common::DocConfig;
use BackRestDoc::Html::DocHtmlPage;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_HTML_SITE                                       => 'DocHtmlSite';

use constant OP_DOC_HTML_SITE_NEW                                   => OP_DOC_HTML_SITE . '->new';
use constant OP_DOC_HTML_SITE_PROCESS                               => OP_DOC_HTML_SITE . '->process';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    $self->{strClass} = $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oRender},
        $self->{oReference},
        $self->{strXmlPath},
        $self->{strHtmlPath},
        $self->{strCssFile},
        $self->{strHtmlRoot},
        $self->{bExe}
    ) =
        logDebugParam
        (
            OP_DOC_HTML_SITE_NEW, \@_,
            {name => 'oRender'},
            {name => 'oReference'},
            {name => 'strXmlPath'},
            {name => 'strHtmlPath'},
            {name => 'strCssFile'},
            {name => 'strHtmlRoot'},
            {name => 'bExe'}
        );

    # Remove the current html path if it exists
    if (-e $self->{strHtmlPath})
    {
        executeTest("rm -rf $self->{strHtmlPath}/*");
    }
    # Else create the html path
    else
    {
        mkdir($self->{strHtmlPath})
            or confess &log(ERROR, "unable to create path $self->{strHtmlPath}");
    }

    # Create the footer
    $self->{strFooter} = 'Copyright © 2015' . (strftime('%Y', localtime) ne '2015' ?  '-' . strftime('%Y', localtime) : '') .
                         ', The PostgreSQL Global Development Group, <a href="{[github-url-license]}">MIT License</a>.  Updated ' .
                         strftime('%B ', localtime) . trim(strftime('%e,', localtime)) . strftime(' %Y.', localtime);

    # Insert pages into the site hash
    $self->{oSite} =
    {
        'common' =>
        {
            'oRender' => $self->{oRender},
            'oReference' => $self->{oReference},
            'strFooter' => $self->{strFooter}
        },

        'page' =>
        {
            'index' =>
            {
                strMenuTitle => 'Home',
                oDoc => new BackRestDoc::Common::Doc("$self->{strXmlPath}/index.xml")
            },

            'command' =>
            {
                strMenuTitle => 'Commands',
                oDoc => $self->{oReference}->helpCommandDocGet()
            },

            'configuration' =>
            {
                strMenuTitle => 'Configuration',
                oDoc => $self->{oReference}->helpConfigDocGet()
            # }
            },

            'user-guide' =>
            {
                strMenuTitle => 'User Guide',
                oDoc => new BackRestDoc::Common::Doc("$self->{strXmlPath}/user-guide.xml")
            }
        }
    };

    # Create common variables
    ${$self->{var}}{version} = $VERSION;
    ${$self->{var}}{'backrest-exe'} = $self->{oRender}->{strExeName};
    ${$self->{var}}{'postgres'} = 'PostgreSQL';
    ${$self->{var}}{'backrest-url-root'} = $self->{strHtmlRoot};
    ${$self->{var}}{'dash'} = '-';

    # Read variables from pages
    foreach my $strPageId (sort(keys(${$self->{oSite}}{page})))
    {
        my $oPage = ${$self->{oSite}}{page}{$strPageId};

        if (defined($$oPage{oDoc}->nodeGet('variable-list', false)))
        {
            foreach my $oVariable ($$oPage{oDoc}->nodeGet('variable-list')->nodeList('variable'))
            {
                my $strName = $oVariable->fieldGet('variable-name');
                my $strValue = $oVariable->fieldGet('variable-value');

                ${$self->{var}}{$strName} = $self->variableReplace($strValue);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# variableReplace
#
# Replace variables in the string.
####################################################################################################################################
sub variableReplace
{
    my $self = shift;
    my $strBuffer = shift;

    if (!defined($strBuffer))
    {
        return undef;
    }

    foreach my $strName (sort(keys(%{$self->{var}})))
    {
        my $strValue = $self->{var}{$strName};

        $strBuffer =~ s/\{\[$strName\]\}/$strValue/g;
    }

    return $strBuffer;
}

####################################################################################################################################
# variableSet
#
# Set a variable to be replaced later.
####################################################################################################################################
sub variableSet
{
    my $self = shift;
    my $strKey = shift;
    my $strValue = shift;

    ${$self->{var}}{$strKey} = $strValue;
}

####################################################################################################################################
# variableGet
#
# Get the current value of a variable.
####################################################################################################################################
sub variableGet
{
    my $self = shift;
    my $strKey = shift;

    return ${$self->{var}}{$strKey};
}

####################################################################################################################################
# process
#
# Generate the site html
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_HTML_SITE_PROCESS);

    # Copy the css file
    my $strCssFileDestination = "$self->{strHtmlPath}/default.css";
    copy($self->{strCssFile}, $strCssFileDestination)
        or confess &log(ERROR, "unable to copy $self->{strCssFile} to ${strCssFileDestination}");

    # Render pages
    my $oSite = $self->{oSite};

    foreach my $strPageId (sort(keys($$oSite{'page'})))
    {
        # Save the html page
        fileStringWrite("$self->{strHtmlPath}/${strPageId}.html",
                        $self->variableReplace((new BackRestDoc::Html::DocHtmlPage($self, $strPageId, $self->{bExe}))->process()),
                        false);
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
