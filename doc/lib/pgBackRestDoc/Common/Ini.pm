####################################################################################################################################
# COMMON INI MODULE
####################################################################################################################################
package pgBackRestDoc::Common::Ini;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use JSON::PP;
use Storable qw(dclone);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

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

use constant INI_SECTION_CIPHER                                     => 'cipher';
    push @EXPORT, qw(INI_SECTION_CIPHER);

use constant INI_KEY_CIPHER_PASS                                    => 'cipher-pass';
    push @EXPORT, qw(INI_KEY_CIPHER_PASS);

####################################################################################################################################
# Ini file copy extension
####################################################################################################################################
use constant INI_COPY_EXT                                           => '.copy';
    push @EXPORT, qw(INI_COPY_EXT);

####################################################################################################################################
# Ini sort orders
####################################################################################################################################
use constant INI_SORT_FORWARD                                       => 'forward';
    push @EXPORT, qw(INI_SORT_FORWARD);
use constant INI_SORT_REVERSE                                       => 'reverse';
    push @EXPORT, qw(INI_SORT_REVERSE);
use constant INI_SORT_NONE                                          => 'none';
    push @EXPORT, qw(INI_SORT_NONE);

####################################################################################################################################
# new()
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oStorage},
        $self->{strFileName},
        my $bLoad,
        my $strContent,
        $self->{iInitFormat},
        $self->{strInitVersion},
        my $bIgnoreMissing,
        $self->{strCipherPass},                                     # Passphrase to read/write the file
        my $strCipherPassSub,                                       # Passphrase to read/write subsequent files
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorage', trace => true},
            {name => 'strFileName', trace => true},
            {name => 'bLoad', optional => true, default => true, trace => true},
            {name => 'strContent', optional => true, trace => true},
            {name => 'iInitFormat', optional => true, default => REPOSITORY_FORMAT, trace => true},
            {name => 'strInitVersion', optional => true, default => PROJECT_VERSION, trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
            {name => 'strCipherPass', optional => true, trace => true},
            {name => 'strCipherPassSub', optional => true, trace => true},
        );

    # Set changed to false
    $self->{bModified} = false;

    # Set exists to false
    $self->{bExists} = false;

    # Load the file if requested
    if ($bLoad)
    {
        $self->load($bIgnoreMissing);
    }
    # Load from a string if provided
    elsif (defined($strContent))
    {
        $self->{oContent} = iniParse($strContent);
        $self->headerCheck();
    }

    # Initialize if not loading the file and not loading from string or if a load was attempted and the file does not exist
    if (!$self->{bExists} && !defined($strContent))
    {
        $self->numericSet(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, $self->{iInitFormat});
        $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion});

        # Determine if the passphrase section should be set
        if (defined($self->{strCipherPass}) && defined($strCipherPassSub))
        {
            $self->set(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS, undef, $strCipherPassSub);
        }
    }

    return $self;
}

####################################################################################################################################
# loadVersion() - load a version (main or copy) of the ini file
####################################################################################################################################
sub loadVersion
{
    my $self = shift;
    my $bCopy = shift;
    my $bIgnoreError = shift;

    # Load main
    my $rstrContent = $self->{oStorage}->get(
        $self->{oStorage}->openRead($self->{strFileName} . ($bCopy ? INI_COPY_EXT : ''),
        {bIgnoreMissing => $bIgnoreError, strCipherPass => $self->{strCipherPass}}));

    # If the file exists then attempt to parse it
    if (defined($rstrContent))
    {
        my $rhContent = iniParse($$rstrContent, {bIgnoreInvalid => $bIgnoreError});

        # If the content is valid then check the header
        if (defined($rhContent))
        {
            $self->{oContent} = $rhContent;

            # If the header is invalid then undef content
            if (!$self->headerCheck({bIgnoreInvalid => $bIgnoreError}))
            {
                delete($self->{oContent});
            }
        }
    }

    return defined($self->{oContent});
}

####################################################################################################################################
# load() - load the ini
####################################################################################################################################
sub load
{
    my $self = shift;
    my $bIgnoreMissing = shift;

    # If main was not loaded then try the copy
    if (!$self->loadVersion(false, true))
    {
        if (!$self->loadVersion(true, true))
        {
            return if $bIgnoreMissing;

            confess &log(ERROR, "unable to open $self->{strFileName} or $self->{strFileName}" . INI_COPY_EXT, ERROR_FILE_MISSING);
        }
    }

    $self->{bExists} = true;
}

####################################################################################################################################
# headerCheck() - check that version and checksum in header are as expected
####################################################################################################################################
sub headerCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bIgnoreInvalid,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->headerCheck', \@_,
            {name => 'bIgnoreInvalid', optional => true, default => false, trace => true},
        );

    # Eval so exceptions can be ignored on bIgnoreInvalid
    my $bValid = true;

    eval
    {

        # Make sure the ini is valid by testing checksum
        my $strChecksum = $self->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef, false);
        my $strTestChecksum = $self->hash();

        if (!defined($strChecksum) || $strChecksum ne $strTestChecksum)
        {
            confess &log(ERROR,
                "invalid checksum in '$self->{strFileName}', expected '${strTestChecksum}' but found " .
                    (defined($strChecksum) ? "'${strChecksum}'" : '[undef]'),
                ERROR_CHECKSUM);
        }

        # Make sure that the format is current, otherwise error
        my $iFormat = $self->get(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, false, 0);

        if ($iFormat != $self->{iInitFormat})
        {
            confess &log(ERROR,
                "invalid format in '$self->{strFileName}', expected $self->{iInitFormat} but found ${iFormat}", ERROR_FORMAT);
        }

        # Check if the version has changed
        if (!$self->test(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion}))
        {
            $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion});
        }

        return true;
    }
    or do
    {
        # Confess the error if it should not be ignored
        if (!$bIgnoreInvalid)
        {
            confess $EVAL_ERROR;
        }

        # Return false when errors are ignored
        $bValid = false;
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bValid', value => $bValid, trace => true}
    );
}

####################################################################################################################################
# iniParse() - parse from standard INI format to a hash.
####################################################################################################################################
push @EXPORT, qw(iniParse);

sub iniParse
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strContent,
        $bRelaxed,
        $bIgnoreInvalid,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniParse', \@_,
            {name => 'strContent', required => false, trace => true},
            {name => 'bRelaxed', optional => true, default => false, trace => true},
            {name => 'bIgnoreInvalid', optional => true, default => false, trace => true},
        );

    # Ini content
    my $oContent = undef;
    my $strSection;

    # Create the JSON object
    my $oJSON = JSON::PP->new()->allow_nonref();

    # Eval so exceptions can be ignored on bIgnoreInvalid
    eval
    {
        # Read the INI file
        foreach my $strLine (split("\n", defined($strContent) ? $strContent : ''))
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
                    if (!defined($strSection))
                    {
                        confess &log(ERROR, "key/value pair '${strLine}' found outside of a section", ERROR_CONFIG);
                    }

                    # Get key and value
                    my $iIndex = index($strLine, '=');

                    if ($iIndex == -1)
                    {
                        confess &log(ERROR, "unable to find '=' in '${strLine}'", ERROR_CONFIG);
                    }

                    my $strKey = substr($strLine, 0, $iIndex);
                    my $strValue = substr($strLine, $iIndex + 1);

                    # If relaxed then read the value directly
                    if ($bRelaxed)
                    {
                        if (defined($oContent->{$strSection}{$strKey}))
                        {
                            if (ref($oContent->{$strSection}{$strKey}) ne 'ARRAY')
                            {
                                $oContent->{$strSection}{$strKey} = [$oContent->{$strSection}{$strKey}];
                            }

                            push(@{$oContent->{$strSection}{$strKey}}, $strValue);
                        }
                        else
                        {
                            $oContent->{$strSection}{$strKey} = $strValue;
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

        # Error if the file is empty
        if (!($bRelaxed || defined($oContent)))
        {
            confess &log(ERROR, 'no key/value pairs found', ERROR_CONFIG);
        }

        return true;
    }
    or do
    {
        # Confess the error if it should not be ignored
        if (!$bIgnoreInvalid)
        {
            confess $EVAL_ERROR;
        }

        # Undef content when errors are ignored
        undef($oContent);
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oContent', value => $oContent, trace => true}
    );
}

####################################################################################################################################
# save() - save the file.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Save only if modified
    if ($self->{bModified})
    {
        # Calculate the hash
        $self->hash();

        # Save the file
        $self->{oStorage}->put($self->{strFileName}, iniRender($self->{oContent}), {strCipherPass => $self->{strCipherPass}});

        if ($self->{oStorage}->can('pathSync'))
        {
            $self->{oStorage}->pathSync(dirname($self->{strFileName}));
        }

        $self->{oStorage}->put($self->{strFileName} . INI_COPY_EXT, iniRender($self->{oContent}),
            {strCipherPass => $self->{strCipherPass}});

        if ($self->{oStorage}->can('pathSync'))
        {
            $self->{oStorage}->pathSync(dirname($self->{strFileName}));
        }

        $self->{bModified} = false;

        # Indicate the file now exists
        $self->{bExists} = true;

        # File was saved
        return true;
    }

    # File was not saved
    return false;
}

####################################################################################################################################
# saveCopy - save only a copy of the file.
####################################################################################################################################
sub saveCopy
{
    my $self = shift;

    if ($self->{oStorage}->exists($self->{strFileName}))
    {
        confess &log(ASSERT, "cannot save copy only when '$self->{strFileName}' exists");
    }

    $self->hash();
    $self->{oStorage}->put($self->{strFileName} . INI_COPY_EXT, iniRender($self->{oContent}),
        {strCipherPass => $self->{strCipherPass}});
}

####################################################################################################################################
# iniRender() - render hash to standard INI format.
####################################################################################################################################
push @EXPORT, qw(iniRender);

sub iniRender
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oContent,
        $bRelaxed,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniRender', \@_,
            {name => 'oContent', trace => true},
            {name => 'bRelaxed', default => false, trace => true},
        );

    # Open the ini file for writing
    my $strContent = '';
    my $bFirst = true;

    # Create the JSON object canonical so that fields are alpha ordered to pass unit tests
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();

    # Write the INI file
    foreach my $strSection (sort(keys(%$oContent)))
    {
        # Add a linefeed between sections
        if (!$bFirst)
        {
            $strContent .= "\n";
        }

        # Write the section
        $strContent .= "[${strSection}]\n";

        # Iterate through all keys in the section
        foreach my $strKey (sort(keys(%{$oContent->{$strSection}})))
        {
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
                        $strContent .= "${strKey}=${strArrayValue}\n";
                    }
                }
                # Else write a standard key/value pair
                else
                {
                    $strContent .= "${strKey}=${strValue}\n";
                }
            }
            # Else write as stricter JSON
            else
            {
                # Skip the checksum for now but write all other key/value pairs
                if (!($strSection eq INI_SECTION_BACKREST && $strKey eq INI_KEY_CHECKSUM))
                {
                    $strContent .= "${strKey}=" . $oJSON->encode($strValue) . "\n";
                }
            }
        }

        $bFirst = false;
    }

    # If there is a checksum write it at the end of the file.  Having the checksum at the end of the file allows some major
    # performance optimizations which we won't implement in Perl, but will make the C code much more efficient.
    if (!$bRelaxed && defined($oContent->{&INI_SECTION_BACKREST}) && defined($oContent->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM}))
    {
        $strContent .=
            "\n[" . INI_SECTION_BACKREST . "]\n" .
            INI_KEY_CHECKSUM . '=' . $oJSON->encode($oContent->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM}) . "\n";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strContent', value => $strContent, trace => true}
    );
}

####################################################################################################################################
# hash() - generate hash for the manifest.
####################################################################################################################################
sub hash
{
    my $self = shift;

    # Remove the old checksum
    delete($self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

    # Set the new checksum
    $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} =
        sha1_hex(JSON::PP->new()->canonical()->allow_nonref()->encode($self->{oContent}));

    return $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM};
}

####################################################################################################################################
# get() - get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    # Parameter constraints
    if (!defined($strSection))
    {
        confess &log(ASSERT, 'strSection is required');
    }

    if (defined($strSubKey) && !defined($strKey))
    {
        confess &log(ASSERT, "strKey is required when strSubKey '${strSubKey}' is requested");
    }

    # Get the result
    my $oResult = $self->{oContent}->{$strSection};

    if (defined($strKey) && defined($oResult))
    {
        $oResult = $oResult->{$strKey};

        if (defined($strSubKey) && defined($oResult))
        {
            $oResult = $oResult->{$strSubKey};
        }
    }

    # When result is not defined
    if (!defined($oResult))
    {
        # Error if a result is required
        if (!defined($bRequired) || $bRequired)
        {
            confess &log(ASSERT, "strSection '$strSection'" . (defined($strKey) ? ", strKey '$strKey'" : '') .
                                  (defined($strSubKey) ? ", strSubKey '$strSubKey'" : '') . ' is required but not defined');
        }

        # Return default if specified
        if (defined($oDefault))
        {
            return $oDefault;
        }
    }

    return $oResult
}

####################################################################################################################################
# boolGet() - get a boolean value.
####################################################################################################################################
sub boolGet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $bDefault = shift;

    return $self->get(
        $strSection, $strKey, $strSubKey, $bRequired,
        defined($bDefault) ? ($bDefault ? INI_TRUE : INI_FALSE) : undef) ? true : false;
}

####################################################################################################################################
# numericGet() - get a numeric value.
####################################################################################################################################
sub numericGet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $nDefault = shift;

    return $self->get($strSection, $strKey, $strSubKey, $bRequired, defined($nDefault) ? $nDefault + 0 : undef) + 0;
}

####################################################################################################################################
# set - set a value.
####################################################################################################################################
sub set
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $oValue = shift;

    # Parameter constraints
    if (!(defined($strSection) && defined($strKey)))
    {
        confess &log(ASSERT, 'strSection and strKey are required');
    }

    my $oCurrentValue;

    if (defined($strSubKey))
    {
        $oCurrentValue = \$self->{oContent}{$strSection}{$strKey}{$strSubKey};
    }
    else
    {
        $oCurrentValue = \$self->{oContent}{$strSection}{$strKey};
    }

    if (!defined($$oCurrentValue) ||
        defined($oCurrentValue) != defined($oValue) ||
        ${dclone($oCurrentValue)} ne ${dclone(\$oValue)})
    {
        $$oCurrentValue = $oValue;

        if (!$self->{bModified})
        {
            $self->{bModified} = true;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# boolSet - set a boolean value.
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
# numericSet - set a numeric value.
####################################################################################################################################
sub numericSet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $nValue = shift;

    $self->set($strSection, $strKey, $strSubKey, defined($nValue) ? $nValue + 0 : undef);
}

####################################################################################################################################
# remove - remove a value.
####################################################################################################################################
sub remove
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;

    # Test if the value exists
    if ($self->test($strSection, $strKey, $strSubKey))
    {
        # Remove a subkey
        if (defined($strSubKey))
        {
            delete($self->{oContent}{$strSection}{$strKey}{$strSubKey});
        }

        # Remove a key
        if (defined($strKey))
        {
            if (!defined($strSubKey))
            {
                delete($self->{oContent}{$strSection}{$strKey});
            }

            # Remove the section if it is now empty
            if (keys(%{$self->{oContent}{$strSection}}) == 0)
            {
                delete($self->{oContent}{$strSection});
            }
        }

        # Remove a section
        if (!defined($strKey))
        {
            delete($self->{oContent}{$strSection});
        }

        # Record changes
        if (!$self->{bModified})
        {
            $self->{bModified} = true;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# keys - get the list of keys in a section.
####################################################################################################################################
sub keys
{
    my $self = shift;
    my $strSection = shift;
    my $strSortOrder = shift;

    if ($self->test($strSection))
    {
        if (!defined($strSortOrder) || $strSortOrder eq INI_SORT_FORWARD)
        {
            return (sort(keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq INI_SORT_REVERSE)
        {
            return (sort {$b cmp $a} (keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq INI_SORT_NONE)
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
# test - test a value.
#
# Test a value to see if it equals the supplied test value.  If no test value is given, tests that the section, key, or subkey
# is defined.
####################################################################################################################################
sub test
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $strTest = shift;

    # Get the value
    my $strResult = $self->get($strSection, $strValue, $strSubValue, false);

    # Is there a result
    if (defined($strResult))
    {
        # Is there a value to test against?
        if (defined($strTest))
        {
            # Make sure these are explicit strings or Devel::Cover thinks they are equal if one side is a boolean
            return ($strResult . '') eq ($strTest . '') ? true : false;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# boolTest - test a boolean value, see test().
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

####################################################################################################################################
# cipherPassSub - gets the passphrase (if it exists) used to read/write subsequent files
####################################################################################################################################
sub cipherPassSub
{
    my $self = shift;

    return $self->get(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS, undef, false);
}

####################################################################################################################################
# Properties.
####################################################################################################################################
sub modified {shift->{bModified}}                                   # Has the data been modified since last load/save?
sub exists {shift->{bExists}}                                       # Is the data persisted to file?
sub cipherPass {shift->{strCipherPass}}                             # Return passphrase (will be undef if repo not encrypted)

1;
