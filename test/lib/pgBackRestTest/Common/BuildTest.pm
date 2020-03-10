####################################################################################################################################
# Build Binaries and Auto-Generate Code
####################################################################################################################################
package pgBackRestTest::Common::BuildTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

# use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

####################################################################################################################################
# VM hash keywords
####################################################################################################################################
use constant BUILD_AUTO_H                                           => 'build.auto.h';
    push @EXPORT, qw(BUILD_AUTO_H);

####################################################################################################################################
# Save contents to a file if the file is missing or the contents are different.  This saves write IO and prevents the timestamp from
# changing.
####################################################################################################################################
sub buildPutDiffers
{
    my $oStorage = shift;
    my $strFile = shift;
    my $strContentNew = shift;

    # Attempt to load the file
    my $bSave = true;
    my $oFile = $oStorage->openRead($strFile, {bIgnoreMissing => true});

    # If file was found see if the content is the same
    if (defined($oFile))
    {
        my $strContentFile = ${$oStorage->get($oFile)};

        if ((defined($strContentFile) ? $strContentFile : '') eq (defined($strContentNew) ? $strContentNew : ''))
        {
            $bSave = false;
        }
    }

    # Save if the contents are different or missing
    if ($bSave)
    {
        $oStorage->put($oStorage->openWrite($strFile, {bPathCreate => true}), $strContentNew);
    }

    # Was the file saved?
    return $bSave;
}

push @EXPORT, qw(buildPutDiffers);

####################################################################################################################################
# Find last modification time in a list of directories, with optional filters
####################################################################################################################################
sub buildLastModTime
{
    my $oStorage = shift;
    my $strBasePath = shift;
    my $rstrySubPath = shift;
    my $strPattern = shift;

    my $lTimestampLast = 0;

    foreach my $strSubPath (defined($rstrySubPath) ? @{$rstrySubPath} : (''))
    {
        my $hManifest = $oStorage->manifest($strBasePath . ($strSubPath eq '' ? '' : "/${strSubPath}"));

        foreach my $strFile (sort(keys(%{$hManifest})))
        {
            next if (defined($strPattern) && $strFile !~ /$strPattern/);

            if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampLast)
            {
                $lTimestampLast = $hManifest->{$strFile}{modification_time};
            }
        }
    }

    if ($lTimestampLast == 0)
    {
        confess &log(ERROR, "no files found");
    }

    return $lTimestampLast;
}

push @EXPORT, qw(buildLastModTime);

####################################################################################################################################
# Build a dependency tree for C files
####################################################################################################################################
sub buildDependencyTree
{
    my $oStorage = shift;

    my $rhDependencyTree = {};

    # Iterate all files
    foreach my $strFile (sort(keys(%{$oStorage->manifest('src')})))
    {
        # Only process non-auto files
        if ($strFile =~ /^[A-Za-z0-9\/]+\.(c|h)$/)
        {
            buildDependencyTreeSub($oStorage, $rhDependencyTree, $strFile, true, undef, ['src']);
        }
    }

    return $rhDependencyTree;
}

push @EXPORT, qw(buildDependencyTree);

sub buildDependencyTreeSub
{
    my $oStorage = shift;
    my $rhDependencyTree = shift;
    my $strFile = shift;
    my $bErrorOnBuildHeaderMissing = shift;
    my $strBasePath = shift;
    my $rstryPath = shift;

    if (!defined($rhDependencyTree->{$strFile}))
    {
        $rhDependencyTree->{$strFile} = {};

        # Load file contents
        my $rstrContent;

        foreach my $strPath (@{$rstryPath})
        {
            $rstrContent = $oStorage->get(
                $oStorage->openRead(
                    (defined($strBasePath) ? "${strBasePath}/" : '') . ($strPath ne '' ? "${strPath}/" : '') . "${strFile}",
                    {bIgnoreMissing => true}));

            if (defined($rstrContent) || $strFile eq BUILD_AUTO_H)
            {
                $rhDependencyTree->{$strFile}{path} = $strPath;
                last;
            }
        }

        if ($strFile ne BUILD_AUTO_H)
        {
            if (!defined($rstrContent))
            {
                confess &log(ERROR,
                    "unable to find ${strFile} in " . $oStorage->pathGet($strBasePath) . " + [" . join(', ', @{$rstryPath}) . "]");
            }

            # Process includes
            my $rhInclude = {};
            my $first = true;

            foreach my $strInclude ($$rstrContent =~ /^\s*\#include [\"\<].+[\"\>]\s*$/mg)
            {
                if ($bErrorOnBuildHeaderMissing && $first &&
                    $strFile =~ /\.c/ && trim($strInclude) ne ('#include "' . BUILD_AUTO_H . '"'))
                {
                    confess &log(ERROR, "'" . BUILD_AUTO_H . "' must be included first in '${strFile}'");
                }

                if (trim($strInclude) =~ /^\#include \"/)
                {
                    $strInclude = (split('"', $strInclude))[1];
                    $rhInclude->{$strInclude} = true;

                    buildDependencyTreeSub(
                        $oStorage, $rhDependencyTree, $strInclude, $bErrorOnBuildHeaderMissing, $strBasePath, $rstryPath);

                    foreach my $strIncludeSub (@{$rhDependencyTree->{$strInclude}{include}})
                    {
                        $rhInclude->{$strIncludeSub} = true;
                    }
                }

                $first = false;
            }

            my @stryInclude = sort(keys(%{$rhInclude}));
            $rhDependencyTree->{$strFile}{include} = \@stryInclude;
        }
    }
}

push @EXPORT, qw(buildDependencyTreeSub);

####################################################################################################################################
# Build Makefile object compile rules
####################################################################################################################################
sub buildMakefileObjectCompile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $rhOption,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::buildMakefile', \@_,
            {name => 'oStorage'},
            {name => 'rhOption', optional => true},
        );

    my $rhDependencyTree = buildDependencyTree($oStorage);
    my $strMakefile;

    foreach my $strFile (sort(keys(%{$rhDependencyTree})))
    {
        if ($strFile =~ /^[A-Za-z0-9\/]+\.c$/)
        {
            my $strObject = substr($strFile, 0, length($strFile) - 1) . 'o';

            my $strDepend = "${strFile}";

            foreach my $strInclude (@{$rhDependencyTree->{$strFile}{include}})
            {
                $strDepend .= " ${strInclude}";
            }

            $strMakefile .=
                (defined($strMakefile) ? "\n" : '') .
                "${strObject}: ${strDepend}\n" .
                "\t\$(CC) \$(CPPFLAGS) \$(CFLAGS) \$(CMAKE)" .
                    (defined($rhOption->{$strObject}) ? ' ' . $rhOption->{$strObject} : '') . " -c ${strFile} -o ${strObject}\n";
        }
    }

    return $strMakefile;
}

push @EXPORT, qw(buildMakefileObjectCompile);

####################################################################################################################################
# Update a Makefile with object compile rules
####################################################################################################################################
sub buildMakefile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $strMakefile,
        $rhOption,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::buildMakefile', \@_,
            {name => 'oStorage'},
            {name => 'strMakeFile'},
            {name => 'rhOption', optional => true},
        );

    # Trim off the end where the object compile is
    my $strHeader = 'Compile rules';

    $strMakefile =~ s/^\#.*${strHeader}(.|\s)+//mg;

    # Add object compile section
    $strMakefile .=
        "# $strHeader\n" .
        ('#' x 132) . "\n" .
        buildMakefileObjectCompile($oStorage, {rhOption => $rhOption});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strMakefile', value => $strMakefile},
    );
}

push @EXPORT, qw(buildMakefile);


1;
