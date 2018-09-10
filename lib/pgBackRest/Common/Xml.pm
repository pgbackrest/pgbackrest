####################################################################################################################################
# XML Helper Functions
####################################################################################################################################
package pgBackRest::Common::Xml;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use XML::LibXML;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

####################################################################################################################################
# xmlParse - parse a string into an xml document and return the root node
####################################################################################################################################
use constant XML_HEADER                                             => '<?xml version="1.0" encoding="UTF-8"?>';
    push @EXPORT, qw(XML_HEADER);

####################################################################################################################################
# Convert a string to xml so that it is suitable to be appended into xml
####################################################################################################################################
sub xmlFromText
{
    my $strText = shift;

    return XML::LibXML::Text->new($strText)->toString();
}

push @EXPORT, qw(xmlFromText);

####################################################################################################################################
# xmlParse - parse a string into an xml document and return the root node
####################################################################################################################################
sub xmlParse
{
    my $rstrXml = shift;

    my $oXml = XML::LibXML->load_xml(string => $rstrXml)->documentElement();

    return $oXml;
}

push @EXPORT, qw(xmlParse);

####################################################################################################################################
# xmlTagChildren - get all children that match the tag
####################################################################################################################################
sub xmlTagChildren
{
    my $oXml = shift;
    my $strTag = shift;

    return $oXml->getChildrenByTagName($strTag);
}

push @EXPORT, qw(xmlTagChildren);

####################################################################################################################################
# xmlTagText - get the text content for a tag, error if the tag is required and does not exist
####################################################################################################################################
sub xmlTagText
{
    my $oXml = shift;
    my $strTag = shift;
    my $bRequired = shift;
    # my $strDefault = shift;

    # Get the tag or tags
    my @oyTag = $oXml->getElementsByTagName($strTag);

    # Error if the tag does not exist and is required
    if (@oyTag > 1)
    {
        confess &log(ERROR, @oyTag . " '${strTag}' tag(s) exist, but only one was expected", ERROR_FORMAT);
    }
    elsif (@oyTag == 0)
    {
        if (!defined($bRequired) || $bRequired)
        {
            confess &log(ERROR, "tag '${strTag}' does not exist", ERROR_FORMAT);
        }
    }
    else
    {
        return $oyTag[0]->textContent();
    }

    return;
}

push @EXPORT, qw(xmlTagText);

####################################################################################################################################
# xmlTagBool - get the boolean content for a tag, error if the tag is required and does not exist or is not boolean
####################################################################################################################################
sub xmlTagBool
{
    my $oXml = shift;
    my $strTag = shift;
    my $bRequired = shift;
    # my $strDefault = shift;

    # Test content for boolean value
    my $strContent = xmlTagText($oXml, $strTag, $bRequired);

    if (defined($strContent))
    {
        if ($strContent eq 'true')
        {
            return true;
        }
        elsif ($strContent eq 'false')
        {
            return false;
        }
        else
        {
            confess &log(ERROR, "invalid boolean value '${strContent}' for tag '${strTag}'", ERROR_FORMAT);
        }
    }

    return;
}

push @EXPORT, qw(xmlTagBool);

####################################################################################################################################
# xmlTagInt - get the integer content for a tag, error if the tag is required and does not exist or is not an integer
####################################################################################################################################
sub xmlTagInt
{
    my $oXml = shift;
    my $strTag = shift;
    my $bRequired = shift;
    # my $strDefault = shift;

    # Test content for boolean value
    my $iContent = xmlTagText($oXml, $strTag, $bRequired);

    if (defined($iContent))
    {
        eval
        {
            $iContent = $iContent + 0;
            return 1;
        }
        or do
        {
            confess &log(ERROR, "invalid integer value '${iContent}' for tag '${strTag}'", ERROR_FORMAT);
        }
    }

    return $iContent;
}

push @EXPORT, qw(xmlTagInt);

1;
