"""Build Preprocessor.

Prepares a document for rendering by resolving everything that is written once and used many times: the variables a document
declares, the blocks it defines and repeats with different values, the descriptions it takes from the help, and the conditions that
decide which parts of it apply at all.

Everything here happens before a renderer sees the document, so a renderer only ever deals with content that is meant to be
there."""

####################################################################################################################################
from common.eval import eval_expression
from common.var_store import VarStore
from common.error import ToolError, check
from common.xml import (
    xml_node_attribute,
    xml_node_attribute_remove,
    xml_node_child_list,
    xml_node_child_replace,
    xml_node_content,
    xml_node_dup,
)


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list or []:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _description(node, parent, item_list, key, what):
    """Replace a node with the description the help holds for what it names."""

    item = _find(item_list, key)
    check(item is not None, "%s '%s' has no help to describe it" % (what, key))

    xml_node_child_replace(parent, node, item.description)


####################################################################################################################################
def _block_replace(node, parent, block, bld_hlp, block_map, var_store):
    """Replace a node with a copy of the block it names, with the values it gives the block filled in."""

    # Values for this use of the block, which are variables that apply to the copy only
    var_block_store = VarStore()

    for var_replace in xml_node_child_list(node, "block-variable-replace"):
        var_block_store.add(xml_node_attribute(var_replace, "key", True), xml_node_content(var_replace))

    copy = xml_node_dup(block)
    var_block_store.replace_node(copy)

    # A block may use another block, so what it holds is preprocessed the same as anything else
    _pre_recurse(copy, bld_hlp, block_map, var_store)

    xml_node_child_replace(parent, node, copy)


####################################################################################################################################
def _pre_recurse(node, bld_hlp, block_map, var_store):
    """Preprocess everything under a node."""

    # The children are taken first because handling one can remove it
    for child in list(node):
        # A node with a condition stays only when the condition holds, and the condition itself is not rendered
        if_expr = xml_node_attribute(child, "if")

        if if_expr is not None:
            if not eval_expression(var_store.replace_str(if_expr)):
                node.remove(child)

                continue

            xml_node_attribute_remove(child, "if")

        if child.tag == "option-description":
            _description(child, node, bld_hlp.opt_list, xml_node_attribute(child, "key", True), "option")
        elif child.tag == "cmd-description":
            _description(child, node, bld_hlp.cmd_list, xml_node_attribute(child, "key", True), "command")
        elif child.tag == "variable":
            var_store.add_node(child)
        elif child.tag == "block-define":
            id = xml_node_attribute(child, "id", True)

            if id in block_map:
                raise ToolError("block '%s' is already defined" % id)

            block_map[id] = xml_node_dup(child)
            node.remove(child)
        elif child.tag == "block":
            id = xml_node_attribute(child, "id", True)

            if id not in block_map:
                raise ToolError("block '%s' does not exist" % id)

            _block_replace(child, node, block_map[id], bld_hlp, block_map, var_store)
        else:
            _pre_recurse(child, bld_hlp, block_map, var_store)


####################################################################################################################################
def build_pre(document, bld_hlp, var_store):
    """Preprocess a document and return it."""

    _pre_recurse(document, bld_hlp, {}, var_store)

    return document
