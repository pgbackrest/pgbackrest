####################################################################################################################################
# This meson script generates doxygen output from the 'src' directory tree.
#
# Doxygen documentation is not built by default.
# To create doxygen documentation, give the following command:
#         ninja  doxygen
#
# This script does the following:
#    - Verify flex and doxygen are present.
#    - Run "doxyCgen" script to generate C documentation under "docs/doxygen" in the build directory.
#
# Note these build rules run the "doxyCgen' script rather than running doxygen directly.
#   The script configures doxygen and builds the C filter if not already built.
####################################################################################################################################

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

####################################################################################################################################
# Generate documentation when the command  "ninja doxygen" is given.
####################################################################################################################################
doxyCgen=files('./doxyCgen')                                        # Script for generating doxygen docs
Doxyfile=files('./Doxyfile')                                        # Configuration file for doxygen. Make sure it exists.
c_src = meson.source_root() / 'src'                                 # Where the C source tree is located.
doxy_out = meson.build_root() / 'doc/doxygen'                       # Where to create the documentation.
c_filter = doxy_out / 'doxyCfilter'                                 # Where the doxyCfilter should be found or built.

# Run the script to create doxygen docs.
run_target('doxygen',
    command: [doxyCgen, c_src, doxy_out, c_filter],
    env: {'PROJECT_NAME':'pgbackrest', 'PROJECT_BRIEF':'BACKUP for PostgreSQL'},
)
