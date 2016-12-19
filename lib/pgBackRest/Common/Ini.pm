####################################################################################################################################
# COMMON INI MODULE
####################################################################################################################################
package pgBackRest::Common::Ini;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname basename);
use IO::Handle;
use JSON::PP;
use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::FileCommon;
use pgBackRest::Version;

####################################################################################################################################
# Boolean constants
####################################################################################################################################
use constant INI_TRUE                                               => JSON::PP::true;
    push @EXPORT, qw(INI_TRUE);
use constant INI_FALSE                                              => JSON::PP::false;
    push @EXPORT, qw(INI_FALSE);

####################################################################################################################################
# Ini control constants
####################################################################################################################################
use constant INI_SECTION_BACKREST                                   => 'backrest';
    push @EXPORT, qw(INI_SECTION_BACKREST);

use constant INI_KEY_CHECKSUM                                       => 'backrest-checksum';
    push @EXPORT, qw(INI_KEY_CHECKSUM);
use constant INI_KEY_FORMAT                                         => 'backrest-format';
    push @EXPORT, qw(INI_KEY_FORMAT);
use constant INI_KEY_VERSION                                        => 'backrest-version';
    push @EXPORT, qw(INI_KEY_VERSION);

use constant INI_COMMENT                                            => '[comment]';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $strFileName = shift;            # Manifest filename
    my $bLoad = shift;                  # Load the ini?

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Filename must be specified
    if (!defined($strFileName))
    {
        confess &log(ASSERT, 'filename must be provided');
    }

    # Set variables
    my $oContent = {};
    $self->{oContent} = $oContent;
    $self->{strFileName} = $strFileName;

    # Load the ini if specified
    if (!defined($bLoad) || $bLoad)
    {
        $self->load();

        # Make sure the ini is valid by testing checksum
        my $strChecksum = $self->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM);
        my $strTestChecksum = $self->hash();

        if ($strChecksum ne $strTestChecksum)
        {
            confess &log(ERROR, "${strFileName} checksum is invalid, should be ${strTestChecksum} but found ${strChecksum}",
                         ERROR_CHECKSUM);
        }

        # Make sure that the format is current, otherwise error
        my $iFormat = $self->get(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, false, 0);

        if ($iFormat != BACKREST_FORMAT)
        {
            confess &log(ERROR, "format of ${strFileName} is ${iFormat} but " . BACKREST_FORMAT . ' is required', ERROR_FORMAT);
        }

        # Check if the version has changed
        if (!$self->test(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, BACKREST_VERSION))
        {
            $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, BACKREST_VERSION);
        }
    }
    else
    {
        $self->numericSet(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, BACKREST_FORMAT);
        $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, BACKREST_VERSION);
    }

    return $self;
}

####################################################################################################################################
# load
#
# Load the ini.
####################################################################################################################################
sub load
{
    my $self = shift;

    iniLoad($self->{strFileName}, $self->{oContent});
}

####################################################################################################################################
# iniLoad
#
# Load file from standard INI format to a hash.
####################################################################################################################################
push @EXPORT, qw(iniLoad);

sub iniLoad
{
    my $strFileName = shift;
    my $oContent = shift;
    my $bRelaxed = shift;

    # Open the ini file for reading
    my $hFile;
    my $strSection;

    open($hFile, '<', $strFileName)
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Create the JSON object
    my $oJSON = JSON::PP->new()->allow_nonref();

    # Read the INI file
    while (my $strLine = readline($hFile))
    {
        $strLine = trim($strLine);

        # Skip lines that are blank or comments
        if ($strLine ne '' && $strLine !~ '^[ ]*#.*')
        {
            # Get the section
            if (index($strLine, '[') == 0)
            {
                $strSection = substr($strLine, 1, length($strLine) - 2);
            }
            else
            {
                # Get key and value
                my $iIndex = index($strLine, '=');

                if ($iIndex == -1)
                {
                    confess &log(ERROR, "unable to read from ${strFileName}: ${strLine}");
                }

                my $strKey = substr($strLine, 0, $iIndex);
                my $strValue = substr($strLine, $iIndex + 1);

                # If relaxed then read the value directly
                if ($bRelaxed)
                {
                    if (defined($$oContent{$strSection}{$strKey}))
                    {
                        if (ref($$oContent{$strSection}{$strKey}) ne 'ARRAY')
                        {
                            $$oContent{$strSection}{$strKey} = [$$oContent{$strSection}{$strKey}];
                        }

                        push(@{$$oContent{$strSection}{$strKey}}, $strValue);
                    }
                    else
                    {
                        $$oContent{$strSection}{$strKey} = $strValue;
                    }
                }
                # Else read the value as stricter JSON
                else
                {
                    ${$oContent}{$strSection}{$strKey} = $oJSON->decode($strValue);
                }
            }
        }
    }

    close($hFile);
    return($oContent);
}

####################################################################################################################################
# save
#
# Save the file.
####################################################################################################################################
sub save
{
    my $self = shift;

    $self->hash();
    iniSave($self->{strFileName}, $self->{oContent}, false, true);

    # Indicate the file now exists
    $self->{bExists} = true;
}

####################################################################################################################################
# iniSave
#
# Save from a hash to standard INI format.
####################################################################################################################################
push @EXPORT, qw(iniSave);

sub iniSave
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $oContent,
        $bRelaxed,
        $bTemp
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniSave', \@_,
            {name => 'strFileName', trace => true},
            {name => 'oContent', trace => true},
            {name => 'bRelaxed', default => false, trace => true},
            {name => 'bTemp', default => false, trace => true}
        );

    # Open the ini file for writing
    my $strFileTemp = $bTemp ? "${strFileName}.new" : $strFileName;
    my $hFile;
    my $bFirst = true;

    sysopen($hFile, $strFileTemp, O_WRONLY | O_CREAT | O_TRUNC, 0640)
        or confess &log(ERROR, "unable to open ${strFileTemp}");

    # Create the JSON object canonical so that fields are alpha ordered to pass unit tests
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();

    # Write the INI file
    foreach my $strSection (sort(keys(%$oContent)))
    {
        # Add a linefeed between sections
        if (!$bFirst)
        {
            syswrite($hFile, "\n")
                or confess "unable to write lf: $!";
        }

        # Write the section comment if present
        if (defined(${$oContent}{$strSection}{&INI_COMMENT}))
        {
            my $strComment = trim(${$oContent}{$strSection}{&INI_COMMENT});
            $strComment =~ s/\n/\n# /g;

            # syswrite($hFile, ('#' x 80) . "\n# ${strComment}\n" . ('#' x 80) . "\n")
            #     or confess "unable to comment for section ${strSection}: $!";
            syswrite($hFile, "# ${strComment}\n")
                or confess "unable to comment for section ${strSection}: $!";
        }

        # Write the section
        syswrite($hFile, "[${strSection}]\n")
            or confess "unable to write section ${strSection}: $!";

        # Iterate through all keys in the section
        foreach my $strKey (sort(keys(%{${$oContent}{"${strSection}"}})))
        {
            # Skip comments
            if ($strKey eq INI_COMMENT)
            {
                next;
            }

            # If the value is a hash then convert it to JSON, otherwise store as is
            my $strValue = ${$oContent}{$strSection}{$strKey};

            # If relaxed then store as old-style config
            if ($bRelaxed)
            {
                # If the value is an array then save each element to a separate key/value pair
                if (ref($strValue) eq 'ARRAY')
                {
                    foreach my $strArrayValue (@{$strValue})
                    {
                        syswrite($hFile, "${strKey}=${strArrayValue}\n")
                            or confess "unable to write relaxed key array ${strKey}: $!";
                    }
                }
                # Else write a standard key/value pair
                else
                {
                    syswrite($hFile, "${strKey}=${strValue}\n")
                        or confess "unable to write relaxed key ${strKey}: $!";
                }
            }
            # Else write as stricter JSON
            else
            {
                syswrite($hFile, "${strKey}=" . $oJSON->encode($strValue) . "\n")
                    or confess "unable to write json key ${strKey}: $!";
            }
        }

        $bFirst = false;
    }

    # Sync and close temp file
    $hFile->sync();
    close($hFile);

    # Rename temp file to ini file
    if ($bTemp && !rename($strFileTemp, $strFileName))
    {
        unlink($strFileTemp);
        confess &log(ERROR, "unable to move ${strFileTemp} to ${strFileName}", ERROR_FILE_MOVE);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# hash
#
# Generate hash for the manifest.
####################################################################################################################################
sub hash
{
    my $self = shift;

    # Remove the old checksum
    $self->remove(INI_SECTION_BACKREST, INI_KEY_CHECKSUM);

    # Calculate the checksum
    my $oChecksumContent = dclone($self->{oContent});

    foreach my $strSection (keys(%$oChecksumContent))
    {
        delete(${$oChecksumContent}{$strSection}{&INI_COMMENT});
    }

    my $oSHA = Digest::SHA->new('sha1');
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
    $oSHA->add($oJSON->encode($oChecksumContent));

    # Set the new checksum
    my $strHash = $oSHA->hexdigest();

    $self->set(INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef, $strHash);

    return $strHash;
}

####################################################################################################################################
# get
#
# Get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    my $oContent = $self->{oContent};

    # Section must always be defined
    if (!defined($strSection))
    {
        confess &log(ASSERT, 'section is not defined');
    }

    # Set default for required
    $bRequired = defined($bRequired) ? $bRequired : true;

    # Store the result
    my $oResult = undef;

    if (defined($strSubValue))
    {
        if (!defined($strValue))
        {
            confess &log(ASSERT, "subvalue '${strSubValue}' requested but value is not defined");
        }

        if (defined(${$oContent}{$strSection}{$strValue}))
        {
            $oResult = ${$oContent}{$strSection}{$strValue}{$strSubValue};
        }
    }
    elsif (defined($strValue))
    {
        if (defined(${$oContent}{$strSection}))
        {
            $oResult = ${$oContent}{$strSection}{$strValue};
        }
    }
    else
    {
        $oResult = ${$oContent}{$strSection};
    }

    if (!defined($oResult) && $bRequired)
    {
        confess &log(ASSERT, "manifest section '$strSection'" . (defined($strValue) ? ", value '$strValue'" : '') .
                              (defined($strSubValue) ? ", subvalue '$strSubValue'" : '') . ' is required but not defined');
    }

    if (!defined($oResult) && defined($oDefault))
    {
        $oResult = $oDefault;
    }

    return $oResult
}

####################################################################################################################################
# boolGet
#
# Get a numeric value.
####################################################################################################################################
sub boolGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $bDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($bDefault) ? ($bDefault ? INI_TRUE : INI_FALSE) : undef) ? true : false;
}

####################################################################################################################################
# numericGet
#
# Get a numeric value.
####################################################################################################################################
sub numericGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $nDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($nDefault) ? $nDefault + 0 : undef) + 0;
}

####################################################################################################################################
# set
#
# Set a value.
####################################################################################################################################
sub set
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;

    my $oContent = $self->{oContent};

    if (defined($strSubKey))
    {
        ${$oContent}{$strSection}{$strKey}{$strSubKey} = $strValue;
    }
    else
    {
        ${$oContent}{$strSection}{$strKey} = $strValue;
    }
}

####################################################################################################################################
# boolSet
#
# Set a boolean value.
####################################################################################################################################
sub boolSet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bValue = shift;

    $self->set($strSection, $strKey, $strSubKey, $bValue ? INI_TRUE : INI_FALSE);
}

####################################################################################################################################
# numericSet
#
# Set a numeric value.
####################################################################################################################################
sub numericSet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $nValue = shift;

    $self->set($strSection, $strKey, $strSubKey, $nValue + 0);
}

####################################################################################################################################
# commentSet
#
# Set a section comment.
####################################################################################################################################
# sub commentSet
# {
#     my $self = shift;
#     my $strSection = shift;
#     my $strComment = shift;
#
#     my $oContent = $self->{oContent};
#
#     ${$oContent}{$strSection}{&INI_COMMENT} = $strComment;
# }

####################################################################################################################################
# remove
#
# Remove a value.
####################################################################################################################################
sub remove
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;

    my $oContent = $self->{oContent};

    if (defined($strSubKey))
    {
        delete(${$oContent}{$strSection}{$strKey}{$strSubKey});
    }
    elsif (defined($strKey))
    {
        delete(${$oContent}{$strSection}{$strKey});
    }
    else
    {
        delete(${$oContent}{$strSection});
    }
}

####################################################################################################################################
# keys
#
# Get a list of keys.
####################################################################################################################################
sub keys
{
    my $self = shift;
    my $strSection = shift;
    my $strSortOrder = shift;

    if (!defined($strSection))
    {
        confess &log(ASSERT, 'strSection must be set');
    }

    if ($self->test($strSection))
    {
        if (!defined($strSortOrder) || $strSortOrder eq 'forward')
        {
            return (sort(keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq 'reverse')
        {
            return (sort {$b cmp $a} (keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq 'none')
        {
            return (keys(%{$self->get($strSection)}));
        }
        else
        {
            confess &log(ASSERT, "invalid strSortOrder '${strSortOrder}'");
        }
    }

    my @stryEmptyArray;
    return @stryEmptyArray;
}

####################################################################################################################################
# test
#
# Test a value to see if it equals the supplied test value.  If no test value is given, tests that it is defined.
####################################################################################################################################
sub test
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $strTest = shift;

    my $strResult = $self->get($strSection, $strValue, $strSubValue, false);

    if (defined($strResult))
    {
        if (defined($strTest))
        {
            return $strResult eq $strTest ? true : false;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# boolTest
#
# Test a value to see if it equals the supplied test boolean value.  If no test value is given, tests that it is defined.
####################################################################################################################################
sub boolTest
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bTest = shift;

    return $self->test($strSection, $strValue, $strSubValue, defined($bTest) ? ($bTest ? INI_TRUE : INI_FALSE) : undef);
}

1;
