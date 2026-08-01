"""Test Html Renderer.

The commands are not run, so what a command wrote stands in for itself. What is checked is the page that comes out, since where a
linefeed goes is what makes one build of the documentation comparable with the next."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_read, file_write, path_list
from common.var_store import VarStore
from common.xml import xml_parse
from command.render.execute import CacheInvalidError
from command.render.html import DocHtmlPage, HtmlBuilder, HtmlElement, html_render
from command.render.manifest import RENDER_HTML, Manifest

MANIFEST = """<doc>
    <variable-list>
        <variable key="project">pgBackRest</variable>
        <variable key="project-exe">pgbackrest</variable>
        <variable key="project-url-root">/</variable>
        <variable key="html-footer">Footer.</variable>
    </variable-list>

    <source-list>
        <source key="index"/>
        <source key="user-guide"/>
    </source-list>

    <render-list>
        <render type="html">
            <render-source key="index" menu="Home"/>
            <render-source key="user-guide" menu="Guide"/>
        </render>
    </render-list>
</doc>
"""

USER_GUIDE = """<doc title="User Guide" subtitle="Reliable">
    <description>How to use it.</description>

    <section id="start">
        <title>Start</title>

        <text><p>An introduction.</p></text>

        <p>A paragraph with <id>markup</id>.</p>

        <admonition type="note">Careful.</admonition>

        <list>
            <list-item>One</list-item>
            <list-item>Two</list-item>
        </list>

        <code-block title="Example" type="bash">
            echo one
              echo two
        </code-block>

        <table>
            <title label="Table 1">Options</title>

            <table-header>
                <table-column fill="y">Name</table-column>
                <table-column align="right">Value</table-column>
            </table-header>

            <table-data>
                <table-row>
                    <table-cell>one</table-cell>
                    <table-cell>1</table-cell>
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

        <backrest-config host="repo" file="/etc/pgbackrest.conf">
            <title>Configure</title>

            <backrest-config-option section="global" key="repo1-path">/var/lib</backrest-config-option>
        </backrest-config>

        <postgres-config host="repo" file="/pg/postgresql.conf">
            <title>Configure PostgreSQL</title>

            <postgres-config-option key="archive_mode">on</postgres-config-option>
        </postgres-config>

        <section id="shown">
            <title>Shown In Contents</title>
        </section>

        <section id="inner" toc="n">
            <title>Inner</title>

            <subtitle><text>A Subtitle</text></subtitle>

            <subsubtitle><text>A Subsubtitle</text></subsubtitle>
        </section>
    </section>

    <section id="quiet" toc="n">
        <title>Quiet</title>

        <p>Not in the contents.</p>
    </section>
</doc>
"""

SIMPLE_GUIDE = """<doc title="User Guide">
    <description>How to use it.</description>

    <section id="start">
        <title>Start</title>

        <execute-list host="repo">
            <title>Run it</title>

            <execute output="y"><exe-cmd>pgbackrest info</exe-cmd></execute>
        </execute-list>
    </section>
</doc>
"""

INDEX = """<doc title="Index">
    <description>The index.</description>

    <section id="about">
        <title>About</title>

        <sponsor-list>
            <sponsor url="https://one" img="one.png" width="100">One</sponsor>
            <sponsor url="https://two" img="two.png" img-dark="two-dark.png" width="50">Two</sponsor>
        </sponsor-list>
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
def _doc_path(path, manifest=MANIFEST, user_guide=USER_GUIDE):
    """Write the documentation a page is rendered from."""

    file_write(os.path.join(path, "manifest.xml"), manifest)
    file_write(os.path.join(path, "xml/index.xml"), INDEX)
    file_write(os.path.join(path, "xml/user-guide.xml"), user_guide)
    file_write(os.path.join(path, "resource/html/default.css"), "body\n{\n    color: black; /* black */\n}\n")

    return path


####################################################################################################################################
def _manifest(path, config=None):
    """Load the documentation."""

    return Manifest(path, VarStore(), {}, _Config() if config is None else config)


####################################################################################################################################
def _page(path, key="user-guide", config=None):
    """Render a page without running any of the commands it holds."""

    manifest = _manifest(path, config)
    render = manifest.render_get(RENDER_HTML)

    return manifest.var_store.replace_str(DocHtmlPage(manifest, key, render.menu, False).process())


####################################################################################################################################
def test_element():
    """A page is built as a tree of elements so the code that decides what it holds never deals with markup."""

    builder = HtmlBuilder("Name", "Title", None, None, None)
    body = builder.body

    body.add_new("div", "one", content="Content")
    body.add(HtmlElement("div", "two"))

    assert_in('<div class="one">\nContent\n', builder.render())
    assert_in('<div class="two"></div>', builder.render())


####################################################################################################################################
def test_builder():
    """A page says what it is."""

    builder = HtmlBuilder("pgBackRest", "Guide & More", "logo.svg", "logo.png", "How to use it.")

    result = builder.render(analytics=True)

    assert_in("<!DOCTYPE html PUBLIC", result)
    assert_in('<link rel="icon" href="logo.svg" type="image/svg+xml"></link>', result)
    assert_in('<meta property="og:image" content="{[backrest-url-base]}/logo.png"></meta>', result)
    assert_in('<meta name="description" content="How to use it."></meta>', result)
    assert_in('<link rel="stylesheet" href="default.css" type="text/css"></link>', result)
    assert_in("googletagmanager.com", result)

    # What cannot appear in an attribute is escaped
    assert_in("Guide &amp; More", result)


####################################################################################################################################
def test_page():
    """A page holds a header, a menu, a table of contents, and the sections of the document."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_in('<div class="page-header-title">\nUser Guide\n', page)
        assert_in('<div class="page-header-subtitle">\nReliable\n', page)
        assert_in("<title>", page)

        # The page a reader is on is not in the menu, since they are already there
        assert_in('<a class="menu-link" href="/">\nHome\n', page)
        assert_not_in(">\nGuide\n<", page)

        # A section is numbered and anchored so a link can point at it
        assert_in('<a id="start"></a>', page)
        assert_in('<a id="start/inner"></a>', page)
        assert_in('<div class="section1-number">', page)
        assert_in('<div class="page-toc-title">\nTable of Contents\n', page)

        # A section that says it is not in the contents is still on the page
        assert_in('<a id="quiet"></a>', page)
        assert_not_in('href="#quiet"', page)

        assert_in('<div class="page-footer">\nFooter.\n', page)


####################################################################################################################################
def test_page_content():
    """Everything a section can hold is rendered as what the page writes for it."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_in('<div class="section-intro">\nAn introduction.\n', page)
        assert_in('A paragraph with <span class="id">markup</span>.', page)
        assert_in('<div class="note">\nNOTE:\n', page)
        assert_in('<li class="list-unordered">\nOne\n', page)
        assert_in('<div class="section2-subtitle">\nA Subtitle\n', page)
        assert_in('<div class="section2-subsubtitle">\nA Subsubtitle\n', page)

        # A code block is shown at the indent it was written at rather than at the indent it sits at in the xml
        assert_in('<pre class="code-block">echo one\n  echo two</pre>', page)

        # A table says what it is and how each column is lined up
        assert_in('<caption class="table-caption">\nTable 1: Options\n', page)
        assert_in('<th class="table-header-left table-header-fill">', page)
        assert_in('<th class="table-header-right">', page)
        assert_in('<td class="table-data-right">', page)


####################################################################################################################################
def test_page_execute():
    """A command is shown with what it wrote, with the part the documentation is pointing at marked."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path))

        assert_in('<span class="host">repo</span> <b>&#x21d2;</b> Run it', page)
        assert_in('<pre class="execute-body-cmd">pgbackrest info</pre>', page)
        assert_in('<pre class="execute-body-output-highlight">Output suppressed for testing</pre>', page)

        # A configuration is shown as the file it leaves behind
        assert_in('<span class="host">repo</span>:<span class="file">/etc/pgbackrest.conf</span>', page)
        assert_in('<div class="config-body-output">\nConfig suppressed for testing\n', page)

        # Laid out content is separated from what is around it, and two of them in a row are not separated again
        assert_in('\n<pre class="execute-body-cmd">', page)
        assert_in("</pre>\n<pre", page)


####################################################################################################################################
def test_page_sponsor():
    """A sponsor is a link with a logo for a light page and a logo for a dark one."""

    with tempfile.TemporaryDirectory() as path:
        page = _page(_doc_path(path), "index")

        assert_in('<a href="https://one">', page)
        assert_in('<img class="sponsor-img sponsor-img-light" src="sponsor/one.png" alt="One" width="100">', page)
        assert_in('<img class="sponsor-img sponsor-img-dark" src="sponsor/one.png" alt="One" width="100">', page)

        # A sponsor with a logo of its own for a dark page
        assert_in('<img class="sponsor-img sponsor-img-dark" src="sponsor/two-dark.png" alt="Two" width="50">', page)


####################################################################################################################################
def test_page_error():
    """A page that cannot be rendered as it is written is reported rather than rendered wrongly."""

    with tempfile.TemporaryDirectory() as path:
        # A page can only be sectioned so deep before the numbering runs out of styles
        deep = "<doc title='Deep'><description>D</description>%s%s</doc>" % (
            "".join("<section id='s%d'><title>S%d</title>" % (idx, idx) for idx in range(4)),
            "</section>" * 4,
        )

        with assert_raises(ToolError) as raised:
            _page(_doc_path(path, user_guide=deep))

        assert_equal(str(raised.exception), "section depth of 4 exceeds maximum")


####################################################################################################################################
def test_page_no_toc():
    """A document that says it has no table of contents has no numbers on its sections either."""

    with tempfile.TemporaryDirectory() as path:
        guide = "<doc title='G' toc='n'><description>D</description><section id='a'><title>A</title></section></doc>"
        page = _page(_doc_path(path, user_guide=guide))

        assert_not_in("page-toc", page)
        assert_not_in("section1-number", page)


####################################################################################################################################
def test_html_render():
    """Every page is rendered, along with everything that goes beside them."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        file_write(os.path.join(path, "resource/logo.png"), "png")
        file_write(os.path.join(path, "resource/logo.svg"), "svg")
        file_write(os.path.join(path, "resource/sponsor/one.png"), "png")

        manifest = _manifest(path)
        manifest.var_store.add("project-logo", "logo.png")
        manifest.var_store.add("project-favicon", "logo.svg")

        path_out = os.path.join(path, "output/html")

        os.makedirs(path_out)
        html_render(manifest, path, path_out, False)

        assert_equal(
            sorted(path_list(path_out)), ["default.css", "index.html", "logo.png", "logo.svg", "sponsor", "user-guide.html"]
        )
        assert_equal(path_list(os.path.join(path_out, "sponsor")), ["one.png"])


####################################################################################################################################
def test_html_render_cache_reset():
    """A cache that no longer describes a document is thrown away and the document built again."""

    import command.render.execute as execute_module

    execute_run_real = execute_module.DocExecute._execute_run
    execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "ran for real"

    try:
        with tempfile.TemporaryDirectory() as path:
            _doc_path(path, user_guide=SIMPLE_GUIDE)

            manifest = _manifest(path)

            # A cache entry that does not describe the command the page is about to run
            manifest.source_map["user-guide"].cache_list = [{"key": {"host": "other"}, "type": "exe"}]

            path_out = os.path.join(path, "output/html")

            os.makedirs(path_out)
            html_render(manifest, path, path_out, True)

            # The cache was thrown away, so the page rendered from what the command wrote rather than from what was cached
            assert_is_not_none(manifest.source_map["user-guide"].cache_list)
            assert_in("ran for real", file_read(os.path.join(path_out, "user-guide.html")))
    finally:
        execute_module.DocExecute._execute_run = execute_run_real


####################################################################################################################################
def test_page_highlight_missing():
    """A command whose output does not hold what the documentation is pointing at is reported."""

    import command.render.execute as execute_module

    execute_run_real = execute_module.DocExecute._execute_run
    execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "nothing of interest"

    try:
        with tempfile.TemporaryDirectory() as path:
            manifest = _manifest(_doc_path(path))
            render = manifest.render_get(RENDER_HTML)

            with assert_raises(ToolError) as raised:
                DocHtmlPage(manifest, "user-guide", render.menu, True).process()

            assert_equal(str(raised.exception), "unable to find a match for highlight: suppressed")
    finally:
        execute_module.DocExecute._execute_run = execute_run_real


# A manifest where one page is in the menu and the other is not, so the menu is built from what is in it
MANIFEST_MENU = MANIFEST.replace(' menu="Home"', "")

# A manifest where no page is in the menu, so there is no menu at all
MANIFEST_NO_MENU = MANIFEST_MENU.replace(' menu="Guide"', "")

# A document of the cases the main one does not hold
OTHER_GUIDE = """<doc title="User Guide">
    <description>How to use it.</description>

    <section id="start">
        <title>Start</title>

        <table>
            <table-data>
                <table-row>
                    <table-cell>one</table-cell>
                </table-row>
            </table-data>
        </table>

        <execute-list host="repo" show="n">
            <title>Hidden</title>

            <execute><exe-cmd>hidden</exe-cmd></execute>
        </execute-list>

        <execute-list host="repo">
            <title>Shown</title>

            <execute show="n"><exe-cmd>not shown</exe-cmd></execute>

            <execute><exe-cmd>no output</exe-cmd></execute>

            <execute output="y">
                <exe-cmd>pgbackrest info</exe-cmd>
                <exe-highlight-type>error</exe-highlight-type>
                <exe-highlight>suppressed</exe-highlight>
            </execute>
        </execute-list>

        <backrest-config host="repo" file="/etc/pgbackrest.conf" show="n">
            <title>Hidden</title>

            <backrest-config-option section="global" key="a">1</backrest-config-option>
        </backrest-config>

        <postgres-config host="repo" file="/pg/postgresql.conf" show="n">
            <title>Hidden</title>

            <postgres-config-option key="a">1</postgres-config-option>
        </postgres-config>
    </section>
</doc>
"""


####################################################################################################################################
def test_page_other():
    """The cases a page holds that the documentation does not use everywhere."""

    with tempfile.TemporaryDirectory() as path:
        manifest = _manifest(_doc_path(path, user_guide=OTHER_GUIDE))
        manifest.var_store.add("html-logo", "<img src='logo.png'>")

        render = manifest.render_get(RENDER_HTML)
        page = DocHtmlPage(manifest, "user-guide", render.menu, False).process()

        # A logo of its own for the header
        assert_in('<div class="page-header-logo">', page)

        # A table with no header lines every column up left
        assert_in('<td class="table-data-left">', page)
        assert_not_in("table-header-row", page)

        # A command list that is not shown, and a command of a list that is not shown
        assert_not_in("hidden", page)
        assert_not_in("not shown", page)

        # Output the documentation is pointing at as an error looks different from output it is pointing at
        assert_in('<pre class="execute-body-output-highlight-error">', page)

        # A configuration that is not shown
        assert_not_in("config-title", page)


####################################################################################################################################
def test_page_postgres_empty():
    """A PostgreSQL configuration the documentation added nothing to says so rather than showing nothing."""

    import command.render.execute as execute_module

    postgres_real = execute_module.DocExecute.postgres_config
    execute_module.DocExecute.postgres_config = lambda self, section, config, depth: ("/pg/x", None, True)

    try:
        guide = """<doc title="G"><description>D</description><section id="a"><title>A</title>
            <postgres-config host="repo" file="/pg/x"><title>C</title></postgres-config>
        </section></doc>"""

        with tempfile.TemporaryDirectory() as path:
            manifest = _manifest(_doc_path(path, user_guide=guide))
            render = manifest.render_get(RENDER_HTML)
            page = DocHtmlPage(manifest, "user-guide", render.menu, False).process()

            assert_in("<No PgBackRest Settings>", page)
    finally:
        execute_module.DocExecute.postgres_config = postgres_real


####################################################################################################################################
def test_html_render_plain():
    """A build with no logo and no sponsors copies only the style."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        manifest = _manifest(path)
        manifest.var_store.add("sponsor", "n")
        manifest.var_store.add("logo", "n")
        manifest.var_store.add("project-logo", "logo.png")

        path_out = os.path.join(path, "output/html")

        os.makedirs(path_out)
        html_render(manifest, path, path_out, False)

        assert_equal(sorted(path_list(path_out)), ["default.css", "index.html", "user-guide.html"])


####################################################################################################################################
def test_builder_plain():
    """A page that says nothing about itself beyond what it must."""

    result = HtmlBuilder("pgBackRest", "Guide", None, None, None).render()

    assert_not_in("og:image", result)
    assert_not_in('name="description"', result)
    assert_not_in('rel="icon"', result)


####################################################################################################################################
def test_page_menu():
    """A page that is not in the menu is not in it, however it is rendered."""

    with tempfile.TemporaryDirectory() as path:
        assert_not_in("menu-link", _page(_doc_path(path, MANIFEST_MENU)))

    # A build where no page is in the menu has no menu at all
    with tempfile.TemporaryDirectory() as path:
        assert_not_in("page-menu", _page(_doc_path(path, MANIFEST_NO_MENU)))


####################################################################################################################################
def test_page_output_run():
    """Runs of output are grouped by whether the documentation is pointing at them, so a run is one block rather than one line."""

    import command.render.execute as execute_module

    execute_run_real = execute_module.DocExecute._execute_run
    execute_module.DocExecute._execute_run = lambda self, host_name, command, key, cmd: "before\nsuppressed here\nafter"

    try:
        with tempfile.TemporaryDirectory() as path:
            manifest = _manifest(
                _doc_path(
                    path,
                    user_guide=SIMPLE_GUIDE.replace(
                        '<execute output="y"><exe-cmd>pgbackrest info</exe-cmd></execute>',
                        '<execute output="y"><exe-cmd>pgbackrest info</exe-cmd><exe-highlight>suppressed</exe-highlight></execute>',
                    ),
                )
            )
            render = manifest.render_get(RENDER_HTML)
            page = DocHtmlPage(manifest, "user-guide", render.menu, True).process()

            assert_in('<pre class="execute-body-output">before</pre>', page)
            assert_in('<pre class="execute-body-output-highlight">suppressed here</pre>', page)
            assert_in('<pre class="execute-body-output">after</pre>', page)
    finally:
        execute_module.DocExecute._execute_run = execute_run_real
