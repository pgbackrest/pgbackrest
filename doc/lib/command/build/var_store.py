"""Variable Store.

Holds the variables a document refers to as {[name]} and replaces them wherever they appear. A value may itself refer to a variable,
so replacement repeats until nothing more is replaced rather than passing over the text once."""

####################################################################################################################################
from common.xml import xml_node_attribute, xml_node_attribute_set


####################################################################################################################################
class VarStore:
    """A set of variables and their values."""

    def __init__(self):
        self.var_map = {}

    ################################################################################################################################
    def add(self, variable, value):
        """Add a variable, keeping the value it already has when it has one.

        The first value wins because the command line is loaded before the document, so what the caller asked for is not overridden by
        what the document declares."""

        self.var_map.setdefault("{[%s]}" % variable, value)

    ################################################################################################################################
    def replace_str(self, string):
        """Replace every variable in a string."""

        result = string
        replace = True

        while replace:
            replace = False

            for variable, value in self.var_map.items():
                if variable in result:
                    result = result.replace(variable, value)
                    replace = True

        return result

    ################################################################################################################################
    def replace_node(self, node):
        """Replace every variable in the attributes and the text of a node and everything under it."""

        for name in list(node.attrib):
            xml_node_attribute_set(node, name, self.replace_str(xml_node_attribute(node, name)))

        if node.text is not None:
            node.text = self.replace_str(node.text)

        for child in node:
            self.replace_node(child)

            if child.tail is not None:
                child.tail = self.replace_str(child.tail)
