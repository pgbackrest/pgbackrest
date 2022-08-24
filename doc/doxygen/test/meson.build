


##################################################################################
# Test the doxy_filter to verify it is flagging comments correctly.
##################################################################################
# A standard env for running test scripts under meson.
doxygen_test_env = {'MESON_SOURCE_DIR':meson.current_source_dir(), 'MESON_BUILD_DIR':meson.current_build_dir()}

# Test the doxy_filter executable create earlier.
test('doxy_filter',
    files('filter_test.sh'), args:[doxy_filter.full_path(), 'doxy_filter'],
    depends: doxy_filter,
    env:doxygen_test_env)
