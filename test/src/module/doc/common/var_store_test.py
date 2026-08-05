"""Test Variable Store."""

####################################################################################################################################
import xml.etree.ElementTree as etree

from harness.test import *

from common.var_store import *
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


####################################################################################################################################
def test_var_store_set():
    """A variable that is only known once the build is running replaces what it had, unlike one a document declares."""

    var_store = VarStore()

    var_store.add("host", "one")
    var_store.add("host", "two")

    assert_equal(var_store.get("host"), "one")

    # An address or the output of a command is set rather than added, since it is not known until it is known
    var_store.set("host", "three")

    assert_equal(var_store.get("host"), "three")
    assert_true(var_store.test("host", "three"))
    assert_false(var_store.test("host", "one"))

    # A variable that was never set has no value rather than an empty one
    assert_is_none(var_store.get("missing"))
    assert_false(var_store.test("missing", ""))

    # Nothing is what nothing replaces to, so a caller with something optional in hand does not have to check first
    assert_is_none(var_store.replace_str(None))


####################################################################################################################################
def test_var_store_eval():
    """A variable may ask for a fact about the environment that the document has no other way to know."""

    import os

    from common.user import user_name

    var_store = VarStore()

    assert_equal(eval_value("cwd"), os.getcwd())
    assert_equal(eval_value("'vagrant' if user == 'root' else user"), "vagrant" if user_name() == "root" else user_name())

    # The result is text, since that is what a variable holds
    assert_equal(eval_value("1 + 1"), "2")

    # What it may use is a short list rather than the whole interpreter
    with assert_raises(ToolError) as raised:
        eval_value("open('/etc/passwd')")

    assert_in("unable to evaluate 'open('/etc/passwd')'", str(raised.exception))


####################################################################################################################################
def test_var_store_add_node():
    """A variable is declared by a node, which says whether what it holds is the value or how to work it out."""

    from common.xml import xml_parse

    var_store = VarStore()

    var_store.add("project", "pgBackRest")
    var_store.add_node(xml_parse("<variable key='title'>{[project]} Guide</variable>", "test.xml"))

    assert_equal(var_store.get("title"), "pgBackRest Guide")

    assert_equal(var_store.add_node(xml_parse("<variable key='sum' eval='y'>2 + 2</variable>", "test.xml")), "4")
    assert_equal(var_store.get("sum"), "4")
