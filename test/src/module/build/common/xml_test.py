"""Test Xml Handler.

Mixed content is what most of this is about, so the checks are written as the xml that comes out rather than as the tree, since that is
where an error in where text ended up actually shows."""

####################################################################################################################################
import xml.etree.ElementTree as etree

from harness.test import *

from common.error import *
from common.xml import *


####################################################################################################################################
def _render(node):
    """Render a node as the xml text of it."""

    return etree.tostring(node, encoding="unicode")


####################################################################################################################################
def test_xml_document():
    """A document is a root node and the declarations above it."""

    document = xml_document_new("doc")
    xml_node_attribute_set(document.root, "title", "Reference")

    assert_equal(document.render(), '<?xml version="1.0" encoding="UTF-8"?>\n<doc title="Reference" />\n')

    # A document type, which is what says where the tags it uses are declared
    document = xml_document_new("doc", dtd_name="doc", dtd_file="doc.dtd")
    xml_node_add(document.root, "p")

    assert_equal(
        document.render(),
        '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE doc SYSTEM "doc.dtd">\n<doc><p /></doc>\n',
    )


####################################################################################################################################
def test_xml_document_parse():
    """A document that is read and written again declares the same document type."""

    document = xml_document_parse('<?xml version="1.0"?>\n<!DOCTYPE doc SYSTEM "doc.dtd">\n<doc><p>text</p></doc>\n', "test.xml")

    assert_equal(document.dtd_name, "doc")
    assert_equal(document.dtd_file, "doc.dtd")
    assert_equal(
        document.render(), '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE doc SYSTEM "doc.dtd">\n<doc><p>text</p></doc>\n'
    )

    # A document that declares none
    document = xml_document_parse("<doc><p>text</p></doc>", "test.xml")

    assert_is_none(document.dtd_name)
    assert_equal(document.render(), '<?xml version="1.0" encoding="UTF-8"?>\n<doc><p>text</p></doc>\n')


####################################################################################################################################
def test_xml_parse():
    """Xml that cannot be parsed is reported with the name of the file, since the error is otherwise only a position."""

    assert_equal(xml_parse("<doc><p>text</p></doc>", "test.xml").tag, "doc")

    with assert_raises(ToolError) as error:
        xml_parse("<doc><p></doc>", "test.xml")

    assert_in("unable to parse 'test.xml':", str(error.exception))


####################################################################################################################################
def test_xml_attribute():
    """An attribute is read, set, and removed by name."""

    node = xml_parse('<doc title="Reference" toc="y"/>', "test.xml")

    assert_equal(xml_node_attribute(node, "title"), "Reference")
    assert_is_none(xml_node_attribute(node, "missing"))

    with assert_raises(ToolError) as error:
        xml_node_attribute(node, "missing", True)

    assert_equal(str(error.exception), "unable to find attribute 'missing' in node 'doc'")

    xml_node_attribute_set(node, "title", "Other")

    assert_equal(xml_node_attribute(node, "title"), "Other")

    # Removing one that is there and one that is not, since a node that never had it is already how it should end up
    xml_node_attribute_remove(node, "toc")
    xml_node_attribute_remove(node, "missing")

    assert_equal(_render(node), '<doc title="Other" />')


####################################################################################################################################
def test_xml_child():
    """A child is found by name, and every child of a name can be found at once."""

    node = xml_parse("<doc><p>one</p><p>two</p><title>three</title></doc>", "test.xml")

    assert_equal(xml_node_content(xml_node_child(node, "p")), "one")
    assert_equal([xml_node_content(child) for child in xml_node_child_list(node, "p")], ["one", "two"])
    assert_is_none(xml_node_child(node, "missing"))

    with assert_raises(ToolError) as error:
        xml_node_child(node, "missing", True)

    assert_equal(str(error.exception), "unable to find child 'missing' in node 'doc'")

    # A node holds all the text under it, whatever markup it is spread across
    node = xml_parse("<p>Run <id>pgbackrest</id> as <b>root</b>.</p>", "test.xml")

    assert_equal(xml_node_content(node), "Run pgbackrest as root.")


####################################################################################################################################
def test_xml_dup():
    """A copy can be changed without changing what it came from, which is what makes a block reusable."""

    node = xml_parse("<block><p>text</p></block>", "test.xml")
    dup = xml_node_dup(node)

    xml_node_text_set(xml_node_child(dup, "p"), "other")

    assert_equal(_render(node), "<block><p>text</p></block>")
    assert_equal(_render(dup), "<block><p>other</p></block>")


####################################################################################################################################
def test_xml_content_add():
    """Text is added to the end of what a node holds, which is how a title of text and markup in turn is built."""

    node = etree.Element("title")

    xml_node_content_add(node, "Backup Command (")
    xml_node_content_add(xml_node_add(node, "id"), "backup")
    xml_node_content_add(node, ")")

    assert_equal(_render(node), "<title>Backup Command (<id>backup</id>)</title>")

    # Text added to a node that already ends with text goes on the end of it rather than replacing it
    xml_node_content_add(node, " and more")

    assert_equal(_render(node), "<title>Backup Command (<id>backup</id>) and more</title>")


####################################################################################################################################
def test_xml_text_set():
    """Setting text replaces everything a node holds, unlike adding it."""

    node = xml_parse("<p>Run <id>pgbackrest</id> now.</p>", "test.xml")

    xml_node_text_set(node, "Run it now.")

    assert_equal(_render(node), "<p>Run it now.</p>")


####################################################################################################################################
def test_xml_child_add():
    """Everything a source node holds is copied, with the text and the markup left in the order they were written."""

    node = etree.Element("p")
    source = xml_parse('<text>Run <id class="cmd">pgbackrest</id> as <b>root</b> now.</text>', "test.xml")

    xml_node_child_add(node, source)

    assert_equal(_render(node), '<p>Run <id class="cmd">pgbackrest</id> as <b>root</b> now.</p>')

    # A comment is dropped by the parser, so it is never there to copy
    node = etree.Element("p")
    source = xml_parse("<text>one<!-- note -->two</text>", "test.xml")

    assert_equal(len(source), 0)

    xml_node_child_add(node, source)

    assert_equal(_render(node), "<p>onetwo</p>")

    # Markup nested more than one deep
    node = etree.Element("p")
    source = xml_parse("<text><list><list-item>one</list-item></list></text>", "test.xml")

    xml_node_child_add(node, source)

    assert_equal(_render(node), "<p><list><list-item>one</list-item></list></p>")


####################################################################################################################################
def test_xml_child_replace():
    """A node is replaced by what a source node holds, in the place the node was rather than at the end."""

    # A node in the middle, with text on both sides of it
    node = xml_parse("<p>before <insert/> after</p>", "test.xml")
    source = xml_parse("<text>one <b>two</b> three</text>", "test.xml")

    xml_node_child_replace(node, xml_node_child(node, "insert"), source)

    assert_equal(_render(node), "<p>before one <b>two</b> three after</p>")

    # The first node, so the text it begins with belongs to what holds it
    node = xml_parse("<p><insert/> after</p>", "test.xml")
    source = xml_parse("<text>one <b>two</b></text>", "test.xml")

    xml_node_child_replace(node, xml_node_child(node, "insert"), source)

    assert_equal(_render(node), "<p>one <b>two</b> after</p>")

    # A node that follows another, so the text belongs to the one before it
    node = xml_parse("<p><b>first</b><insert/>last</p>", "test.xml")
    source = xml_parse("<text>middle</text>", "test.xml")

    xml_node_child_replace(node, xml_node_child(node, "insert"), source)

    assert_equal(_render(node), "<p><b>first</b>middlelast</p>")

    # A source that holds nothing, which removes the node and leaves what was around it
    node = xml_parse("<p>before <insert/> after</p>", "test.xml")
    source = xml_parse("<text/>", "test.xml")

    xml_node_child_replace(node, xml_node_child(node, "insert"), source)

    assert_equal(_render(node), "<p>before  after</p>")

    # A source that holds nothing, replacing the first node
    node = xml_parse("<p><insert/>after</p>", "test.xml")
    source = xml_parse("<text/>", "test.xml")

    xml_node_child_replace(node, xml_node_child(node, "insert"), source)

    assert_equal(_render(node), "<p>after</p>")
