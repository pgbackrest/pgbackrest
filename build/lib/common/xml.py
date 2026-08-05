"""Xml Handler.

Thin helpers over the standard library element tree, shaped the way the documentation reads and writes xml.

Text is the awkward part. A document like the user guide is mixed content -- text and markup interleaved in one paragraph -- which the
element tree stores as the text before a node's first child plus the text trailing each child. So adding text to a node means appending
to whichever of those came last, and reading a node's text means gathering all of it. Both are done here so nothing else has to know.

A document may also be assembled from parts, which it declares as external entities and refers to by name. The parser does not fetch
them so they are expanded here before it sees the document, which is what lets the release list be one file per release.

Comments never appear in a parsed document, since the parser drops them, so nothing here has to skip them."""

####################################################################################################################################
import copy
import os
import re
import xml.etree.ElementTree as etree

from common.error import ToolError
from common.storage import file_read


# The parts a document is assembled from, which are declared in the internal subset of its document type. Nothing validates against
# a document type, so the only reason to look at one is to find these.
_SUBSET_EXP = re.compile(r"<!DOCTYPE\s+\S+\s*\[(.*?)\]\s*>", re.DOTALL)

# Part of a document declared in the internal subset, which is named as an entity but used as an include
_ENTITY_EXP = re.compile(r"""<!ENTITY\s+(\S+)\s+SYSTEM\s+"([^"]+)"\s*>""")


####################################################################################################################################
def xml_document_new(root_name):
    """Build an empty document."""

    return etree.Element(root_name)


####################################################################################################################################
def _entity_expand(content, subset, path_base):
    """Replace every reference to a part of the document with what the file holding that part holds.

    A part is named relative to the document that declares it, since that is where a reader looking for it would start."""

    for name, file in _ENTITY_EXP.findall(subset):
        reference = "&%s;" % name

        # A declaration that is never used is a leftover, and leaving it be would mean the file it names is silently not read
        if reference not in content:
            raise ToolError("part '%s' is declared but never used" % name)

        content = content.replace(reference, file_read(os.path.join(path_base, file)))

    return content


####################################################################################################################################
def xml_document_parse(content, path):
    """Parse a document, expanding the parts it is assembled from."""

    match = _SUBSET_EXP.search(content)

    if match is not None:
        content = _entity_expand(content, match.group(1), os.path.dirname(path))

    return xml_parse(content, path)


####################################################################################################################################
def xml_parse(content, path):
    """Parse xml, naming the file in any error since the message is otherwise only a line number."""

    try:
        return etree.fromstring(content)
    except etree.ParseError as error:
        raise ToolError("unable to parse '%s': %s" % (path, error))


####################################################################################################################################
def xml_node_attribute(node, name, error_on_missing=False):
    """Attribute of a node, or None when it is not there."""

    result = node.get(name)

    if result is None and error_on_missing:
        raise ToolError("unable to find attribute '%s' in node '%s'" % (name, node.tag))

    return result


####################################################################################################################################
def xml_node_attribute_set(node, name, value):
    """Set an attribute of a node."""

    node.set(name, value)


####################################################################################################################################
def xml_node_attribute_remove(node, name):
    """Remove an attribute of a node, which is not an error when it is not there."""

    node.attrib.pop(name, None)


####################################################################################################################################
def xml_node_child(node, name, error_on_missing=False):
    """The one child of a node with a name, or None when there is none.

    More than one is an error because a caller asking for the child rather than the list of them is asking for something a node has
    at most one of, e.g. its title, so a second is a mistake in the document rather than a choice to make."""

    result = node.findall(name)

    if len(result) > 1:
        raise ToolError("found more than one child '%s' in node '%s'" % (name, node.tag))

    if len(result) == 0:
        if error_on_missing:
            raise ToolError("unable to find child '%s' in node '%s'" % (name, node.tag))

        return None

    return result[0]


####################################################################################################################################
def xml_node_child_list(node, name):
    """Every child of a node with a name."""

    return node.findall(name)


####################################################################################################################################
def xml_node_field(node, name, error_on_missing=False):
    """Text of the one child of a node with a name, or None when there is none.

    A field is a child that holds nothing but text, e.g. the command an execute runs, so it reads as a property of the node that
    holds it rather than as part of the document."""

    child = xml_node_child(node, name, error_on_missing)

    return None if child is None else xml_node_content(child)


####################################################################################################################################
def xml_node_field_test(node, name, value):
    """Does a node have a field with a value?"""

    return xml_node_field(node, name) == value


# Tags that are text rather than tags that hold some, which is what decides where the text of a node is found
_TAG_IS_TEXT = ("admonition", "list-item", "p", "summary", "table-cell", "table-column", "title")

# Tags that may hold text and markup at once, which is every tag that is text plus the tag that holds nothing else and a link, which
# says what it points at the way the text around it would say it
_TAG_MIXED = _TAG_IS_TEXT + ("link", "text")

# What a document uses to lay itself out but does not mean. Carriage returns are not here because the parser has already turned them
# into linefeeds by the time a document is walked.
_LAYOUT = "\t"


####################################################################################################################################
def xml_node_text(node, error_on_missing=False):
    """The node holding a node's text, or None when there is none.

    A tag that is text itself holds it directly and everything else holds it in a text child, which is the difference between a
    paragraph, which is text, and a section, which has some."""

    if node.tag in _TAG_IS_TEXT:
        return node

    return xml_node_child(node, "text", error_on_missing)


####################################################################################################################################
def xml_node_normalize(node):
    """Drop what a document uses to lay itself out but does not mean, and check that text and markup are not mixed where they cannot
    be.

    A tab is how the xml is written rather than something to render, so it is dropped once rather than everywhere text is used. A node
    that holds markup may not also hold text, since there is nowhere in the output for text that belongs to no tag -- except in a node
    that is text itself, where interleaving the two is the whole point."""

    if node.text is not None:
        node.text = node.text.replace(_LAYOUT, "")

    for child in node:
        xml_node_normalize(child)

        if child.tail is not None:
            child.tail = child.tail.replace(_LAYOUT, "")

    if node.tag not in _TAG_MIXED and len(node) > 0:
        text = (node.text or "") + "".join(child.tail or "" for child in node)

        if text.strip() != "":
            raise ToolError("text mixed with markup in node '%s'" % node.tag)


####################################################################################################################################
def xml_node_text_add(node):
    """The node to put a node's text in, adding it when the node holds its text in a child.

    A caller building a document says what a node says without having to know which kind of node it is building."""

    return node if node.tag in _TAG_IS_TEXT else xml_node_add(node, "text")


####################################################################################################################################
def xml_node_add(node, name, attrib=None):
    """Add a child to a node."""

    return etree.SubElement(node, name, attrib or {})


####################################################################################################################################
def xml_node_insert(node, index, name, attrib=None):
    """Add a child to a node at a position rather than at the end, which is how a node goes where it belongs in what is already
    there."""

    result = etree.Element(name, attrib or {})
    node.insert(index, result)

    return result


####################################################################################################################################
def xml_node_dup(node):
    """Copy a node and everything under it, so what is copied can be changed without changing what it came from."""

    return copy.deepcopy(node)


####################################################################################################################################
def xml_node_content(node):
    """All the text under a node, with the markup dropped."""

    return "".join(node.itertext())


####################################################################################################################################
def xml_node_content_add(node, content):
    """Add text to the end of what a node holds.

    Text is added rather than set because a title is built from text and markup in turn, e.g. "Backup Command (" then an id then ")".
    """

    if len(node) == 0:
        node.text = (node.text or "") + content
    else:
        node[-1].tail = (node[-1].tail or "") + content


####################################################################################################################################
def xml_node_text_set(node, content):
    """Replace everything a node holds with text."""

    for child in list(node):
        node.remove(child)

    node.text = content


####################################################################################################################################
def xml_node_child_add(node, source):
    """Add a copy of everything a source node holds to another node."""

    xml_node_content_add(node, source.text or "")

    for child in source:
        added = xml_node_add(node, child.tag)

        for name, value in child.attrib.items():
            xml_node_attribute_set(added, name, value)

        xml_node_child_add(added, child)
        xml_node_content_add(node, child.tail or "")


####################################################################################################################################
def xml_node_child_replace(parent, node, source):
    """Replace a node with a copy of everything a source node holds.

    The parent is passed because a node does not know what holds it, and what replaces it takes its place rather than going at the
    end."""

    index = list(parent).index(node)

    # Build the replacement in a node of its own so the copy can be made with the same code that adds children anywhere else
    replace = etree.Element(node.tag)
    xml_node_child_add(replace, source)

    # Whatever trailed the node goes after whatever now ends the replacement
    tail = node.tail or ""
    parent.remove(node)

    for offset, child in enumerate(list(replace)):
        parent.insert(index + offset, child)

    # Text the replacement begins with belongs to what precedes the node, i.e. the parent or the child before it
    if index == 0:
        parent.text = (parent.text or "") + replace.text
    else:
        parent[index - 1].tail = (parent[index - 1].tail or "") + replace.text

    if len(replace) > 0:
        parent[index + len(replace) - 1].tail = (parent[index + len(replace) - 1].tail or "") + tail
    elif index == 0:
        parent.text = (parent.text or "") + tail
    else:
        parent[index - 1].tail = (parent[index - 1].tail or "") + tail
