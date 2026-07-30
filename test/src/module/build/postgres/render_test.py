"""Test PostgreSQL Interface Render."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.storage import file_read, file_write
from postgres.parse import bld_pg_parse
from postgres.render import *

VENDOR = """typedef uint32 TransactionId;
"""

INTERN = """#define PG_INTERFACE_CONTROL_IS(version)
"""


####################################################################################################################################
def _render(version):
    """Render an interface declaration and return both generated files."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/postgres.yaml"), version)
        file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), VENDOR)
        file_write(os.path.join(path, "src/postgres/interface/version.intern.h"), INTERN)

        bld_pg = bld_pg_parse(path)
        bld_pg_render(path, bld_pg)
        bld_pg_version_render(path, bld_pg)

        return (
            file_read(os.path.join(path, "src/postgres/interface.auto.c.inc")),
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

    # The interface is included again for each version and everything it defined is undefined after it
    assert_in('#include "postgres/interface/version.intern.h"\n', interface)
    assert_in("PG_INTERFACE_CONTROL_IS(96);\n", interface)
    assert_in("#undef TransactionId\n", interface)
    assert_in("#undef CATALOG_VERSION_NO_MAX\n", interface)
    assert_in("#undef PG_INTERFACE_CONTROL_IS\n", interface)

    # The struct names each function after the macro that defines it
    assert_in(
        """static const PgInterface pgInterface[] =
{
    {
        .version = PG_VERSION_10,

        .controlIs = pgInterfaceControlIs10,
    },
    {
        .version = PG_VERSION_96,

        .controlIs = pgInterfaceControlIs96,
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
