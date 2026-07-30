"""Xml Handler.

Thin helpers over the standard library element tree, shaped the way the documentation reads and writes xml.

Text is the awkward part. A document like the user guide is mixed content -- text and markup interleaved in one paragraph -- which the
element tree stores as the text before a node's first child plus the text trailing each child. So adding text to a node means appending
to whichever of those came last, and reading a node's text means gathering all of it. Both are done here so nothing else has to know.

Comments never appear in a parsed document, since the parser drops them, so nothing here has to skip them."""

####################################################################################################################################
import copy
import re
import xml.etree.ElementTree as etree

from common.error import ToolError


####################################################################################################################################
class XmlDocument:
    """An xml document, i.e. a root node plus the declarations that go above it."""

    def __init__(self, root, dtd_name=None, dtd_file=None):
        self.root = root
        self.dtd_name = dtd_name  # Document type, if any
        self.dtd_file = dtd_file  # File the document type is declared in

    ################################################################################################################################
    def render(self):
        """Render the document as the xml text of it."""

        result = '<?xml version="1.0" encoding="UTF-8"?>\n'

        if self.dtd_name is not None:
            result += '<!DOCTYPE %s SYSTEM "%s">\n' % (self.dtd_name, self.dtd_file)

        return result + etree.tostring(self.root, encoding="unicode") + "\n"


# Document type a document declares, which is kept so a document that is read and written again declares the same
_DTD_EXP = re.compile(r"""<!DOCTYPE\s+(\S+)\s+SYSTEM\s+"([^"]+)"\s*>""")


####################################################################################################################################
def xml_document_new(root_name, dtd_name=None, dtd_file=None):
    """Build an empty document."""

    return XmlDocument(etree.Element(root_name), dtd_name, dtd_file)


####################################################################################################################################
def xml_document_parse(content, path):
    """Parse a document, keeping the document type it declares."""

    match = _DTD_EXP.search(content)

    return XmlDocument(xml_parse(content, path), *(match.groups() if match is not None else (None, None)))


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
    """First child of a node with a name, or None when there is none."""

    result = node.find(name)

    if result is None and error_on_missing:
        raise ToolError("unable to find child '%s' in node '%s'" % (name, node.tag))

    return result


####################################################################################################################################
def xml_node_child_list(node, name):
    """Every child of a node with a name."""

    return node.findall(name)


####################################################################################################################################
def xml_node_add(node, name):
    """Add a child to a node."""

    return etree.SubElement(node, name)


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
