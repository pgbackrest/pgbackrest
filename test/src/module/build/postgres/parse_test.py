"""Test PostgreSQL Interface Parse.

The headers written here are cut down to the shapes the scan has to recognize, since what matters is which names it finds rather than
what any of them mean to PostgreSQL."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_write
from postgres.parse import *

# A vendored header holding each shape a type can be declared in
VENDOR = """#define CATALOG_VERSION_NO 202411051
#define PG_CONTROL_VERSION\t1700
#define FirstNormalObjectId(x) (x)
#define CATALOG_VERSION_NO 202411051

typedef uint32 TransactionId;

typedef struct ControlFileData
{
    uint64 system_identifier;
} ControlFileData;

typedef enum DBState
{
    DB_STARTUP = 0,
    DB_SHUTDOWNED,
} DBState;

typedef uint32 TransactionId;
"""

# The header that declares the interface functions as macros
INTERN = """#define PG_INTERFACE_CONTROL_IS(version)
#define PG_INTERFACE_CONTROL(version)
"""


####################################################################################################################################
def _parse(version, vendor=VENDOR, intern=INTERN):
    """Parse an interface declaration."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/postgres.yaml"), version)
        file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), vendor)
        file_write(os.path.join(path, "src/postgres/interface/version.intern.h"), intern)

        return bld_pg_parse(path)


####################################################################################################################################
def test_postgres_parse_version():
    """A version is a version on its own or a version with the attributes that go with it."""

    bld_pg = _parse("version:\n  - 9.6\n  - 10\n  - 19:\n      release: false\n")

    assert_equal([(pg.version, pg.release) for pg in bld_pg.pg_list], [("9.6", True), ("10", True), ("19", False)])

    # A version keeps the text it was written as, since it is rendered as both a name and a string
    assert_equal(bld_pg.pg_list[0].version, "9.6")


####################################################################################################################################
def test_postgres_parse_version_error():
    """A version declaration that is not one is reported."""

    with assert_raises(ToolError) as error:
        _parse("bogus:\n  - 10\n")

    assert_equal(str(error.exception), "unknown postgres definition 'bogus'")

    with assert_raises(ToolError) as error:
        _parse("version:\n  - 10:\n      bogus: false\n")

    assert_equal(str(error.exception), "unknown postgres definition 'bogus'")


####################################################################################################################################
def test_postgres_parse_type():
    """Every name the vendored header declares is a name the interface has to rename, including the values of an enum."""

    bld_pg = _parse("version:\n  - 10\n")

    # A plain typedef is named before the type, a struct and an enum after it, and an enum also contributes its values. A name the
    # header declares more than once is listed once.
    assert_equal(bld_pg.type_list, ["ControlFileData", "DBState", "DB_SHUTDOWNED", "DB_STARTUP", "TransactionId"])


####################################################################################################################################
def test_postgres_parse_define():
    """Every define the vendored header declares is a name the interface has to undefine."""

    bld_pg = _parse("version:\n  - 10\n")

    # A define may be followed by a parameter list or separated from its value by a tab, and a repeat is only listed once. The two
    # that no header declares are added so that every interface undefines them.
    assert_equal(
        bld_pg.define_list,
        ["CATALOG_VERSION_NO", "CATALOG_VERSION_NO_MAX", "FirstNormalObjectId", "PG_CONTROL_VERSION", "PG_VERSION"],
    )

    # Functions keep the order they were declared in, since that is the order the interface struct is filled in
    assert_equal(bld_pg.function_list, ["PG_INTERFACE_CONTROL_IS", "PG_INTERFACE_CONTROL"])


####################################################################################################################################
def test_postgres_parse_define_error():
    """A define that cannot be read is reported, since a name missed here is a name the interface would not rename."""

    with assert_raises(ToolError) as error:
        _parse("version:\n  - 10\n", vendor="#define  SPACED 1\n")

    assert_equal(str(error.exception), "unable to find define -- are there extra spaces on '#define  SPACED 1'")
