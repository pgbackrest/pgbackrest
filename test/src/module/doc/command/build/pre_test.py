"""Test Build Preprocessor.

The document is written out and the result checked as the xml that comes out, since what the preprocessor does is decide what is in the
document before a renderer ever sees it."""

####################################################################################################################################
import xml.etree.ElementTree as etree

from harness.test import *

from command.build.pre import *
from common.var_store import VarStore
from common.error import *
from common.xml import xml_document_parse


####################################################################################################################################
class Help:
    """Help for an option or a command, which is all the preprocessor reads of it."""

    def __init__(self, name, description):
        self.name = name
        self.description = etree.fromstring("<text>%s</text>" % description)


####################################################################################################################################
class BldHlp:
    """The help the preprocessor takes descriptions from."""

    def __init__(self):
        self.opt_list = [Help("repo-path", "The <b>repository</b> path.")]
        self.cmd_list = [Help("backup", "Perform a backup.")]


####################################################################################################################################
def _pre(content, var_map=None):
    """Preprocess a document and return the xml of it."""

    var_store = VarStore()

    for variable, value in (var_map or {}).items():
        var_store.add(variable, value)

    document = build_pre(xml_document_parse("<doc>%s</doc>" % content, "test.xml"), BldHlp(), var_store)

    return etree.tostring(document, encoding="unicode")


####################################################################################################################################
def _error(content, var_map=None):
    """Preprocess a document that is expected to fail and return the message."""

    with assert_raises(ToolError) as error:
        _pre(content, var_map)

    return str(error.exception)


####################################################################################################################################
def test_pre_if():
    """A node stays only when its condition holds, and the condition itself is not rendered."""

    # A condition that holds, which leaves the node without it
    assert_equal(_pre("<p if=\"'y' eq 'y'\">kept</p>"), "<doc><p>kept</p></doc>")

    # A condition that does not
    assert_equal(_pre("<p if=\"'y' eq 'n'\">dropped</p><p>kept</p>"), "<doc><p>kept</p></doc>")

    # A condition is evaluated after its variables are replaced, since it is about values rather than names
    assert_equal(_pre("<p if=\"'{[debug]}' eq 'y'\">kept</p>", {"debug": "y"}), "<doc><p>kept</p></doc>")
    assert_equal(_pre("<p if=\"'{[debug]}' eq 'y'\">dropped</p>", {"debug": "n"}), "<doc />")

    # A condition on a node that holds others, which go with it
    assert_equal(_pre("<section if=\"'y' eq 'n'\"><p>dropped</p></section>"), "<doc />")


####################################################################################################################################
def test_pre_description():
    """A description is taken from the help rather than written twice."""

    assert_equal(_pre('<p><option-description key="repo-path"/></p>'), "<doc><p>The <b>repository</b> path.</p></doc>")

    assert_equal(_pre('<p><cmd-description key="backup"/></p>'), "<doc><p>Perform a backup.</p></doc>")

    # A description sits where the node was rather than at the end, since it is part of a sentence
    assert_equal(
        _pre('<p>before <option-description key="repo-path"/> after</p>'),
        "<doc><p>before The <b>repository</b> path. after</p></doc>",
    )

    # Help that is not there is reported, since the description would otherwise go missing without saying so
    assert_equal(_error('<p><option-description key="bogus"/></p>'), "option 'bogus' has no help to describe it")
    assert_equal(_error('<p><cmd-description key="bogus"/></p>'), "command 'bogus' has no help to describe it")


####################################################################################################################################
def test_pre_variable():
    """A document can declare a variable, which is then available to the conditions in it.

    The text of the document is left alone, since the renderer is what replaces a variable where it is used. Only a condition and a
    block are resolved here, because both decide what the document holds."""

    assert_equal(
        _pre('<variable key="path">/var/lib</variable><p>{[path]}/pgbackrest</p>'),
        '<doc><variable key="path">/var/lib</variable><p>{[path]}/pgbackrest</p></doc>',
    )

    # A value that refers to a variable is resolved as it is stored, so a condition sees a value rather than a name. What the node
    # holds is left as it was written.
    assert_equal(
        _pre("<variable key=\"path\">{[root]}/lib</variable><p if=\"'{[path]}' eq '/var/lib'\">kept</p>", {"root": "/var"}),
        '<doc><variable key="path">{[root]}/lib</variable><p>kept</p></doc>',
    )


####################################################################################################################################
def test_pre_block():
    """A block is written once and used wherever it applies, with the values each use gives it filled in."""

    document = (
        '<block-define id="repo"><p>Repository {[index]} is {[type]}.</p></block-define>'
        '<block id="repo">'
        '<block-variable-replace key="index">1</block-variable-replace>'
        '<block-variable-replace key="type">posix</block-variable-replace>'
        "</block>"
        '<block id="repo">'
        '<block-variable-replace key="index">2</block-variable-replace>'
        '<block-variable-replace key="type">s3</block-variable-replace>'
        "</block>"
    )

    # The definition is not rendered and each use is replaced by what the block holds
    assert_equal(_pre(document), "<doc><p>Repository 1 is posix.</p><p>Repository 2 is s3.</p></doc>")

    # A block may use another block, since what a block holds is preprocessed the same as anything else
    document = (
        '<block-define id="inner"><p>inner {[value]}</p></block-define>'
        '<block-define id="outer"><block id="inner"><block-variable-replace key="value">one</block-variable-replace></block></block-define>'
        '<block id="outer"/>'
    )

    assert_equal(_pre(document), "<doc><p>inner one</p></doc>")

    # A block is subject to conditions like anything else
    document = (
        "<block-define id=\"repo\"><p if=\"'{[type]}' eq 'posix'\">posix only</p><p>always</p></block-define>"
        '<block id="repo"><block-variable-replace key="type">s3</block-variable-replace></block>'
    )

    assert_equal(_pre(document), "<doc><p>always</p></doc>")


####################################################################################################################################
def test_pre_block_error():
    """A block that is defined twice or used before it is defined is reported."""

    assert_equal(
        _error('<block-define id="repo"><p>one</p></block-define><block-define id="repo"><p>two</p></block-define>'),
        "block 'repo' is already defined",
    )

    assert_equal(_error('<block id="bogus"/>'), "block 'bogus' does not exist")
