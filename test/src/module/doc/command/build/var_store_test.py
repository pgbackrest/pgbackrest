"""Test Variable Store."""

####################################################################################################################################
import xml.etree.ElementTree as etree

from harness.test import *

from command.build.var_store import *
from common.xml import xml_node_content


####################################################################################################################################
def test_var_store_replace_str():
    """A variable is replaced wherever it appears, including in the value of another variable."""

    var_store = VarStore()
    var_store.add("project", "pgBackRest")
    var_store.add("exe", "pgbackrest")

    assert_equal(var_store.replace_str("{[project]} runs as {[exe]}"), "pgBackRest runs as pgbackrest")

    # Nothing to replace
    assert_equal(var_store.replace_str("no variable here"), "no variable here")

    # A value that itself refers to a variable, which is replaced in turn rather than left as it was written
    var_store.add("host", "{[exe]}-host")
    var_store.add("path", "/var/lib/{[host]}")

    assert_equal(var_store.replace_str("{[path]}"), "/var/lib/pgbackrest-host")

    # The value a variable already has wins, since the command line is loaded before the document declares its own
    var_store.add("project", "other")

    assert_equal(var_store.replace_str("{[project]}"), "pgBackRest")


####################################################################################################################################
def test_var_store_replace_node():
    """A variable is replaced in the attributes and the text of a node and everything under it."""

    var_store = VarStore()
    var_store.add("project", "pgBackRest")
    var_store.add("exe", "pgbackrest")

    node = etree.fromstring(
        '<block id="{[exe]}-block">' "<p>Run {[project]} as <id>{[exe]}</id> on {[project]}.</p>" "<p>Nothing here.</p>" "</block>"
    )

    var_store.replace_node(node)

    assert_equal(node.get("id"), "pgbackrest-block")

    # Text before a child, inside it, and trailing it are all replaced, since a paragraph is text and markup interleaved
    assert_equal(xml_node_content(node[0]), "Run pgBackRest as pgbackrest on pgBackRest.")
    assert_equal(xml_node_content(node[1]), "Nothing here.")
