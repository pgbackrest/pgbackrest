"""Test PostgreSQL Interface Render."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_read, file_write
from postgres.parse import BLD_PG_INTERFACE, BLD_PG_INTERFACE_HARNESS, bld_pg_parse
from postgres.render import *

# The comment the vendored header divides itself with, one before each entity it declares
SEPARATOR = "// " + "-" * 129

# A vendored header with a type that never varies and a type that does, since what a function reaches decides what can be shared
VENDOR = (
    "// TransactionId type\n" + SEPARATOR + "\n"
    "typedef uint32 TransactionId;\n"
    "\n"
    "// ControlFileData type\n" + SEPARATOR + "\n"
    "#if PG_VERSION > PG_VERSION_MAX\n"
    "\n"
    "#elif PG_VERSION >= PG_VERSION_10\n"
    "\n"
    "typedef struct ControlFileData\n"
    "{\n"
    "    TransactionId xid;\n"
    "} ControlFileData;\n"
    "\n"
    "#elif PG_VERSION >= PG_VERSION_96\n"
    "\n"
    "typedef struct ControlFileData\n"
    "{\n"
    "    uint64 system_identifier;\n"
    "} ControlFileData;\n"
    "\n"
    "#endif\n"
)

# One function reading the type that varies, so every version renders it, and one reading only the type that does not, so they
# share. The first is declared twice under a conditional, as the real header declares it, so which is rendered is a dependency too.
INTERN = (
    "#ifdef CATALOG_VERSION_NO_MAX\n"
    "\n"
    "#define PG_INTERFACE_CONTROL_IS(version)\\\n"
    "    static bool pgInterfaceControlIs##version(const ControlFileData *control) { return control != NULL; }\n"
    "\n"
    "#else\n"
    "\n"
    "#define PG_INTERFACE_CONTROL_IS(version)\\\n"
    "    static bool pgInterfaceControlIs##version(const ControlFileData *control);\n"
    "\n"
    "#endif\n"
    "\n"
    "#define PG_INTERFACE_CONTROL_CRC_OFFSET(version)\\\n"
    "    static size_t pgInterfaceControlCrcOffset##version(void) { return sizeof(TransactionId); }\n"
)

# An interface where every function is shared, which leaves the newer version with nothing of its own to render
INTERN_SHARE = (
    "#define PG_INTERFACE_CONTROL_CRC_OFFSET(version)\\\n"
    "    static size_t pgInterfaceControlCrcOffset##version(void) { return sizeof(TransactionId); }\n"
)

# An interface whose only function depends on nothing but whether the version has been released, which is the one thing that can be
# true of two versions with another version between them
INTERN_GAP = (
    "#ifdef CATALOG_VERSION_NO_MAX\n"
    "\n"
    "#define PG_INTERFACE_CONTROL_CRC_OFFSET(version)\\\n"
    "    static size_t pgInterfaceControlCrcOffset##version(void) { return 1; }\n"
    "\n"
    "#else\n"
    "\n"
    "#define PG_INTERFACE_CONTROL_CRC_OFFSET(version)\\\n"
    "    static size_t pgInterfaceControlCrcOffset##version(void) { return 0; }\n"
    "\n"
    "#endif\n"
)

INTERN_HARNESS = (
    "#define HRN_PG_INTERFACE_CONTROL(version)\\\n"
    "    static void hrnPgInterfaceControl##version(ControlFileData *control);\n"
    "\n"
    "#define HRN_PG_INTERFACE_WAL(version)\\\n"
    "    static void hrnPgInterfaceWal##version(TransactionId xid);\n"
)

# The harness header, which is hand-written apart from the system id defines that are generated into it
PATH_SYSTEM_ID = "test/src/harness/postgres.h"

HEADER = """#define HRN_PG_CONTROL_SIZE                                         8192

#define HRN_PG_SYSTEMID_11                                          (stale)
#define HRN_PG_SYSTEMID_11_Z                                        "stale"

#define HRN_PG_CONTROL_TIME(storageParam, timeParam, ...)
"""


####################################################################################################################################
def _render(version, intern=INTERN, interface=BLD_PG_INTERFACE):
    """Render an interface declaration and return the interface and the versions."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/postgres.yaml"), version)
        file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), VENDOR)
        file_write(os.path.join(path, interface.path_intern), intern)

        bld_pg = bld_pg_parse(path, interface)
        bld_pg_render(path, bld_pg)
        bld_pg_version_render(path, bld_pg)

        return (
            file_read(os.path.join(path, interface.path_render)),
            file_read(os.path.join(path, "src/postgres/version.auto.h")),
        )


####################################################################################################################################
def test_postgres_render_interface():
    """An interface renames every name of the vendored header for its own version, then puts them all back."""

    interface, _ = _render("version:\n  - 9.6\n  - 10\n")

    # The newest version comes first, so the most likely match is found first at run time
    assert_in("PostgreSQL 10 interface\n", interface)
    assert_true(interface.index("PostgreSQL 10 interface") < interface.index("PostgreSQL 9.6 interface"))

    # A dot is dropped from the version where it appears in a name but kept where it is reported
    assert_in("#define PG_VERSION                                                  PG_VERSION_96\n", interface)
    assert_in("#define TransactionId                                               TransactionId_96\n", interface)

    # The interface is included again for each version and everything it defined is undefined after it, including the functions the
    # version did not render, since the include defines the macro for all of them
    assert_in('#include "postgres/interface/version.intern.h"\n', interface)
    assert_in("#undef TransactionId\n", interface)
    assert_in("#undef CATALOG_VERSION_NO_MAX\n", interface)
    assert_in("#undef PG_INTERFACE_CONTROL_IS\n", interface)

    # A function reaching a type that varies is rendered by each version, since the code the compiler sees is not the same
    assert_in("PG_INTERFACE_CONTROL_IS(96);\n", interface)
    assert_in("PG_INTERFACE_CONTROL_IS(10);\n", interface)

    # A function reaching nothing that varies is rendered once, by the oldest version, and says which versions share it
    assert_in("PG_INTERFACE_CONTROL_CRC_OFFSET(96_10);                             // Shared with 10\n", interface)
    assert_not_in("PG_INTERFACE_CONTROL_CRC_OFFSET(10)", interface)

    # The struct names each function after the macro that defines it, and a version that shares one names the version that rendered
    # it rather than itself
    assert_in(
        """static const PgInterface pgInterface[] =
{
    {
        .version = PG_VERSION_10,

        .controlIs = pgInterfaceControlIs10,
        .controlCrcOffset = pgInterfaceControlCrcOffset96_10,
    },
    {
        .version = PG_VERSION_96,

        .controlIs = pgInterfaceControlIs96,
        .controlCrcOffset = pgInterfaceControlCrcOffset96_10,
    },
};
""",
        interface,
    )

    # A version that shares every one of its functions has nothing to render, so it gets no block at all
    interface, _ = _render("version:\n  - 9.6\n  - 10\n", intern=INTERN_SHARE)

    assert_in("PostgreSQL 9.6 interface\n", interface)
    assert_not_in("PostgreSQL 10 interface", interface)
    assert_in("PG_INTERFACE_CONTROL_CRC_OFFSET(96_10);                             // Shared with 10\n", interface)


####################################################################################################################################
def test_postgres_render_interface_harness():
    """The harness interface is rendered by the same code, so only its header, its names, and where it lands differ."""

    interface, _ = _render("version:\n  - 9.6\n  - 10\n", intern=INTERN_HARNESS, interface=BLD_PG_INTERFACE_HARNESS)

    assert_in("Automatically generated by 'build.py postgres-harness'", interface)

    # The vendored names are renamed for the version the same way, since they are the same names
    assert_in("#define TransactionId                                               TransactionId_96\n", interface)

    # The macros come from the harness header and are expanded for each version, and are shared by the same rule
    assert_in('#include "harness/postgres/version.intern.h"\n', interface)
    assert_in("HRN_PG_INTERFACE_CONTROL(96);\n", interface)
    assert_in("HRN_PG_INTERFACE_WAL(96_10);                                        // Shared with 10\n", interface)
    assert_in("#undef HRN_PG_INTERFACE_CONTROL\n", interface)

    # The struct and its functions carry the harness prefix
    assert_in(
        """static const HrnPgInterface hrnPgInterface[] =
{
    {
        .version = PG_VERSION_10,

        .control = hrnPgInterfaceControl10,
        .wal = hrnPgInterfaceWal96_10,
    },
    {
        .version = PG_VERSION_96,

        .control = hrnPgInterfaceControl96,
        .wal = hrnPgInterfaceWal96_10,
    },
};
""",
        interface,
    )


####################################################################################################################################
def test_postgres_render_interface_unreleased():
    """An unreleased version has no catalog version of its own yet, so it accepts any up to the maximum."""

    interface, _ = _render("version:\n  - 10\n  - 19:\n      release: false\n")

    assert_in("#define CATALOG_VERSION_NO_MAX\n", interface)

    # Only the unreleased version has it
    assert_equal(interface.count("#define CATALOG_VERSION_NO_MAX\n"), 1)

    # The two versions resolve the same types, but the function declared under the conditional is not the same function for both, so
    # it is rendered by each of them rather than shared
    assert_in("PG_INTERFACE_CONTROL_IS(19);\n", interface)
    assert_in("PG_INTERFACE_CONTROL_IS(10);\n", interface)
    assert_in("PG_INTERFACE_CONTROL_CRC_OFFSET(10_19);                             // Shared with 19\n", interface)


####################################################################################################################################
def test_postgres_render_interface_share_error():
    """Versions that share a rendering without being consecutive are reported, since a range would name a version between them."""

    with assert_raises(ToolError) as error:
        _render("version:\n  - 9.6\n  - 10:\n      release: false\n  - 11\n", intern=INTERN_GAP)

    assert_equal(str(error.exception), "versions 9.6, 11 share PG_INTERFACE_CONTROL_CRC_OFFSET but are not consecutive")


####################################################################################################################################
def test_postgres_render_version():
    """A version is a number the code compares and a string the errors report."""

    _, version = _render("version:\n  - 9.6\n  - 10\n")

    assert_in(
        """#define PG_VERSION_96                                               90600
#define PG_VERSION_10                                               100000

#define PG_VERSION_MAX                                              PG_VERSION_10
""",
        version,
    )

    assert_in(
        """#define PG_VERSION_96_Z                                             "9.6"
#define PG_VERSION_10_Z                                             "10"
""",
        version,
    )


####################################################################################################################################
def test_postgres_render_system_id():
    """A system id is derived from the version, and as a string it is rendered rather than built by the preprocessor."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/postgres.yaml"), "version:\n  - 9.6\n  - 10\n")
        file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), VENDOR)
        file_write(os.path.join(path, "src/postgres/interface/version.intern.h"), INTERN)
        file_write(os.path.join(path, PATH_SYSTEM_ID), HEADER)

        bld_pg_system_id_render(path, bld_pg_parse(path))

        # The block replaces every define that was there, wherever the first one was, and an offset is rendered for each version
        assert_equal(
            file_read(os.path.join(path, PATH_SYSTEM_ID)),
            """#define HRN_PG_CONTROL_SIZE                                         8192

#define HRN_PG_SYSTEMID_96                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_96)
#define HRN_PG_SYSTEMID_96_Z                                        "10000000000000090600"
#define HRN_PG_SYSTEMID_96_1_Z                                      "10000000000000090601"
#define HRN_PG_SYSTEMID_10                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_10)
#define HRN_PG_SYSTEMID_10_Z                                        "10000000000000100000"
#define HRN_PG_SYSTEMID_10_1_Z                                      "10000000000000100001"

#define HRN_PG_CONTROL_TIME(storageParam, timeParam, ...)
""",
        )


####################################################################################################################################
def test_postgres_render_system_id_error():
    """A header the defines are no longer in is reported, since the block would otherwise be dropped from it."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/postgres.yaml"), "version:\n  - 10\n")
        file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), VENDOR)
        file_write(os.path.join(path, "src/postgres/interface/version.intern.h"), INTERN)
        file_write(os.path.join(path, PATH_SYSTEM_ID), "#define HRN_PG_CONTROL_SIZE 8192\n")

        with assert_raises(ToolError) as error:
            bld_pg_system_id_render(path, bld_pg_parse(path))

        assert_equal(
            str(error.exception),
            "unable to find HRN_PG_SYSTEMID_ defines in '%s'" % os.path.join(path, PATH_SYSTEM_ID),
        )
