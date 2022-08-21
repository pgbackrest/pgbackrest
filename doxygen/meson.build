###############################################################
# This meson script generates doxygen output from the adjoining "../src" code directory.
#
# Doxygen documentation is not built by default.
# To create doxygen documentation, give the following command:
#         ninja  doxygen
# Note the "doxygen" target will only be defined if flex and doxygen are installed.
#
# This script works in several stages.
#    - Make flex known to meson.
#    - Build a doxygen filter so doxygen recognizes the commenting style.
#    - Update the doxygen configuration file to reflect the current paths.
#    - Generate documentation under "doxygen" in the build directory.
#########################################################################

#########################################################################
# Make Flex known to Meson. Flex is a parsing program used to make scanners.
#   Flex takes lexical analysis file (*.l) and converts it a C file (*.c).
#########################################################################

# Flex is a program which generates C code.
flex = find_program('flex', disabler:true, required:false)
flex_lib = meson.get_compiler('c').find_library('fl', disabler:true, required:false)
flex_gen = generator(flex,
               output: '@PLAINNAME@.c',
               arguments: ['-o', '@OUTPUT@', '@INPUT@'])

##########################################################################
# Build the doxygen scanner. This scanner annotates comments in a C file
#    so the comments become visible to doxygen.
###########################################################################

# Step #1: Process flex code to generate C code.
doxy_filter_c = flex_gen.process('doxy_filter.l')  # only on demand?

# Step #2. Compile C code and create doxy_filter executable.
doxy_filter = executable('doxy_filter', doxy_filter_c,
                  dependencies: flex_lib,
                  c_args: ['-w'],  # TODO: disable specific warnings.
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
doxy_file = doxy_filter and configure_file(  # Pythonic "and" is alternative to "dependencies"
    input: doxy_file_template,
    output: 'Doxyfile',
    # dependencies: doxy_filter, # unfortunately, doesn't accept dependencies.
    configuration: {
        'OUTPUT_DIRECTORY': meson.build_root() / 'doxygen' ,
        'INPUT': meson.source_root() / 'src',
        'INPUT_FILTER': doxy_filter.full_path(),
    })


##########################################################################
# Generate documentation when the command  "ninja doxygen" is given.
#############################################################################
doxy = find_program('doxygen', disabler:true, required:false)
run_target('doxygen', command: [doxy, doxy_file])

