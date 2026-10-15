"""Test Release Notes.

The release list is checked against the git history as it is built, so the history here is written to match the releases rather than
taken from anywhere."""

####################################################################################################################################
import json
import os
import tempfile

from harness.test import *

from common.error import *
from common.exec import exec_one
from common.storage import file_read, file_write
from common.xml import xml_node_attribute, xml_node_child, xml_node_child_list, xml_node_content, xml_parse
from command.render.release import release_current_stable, release_history_update, release_last, release_list, release_render
from command.render.render import DocRender

# The history a release is checked against, newest first, which is the order git writes it in
HISTORY = [
    ("h1", "Improve a thing."),
    ("h2", "v2.02: Second."),
    ("h3", "Fix a bug."),
    ("h4", "Begin v2.02 development."),
    ("h5", "v2.01: First."),
    ("h6", "Add a feature."),
    ("h7", "v1.00: Initial."),
]

CONTRIBUTOR = """
    <contributor-list>
        <contributor id="david.steele">
            <contributor-name-display>David Steele</contributor-name-display>
        </contributor>
        <contributor id="other.person">
            <contributor-name-display>Other Person</contributor-name-display>
        </contributor>
        <contributor id="third.person">
            <contributor-name-display>Third Person</contributor-name-display>
        </contributor>
    </contributor-list>
"""

RELEASE_LIST = """
        <release date="XXXX-XX-XX" version="2.03dev" title="UNDER DEVELOPMENT">
            <release-core-list>
                <release-improvement-list>
                    <release-item>
                        <p>Improve a thing.</p>
                    </release-item>
                </release-improvement-list>

                <release-development-list>
                    <release-item>
                        <p>Rework the internals.</p>
                    </release-item>
                </release-development-list>
            </release-core-list>
        </release>

        <release date="2026-07-30" version="2.02" title="Second">
            <release-core-list>
                <text><p>A note about this release.</p></text>

                <release-bug-list>
                    <release-item>
                        <release-item-contributor-list>
                            <release-item-ideator id="other.person"/>
                        </release-item-contributor-list>

                        <p>Fix a bug.</p>
                    </release-item>
                </release-bug-list>
            </release-core-list>
        </release>

        <release date="2026-01-02" version="2.01" title="First">
            <release-doc-list>
                <release-feature-list>
                    <release-item>
                        <commit subject="Add a feature."/>

                        <release-item-contributor-list>
                            <release-item-contributor id="other.person"/>
                            <release-item-reviewer id="third.person"/>
                        </release-item-contributor-list>

                        <p>Add a feature.</p>

                        <p>And say more about it.</p>
                    </release-item>
                </release-feature-list>
            </release-doc-list>
        </release>

        <release date="2025-06-01" version="2.00.1" title="Patch">
            <release-core-list>
                <release-bug-list>
                    <release-item>
                        <p>Fix an old bug.</p>
                    </release-item>
                </release-bug-list>
            </release-core-list>
        </release>

        <release date="2025-01-01" version="1.00" title="Initial">
            <release-test-list>
                <release-improvement-list>
                    <release-item>
                        <p>Start the test suite.</p>
                    </release-item>
                </release-improvement-list>
            </release-test-list>
        </release>

        <release date="2024-01-01" version="0.90" title="Early">
            <release-core-list>
                <release-improvement-list>
                    <release-item>
                        <p>Try things out.</p>
                    </release-item>
                </release-improvement-list>
            </release-core-list>
        </release>

        <release date="2023-01-01" version="0.80" title="Earlier">
            <release-core-list>
                <release-improvement-list>
                    <release-item>
                        <p>Try other things out.</p>
                    </release-item>
                </release-improvement-list>
            </release-core-list>
        </release>
"""


# Releases with details and the history they are checked against
DETAIL_HISTORY = [
    ("dB", "Merge v2.59.1 release."),
    ("dA", "Fix a bug on a branch."),
    ("d8", "Rework the internals."),
    ("d0", "Improve speed."),
    ("d1", "Add a feature."),
    ("d2", "Fix a bug."),
    ("d3", 'Fix a "quoted" <bug>.'),
    ("d4", "Begin v2.60.0 development."),
    ("d5", "v2.59.0: Prior."),
    ("d6", "Improve a thing."),
    ("d7", "v1.00: Initial."),
]

DETAIL_RELEASE_LIST = """
        <release date="2026-10-01" version="2.60.0" title="Detail">
            <release-core-list>
                <release-feature-list>
                    <release-item>
                        <github-issue id="1"/>
                        <github-pull-request id="2"/>

                        <p>Add a feature.</p>

                        <p>Say more about it.</p>

                        <p>And <id>more</id>.</p>
                    </release-item>

                    <release-item>
                        <commit subject="Fix a bug.">
                            <github-pull-request id="3"/>
                        </commit>
                        <commit subject="Fix a &quot;quoted&quot;"/>

                        <p>Fix bugs.</p>
                    </release-item>

                    <release-item>
                        <commit subject="Improve speed.">
                            <github-issue id="5"/>
                            <github-pull-request id="6"/>
                        </commit>

                        <p>Speed things up.</p>
                    </release-item>
                </release-feature-list>

                <release-development-list>
                    <release-item>
                        <p>Rework the internals.</p>
                    </release-item>
                </release-development-list>
            </release-core-list>

            <release-backpatch-list>
                <release-backpatch version="2.59.1">
                    <commit subject="Fix a bug on a branch."/>
                    <commit subject="Merge v2.59.1 release."/>
                </release-backpatch>
            </release-backpatch-list>
        </release>

        <release date="2026-09-15" version="2.59.1" title="Patch">
            <release-core-list>
                <release-bug-list>
                    <release-item>
                        <p>Fix a bug on a branch.</p>
                    </release-item>
                </release-bug-list>
            </release-core-list>
        </release>

        <release date="2026-09-01" version="2.59.0" title="Prior">
            <release-core-list>
                <release-improvement-list>
                    <release-item>
                        <github-pull-request id="9"/>

                        <p>Improve a thing.</p>

                        <p>Say more.</p>
                    </release-item>
                </release-improvement-list>
            </release-core-list>
        </release>

        <release date="2025-01-01" version="1.00" title="Initial"/>
"""

# Html of the details up to the first paragraph or line of links
DETAIL_HTML = '\n<details class="list-detail"><summary class="list-detail-toggle" title="Details" aria-label="Details"></summary>'


####################################################################################################################################
def _doc(release_list=RELEASE_LIST, contributor=CONTRIBUTOR):
    """The release document, which is the list of releases and who worked on them."""

    return (
        '<doc title="Releases" subtitle="What Changed"><description>What changed.</description>'
        "<intro><text><p>About the releases.</p></text></intro>"
        "<release-list>%s</release-list>%s</doc>" % (release_list, contributor)
    )


####################################################################################################################################
def _render(content=None, dev=False, history=HISTORY, claim=True):
    """Build the release document from a release list."""

    with tempfile.TemporaryDirectory() as path:
        file_write(
            os.path.join(path, "resource/git-history.cache"),
            json.dumps(
                [{"commit": commit, "date": "2026-07-30 00:00:00 +0000", "subject": subject} for commit, subject in history]
            ),
        )

        return release_render(xml_parse(_doc() if content is None else content, "test.xml"), path, dev, claim)


####################################################################################################################################
def _commit(path, subject, body=None):
    """Commit to a repository with nothing changed."""

    exec_one(
        "git -C %s -c user.name=test -c user.email=test@pgbackrest.org -c commit.gpgsign=false commit -q --allow-empty -m '%s'%s"
        % (path, subject, "" if body is None else " -m '%s'" % body)
    )


####################################################################################################################################
def _section_list(root):
    """The sections of the release document, which is how the releases are grouped."""

    return [xml_node_attribute(section, "id") for section in xml_node_child_list(root, "section")]


####################################################################################################################################
def _item_html(release):
    """The html of each item of a release."""

    render = DocRender("html")

    return [
        render.process_text(item)
        for list_node in xml_node_child_list(release, "list")
        for item in xml_node_child_list(list_node, "list-item")
    ]


####################################################################################################################################
def test_release_list():
    """The releases are listed newest first, which is the order they are read in."""

    root = xml_parse(_doc(), "test.xml")

    assert_equal([xml_node_attribute(node, "version") for node in release_list(root)][:2], ["2.03dev", "2.02"])
    assert_equal(xml_node_attribute(release_last(root), "version"), "2.03dev")

    # The most recent release that is out is what a reader who is not tracking development would install
    assert_equal(xml_node_attribute(release_current_stable(root), "version"), "2.02")

    with assert_raises(ToolError) as raised:
        release_current_stable(xml_parse(_doc('<release date="XXXX-XX-XX" version="1.00dev" title="D"/>'), "test.xml"))

    assert_equal(str(raised.exception), "unable to find a released version")


####################################################################################################################################
def test_release_render():
    """Releases are grouped by how interesting they still are rather than by when they happened."""

    root = _render()

    assert_equal(_section_list(root), ["introduction", "development", "current", "supported", "unsupported"])

    # The introduction says what the page is, which the title of the page already says, so it has no header of its own
    assert_equal(xml_node_attribute(xml_node_child_list(root, "section")[0], "header"), "n")

    # The document says what it is and what a search engine should show for it, and leaves out the contents since the releases are
    # listed at the top of the page anyway
    assert_equal(xml_node_attribute(root, "title"), "Releases")
    assert_equal(xml_node_attribute(root, "subtitle"), "What Changed")
    assert_equal(xml_node_attribute(root, "toc"), "n")
    assert_equal(xml_node_content(xml_node_child(root, "description")), "What changed.")
    assert_in("About the releases.", xml_node_content(xml_node_child_list(root, "section")[0]))

    # A release keeps its own anchor whichever group it ends up in, so a link to it does not break when it moves
    development = xml_node_child_list(root, "section")[1]
    release = xml_node_child_list(development, "section")[0]

    assert_equal(xml_node_attribute(release, "id"), "2.03dev")
    assert_equal(xml_node_attribute(release, "anchor"), "no-inherit")
    assert_equal(xml_node_content(xml_node_child(release, "title")), "v2.03dev Notes")
    assert_equal(xml_node_content(xml_node_child(release, "subsubtitle")), "No Release Date Set")

    # A release that is out says when it came out
    supported = xml_node_child_list(root, "section")[3]

    assert_equal(
        xml_node_content(xml_node_child(xml_node_child_list(supported, "section")[0], "subsubtitle")), "Released January 2, 2026"
    )


####################################################################################################################################
def test_release_render_item():
    """An item says what changed, who did it, and which part of the project it was in."""

    root = _render()
    current = xml_node_child_list(root, "section")[2]
    release = xml_node_child_list(current, "section")[0]

    text = "".join(xml_node_content(node) for node in xml_node_child_list(release, "p"))

    # A note about the release as a whole goes before the first list, and a bug fix is named as a bug fix
    assert_in("A note about this release.", text)
    assert_in(" Bug Fixes:", text)

    item = xml_node_child(xml_node_child(release, "list"), "list-item")

    # The default contributor is dropped where they are the only one, so their name is not on every line of the page
    assert_equal(xml_node_content(item), "Fix a bug. (Reported by Other Person.)")

    # A release of the documentation says so, and an item written as more than one paragraph reads as one line
    supported = xml_node_child_list(root, "section")[3]
    release = xml_node_child_list(supported, "section")[0]

    assert_in("Documentation Features:", "".join(xml_node_content(node) for node in xml_node_child_list(release, "p")))
    assert_equal(
        xml_node_content(xml_node_child(xml_node_child(release, "list"), "list-item")),
        "Add a feature. And say more about it. (Contributed by Other Person. Reviewed by Third Person.)",
    )


####################################################################################################################################
def test_release_render_dev():
    """Development items are of no interest to a reader, so they are only listed on a development build."""

    development = xml_node_child_list(_render(), "section")[1]

    assert_not_in("Rework the internals.", xml_node_content(development))

    development = xml_node_child_list(_render(dev=True), "section")[1]

    assert_in("Rework the internals.", xml_node_content(development))
    assert_in("Development:", xml_node_content(development))


####################################################################################################################################
def test_release_render_error():
    """A release list that does not describe what happened is reported rather than rendered."""

    # Only one release can be in development at a time
    with assert_raises(ToolError) as raised:
        _render(
            _doc(
                '<release date="XXXX-XX-XX" version="1.01dev" title="D"/>'
                '<release date="XXXX-XX-XX" version="1.00dev" title="D"/>'
            )
        )

    assert_equal(str(raised.exception), "only one development release is allowed")

    # A date that is not a date
    with assert_raises(ToolError) as raised:
        _render(_doc(RELEASE_LIST.replace('date="2026-07-30"', 'date="2026-13"')))

    assert_equal(str(raised.exception), "invalid date 2026-13 for release 2.02")

    # A release that has no release before it in the list has no commits to take
    with assert_raises(ToolError) as raised:
        _render(_doc('<release date="2026-07-30" version="2.02" title="Second"/>'))

    assert_equal(str(raised.exception), "release 2.02 has no release before it to take its commits from")


####################################################################################################################################
def test_release_render_commit():
    """A release item is tied to the commit that did the work, so an item with nothing behind it is reported."""

    # An item with no commit named uses what it says as the subject to look for
    with assert_raises(ToolError) as raised:
        _render(_doc(RELEASE_LIST.replace("<p>Fix a bug.</p>", "<p>Fix a different bug.</p>")))

    assert_in("unable to find commit or no subject match for release 2.02 item 'Fix a different bug.'", str(raised.exception))

    # The commits of the release that nothing has claimed are listed, so the one that was meant is easy to find
    assert_in("h3: Fix a bug.", str(raised.exception))

    # An item that names a commit is held to it
    with assert_raises(ToolError) as raised:
        _render(_doc(RELEASE_LIST.replace('subject="Add a feature."', 'subject="Add another feature."')))

    assert_in("unable to find release 2.01 commit subject 'Add another feature.' in list", str(raised.exception))

    # A release that is out must have a commit that closed it and at least one commit in it
    with assert_raises(ToolError) as raised:
        _render(history=[entry for entry in HISTORY if entry[1] != "v2.01: First."])

    assert_equal(str(raised.exception), "release 2.01 must have an end commit")

    with assert_raises(ToolError) as raised:
        _render(history=[entry for entry in HISTORY if entry[1] != "Fix a bug."])

    assert_equal(str(raised.exception), "no commits found for release 2.02")


####################################################################################################################################
def test_release_render_contributor():
    """Who did the work is said once, and a contributor list that contradicts itself is reported."""

    # A reviewer cannot also be a contributor, since a reviewer is someone other than whoever did the work
    with assert_raises(ToolError) as raised:
        _render(
            _doc(RELEASE_LIST.replace('<release-item-reviewer id="third.person"/>', '<release-item-reviewer id="other.person"/>'))
        )

    assert_equal(str(raised.exception), "other.person cannot be both a contributor and a reviewer")

    # An item whose ideator is also the only contributor says the same thing twice
    with assert_raises(ToolError) as raised:
        _render(
            _doc(
                RELEASE_LIST.replace(
                    '<release-item-contributor id="other.person"/>\n'
                    '                            <release-item-reviewer id="third.person"/>',
                    '<release-item-contributor id="other.person"/>\n'
                    '                            <release-item-ideator id="other.person"/>',
                )
            )
        )

    assert_equal(str(raised.exception), "cannot have same contributor and ideator list: other.person")

    # A contributor who is not in the list has no name to render
    with assert_raises(ToolError) as raised:
        _render(
            _doc(
                RELEASE_LIST.replace(
                    'id="other.person"/>\n                        </release-item-contributor-list>\n\n'
                    "                        <p>Fix a bug.",
                    'id="nobody"/>\n                        </release-item-contributor-list>\n\n'
                    "                        <p>Fix a bug.",
                )
            )
        )

    assert_equal(str(raised.exception), "contributor nobody does not exist")


####################################################################################################################################
def test_release_render_contributor_default():
    """An item the default contributor did not work on was reviewed by them, and one they did needs no reviewer."""

    root = _render()

    # The item of 2.01 was contributed by someone else, so the default contributor is named as the reviewer of it
    supported = xml_node_child_list(root, "section")[3]
    item = xml_node_child(xml_node_child(xml_node_child_list(supported, "section")[0], "list"), "list-item")

    assert_in("Reviewed by Third Person.", xml_node_content(item))

    # The item of 1.00 has no contributor list at all, so it was done by the default contributor and needs no reviewer
    release = xml_node_child_list(supported, "section")[2]
    item = xml_node_child(xml_node_child(release, "list"), "list-item")

    assert_equal(xml_node_content(item), "Start the test suite.")
    assert_in("Test Suite Improvements:", "".join(xml_node_content(node) for node in xml_node_child_list(release, "p")))


####################################################################################################################################
def test_release_render_commit_share():
    """Two items that name the same commit both find it, since a commit can do more than one thing."""

    root = _render(
        _doc(
            RELEASE_LIST.replace(
                "<p>Add a feature.</p>\n\n                        <p>And say more about it.</p>",
                "<p>Add a feature.</p>",
            ).replace(
                "</release-feature-list>",
                """<release-item>
                        <commit subject="Add a feature."/>

                        <p>Add a feature as well.</p>
                    </release-item>
                </release-feature-list>""",
            )
        )
    )

    supported = xml_node_child_list(root, "section")[3]
    item_list = xml_node_child_list(xml_node_child(xml_node_child_list(supported, "section")[0], "list"), "list-item")

    assert_equal(len(item_list), 2)


####################################################################################################################################
def test_release_render_commit_ambiguous():
    """A subject that more than one commit begins with does not say which commit an item was, so it is reported."""

    with assert_raises(ToolError) as raised:
        _render(history=HISTORY[:3] + [("h8", "Fix a bug. Again.")] + HISTORY[3:])

    assert_in("subject prefix 'Fix a bug.' already found in commit", str(raised.exception))


####################################################################################################################################
def test_release_render_detail():
    """Releases with version >= 2.60 have the paragraphs after the first and the links of an item in details."""

    root = _render(_doc(DETAIL_RELEASE_LIST), history=DETAIL_HISTORY)

    assert_equal(_section_list(root), ["introduction", "current", "supported"])

    release = xml_node_child_list(xml_node_child_list(root, "section")[1], "section")[0]

    assert_equal(
        _item_html(release),
        [
            "Add a feature."
            + DETAIL_HTML
            + '\n<div class="list-detail-text">Say more about it.</div>'
            + '\n<div class="list-detail-text">And <span class="id">more</span>.</div>'
            + '\n<div class="list-detail-ref">'
            + '<span class="list-detail-ref-item">Issue <a href="{[github-url-issues]}/1">#1</a></span>'
            + '<span class="list-detail-ref-item">Pull request <a href="{[github-url-base]}/pull/2">#2</a></span>'
            + '<span class="list-detail-ref-item">Commit '
            + '<a href="{[github-url-base]}/commit/d1"><span class="id">d1</span></a></span>'
            + "</div></details>",
            # An item with more than one commit has a line for each commit
            "Fix bugs."
            + DETAIL_HTML
            + '\n<div class="list-detail-ref">'
            + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/d2"><span class="id">d2</span></a>'
            + " Fix a bug.</span>"
            + '<span class="list-detail-ref-item">Pull request <a href="{[github-url-base]}/pull/3">#3</a></span></div>'
            + '\n<div class="list-detail-ref">'
            + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/d3"><span class="id">d3</span></a>'
            + " Fix a &quot;quoted&quot; &lt;bug&gt;.</span></div></details>",
            # An item with one commit has the commit after the issues and pull requests the commit element names
            "Speed things up."
            + DETAIL_HTML
            + '\n<div class="list-detail-ref">'
            + '<span class="list-detail-ref-item">Issue <a href="{[github-url-issues]}/5">#5</a></span>'
            + '<span class="list-detail-ref-item">Pull request <a href="{[github-url-base]}/pull/6">#6</a></span>'
            + '<span class="list-detail-ref-item">Commit '
            + '<a href="{[github-url-base]}/commit/d0"><span class="id">d0</span></a></span>'
            + "</div></details>",
        ],
    )

    # Releases with version < 2.60 have no details
    supported = xml_node_child_list(root, "section")[2]

    assert_equal(_item_html(xml_node_child_list(supported, "section")[1]), ["Improve a thing. Say more."])


####################################################################################################################################
def test_release_render_detail_dev():
    """A development release has links for the commits that are found and no error for the commits that are not."""

    root = _render(
        _doc(
            DETAIL_RELEASE_LIST.replace('date="2026-10-01" version="2.60.0"', 'date="XXXX-XX-XX" version="2.60.0dev"').replace(
                "</release-feature-list>",
                """<release-item>
                        <p>Do a thing.</p>
                    </release-item>

                    <release-item>
                        <commit subject="Do another thing.">
                            <github-pull-request id="4"/>
                        </commit>

                        <p>Do another thing.</p>
                    </release-item>

                    <release-item>
                        <commit subject="Do a third thing.">
                            <github-pull-request id="7"/>
                        </commit>
                        <commit subject="Improve speed."/>

                        <p>Do a third thing.</p>
                    </release-item>
                </release-feature-list>""",
                1,
            )
        ),
        history=DETAIL_HISTORY,
    )

    development = xml_node_child_list(xml_node_child_list(root, "section")[1], "section")[0]
    item_list = _item_html(development)

    assert_equal(len(item_list), 6)
    assert_equal(item_list[3], "Do a thing.")
    assert_equal(
        item_list[4],
        "Do another thing."
        + DETAIL_HTML
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item">Pull request <a href="{[github-url-base]}/pull/4">#4</a></span></div></details>',
    )

    # A line for a commit that is not found has only the links the commit element names
    assert_equal(
        item_list[5],
        "Do a third thing."
        + DETAIL_HTML
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item">Pull request <a href="{[github-url-base]}/pull/7">#7</a></span></div>'
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/d0"><span class="id">d0</span></a>'
        + " Improve speed.</span></div></details>",
    )


####################################################################################################################################
def test_release_render_commit_id():
    """A commit can be named by id when more than one commit has the same subject."""

    history = DETAIL_HISTORY[:3] + [("e1", "Fix lines."), ("e2", "Fix lines.")] + DETAIL_HISTORY[3:]
    release_list = DETAIL_RELEASE_LIST.replace(
        "</release-feature-list>",
        """<release-item>
                        <commit id="e1" subject="Fix lines."/>
                        <commit id="e2" subject="Fix lines."/>

                        <p>Fix lines.</p>
                    </release-item>
                </release-feature-list>""",
        1,
    )

    root = _render(_doc(release_list), history=history)
    release = xml_node_child_list(xml_node_child_list(root, "section")[1], "section")[0]

    assert_equal(
        _item_html(release)[3],
        "Fix lines."
        + DETAIL_HTML
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/e1"><span class="id">e1</span></a>'
        + " Fix lines.</span></div>"
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/e2"><span class="id">e2</span></a>'
        + " Fix lines.</span></div></details>",
    )

    # An id that is not in the release
    with assert_raises(ToolError) as raised:
        _render(_doc(release_list.replace('<commit id="e2"', '<commit id="e3"')), history=history)

    assert_in("unable to find release 2.60.0 commit id 'e3' in list", str(raised.exception))

    # An id that more than one commit begins with
    with assert_raises(ToolError) as raised:
        _render(_doc(release_list.replace('<commit id="e2"', '<commit id="e"')), history=history)

    assert_equal(str(raised.exception), "commit prefix 'e' already found in commit e2")

    # An id needs a subject and the subject of the commit must begin with it
    with assert_raises(ToolError) as raised:
        _render(_doc(release_list.replace('<commit id="e2" subject="Fix lines."/>', '<commit id="e2"/>')), history=history)

    assert_equal(str(raised.exception), "unable to find attribute 'subject' in node 'commit'")

    with assert_raises(ToolError) as raised:
        _render(
            _doc(release_list.replace('<commit id="e2" subject="Fix lines."/>', '<commit id="e2" subject="Fix more lines."/>')),
            history=history,
        )

    assert_equal(str(raised.exception), "release 2.60.0 commit id 'e2' subject 'Fix lines.' does not begin with 'Fix more lines.'")


####################################################################################################################################
def test_release_render_backpatch():
    """Commits back-patched to a patch release are listed on a development build, grouped by the version of the patch release."""

    root = _render(_doc(DETAIL_RELEASE_LIST), dev=True, history=DETAIL_HISTORY)
    release = xml_node_child_list(xml_node_child_list(root, "section")[1], "section")[0]

    assert_in("Back-Patched:", "".join(xml_node_content(node) for node in xml_node_child_list(release, "p")))
    assert_equal(
        _item_html(release)[-1],
        'Back-patched to <a href="#2.59.1">v2.59.1</a>.'
        + DETAIL_HTML
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/dA"><span class="id">dA</span></a>'
        + " Fix a bug on a branch.</span></div>"
        + '\n<div class="list-detail-ref">'
        + '<span class="list-detail-ref-item"><a href="{[github-url-base]}/commit/dB"><span class="id">dB</span></a>'
        + " Merge v2.59.1 release.</span></div></details>",
    )

    # The commits are claimed but not listed on a release build
    root = _render(_doc(DETAIL_RELEASE_LIST), history=DETAIL_HISTORY)

    assert_not_in("Back-Patched:", xml_node_content(xml_node_child_list(xml_node_child_list(root, "section")[1], "section")[0]))

    # The version must be a patch release in the list
    for backpatch_version in ("2.59.0", "2.58.1"):
        with assert_raises(ToolError) as raised:
            _render(
                _doc(
                    DETAIL_RELEASE_LIST.replace(
                        '<release-backpatch version="2.59.1">', '<release-backpatch version="%s">' % backpatch_version
                    )
                ),
                history=DETAIL_HISTORY,
            )

        assert_equal(str(raised.exception), "release 2.60.0 back-patch version %s is not a patch release" % backpatch_version)

    # A back-patch has commits
    with assert_raises(ToolError) as raised:
        _render(
            _doc(
                DETAIL_RELEASE_LIST.replace(
                    '<release-backpatch version="2.59.1">',
                    '<release-backpatch version="2.59.1"/>\n                <release-backpatch version="2.59.1">',
                )
            ),
            history=DETAIL_HISTORY,
        )

    assert_equal(str(raised.exception), "release 2.60.0 back-patch version 2.59.1 has no commits")


####################################################################################################################################
def test_release_render_bug_fix():
    """A bug fix release x.y.z with z > 0 is skipped when it is not the most recent and a release x.y.0 is checked."""

    # The commits of 2.60.0 end at the commit that closed 2.59.0
    with assert_raises(ToolError) as raised:
        _render(_doc(DETAIL_RELEASE_LIST.replace("<p>Add a feature.</p>", "<p>Improve a thing.</p>")), history=DETAIL_HISTORY)

    assert_in("unable to find commit or no subject match for release 2.60.0 item 'Improve a thing.'", str(raised.exception))

    # Every commit of 2.60.0 is claimed by an item
    with assert_raises(ToolError) as raised:
        _render(_doc(DETAIL_RELEASE_LIST), history=[("d9", "Do something unclaimed.")] + DETAIL_HISTORY)

    assert_in("release 2.60.0 has commits that no item claims:\n2026-07-30 d9: Do something unclaimed.", str(raised.exception))

    # Commits that no item claims are allowed when claim is false
    _render(_doc(DETAIL_RELEASE_LIST), history=[("d9", "Do something unclaimed.")] + DETAIL_HISTORY, claim=False)

    # 2.59.0 is checked
    with assert_raises(ToolError) as raised:
        _render(_doc(DETAIL_RELEASE_LIST.replace("<p>Improve a thing.</p>", "<p>Improve two things.</p>")), history=DETAIL_HISTORY)

    assert_in("unable to find commit or no subject match for release 2.59.0 item 'Improve two things.'", str(raised.exception))


####################################################################################################################################
def test_release_history_update():
    """The history is written from git when the newest commit in it has changed."""

    with tempfile.TemporaryDirectory() as path:
        path_doc = os.path.join(path, "doc")
        path_history = os.path.join(path_doc, "resource/git-history.cache")

        exec_one("git -C %s init -q" % path)
        _commit(path, "Add a feature.", "What the feature does.")
        _commit(path, "v2.01: First.", "The release notes.")

        # With no integration branch the history ends at HEAD
        assert_false(release_history_update(path, path_doc))

        history = json.loads(file_read(path_history))

        assert_equal(history[0]["commit"], exec_one("git -C %s rev-parse HEAD" % path).strip())
        assert_equal(sorted(history[1]), ["body", "commit", "date", "subject"])

        # The body of a commit that closed a release is left out
        assert_equal(
            [(commit["subject"], commit.get("body")) for commit in history],
            [("v2.01: First.", None), ("Add a feature.", "What the feature does.")],
        )

        # The history is not written while HEAD is the newest commit in it
        file_write(path_history, json.dumps(history[:1]))

        release_history_update(path, path_doc)

        assert_equal(len(json.loads(file_read(path_history))), 1)

        # A new commit is first in the history
        _commit(path, "Fix a bug.")

        release_history_update(path, path_doc)

        assert_equal(
            [commit["subject"] for commit in json.loads(file_read(path_history))], ["Fix a bug.", "v2.01: First.", "Add a feature."]
        )

        # With an integration branch related to HEAD the history ends at the newest commit of HEAD that is also in integration
        exec_one("git -C %s branch integration" % path)
        _commit(path, "RELEASE: Add release notes.")

        assert_true(release_history_update(path, path_doc))
        assert_equal(json.loads(file_read(path_history))[0]["subject"], "Fix a bug.")

        # With an integration branch that is not related to HEAD the history ends at HEAD
        tree = exec_one("git -C %s hash-object -t tree /dev/null" % path).strip()
        commit = exec_one(
            "git -C %s -c user.name=test -c user.email=test@pgbackrest.org commit-tree %s -m 'Unrelated.'" % (path, tree)
        ).strip()
        exec_one("git -C %s update-ref refs/heads/integration %s" % (path, commit))

        assert_false(release_history_update(path, path_doc))
        assert_equal(json.loads(file_read(path_history))[0]["subject"], "RELEASE: Add release notes.")
