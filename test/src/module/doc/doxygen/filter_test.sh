#!/usr/bin/bash
####################################################################################################################################
# Script to test a generic doxygen filter program.
#   filter_test.sh   <path to executable>  <data set name>
#
# The test script makes use of the following data files.
#    $SRC/<dataset>.example   - the source file to run the filter on.
#    $SRC/<dataset>.expected  - the expected output from the filter.
#    $BLD/<dataset>.actual    - the actual output from the filter.
#    $BLD/<dataset>.diff      - the differences between the expected and the actual.
#
# When run under meson, $SRC and $BLD will be the meson build and source directories.
# When run standalone, $SRC and $BLD will default to the current directory.
####################################################################################################################################

# Get the arguments into terms we can use.
BLD=${MESON_BUILD_DIR:-.}
SRC=${MESON_SOURCE_DIR:-.}
EXE=$1  # Path to the filter executable.
DATA=$2  # Base name of the data files.

# Run the filter to generate the output. Doxygen filters accept an input file name and send to stdout.
$EXE $SRC/$DATA.example > $BLD/$DATA.actual

# Compare the actual output with the expected output. If different, generate an error.
if ! diff $SRC/$DATA.expected $BLD/$DATA.actual > $BLD/$DATA.diff; then
    cat $BLD/$DATA.diff 1>&2
    exit 1
fi

# Success.
exit 0