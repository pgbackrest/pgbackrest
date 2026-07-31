"""Test Document Renderer.

The manifest is stood in for here rather than built, since what is being checked is what the renderer does with a document and not
how the documentation as a whole is put together."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.var_store import VarStore
from common.xml import xml_node_child, xml_node_normalize, xml_parse
from command.render.render import DocRender, child_list, content_list


####################################################################################################################################
class _Manifest:
    """Stands in for the documentation as a whole, which a renderer only asks a few things of."""

    def __init__(self, source_map=None, out_list=None, require_list=None, pre=False):
        self.var_store = VarStore()
        self.source_map = source_map or {}
        self.out_list = ["index", "user-guide"] if out_list is None else out_list
        self.require_list = require_list or []
        self.pre = pre

        self.page_anchor_map = {}
        self.link_list = []

    def evaluate_if(self, node):
        from common.eval import eval_expression
        from common.xml import xml_node_attribute

        expression = xml_node_attribute(node, "if")

        return True if expression is None else eval_expression(self.var_store.replace_str(expression))

    def source_get(self, key):
        return _Source(self.source_map[key])

    def render_out_get(self, type, key):
        return _Out() if key in self.out_list else None


####################################################################################################################################
class _Source:
    """Stands in for a document of the documentation."""

    def __init__(self, root):
        self.root = root
        self.cache_list = None


####################################################################################################################################
class _Out:
    """Stands in for a page of the documentation."""

    def __init__(self):
        self.source = "user-guide"
        self.file = None
        self.menu = None


####################################################################################################################################
def _doc(content):
    """Parse a document the way the tool reads one."""

    result = xml_parse(content, "test.xml")

    xml_node_normalize(result)

    return result


####################################################################################################################################
def _render(type, content, manifest=None, exe=False):
    """Build a renderer for a document, which is where filtering and the section map happen."""

    root = _doc(content)
    manifest = _Manifest() if manifest is None else manifest
    manifest.source_map["user-guide"] = root

    return DocRender(type, manifest, exe, "user-guide")


####################################################################################################################################
def test_child_list():
    """A child that holds nothing but text and says nothing about itself is a property of the node rather than part of it."""

    node = _doc(
        "<execute><exe-cmd>ls</exe-cmd><exe-user/><p>Text.</p><title>Title</title><host-add name='x'/><text>t</text></execute>"
    )

    # The command and the user are properties, and the empty text tags are content however little they hold
    assert_equal([child.tag for child in child_list(node)], ["p", "title", "host-add"])

    # Text is never content of the node holding it, since it is the text of that node rather than a child
    assert_equal([child.tag for child in child_list(_doc("<section><text>t</text><p>p</p></section>"))], ["p"])


####################################################################################################################################
def test_content_list():
    """Text and markup come back in the order they were written, since that is the order they are rendered in."""

    node = _doc("<p>Run <id>pgbackrest</id> as <b>root</b>.</p>")

    assert_equal([item if isinstance(item, str) else item.tag for item in content_list(node)], ["Run ", "id", " as ", "b", "."])

    # A node that holds nothing holds nothing rather than an empty run of text
    assert_equal(content_list(_doc("<p/>")), [])


####################################################################################################################################
def test_process_text():
    """Text is rendered with the markup each output type writes for a tag."""

    render = DocRender("html")

    assert_equal(
        render.process_text(_doc("<p>Run <id>pgbackrest</id> as <b>root</b>.</p>")),
        'Run <span class="id">pgbackrest</span> as <b>root</b>.',
    )
    assert_equal(DocRender("markdown").process_text(_doc("<p>Run <id>x</id>.</p>")), "Run `x`.")
    assert_equal(DocRender("text").process_text(_doc("<p>Run <id>x</id>.</p>")), "Run x.")

    # A run of spaces is how the xml is laid out to be read rather than something to render
    assert_equal(render.process_text(_doc("<p>a    b</p>")), "a b")

    # Text that runs over more than one line is layout as well, so it is dropped rather than rendered
    assert_equal(render.process_text(_doc("<p>a\n    <b>b</b></p>")), "<b>b</b>")

    # A quote has a tag of its own, so a quote in the text would be markup the renderer cannot see
    with assert_raises(ToolError) as raised:
        render.process_text(_doc('<p>say "no"</p>'))

    assert_in("unable to process quotes in string", str(raised.exception))


####################################################################################################################################
def test_process_text_type():
    """Each output type says what it has to about the text it writes."""

    # A page separates paragraphs itself, since the help is rendered to html and then put into a page rather than rendered in one
    assert_equal(DocRender("html").process_text(_doc("<text><p>one</p><p>two</p></text>")), "one<br/>\n<br/>\ntwo")
    assert_equal(DocRender("html").process_text(_doc("<text><p>one</p><b>two</b></text>")), "one<br/>\n<b>two</b>")

    # A word that a page would rather not say
    assert_equal(DocRender("html").process_text(_doc("<p>master</p>")), "ma&#115;ter")

    # Console text has no markup to escape into, so what a document escaped is written out
    assert_equal(DocRender("text").process_text(_doc("<p>a &amp;mdash; b &amp;lt; c &amp;ge; d</p>")), "a -- b < c >= d")


####################################################################################################################################
def test_process_tag():
    """A tag renders as what the output type writes for it, and a tag the output type has no markup for is an error."""

    render = DocRender("html")

    # A tag that holds text renders the text, and a tag that holds markup renders the markup
    assert_equal(render.process_tag(_doc("<id>x</id>")), '<span class="id">x</span>')
    assert_equal(render.process_tag(_doc("<p><b>x</b></p>")), "<b>x</b>")

    # An admonition says what kind it is before it says anything else
    assert_equal(
        render.process_tag(_doc("<admonition type='note'>Careful.</admonition>")),
        '<div class="admonition"><div class="note">NOTE:</div><div class="note-text">Careful.</div></div>',
    )
    assert_equal(DocRender("markdown").process_tag(_doc("<admonition type='note'>Careful.</admonition>")), "\n> **NOTE: Careful.\n")

    # A tag that holds markup and no text of its own renders what it holds
    assert_equal(render.process_tag(_doc("<b><i>x</i></b>")), "<b><i>x</i></b>")

    with assert_raises(ToolError) as raised:
        DocRender("markdown").process_tag(_doc("<user>x</user>"))

    assert_equal(str(raised.exception), "invalid type markdown or tag user")


####################################################################################################################################
def test_tag_set():
    """What a renderer writes for a tag can be set, which is how the release notes name the project rather than refer to it."""

    render = DocRender("text")

    assert_equal(render.process_text(_doc("<p><backrest/> works.</p>")), "{[project]} works.")

    render.tag_set("backrest", "pgBackRest")

    assert_equal(render.process_text(_doc("<p><backrest/> works.</p>")), "pgBackRest works.")

    # Setting a tag applies to this renderer rather than to every renderer of the type
    assert_equal(DocRender("text").process_text(_doc("<p><backrest/> works.</p>")), "{[project]} works.")


####################################################################################################################################
def test_variable_replace():
    """A renderer with no documentation to draw on writes what it was given, since there is nothing to replace it with."""

    assert_equal(DocRender("text").variable_replace("{[project]}"), "{[project]}")
    assert_is_none(DocRender("text").variable_replace(None))

    manifest = _Manifest()
    manifest.var_store.add("project", "pgBackRest")

    assert_equal(
        _render("text", "<doc><section id='a'><title>T</title></section></doc>", manifest).variable_replace("{[project]}"),
        "pgBackRest",
    )


####################################################################################################################################
def test_build_section():
    """A section knows where it sits, which is what a link to it and the sections that depend on it are written against."""

    render = _render(
        "html",
        """<doc>
            <section id="one"><title>One</title>
                <section id="two"><title>Two</title></section>
            </section>
            <section id="three"><title>Three</title></section>
        </doc>""",
    )

    assert_equal(sorted(render.section_map), ["/one", "/one/two", "/three"])

    # A section depends on the section before it unless it says otherwise, since the documentation is read in order
    from common.xml import xml_node_attribute

    assert_equal(xml_node_attribute(render.section_map["/three"], "depend"), "/one")
    assert_is_none(xml_node_attribute(render.section_map["/one"], "depend"))
    assert_equal(xml_node_attribute(render.section_map["/one/two"], "path-parent"), "/one")


####################################################################################################################################
def test_build_depend():
    """A depend is written relative to the section holding it, and one that points nowhere is a mistake in the document."""

    render = _render(
        "html",
        """<doc>
            <section id="one"><title>One</title>
                <section id="a"><title>A</title></section>
                <section id="b" depend="a"><title>B</title></section>
            </section>
            <section id="two" depend="/one"><title>Two</title></section>
        </doc>""",
    )

    from common.xml import xml_node_attribute

    assert_equal(xml_node_attribute(render.section_map["/one/b"], "depend"), "/one/a")
    assert_equal(xml_node_attribute(render.section_map["/two"], "depend"), "/one")

    with assert_raises(ToolError) as raised:
        _render(
            "html",
            "<doc><section id='one'><title>One</title></section><section id='two' depend='/missing'><title>Two</title></section></doc>",
        )

    assert_equal(str(raised.exception), "section 'two' depend '/missing' is not valid")


####################################################################################################################################
def test_build_filter():
    """A section whose condition does not hold is not part of this build of the document."""

    manifest = _Manifest()
    manifest.var_store.add("mode", "release")

    render = _render(
        "html",
        """<doc>
            <section id="one"><title>One</title></section>
            <section id="two" if="'{[mode]}' eq 'debug'"><title>Two</title></section>
            <section id="three" if="'{[mode]}' eq 'release'"><title>Three</title></section>
        </doc>""",
        manifest,
    )

    assert_equal(sorted(render.section_map), ["/one", "/three"])


####################################################################################################################################
def test_build_source():
    """A section that takes its content from another document takes its title too, and renames the sections it takes."""

    manifest = _Manifest()
    manifest.source_map["other"] = _doc(
        "<doc title='Other'><section id='inner'><title>Inner</title>"
        "<p>See <link section='/inner'>this</link>.</p></section></doc>"
    )

    render = _render("html", "<doc><section id='outer' source='other'/></doc>", manifest)

    assert_equal(sorted(render.section_map), ["/outer", "/outer/inner"])
    assert_equal(render.process_text(xml_node_child(render.section_map["/outer"], "title")), "Other")

    # A link to a section that was taken points at where the section is now rather than where it was written
    assert_in('href="#outer/inner"', render.process_text(xml_node_child(render.section_map["/outer/inner"], "p")))

    # A link that points somewhere else is left alone, since there is no section of it to move
    manifest = _Manifest()
    manifest.source_map["other"] = _doc(
        "<doc title='Other'><section id='inner'><title>Inner</title>"
        "<p>See <link url='https://x'>this</link>.</p></section></doc>"
    )

    render = _render("html", "<doc><section id='outer' source='other'/></doc>", manifest)

    assert_in('href="https://x"', render.process_text(xml_node_child(render.section_map["/outer/inner"], "p")))

    with assert_raises(ToolError) as raised:
        manifest = _Manifest()
        manifest.source_map["other"] = _doc("<doc title='Other'/>")

        _render("html", "<doc><section id='outer' source='other'><title>Mine</title></section></doc>", manifest)

    assert_equal(str(raised.exception), "cannot specify title in section that sources another document")


####################################################################################################################################
def test_build_pre():
    """A command marked pre is run while the image for the host is built, so it is skipped when the host is up."""

    manifest = _Manifest(pre=True)
    manifest.var_store.add("host-repo", "repo")

    render = _render(
        "html",
        """<doc>
            <section id="one"><title>One</title>
                <execute-list host="{[host-repo]}"><title>Setup</title>
                    <execute pre="y"><exe-cmd>apt-get update</exe-cmd></execute>
                    <execute><exe-cmd>ls</exe-cmd></execute>
                </execute-list>
            </section>
        </doc>""",
        manifest,
        exe=True,
    )

    from common.xml import xml_node_attribute, xml_node_field

    assert_equal([xml_node_field(node, "exe-cmd") for node in render.pre_execute("repo")], ["apt-get update"])
    assert_equal(render.pre_execute("missing"), [])

    # A section that runs commands is worth logging, which keeps the log to the sections that take a while
    assert_equal(xml_node_attribute(render.section_map["/one"], "log"), "y")


####################################################################################################################################
def test_require():
    """A build of part of a document renders the sections that were asked for and the sections they need."""

    document = """<doc>
        <section id="one"><title>One</title></section>
        <section id="two"><title>Two</title>
            <section id="inner"><title>Inner</title></section>
        </section>
        <section id="three"><title>Three</title></section>
    </doc>"""

    # A section brings the sections it holds and the sections it depends on, which for these is the section before it
    render = _render("html", document, _Manifest(require_list=["/two"]))

    assert_true(render.is_required(render.section_map["/two"]))
    assert_true(render.is_required(render.section_map["/two/inner"]))
    assert_true(render.is_required(render.section_map["/one"]))
    assert_false(render.is_required(render.section_map["/three"]))

    # Everything is required when nothing was asked for
    assert_true(_render("html", document).is_required(_render("html", document).section_map["/three"]))

    # A section inside another brings the section holding it for what that section does rather than for what it says
    render = _render("html", document, _Manifest(require_list=["/two/inner"]))

    assert_true(render.is_required(render.section_map["/two/inner"]))
    assert_true(render.is_required(render.section_map["/one"]))
    assert_false(render.is_required(render.section_map["/two"]))

    # A section named twice is added once
    render = _render("html", document, _Manifest(require_list=["/two", "/two/inner"]))

    assert_true(render.is_required(render.section_map["/two/inner"]))

    # A section inside the first section of a document has nothing before it to depend on, so only what holds it is needed
    render = _render(
        "html",
        """<doc>
            <section id="one"><title>One</title>
                <section id="inner"><title>Inner</title></section>
            </section>
            <section id="two"><title>Two</title></section>
        </doc>""",
        _Manifest(require_list=["/one/inner"]),
    )

    assert_true(render.is_required(render.section_map["/one/inner"]))
    assert_false(render.is_required(render.section_map["/one"]))
    assert_false(render.is_required(render.section_map["/two"]))

    with assert_raises(ToolError) as raised:
        _render("html", document, _Manifest(require_list=["two"]))

    assert_equal(str(raised.exception), "path two must begin with a /")

    with assert_raises(ToolError) as raised:
        _render("html", document, _Manifest(require_list=["/missing"]))

    assert_equal(str(raised.exception), "required section '/missing' does not exist")


####################################################################################################################################
def test_anchor_map():
    """An anchor is the id of a section prefixed with the anchors of the sections holding it."""

    manifest = _Manifest()

    _render(
        "html",
        """<doc>
            <section id="one"><title>One</title>
                <section id="two"><title>Two</title></section>
                <section id="free" anchor="no-inherit"><title>Free</title></section>
            </section>
        </doc>""",
        manifest,
    )

    # A section that says it does not take the anchor of the section holding it keeps its own
    assert_equal(sorted(manifest.page_anchor_map["user-guide"]), ["free", "one", "one/two"])


####################################################################################################################################
def test_toc():
    """A document says whether it has a table of contents and whether the entries in it are numbered."""

    assert_true(_render("html", "<doc><section id='a'><title>A</title></section></doc>").toc)
    assert_true(_render("html", "<doc><section id='a'><title>A</title></section></doc>").toc_number)
    assert_false(_render("html", "<doc toc='n'><section id='a'><title>A</title></section></doc>").toc)
    assert_false(_render("html", "<doc toc-number='n'><section id='a'><title>A</title></section></doc>").toc_number)


####################################################################################################################################
def test_link_url():
    """A link that says where it points needs nothing worked out."""

    render = _render("html", "<doc><section id='a'><title>A</title></section></doc>")

    assert_equal(render.process_tag(_doc("<link url='https://x'>X</link>")), '<a href="https://x">X</a>')
    assert_equal(DocRender("markdown").process_tag(_doc("<link url='https://x'>X</link>")), "[X](https://x)")
    assert_equal(DocRender("text").process_tag(_doc("<link url='https://x'>X</link>")), "X")


####################################################################################################################################
def test_link_page():
    """A link to a page points at the page when it is part of this build and at the website when it is not."""

    render = _render("html", "<doc><section id='a'><title>A</title></section></doc>")

    assert_equal(render.process_tag(_doc("<link page='index'>Home</link>")), '<a href="index.html">Home</a>')

    # A page that is not part of this build is on the website rather than beside this one
    assert_in("{[backrest-url-base]}/other.html", render.process_tag(_doc("<link page='other'>Other</link>")))

    # A link to a section of another page is recorded so it can be checked once every page is rendered
    assert_equal(
        render.process_tag(_doc("<link page='index' section='/setup'>Setup</link>")), '<a href="index.html#setup">Setup</a>'
    )
    assert_equal(render.manifest.link_list, [{"source": "user-guide", "page": "index", "section": "/setup"}])

    # A link to a section of the page it is on is a link to the section
    assert_equal(render.process_tag(_doc("<link page='user-guide' section='/a'>A</link>")), '<a href="#a">A</a>')


####################################################################################################################################
def test_link_page_markdown():
    """Markdown has no anchor for a section of another page, so that link points at the website."""

    manifest = _Manifest()
    render = _render("markdown", "<doc><section id='a'><title>A</title></section></doc>", manifest)

    assert_equal(render.process_tag(_doc("<link page='index'>Home</link>")), "[Home](index.md)")
    assert_in("{[backrest-url-base]}/index.html#setup", render.process_tag(_doc("<link page='index' section='/setup'>S</link>")))

    with assert_raises(ToolError) as raised:
        _render("text", "<doc><section id='a'><title>A</title></section></doc>").process_tag(_doc("<link page='index'>Home</link>"))

    assert_equal(str(raised.exception), "page links not supported for type text, value 'Home'")


####################################################################################################################################
def test_link_section():
    """A link to a section of this page is an anchor in html and the title of the section in markdown."""

    document = "<doc><section id='a'><title>Set Up The Repo</title></section></doc>"

    assert_equal(_render("html", document).process_tag(_doc("<link section='/a'>A</link>")), '<a href="#a">A</a>')
    assert_equal(_render("markdown", document).process_tag(_doc("<link section='/a'>A</link>")), "[A](#set-up-the-repo)")

    # A section link is written rooted so that stripping the leading / gives the anchor
    with assert_raises(ToolError) as raised:
        _render("html", document).process_tag(_doc("<link section='a'>A</link>"))

    assert_equal(str(raised.exception), "link section 'a' must begin with '/'")

    with assert_raises(ToolError) as raised:
        _render("html", document).process_tag(_doc("<link section='/missing'>A</link>"))

    assert_equal(str(raised.exception), "section link '/missing' does not exist")
