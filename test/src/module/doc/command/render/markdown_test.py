"""Test Markdown Renderer."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_read, file_write, path_list
from common.var_store import VarStore
from command.render.manifest import RENDER_MARKDOWN, Manifest
from command.render.markdown import DocMarkdownRender, markdown_render

MANIFEST = """<doc>
    <variable-list>
        <variable key="project">pgBackRest</variable>
    </variable-list>

    <source-list>
        <source key="index"/>
    </source-list>

    <render-list>
        <render type="markdown">
            <render-source key="index" file="../README.md"/>
        </render>
    </render-list>
</doc>
"""

INDEX = """<doc title="Index" subtitle="Reliable">
    <description>The index.</description>

    <section id="start">
        <title>Start</title>

        <p>A paragraph with <id>markup</id>.</p>

        <p>Another paragraph.</p>

        <admonition type="note">Careful.</admonition>

        <list>
            <list-item>One</list-item>
            <list-item>Two</list-item>
        </list>

        <code-block title="Example" type="bash">echo one</code-block>

        <code-block>echo two</code-block>

        <sponsor-list>
            <sponsor url="https://one" img="one.png" width="100">One</sponsor>
            <sponsor url="https://two" img="two.png" width="50">Two</sponsor>
        </sponsor-list>

        <table>
            <title label="Table 1">Options</title>

            <table-header>
                <table-column>Name</table-column>
                <table-column align="right">Value</table-column>
                <table-column align="center">Note</table-column>
            </table-header>

            <table-data>
                <table-row>
                    <table-cell>one</table-cell>
                    <table-cell>1</table-cell>
                    <table-cell>ok</table-cell>
                </table-row>
            </table-data>
        </table>

        <table>
            <table-data>
                <table-row>
                    <table-cell>plain</table-cell>
                </table-row>
            </table-data>
        </table>

        <execute-list host="repo">
            <title>Run it</title>

            <execute output="y">
                <exe-cmd>pgbackrest info</exe-cmd>
                <exe-highlight>suppressed</exe-highlight>
            </execute>
        </execute-list>

        <section id="inner">
            <title>Inner</title>
        </section>
    </section>
</doc>
"""


####################################################################################################################################
class _Config:
    """The options a manifest reads."""

    def __init__(self, **kwargs):
        self.deploy = False
        self.cache_only = False
        self.pre = False
        self.require = []
        self.include = []
        self.exclude = []
        self.key_var_map = {}

        for name, value in kwargs.items():
            setattr(self, name, value)


####################################################################################################################################
def _doc_path(path, index=INDEX):
    """Write the documentation a page is rendered from."""

    file_write(os.path.join(path, "manifest.xml"), MANIFEST)
    file_write(os.path.join(path, "xml/index.xml"), index)

    return path


####################################################################################################################################
def _page(path, exe=False):
    """Render a page without running any of the commands it holds."""

    manifest = Manifest(path, VarStore(), {}, _Config())

    return manifest.var_store.replace_str(DocMarkdownRender(manifest, "index", exe).process())


####################################################################################################################################
def test_markdown():
    """A page is mostly text with the structure carried by headings and lists."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_true(page.startswith("# Index <br/> Reliable\n\n## Start\n"))
        assert_in("A paragraph with `markup`.\n\nAnother paragraph.", page)
        assert_in("\n> **NOTE:** Careful.", page)
        assert_in("\n\n- One\n- Two", page)

        # A code block says what it is when it has a title, and what kind of code it holds when it says
        assert_in("\n_Example_:\n```bash\necho one\n```", page)
        assert_in("\n```\necho two\n```", page)

        # A sponsor is a link, and the list of them reads as a sentence
        assert_in(" [One](https://one), [Two](https://two).", page)

        # A section inside another is a heading one deeper
        assert_in("\n### Inner", page)
        assert_true(page.endswith("\n"))


####################################################################################################################################
def test_markdown_table():
    """Markdown has no table without a header, so a table that has none gets an empty one lined up left."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_in("**Table 1: Options**\n\n| Name | Value | Note |\n| :--- | ------: | :------: |\n| one | 1 | ok |\n", page)
        assert_in("|      | \n| :--- | \n| plain |\n", page)


####################################################################################################################################
def test_markdown_execute():
    """A command is shown with what it wrote, with the part the documentation is pointing at marked."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_in("repo => Run it\n```\npgbackrest info\n\n--- output ---\n\n--> Output suppressed for testing\n```", page)


####################################################################################################################################
def test_markdown_execute_error():
    """Output the documentation is pointing at as an error is marked as one, and output that is missing is reported."""

    import command.render.execute as execute_module

    execute_run_real = execute_module.DocExecute._execute_run
    execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "before\nsuppressed here"

    try:
        with tempfile.TemporaryDirectory() as path:
            page = _page(
                _doc_path(path, INDEX.replace("<exe-highlight>", "<exe-highlight-type>error</exe-highlight-type><exe-highlight>")),
                exe=True,
            )

            assert_in("    before\nERR suppressed here\n```", page)

        with tempfile.TemporaryDirectory() as path:
            execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "nothing of interest"

            with assert_raises(ToolError) as raised:
                _page(_doc_path(path), exe=True)

            assert_equal(str(raised.exception), "unable to find a match for highlight: suppressed")
    finally:
        execute_module.DocExecute._execute_run = execute_run_real


####################################################################################################################################
def test_markdown_hidden():
    """A command list that is not shown and a command that is not shown are run but not written."""

    with tempfile.TemporaryDirectory() as path:
        index = INDEX.replace('<execute-list host="repo">', '<execute-list host="repo" show="n">')
        page = _page(_doc_path(path, index))

        assert_not_in("Run it", page)

        index = INDEX.replace('<execute output="y">', '<execute output="y" show="n">')
        page = _page(_doc_path(path, index))

        assert_not_in("pgbackrest info", page)


####################################################################################################################################
def test_markdown_error():
    """A page can only be sectioned so deep before the headings run out."""

    with tempfile.TemporaryDirectory() as path:
        deep = "<doc title='Deep'><description>D</description>%s%s</doc>" % (
            "".join("<section id='s%d'><title>S%d</title>" % (idx, idx) for idx in range(4)),
            "</section>" * 4,
        )

        with assert_raises(ToolError) as raised:
            _page(_doc_path(path, deep))

        assert_equal(str(raised.exception), "section depth of 4 exceeds maximum")


####################################################################################################################################
def test_markdown_render():
    """Every page is written where the manifest says, which for the readme is outside the documentation."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        path_out = os.path.join(path, "output/markdown")

        os.makedirs(path_out)
        markdown_render(Manifest(path, VarStore(), {}, _Config()), path_out, False)

        assert_in("# Index", file_read(os.path.join(path, "output/README.md")))


####################################################################################################################################
def test_markdown_render_cache_reset():
    """A cache that no longer describes a document is thrown away and the document built again."""

    import command.render.execute as execute_module

    execute_run_real = execute_module.DocExecute._execute_run
    execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "ran suppressed"

    try:
        with tempfile.TemporaryDirectory() as path:
            _doc_path(path)

            manifest = Manifest(path, VarStore(), {}, _Config())
            manifest.source_map["index"].cache_list = [{"key": {"host": "other"}, "type": "exe"}]

            path_out = os.path.join(path, "output/markdown")

            os.makedirs(path_out)
            markdown_render(manifest, path_out, True)

            assert_in("ran suppressed", file_read(os.path.join(path, "output/README.md")))
    finally:
        execute_module.DocExecute._execute_run = execute_run_real


####################################################################################################################################
def test_markdown_render_default():
    """A page with no file of its own is written under the key it is rendered from."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        file_write(os.path.join(path, "manifest.xml"), MANIFEST.replace(' file="../README.md"', ""))

        path_out = os.path.join(path, "output/markdown")

        os.makedirs(path_out)
        markdown_render(Manifest(path, VarStore(), {}, _Config()), path_out, False)

        assert_equal(path_list(path_out), ["index.md"])


####################################################################################################################################
def test_markdown_order():
    """What comes before a thing decides how it is separated from it."""

    index = """<doc title="Index"><description>D</description>
        <section id="a"><title>A</title>
            <p>First, with nothing before it.</p>

            <table>
                <table-data><table-row><table-cell>x</table-cell></table-row></table-data>
            </table>

            <p>After a table.</p>

            <code-block title="One">echo one</code-block>

            <code-block title="Two">echo two</code-block>
        </section>
    </doc>"""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path, index))

        # A paragraph that follows a table is already separated from it by the end of the table
        assert_in("| x |\n\nAfter a table.", page)

        # A code block that follows another is not separated from it again
        assert_in("```\n_Two_:", page)

        # One that follows anything else is
        assert_in("After a table.\n\n_One_:", page)


####################################################################################################################################
def test_markdown_execute_no_output():
    """A command whose output the documentation does not show is written on its own."""

    index = """<doc title="Index"><description>D</description>
        <section id="a"><title>A</title>
            <execute-list host="repo"><title>Run</title>
                <execute><exe-cmd>quiet</exe-cmd></execute>
            </execute-list>
        </section>
    </doc>"""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path, index))

        assert_in("repo => Run\n```\nquiet\n```", page)
        assert_not_in("--- output ---", page)
