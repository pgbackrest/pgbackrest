"""Test Xml Handler.

Mixed content is what most of this is about, so the checks are written as the xml that comes out rather than as the tree, since that is
where an error in where text ended up actually shows."""

####################################################################################################################################
import os
import tempfile
import xml.etree.ElementTree as etree

from harness.test import *

from common.error import *
from common.storage import file_write
from common.xml import *


####################################################################################################################################
def _render(node):
    """Render a node as the xml text of it."""

    return etree.tostring(node, encoding="unicode")


####################################################################################################################################
def test_xml_document():
    """A document is built empty and filled in."""

    document = xml_document_new("doc")
    xml_node_attribute_set(document, "title", "Reference")
    xml_node_add(document, "section", {"id": "intro"})
    xml_node_add(document, "p")

    assert_equal(_render(document), '<doc title="Reference"><section id="intro" /><p /></doc>')


####################################################################################################################################
def test_xml_document_parse():
    """A document type says nothing this tool needs beyond the parts it declares, so one that declares none is read the same way."""

    assert_equal(_render(xml_document_parse("<doc><p>text</p></doc>", "test.xml")), "<doc><p>text</p></doc>")
    assert_equal(
        _render(xml_document_parse('<?xml version="1.0"?>\n<!DOCTYPE doc>\n<doc><p>text</p></doc>\n', "test.xml")),
        "<doc><p>text</p></doc>",
    )


####################################################################################################################################
def test_xml_document_parse_part():
    """A document is assembled from the parts it declares, which are named relative to the document that declares them."""

    content = """<!DOCTYPE doc [
    <!ENTITY v1 SYSTEM "part/one.xml">
    <!ENTITY v2 SYSTEM "part/two.xml">
]>
<doc>&v1;&v2;</doc>
"""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "part/one.xml"), '<release version="1.0"/>')
        file_write(os.path.join(path, "part/two.xml"), '<release version="2.0"/>')

        document = xml_document_parse(content, os.path.join(path, "release.xml"))

        assert_equal([xml_node_attribute(node, "version") for node in xml_node_child_list(document, "release")], ["1.0", "2.0"])

        # A part that is declared but never used means the file it names is silently not read, so it is reported instead
        with assert_raises(ToolError) as error:
            xml_document_parse(content.replace("&v2;", ""), os.path.join(path, "release.xml"))

        assert_equal(str(error.exception), "part 'v2' is declared but never used")


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

    assert_equal(xml_node_content(xml_node_child(node, "title")), "three")
    assert_equal([xml_node_content(child) for child in xml_node_child_list(node, "p")], ["one", "two"])
    assert_is_none(xml_node_child(node, "missing"))

    with assert_raises(ToolError) as error:
        xml_node_child(node, "missing", True)

    assert_equal(str(error.exception), "unable to find child 'missing' in node 'doc'")

    # Asking for the child rather than the list of them means there should only be one, so a second is a mistake in the document
    with assert_raises(ToolError) as error:
        xml_node_child(node, "p")

    assert_equal(str(error.exception), "found more than one child 'p' in node 'doc'")

    # A node holds all the text under it, whatever markup it is spread across
    node = xml_parse("<p>Run <id>pgbackrest</id> as <b>root</b>.</p>", "test.xml")

    assert_equal(xml_node_content(node), "Run pgbackrest as root.")


####################################################################################################################################
def test_xml_field():
    """A field is a child that holds nothing but text, so it reads as a property of the node that holds it."""

    node = xml_parse(
        "<execute><exe-cmd>pgbackrest backup</exe-cmd><exe-highlight-type>error</exe-highlight-type></execute>", "test.xml"
    )

    assert_equal(xml_node_field(node, "exe-cmd"), "pgbackrest backup")
    assert_is_none(xml_node_field(node, "exe-cmd-extra"))

    with assert_raises(ToolError) as error:
        xml_node_field(node, "exe-cmd-extra", True)

    assert_equal(str(error.exception), "unable to find child 'exe-cmd-extra' in node 'execute'")

    assert_true(xml_node_field_test(node, "exe-highlight-type", "error"))
    assert_false(xml_node_field_test(node, "exe-highlight-type", "warning"))


####################################################################################################################################
def test_xml_text():
    """Text is held by the node itself when the node is text and in a text child when it is not."""

    node = xml_parse("<p>Run it now.</p>", "test.xml")

    assert_true(xml_node_text(node) is node)

    node = xml_parse("<section><text>Run it now.</text><p>More.</p></section>", "test.xml")

    assert_equal(xml_node_content(xml_node_text(node)), "Run it now.")

    # A section that says nothing before its content has no text
    node = xml_parse("<section><p>More.</p></section>", "test.xml")

    assert_is_none(xml_node_text(node))

    with assert_raises(ToolError) as error:
        xml_node_text(node, True)

    assert_equal(str(error.exception), "unable to find child 'text' in node 'section'")


####################################################################################################################################
def test_xml_insert():
    """A node is added where it belongs in what is already there rather than at the end."""

    node = xml_parse("<section><title>Backup</title><p>text</p></section>", "test.xml")

    xml_node_content_add(xml_node_insert(node, 1, "p", {"class": "date"}), "July 20, 2026")

    assert_equal(_render(node), '<section><title>Backup</title><p class="date">July 20, 2026</p><p>text</p></section>')


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


####################################################################################################################################
def test_xml_normalize():
    """What a document uses to lay itself out is dropped, since it is how the xml is written rather than what it says."""

    node = xml_parse("<p>\ta\n<b>\tb</b>\tc</p>", "test.xml")

    xml_node_normalize(node)

    assert_equal(_render(node), "<p>a\n<b>b</b>c</p>")

    # A node that holds no text at all has none to drop
    node = xml_parse("<doc><p/></doc>", "test.xml")

    xml_node_normalize(node)

    assert_equal(_render(node), "<doc><p /></doc>")


####################################################################################################################################
def test_xml_normalize_mixed():
    """Text that belongs to no tag has nowhere to go in the output, so it is reported rather than dropped."""

    with assert_raises(ToolError) as error:
        xml_node_normalize(xml_parse("<section><title>Title</title>stray</section>", "test.xml"))

    assert_equal(str(error.exception), "text mixed with markup in node 'section'")

    # A node that is text itself is where interleaving text and markup is the whole point, as is a link, which says what it points at
    # the way the text around it would say it
    xml_node_normalize(xml_parse("<p>Run <id>pgbackrest</id> now.</p>", "test.xml"))
    xml_node_normalize(xml_parse("<text>Run <id>pgbackrest</id> now.</text>", "test.xml"))
    xml_node_normalize(xml_parse("<link>Repo Path (<id>--repo-path</id>)</link>", "test.xml"))

    # Whitespace between tags is how the document is laid out to be read rather than text of its own
    xml_node_normalize(xml_parse("<section>\n    <title>Title</title>\n</section>", "test.xml"))


####################################################################################################################################
def test_xml_text_add():
    """A caller building a document says what a node says without having to know which kind of node it is building."""

    node = xml_parse("<section/>", "test.xml")

    xml_node_content_add(xml_node_text_add(node), "Some text.")

    assert_equal(_render(node), "<section><text>Some text.</text></section>")

    # A tag that is text holds it directly, so there is nothing to add it to
    node = xml_parse("<title/>", "test.xml")

    xml_node_content_add(xml_node_text_add(node), "Title")

    assert_equal(_render(node), "<title>Title</title>")
