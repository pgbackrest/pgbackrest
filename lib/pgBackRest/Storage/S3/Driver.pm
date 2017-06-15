####################################################################################################################################
# S3 Storage Driver
####################################################################################################################################
package pgBackRest::Storage::S3::Driver;
use parent 'pgBackRest::Storage::S3::Request';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Digest::MD5 qw(md5_base64);
use File::Basename qw(basename dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Xml;
use pgBackRest::Storage::S3::FileRead;
use pgBackRest::Storage::S3::FileWrite;
use pgBackRest::Storage::S3::Request;
use pgBackRest::Storage::S3::Info;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant STORAGE_S3_DRIVER                                      => __PACKAGE__;
    push @EXPORT, qw(STORAGE_S3_DRIVER);

####################################################################################################################################
# Query constants
####################################################################################################################################
use constant S3_QUERY_CONTINUATION_TOKEN                            => 'continuation-token';
use constant S3_QUERY_DELIMITER                                     => 'delimiter';
use constant S3_QUERY_LIST_TYPE                                     => 'list-type';
use constant S3_QUERY_PREFIX                                        => 'prefix';

####################################################################################################################################
# Batch maximum size
####################################################################################################################################
use constant S3_BATCH_MAX                                           => 1000;

####################################################################################################################################
# openWrite - open a file for write
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->openWrite', \@_,
        {name => 'strFile', trace => true},
    );

    my $oFileIO = new pgBackRest::Storage::S3::FileWrite($self, $strFile);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
    );
}

####################################################################################################################################
# openRead
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $bIgnoreMissing,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->openRead', \@_,
        {name => 'strFile', trace => true},
        {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
    );

    my $oFileIO = new pgBackRest::Storage::S3::FileRead($self, $strFile, {bIgnoreMissing => $bIgnoreMissing});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
    );
}

####################################################################################################################################
# manifest
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $bRecurse,
        $bPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPath', trace => true},
            # Optional parameters not part of the driver spec
            {name => 'bRecurse', optional => true, default => true, trace => true},
            {name => 'bPath', optional => true, default => true, trace => true},
        );

    # Determine the prefix (this is the search path within the bucket)
    my $strPrefix = $strPath eq qw{/} ? undef : substr($strPath, 1) . ($bPath ? qw{/} : '');

    # A delimiter must be used if recursion is not desired
    my $strDelimiter = $bRecurse ? undef : '/';

    # Hash to hold the manifest
    my $hManifest = {};

    # Continuation token - returned from requests where there is more data to be fetched
    my $strContinuationToken;

    do
    {
        # Get the file list
        my $oResponse = $self->request(
            HTTP_VERB_GET, {hQuery =>
            {&S3_QUERY_LIST_TYPE => 2, &S3_QUERY_PREFIX => $strPrefix, &S3_QUERY_DELIMITER => $strDelimiter,
                &S3_QUERY_CONTINUATION_TOKEN => $strContinuationToken}, strResponseType => S3_RESPONSE_TYPE_XML});

        # Modify the prefix for file searches so the filename is not stripped off
        if (defined($strPrefix) && !$bPath)
        {
            # If there are no paths in the prefix then undef it
            if (index($strPrefix, qw{/}) == -1)
            {
                undef($strPrefix);
            }
            else
            {
                $strPrefix = dirname($strPrefix) . qw{/};
            }
        }

        # Store files
        foreach my $oFile (xmlTagChildren($oResponse, "Contents"))
        {
            my $strName = xmlTagText($oFile, "Key");

            # Strip off prefix
            if (defined($strPrefix))
            {
                $strName = substr($strName, length($strPrefix));
            }

            $hManifest->{$strName}->{type} = 'f';
            $hManifest->{$strName}->{size} = xmlTagText($oFile, 'Size') + 0;

            # Generate paths from the name if recursing
            if ($bRecurse)
            {
                my @stryName = split(qw{/}, $strName);

                if (@stryName > 1)
                {
                    $strName = undef;

                    for (my $iIndex = 0; $iIndex < @stryName - 1; $iIndex++)
                    {
                        $strName .= (defined($strName) ? qw{/} : '') . $stryName[$iIndex];
                        $hManifest->{$strName}->{type} = 'd';
                    }
                }
            }
        }

        # Store directories
        if ($bPath && !$bRecurse)
        {
            foreach my $oPath (xmlTagChildren($oResponse, "CommonPrefixes"))
            {
                my $strName = xmlTagText($oPath, "Prefix");

                # Strip off prefix
                if (defined($strPrefix))
                {
                    $strName = substr($strName, length($strPrefix));
                }

                # Strip off final /
                $strName = substr($strName, 0, length($strName) - 1);

                $hManifest->{$strName}->{type} = 'd';
            }
        }

        $strContinuationToken = xmlTagText($oResponse, "NextContinuationToken", false);
    }
    while (defined($strContinuationToken));

    # Add . for the initial path (this is just for compatibility with filesystems that have directories)
    if ($bPath)
    {
        $hManifest->{qw{.}}->{type} = 'd';
    }

    # use Data::Dumper; &log(WARN, 'MANIFEST' . Dumper($hManifest));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

####################################################################################################################################
# list - list a directory
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strPath', trace => true},
        );

    # Get list using manifest function
    my @stryFileList = grep(!/^\.$/i, keys(%{$self->manifest($strPath, {bRecurse => false})}));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList, ref => true, trace => true}
    );
}

####################################################################################################################################
# pathCreate - directories do no exist in s3 so this is a noop
####################################################################################################################################
sub pathCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathCreate', \@_,
            {name => 'strPath', trace => true},
        );

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# pathSync - directories do not exist in s3 so this is a noop
####################################################################################################################################
sub pathSync
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPath', trace => true},
        );

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# exists - check if a file exists
####################################################################################################################################
sub exists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exists', \@_,
            {name => 'strFile', trace => true},
        );

    # Does the path/file exist?
    my $bExists = defined($self->manifest($strFile, {bRecurse => false, bPath => false})->{basename($strFile)}) ? true : false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists, trace => true}
    );
}

####################################################################################################################################
# pathExists
####################################################################################################################################
sub pathExists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathExists', \@_,
            {name => 'strPath', trace => true},
        );

    my $bExists = true;

    # Only check if path <> /
    if ($strPath ne qw{/})
    {
        # Does the path exist?
        my $rhInfo = $self->manifest(dirname($strPath), {bRecurse => false, bPath => true})->{basename($strPath)};
        $bExists = defined($rhInfo) && $rhInfo->{type} eq 'd' ? true : false;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists, trace => true}
    );
}

####################################################################################################################################
# info - get information about a file
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strFile', trace => true},
        );

    # Get the file
    my $rhFile = $self->manifest($strFile, {bRecurse => false, bPath => false})->{basename($strFile)};

    if (!defined($rhFile))
    {
        confess &log(ERROR, "unable to get info for missing file ${strFile}", ERROR_FILE_MISSING);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oInfo', value => new pgBackRest::Storage::S3::Info($rhFile->{size}), trace => true}
    );
}

####################################################################################################################################
# remove
####################################################################################################################################
sub remove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $rstryFile,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'rstryFile', trace => true},
            {name => 'bRecurse', optional => true, default => false, trace => true},
        );

    # Remove a tree
    if ($bRecurse)
    {
        my $rhManifest = $self->manifest($rstryFile);
        my @stryRemoveFile;

        # Iterate all files in the manifest
        foreach my $strFile (sort({$b cmp $a} keys(%{$rhManifest})))
        {
            next if $rhManifest->{$strFile}->{type} eq 'd';
            push(@stryRemoveFile, "${rstryFile}/${strFile}");
        }

        # Remove files
        if (@stryRemoveFile > 0)
        {
            $self->remove(\@stryRemoveFile);
        }
    }
    # Only remove the specified file
    else
    {
        # If stryFile is a scalar, convert to an array
        my $rstryFileAll = ref($rstryFile) ? $rstryFile : [$rstryFile];

        do
        {
            my $strFile = shift(@{$rstryFileAll});
            my $iTotal = 0;
            my $strXml = XML_HEADER . '<Delete><Quiet>true</Quiet>';

            while (defined($strFile))
            {
                $iTotal++;
                $strXml .= '<Object><Key>' . substr($strFile, 1) . '</Key></Object>';

                $strFile = $iTotal < 2 ? shift(@{$rstryFileAll}) : undef;
            }

            $strXml .= '</Delete>';

            my $hHeader = {'content-md5' => md5_base64($strXml) . '=='};

            # Delete a file
            my $oResponse = $self->request(
                HTTP_VERB_POST,
                {hQuery => 'delete=', rstrBody => \$strXml, hHeader => $hHeader, strResponseType => S3_RESPONSE_TYPE_XML});
        }
        while (@{$rstryFileAll} > 0);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => true, trace => true}
    );
}


####################################################################################################################################
# Getters
####################################################################################################################################
sub capability {false}
sub className {STORAGE_S3_DRIVER}

1;
