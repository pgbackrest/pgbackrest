"""Release Notes.

Builds the release document from the release list, which is one file per release holding what changed and who changed it. What comes
out is an ordinary document, so nothing downstream has to know the release notes are assembled rather than written.

Each item is checked against the git history, which is why the history is cached in the repository: a release item with no commit
behind it is either a note that should not be there or a commit whose subject no longer matches what the release says it did."""

####################################################################################################################################
import json
import os
import re
from collections import deque

from common.date import date_render
from common.error import ToolError
from common.storage import file_read
from common.xml import (
    xml_document_new,
    xml_node_add,
    xml_node_attribute,
    xml_node_attribute_set,
    xml_node_child,
    xml_node_child_add,
    xml_node_child_list,
    xml_node_content_add,
    xml_node_field,
    xml_node_text_add,
)
from command.render.render import SECTION_ANCHOR, SECTION_ANCHOR_NO_INHERIT, DocRender
from config.project import PROJECT_NAME

# Where the git history is cached, since the history of a release is fixed once it is out and reading it every build is slow
_FILE_HISTORY = "resource/git-history.cache"

# What the release notes are grouped by, in the order they are listed
_SECTION_LIST = (
    ("release-core-list", ""),
    ("release-doc-list", "Documentation"),
    ("release-test-list", "Test Suite"),
)

# What kind of change an item is, in the order they are listed
_ITEM_LIST = (
    ("release-bug-list", "Bug Fixes", "bug"),
    ("release-feature-list", "Features", "feature"),
    ("release-improvement-list", "Improvements", "improvement"),
    ("release-development-list", "Development", "development"),
)

# What each kind of contributor did, for a bug fix and for everything else
_CONTRIBUTOR_LIST = (
    ("release-item-contributor", "Fixed", "Contributed"),
    ("release-item-reviewer", "Reviewed", "Reviewed"),
    ("release-item-ideator", "Reported", "Suggested"),
)

_VERSION_BUG_FIX_EXP = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
_VERSION_DATE_EXP = re.compile(r"^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$")


####################################################################################################################################
def release_list(root):
    """Every release, most recent first."""

    return xml_node_child_list(xml_node_child(root, "release-list", True), "release")


####################################################################################################################################
def release_last(root):
    """The most recent release, which is the one being worked on when it is still in development."""

    return release_list(root)[0]


####################################################################################################################################
def release_current_stable(root):
    """The most recent release that is out, which is what a reader who is not tracking development would install."""

    for release in release_list(root):
        if not xml_node_attribute(release, "version", True).endswith("dev"):
            return release

    raise ToolError("unable to find a released version")


####################################################################################################################################
def _date_render(date, version):
    """Render the date of a release the way it reads rather than the way it sorts."""

    if date.startswith("X"):
        return "No Release Date Set"

    if _VERSION_DATE_EXP.match(date) is None:
        raise ToolError("invalid date %s for release %s" % (date, version))

    return "Released %s" % date_render(date)


####################################################################################################################################
class _Release:
    """Builds the release document."""

    def __init__(self, root, path_doc, dev):
        self.root = root
        self.dev = dev

        self.history = deque(json.loads(file_read(os.path.join(path_doc, _FILE_HISTORY))))

        # The first contributor is the default for every item, which is what keeps the common case out of the xml
        self.contributor_map = {}
        self.contributor_default = None

        for contributor in xml_node_child_list(xml_node_child(root, "contributor-list", True), "contributor"):
            id = xml_node_attribute(contributor, "id", True)

            if self.contributor_default is None:
                self.contributor_default = id

            self.contributor_map[id] = xml_node_field(contributor, "contributor-name-display", True)

        # A renderer for the plain text of an item, which is what a commit subject is compared against
        self.render = DocRender("text")
        self.render.tag_set("backrest", PROJECT_NAME)

    ################################################################################################################################
    def _contributor_text(self, item, item_type):
        """What the release notes say about who did the work on an item."""

        contributor_list = xml_node_child(item, "release-item-contributor-list")
        id_map = {}

        for type, _, _ in _CONTRIBUTOR_LIST:
            id_list = (
                []
                if contributor_list is None
                else [xml_node_attribute(node, "id", True) for node in xml_node_child_list(contributor_list, type)]
            )

            # An item with no contributor was done by the default contributor
            if len(id_list) == 0 and type == "release-item-contributor":
                id_list.append(self.contributor_default)

            # An item the default contributor did not work on was reviewed by them
            if (
                len(id_list) == 0
                and type == "release-item-reviewer"
                and self.contributor_default not in id_map.get("release-item-contributor", [])
            ):
                id_list.append(self.contributor_default)

            id_map[type] = id_list

        for reviewer in id_map["release-item-reviewer"]:
            if reviewer in id_map["release-item-contributor"]:
                raise ToolError("%s cannot be both a contributor and a reviewer" % reviewer)

        if id_map["release-item-ideator"] == id_map["release-item-contributor"]:
            raise ToolError("cannot have same contributor and ideator list: %s" % ", ".join(id_map["release-item-contributor"]))

        # Drop the default contributor where they are the only one, so their name is not on every line of the page
        for type in ("release-item-ideator", "release-item-contributor"):
            if id_map[type] == [self.contributor_default]:
                id_map[type] = []

        result = None

        for type, text_bug, text_other in _CONTRIBUTOR_LIST:
            if len(id_map[type]) == 0:
                continue

            name_list = []

            for id in id_map[type]:
                if id not in self.contributor_map:
                    raise ToolError("contributor %s does not exist" % id)

                name_list.append(self.contributor_map[id])

            text = "%s by %s." % (text_bug if item_type == "bug" else text_other, ", ".join(name_list))
            result = text if result is None else "%s %s" % (result, text)

        return result

    ################################################################################################################################
    def _commit_find(self, commit_list, subject, regexp=True):
        """Find a commit by what its subject begins with, which is how a release item is tied to the work that did it."""

        result = None

        for commit in commit_list:
            found = re.match(subject, commit["subject"]) is not None if regexp else commit["subject"].startswith(subject)

            if found:
                if result is not None:
                    raise ToolError("subject prefix '%s' already found in commit %s" % (subject, commit["commit"]))

                result = commit

        return result

    ################################################################################################################################
    def _commit_error(self, message, remaining_list, commit_map):
        """Report a problem along with the commits of the release that nothing has claimed yet."""

        raise ToolError(
            "%s:\n%s"
            % (
                message,
                "\n".join(
                    "%s %s: %s" % (commit_map[commit]["date"][:-15], commit, commit_map[commit]["subject"])
                    for commit in remaining_list
                ),
            )
        )

    ################################################################################################################################
    def _commit_list(self, version, release_idx, release_all):
        """Every commit that belongs to a release, and whether the items of the release are checked against them.

        A bug fix release that is not the most recent is skipped since it was built on a branch of its own, so the commits are not
        in the history between this release and the one before it."""

        if version < "2.01" or (_VERSION_BUG_FIX_EXP.match(version) is not None and release_idx != 0):
            return [], [], {}, False

        dev = version.endswith("dev")

        # The commit that opened development on this release and the commit that closed the release before it, which are the ends of
        # what belongs to this release
        commit_begin = self._commit_find(self.history, "Begin v%s development\\." % version)
        commit_begin = None if commit_begin is None else commit_begin["commit"]

        # Skip back over bug fix releases, which were built on branches of their own
        last_idx = release_idx + 1

        while last_idx < len(release_all) and _VERSION_BUG_FIX_EXP.match(
            xml_node_attribute(release_all[last_idx], "version", True)
        ):
            last_idx += 1

        if last_idx >= len(release_all):
            raise ToolError("release %s has no release before it to take its commits from" % version)

        version_last = xml_node_attribute(release_all[last_idx], "version", True)

        commit_last_end = self._commit_find(self.history, "v%s\\: .+" % version_last)

        if commit_last_end is None:
            raise ToolError("release %s must have an end commit" % version_last)

        # A release that is out has a commit that closed it, which the release after it has already checked for
        commit_end = self._commit_find(self.history, "v%s\\: .+" % version)
        commit_end = None if commit_end is None else commit_end["commit"]

        commit_list = []
        remaining_list = []
        commit_map = {}

        while len(self.history) > 0 and self.history[0]["commit"] != commit_last_end["commit"]:
            commit = self.history.popleft()

            # The commits that open and close a release are the release rather than part of it
            if commit["commit"] in (commit_begin, commit_end):
                continue

            commit_list.append(commit)
            remaining_list.append(commit["commit"])
            commit_map[commit["commit"]] = commit

        check = not dev

        if check and len(remaining_list) == 0:
            raise ToolError("no commits found for release %s" % version)

        return commit_list, remaining_list, commit_map, check

    ################################################################################################################################
    def _item_check(self, item, text, version, commit_list, remaining_list, commit_map):
        """Check that every commit a release item claims is in the history of the release."""

        commit_xml_list = xml_node_child_list(item, "commit")

        # An item with no commit named uses what it says as the subject to look for
        if len(commit_xml_list) == 0:
            subject = self.render.process_text(text)

            if self._commit_find(commit_list, subject, False) is None:
                self._commit_error(
                    "unable to find commit or no subject match for release %s item '%s'" % (version, subject),
                    remaining_list,
                    commit_map,
                )

        for commit_xml in commit_xml_list:
            subject = xml_node_attribute(commit_xml, "subject", True)
            commit = self._commit_find(commit_list, subject, False)

            if commit is None:
                self._commit_error(
                    "unable to find release %s commit subject '%s' in list" % (version, subject), remaining_list, commit_map
                )

            if commit["commit"] in remaining_list:
                remaining_list.remove(commit["commit"])

    ################################################################################################################################
    def render_document(self):
        """Build the release document."""

        root = xml_document_new("doc")

        xml_node_attribute_set(root, "title", xml_node_attribute(self.root, "title", True))

        # The releases are listed at the top of the page already, so contents would only repeat them and take space the notes can
        # make better use of
        xml_node_attribute_set(root, "toc", "n")

        # What the page is, which the header shows opposite what the project is
        subtitle = xml_node_attribute(self.root, "subtitle")

        if subtitle is not None:
            xml_node_attribute_set(root, "subtitle", subtitle)

        # The description is what a search engine shows for the page
        xml_node_content_add(xml_node_add(root, "description"), xml_node_field(self.root, "description", True))

        # The header is left off the page because the title of the page already introduces it
        intro = xml_node_add(root, "section", {"id": "introduction", "header": "n"})
        xml_node_content_add(xml_node_text_add(xml_node_add(intro, "title")), "Introduction")
        xml_node_child_add(xml_node_text_add(intro), xml_node_child(xml_node_child(self.root, "intro", True), "text", True))

        section = None
        dev_total = 0
        current_total = 0
        stable_total = 0
        unsupported_total = 0

        release_all = release_list(self.root)

        for release_idx, release in enumerate(release_all):
            version = xml_node_attribute(release, "version", True)
            dev = version.endswith("dev")

            commit_list, remaining_list, commit_map, check = self._commit_list(version, release_idx, release_all)

            # Where the release is listed, which is by how interesting it still is rather than by when it happened
            if dev:
                if dev_total > 0:
                    raise ToolError("only one development release is allowed")

                section = xml_node_add(root, "section", {"id": "development", "if": "'{[dev]}' eq 'y'"})
                xml_node_content_add(xml_node_text_add(xml_node_add(section, "title")), "Development Notes")
                dev_total += 1
            elif current_total == 0:
                section = xml_node_add(root, "section", {"id": "current"})
                xml_node_content_add(xml_node_text_add(xml_node_add(section, "title")), "Current Stable Release")
                current_total += 1
            elif version >= "1.00":
                if stable_total == 0:
                    section = xml_node_add(root, "section", {"id": "supported"})
                    xml_node_content_add(xml_node_text_add(xml_node_add(section, "title")), "Stable Releases")

                stable_total += 1
            else:
                if unsupported_total == 0:
                    section = xml_node_add(root, "section", {"id": "unsupported"})
                    xml_node_content_add(xml_node_text_add(xml_node_add(section, "title")), "Pre-Stable Releases")

                unsupported_total += 1

            release_section = xml_node_add(section, "section", {"id": version})

            # A release keeps its own anchor whichever group it ends up in, so a link to it does not break when it moves
            xml_node_attribute_set(release_section, SECTION_ANCHOR, SECTION_ANCHOR_NO_INHERIT)

            xml_node_content_add(
                xml_node_text_add(xml_node_add(release_section, "title")),
                "v%s %sNotes" % (version, "" if dev else "Release "),
            )
            xml_node_content_add(
                xml_node_text_add(xml_node_add(release_section, "subtitle")), xml_node_attribute(release, "title", True)
            )
            xml_node_content_add(
                xml_node_text_add(xml_node_add(release_section, "subsubtitle")),
                _date_render(xml_node_attribute(release, "date", True), version),
            )

            self._release_render(release, release_section, version, commit_list, remaining_list, commit_map, check)

        return root

    ################################################################################################################################
    def _release_render(self, release, release_section, version, commit_list, remaining_list, commit_map, check):
        """Render what one release changed."""

        note = False

        for section_type, section_title in _SECTION_LIST:
            type_node = xml_node_child(release, section_type)

            if type_node is None:
                continue

            for item_type, item_title, item_name in _ITEM_LIST:
                # Development items are only listed on a development build, since they are of no interest to a reader
                if not self.dev and item_type == "release-development-list":
                    continue

                item_node = xml_node_child(type_node, item_type)

                if item_node is None:
                    continue

                # A note about the release as a whole goes before the first list, whichever list that turns out to be
                if not note and xml_node_child(type_node, "text") is not None:
                    xml_node_child_add(
                        xml_node_text_add(xml_node_add(release_section, "p")), xml_node_child(type_node, "text", True)
                    )
                    note = True

                heading = xml_node_add(release_section, "p")
                xml_node_content_add(
                    xml_node_add(xml_node_text_add(heading), "b"),
                    "%s %s:" % (section_title, item_title),
                )

                list_node = xml_node_add(release_section, "list")

                for item in xml_node_child_list(item_node, "release-item"):
                    text = xml_node_child_list(item, "p")[0]

                    if check and item_type != "release-development-list":
                        self._item_check(item, text, version, commit_list, remaining_list, commit_map)

                    # An item written as more than one paragraph reads as one line in the release notes
                    for extra in xml_node_child_list(item, "p")[1:]:
                        xml_node_content_add(text, " ")
                        xml_node_child_add(text, extra)

                    contributor_text = self._contributor_text(item, item_name)

                    if contributor_text is not None:
                        xml_node_content_add(text, " (")
                        xml_node_content_add(xml_node_add(text, "i"), contributor_text)
                        xml_node_content_add(text, ")")

                    xml_node_child_add(xml_node_text_add(xml_node_add(list_node, "list-item")), text)


####################################################################################################################################
def release_render(root, path_doc, dev):
    """Build the release document from the release list."""

    return _Release(root, path_doc, dev).render_document()
