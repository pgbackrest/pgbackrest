"""Variable Store.

Holds the variables a document refers to as {[name]} and replaces them wherever they appear. A value may itself refer to a variable,
so replacement repeats until nothing more is replaced rather than passing over the text once.

A variable may be evaluated rather than written out, which is how a document asks for a fact about the environment it is being built
in that it has no other way to know, e.g. the name of the user running the build. What it may use is the short list below rather than
the whole interpreter, and it is an expression rather than a program, which is what keeps the feature to a few lines."""

####################################################################################################################################
import os

from common.error import ToolError
from common.user import user_id, user_name
from common.xml import xml_node_attribute, xml_node_attribute_set, xml_node_content


####################################################################################################################################
def _eval_namespace():
    """What an evaluated variable may use."""

    return {"cwd": os.getcwd(), "uid": user_id(), "user": user_name()}


####################################################################################################################################
def eval_value(expression):
    """Evaluate the expression a variable holds, which has had its variables replaced already."""

    try:
        return str(eval(expression, {"__builtins__": {}}, _eval_namespace()))
    except Exception as error:
        raise ToolError("unable to evaluate '%s': %s" % (expression, error))


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
    def add_node(self, node):
        """Add the variable a node declares, evaluating what it holds when it says to."""

        value = self.replace_str(xml_node_content(node))

        if xml_node_attribute(node, "eval") == "y":
            value = eval_value(value)

        self.add(xml_node_attribute(node, "key", True), value)

        return value

    ################################################################################################################################
    def set(self, variable, value):
        """Set a variable, replacing the value it already has.

        Used for what is only known once the build is running, e.g. the address of a host that has just been started or the output of
        a command the documentation ran."""

        self.var_map["{[%s]}" % variable] = value

    ################################################################################################################################
    def get(self, variable):
        """Value of a variable, or None when it is not set."""

        return self.var_map.get("{[%s]}" % variable)

    ################################################################################################################################
    def test(self, variable, value):
        """Does a variable have a value?"""

        return self.get(variable) == value

    ################################################################################################################################
    def replace_str(self, string):
        """Replace every variable in a string.

        Nothing is what nothing replaces to, so a caller with something optional in hand does not have to check first."""

        if string is None:
            return None

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
