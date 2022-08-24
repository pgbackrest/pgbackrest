###############################################################
# This meson script generates doxygen output from the adjoining "../src" code directory.
#
# Doxygen documentation is not built by default.
# To create doxygen documentation, give the following command:
#         ninja  doxygen
#
# This script works in several stages.
#    - Verify flex and doxygen are present.
#    - Make flex known to meson.
#    - Build a doxygen filter so doxygen recognizes the commenting style.
#    - Update the doxygen configuration file to reflect the current paths.
#    - Let doxygen generate documentation under "docs/doxygen" in the build directory.
#########################################################################

####################################################################################################################################
# First, check for dependencies. If flex and doxygen are not installed, then display a message and quit.
# Note: the original version depended on disabler propagation, which is inconsistent between various versions of meson,
#       The current version does an explicit test and exit rather than relying on disablers.
####################################################################################################################################
flex = find_program('flex', required:false)
flex_lib = meson.get_compiler('c').find_library('fl', required: false)
doxy = find_program('doxygen', required:false)
if not doxy.found() or not flex.found() or not flex_lib.found()
    message('Be sure to install flex and doxygen if you want to create doxygen documents.')
    run_target('doxygen', command: ['echo', 'You must install flex and doxygen before creating doxygen documents'])
    subdir_done()
endif


#########################################################################
# Make Flex known to Meson. Flex is a parsing program used to make scanners.
#   Flex takes a lexical analysis file (*.l) and generates a C file (*.c).
#########################################################################
flex_gen = generator(flex,
               output: '@PLAINNAME@.c',
               arguments: ['-o', '@OUTPUT@', '@INPUT@'])

##########################################################################
# Build the doxygen scanner. This scanner annotates comments in a C file
#    so the comments become visible to doxygen.
###########################################################################

# Step #1: Process flex code to generate C code.
doxy_filter_l = files('doxy_filter.l')
doxy_filter_c = flex_gen.process(doxy_filter_l)

# Step #2. Compile C code and create doxy_filter executable.
doxy_filter = executable('doxy_filter', doxy_filter_c,
                  dependencies: flex_lib,
                  c_args: ['-w'],  # TODO: disable specific warnings introduced by flex.
                  )

##############################################################################
# Update the Doxygen configuration file.  ("Doxyfile")
#
# This file tells doxygen how to generate documentation.
# Some of the configuration values depend on paths created by meson,
# so we insert those paths into a template to produce the final Doxyfile.
#
# Ideally, doxygen would allow us to set these values in the command line.
#
# We need to set the following doxygen values:
#      OUTPUT_DIRECTORY - directory where the documentation will be created.
#      INPUT            - project directory containing the "src" subdirectory.
#      INPUT_FILTER     - path to the doxy_filter executable.
#
# Note: when upgrading to meson 5.6 or greater,
#     change build_root-->project_build_root and source_root-->project_source_root.
########################################################################################
doxy_file_template = files('Doxyfile.template')
doxy_file = configure_file(
    input: doxy_file_template,
    output: 'Doxyfile',
    configuration: {
        'OUTPUT_DIRECTORY': meson.build_root() / 'doc/doxygen' ,
        'INPUT': meson.source_root() / 'src',
        'INPUT_FILTER': doxy_filter.full_path(),  # Does not create a dependency on doxy_filter.
    })


##########################################################################
# Generate documentation when the command  "ninja doxygen" is given.
#############################################################################
run_target('doxygen', command: [doxy, doxy_file], depends: doxy_filter)


# While we're here, lets include the 'doxygen' test suite.
#    Although convention would place it under a "../test/doc/doxygen" directory,
#    The tests are closely coupled with running doxygen - both will not run if flex or doxygen are missing and both
#    need to access the filter executable.  For now, we package them together, but this needs to be reconsidered.
# TODO: move ./test directory to ../test/doc/doxygen and subdir from the main meson.build instead of here. ???
subdir('test')