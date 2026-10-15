"""Release Notes.

Builds the release document from the release list, which is one file per release holding what changed and who changed it. What comes
out is an ordinary document, so nothing downstream has to know the release notes are assembled rather than written.

Each item is checked against the git history: a release item with no commit behind it is either a note that should not be there or a
commit whose subject no longer matches what the release reports it did."""

####################################################################################################################################
import html
import json
import os
import re
from collections import deque

from common.date import date_render
from common.error import ToolError
from common.exec import exec_result, exec_status
from common.storage import file_read, file_write
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

# Where the git history is cached
_FILE_HISTORY = "resource/git-history.cache"

# Local branch the git history ends at when it is related to HEAD
_HISTORY_BRANCH = "refs/heads/integration"

# Subject of a commit that closed a release
_RELEASE_SUBJECT_EXP = re.compile(r"^v[0-9]{1,2}\.[0-9]{1,2}(\.[0-9]+)?: ")

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

# Links to an issue and to a pull request, in the order they are listed
_REF_LIST = (
    ("github-issue", "Issue", "{[github-url-issues]}/%s"),
    ("github-pull-request", "Pull request", "{[github-url-base]}/pull/%s"),
)

# Link to a commit and the length of the hash it is named by
_COMMIT_URL = "{[github-url-base]}/commit/%s"
_COMMIT_SIZE = 7

# Releases with version >= this have the paragraphs after the first and the links of an item in details
_DETAIL_VERSION_MIN = "2.60"

# A bug fix release is x.y.z with z > 0
_VERSION_BUG_FIX_EXP = re.compile(r"^[0-9]+\.[0-9]+\.[1-9][0-9]*$")
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
def release_history_update(path_repo, path_doc):
    """Write the git history the release notes are checked against when the newest commit in it has changed.

    When the local integration branch is related to HEAD the history ends at the newest commit of HEAD that is also in integration
    and the result is true. The commits of a release branch are left out this way. Otherwise the history ends at HEAD and the result
    is false. The body of a commit that closed a release is left out."""

    path_history = os.path.join(path_doc, _FILE_HISTORY)
    command = "git -C %s merge-base HEAD %s" % (path_repo, _HISTORY_BRANCH)
    related = exec_status(command) == 0
    head = exec_result(command if related else "git -C %s rev-parse HEAD" % path_repo, suppress_stderr=True)[1].strip()

    if os.path.isfile(path_history) and json.loads(file_read(path_history))[0]["commit"] == head:
        return related

    # Records are separated by a byte that cannot appear in a commit message, and so are the fields within a record
    output = exec_result(
        "git -C %s log --pretty=format:%%H%%x00%%ci%%x00%%s%%x00%%b%%x1e %s" % (path_repo, head), suppress_stderr=True
    )[1]

    history = []

    for record in output.split("\x1e"):
        record = record.strip("\n")

        if record == "":
            continue

        commit, date, subject, body = record.split("\x00", 3)
        entry = {"commit": commit, "date": date, "subject": subject}
        body = body.strip()

        if body != "" and _RELEASE_SUBJECT_EXP.match(subject) is None:
            entry["body"] = body

        history.append(entry)

    file_write(path_history, json.dumps(history, indent=4, ensure_ascii=False) + "\n")

    return related


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

    def __init__(self, root, path_doc, dev, claim):
        self.root = root
        self.dev = dev
        self.claim = claim  # Must every commit of a release be claimed by an item?
        self.version_list = [xml_node_attribute(release, "version", True) for release in release_list(root)]

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
    def _commit_find(self, commit_list, subject, regexp=True, key="subject"):
        """Find a commit by what its subject begins with, which is how a release item is tied to the work that did it.

        With key="commit" the commit is found by what its hash begins with."""

        result = None

        for commit in commit_list:
            found = re.match(subject, commit[key]) is not None if regexp else commit[key].startswith(subject)

            if found:
                if result is not None:
                    raise ToolError("%s prefix '%s' already found in commit %s" % (key, subject, commit["commit"]))

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
    def _item_commit(self, item, text, version, commit_list, remaining_list, commit_map, check):
        """Find every commit a release item claims in the history of the release.

        Each commit is returned with the commit element that names it. The commit element is None when the item names no commit and
        the commit is None when it is not found. A commit that is not found is an error when check is true."""

        result = []
        commit_xml_list = xml_node_child_list(item, "commit")

        # An item with no commit named uses what it says as the subject to look for
        if len(commit_xml_list) == 0:
            subject = self.render.process_text(text)
            commit = self._commit_find(commit_list, subject, False)

            if commit is None and check:
                self._commit_error(
                    "unable to find commit or no subject match for release %s item '%s'" % (version, subject),
                    remaining_list,
                    commit_map,
                )

            result.append((None, commit))

        for commit_xml in commit_xml_list:
            subject = xml_node_attribute(commit_xml, "subject", True)
            commit_id = xml_node_attribute(commit_xml, "id")

            # A commit with an id is found by the id and its subject must begin with the subject named
            if commit_id is not None:
                name = "id '%s'" % commit_id
                commit = self._commit_find(commit_list, commit_id, False, "commit")

                if commit is not None and not commit["subject"].startswith(subject):
                    raise ToolError(
                        "release %s commit id '%s' subject '%s' does not begin with '%s'"
                        % (version, commit_id, commit["subject"], subject)
                    )
            else:
                name = "subject '%s'" % subject
                commit = self._commit_find(commit_list, subject, False)

            if commit is None and check:
                self._commit_error("unable to find release %s commit %s in list" % (version, name), remaining_list, commit_map)

            result.append((commit_xml, commit))

        # Remove the commits found from the commits no item has claimed
        for _, commit in result:
            if commit is not None and commit["commit"] in remaining_list:
                remaining_list.remove(commit["commit"])

        return result

    ################################################################################################################################
    @staticmethod
    def _ref_add(ref, label, url, value, code=False):
        """Add a link to an issue, a pull request, or a commit."""

        ref_item = xml_node_add(ref, "list-detail-ref-item")

        if label is not None:
            xml_node_content_add(ref_item, "%s " % label)

        link = xml_node_add(ref_item, "link", {"url": url})
        xml_node_content_add(xml_node_add(link, "code") if code else link, value)

        return ref_item

    ################################################################################################################################
    def _ref_list_add(self, ref, node):
        """Add a link to each issue and pull request a node names."""

        for type, label, url in _REF_LIST:
            for ref_xml in xml_node_child_list(node, type):
                id = xml_node_attribute(ref_xml, "id", True)
                self._ref_add(ref, label, url % id, "#%s" % id)

    ################################################################################################################################
    def _detail_add(self, list_item, item, item_commit_list, commit_line=False):
        """Add the paragraphs after the first and the issues, pull requests, and commits of an item as details.

        An item with one commit has the commit on the same line as its issues and pull requests, after them. An item with more than
        one commit, or any item when commit_line is true, has a line for each commit, with the subject of the commit and the issues
        and pull requests the commit element names."""

        commit_line = commit_line or len(item_commit_list) > 1

        detail = xml_node_add(list_item, "list-detail")

        for paragraph in xml_node_child_list(item, "p")[1:]:
            xml_node_child_add(xml_node_add(detail, "list-detail-text"), paragraph)

        ref = xml_node_add(detail, "list-detail-ref")
        self._ref_list_add(ref, item)

        for commit_xml, commit in item_commit_list:
            url = None if commit is None else _COMMIT_URL % commit["commit"]
            hash = None if commit is None else commit["commit"][:_COMMIT_SIZE]

            if commit_line:
                ref = xml_node_add(detail, "list-detail-ref")

                # Escape the subject for html
                if commit is not None:
                    xml_node_content_add(self._ref_add(ref, None, url, hash, True), " %s" % html.escape(commit["subject"]))

            if commit_xml is not None:
                self._ref_list_add(ref, commit_xml)

            if not commit_line and commit is not None:
                self._ref_add(ref, "Commit", url, hash, True)

        # Remove lines with no links and details with nothing in them
        for ref in xml_node_child_list(detail, "list-detail-ref"):
            if len(ref) == 0:
                detail.remove(ref)

        if len(detail) == 0:
            list_item.remove(detail)

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
        detail = version >= _DETAIL_VERSION_MIN

        for section_type, section_title in _SECTION_LIST:
            type_node = xml_node_child(release, section_type)

            if type_node is None:
                continue

            for item_type, item_title, item_name in _ITEM_LIST:
                item_node = xml_node_child(type_node, item_type)

                if item_node is None:
                    continue

                # Development items are only listed on a development build, since they are of no interest to a reader. When
                # version >= 2.60 their commits are still claimed.
                if not self.dev and item_type == "release-development-list":
                    if detail and check:
                        for item in xml_node_child_list(item_node, "release-item"):
                            self._item_commit(
                                item, xml_node_child_list(item, "p")[0], version, commit_list, remaining_list, commit_map, True
                            )

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
                    item_check = check and (detail or item_type != "release-development-list")
                    item_commit_list = None

                    if item_check or detail:
                        item_commit_list = self._item_commit(
                            item, text, version, commit_list, remaining_list, commit_map, item_check
                        )

                    # An item written as more than one paragraph reads as one line in the release notes when version < 2.60
                    if not detail:
                        for extra in xml_node_child_list(item, "p")[1:]:
                            xml_node_content_add(text, " ")
                            xml_node_child_add(text, extra)

                    contributor_text = self._contributor_text(item, item_name)

                    if contributor_text is not None:
                        xml_node_content_add(text, " (")
                        xml_node_content_add(xml_node_add(text, "i"), contributor_text)
                        xml_node_content_add(text, ")")

                    list_item = xml_node_text_add(xml_node_add(list_node, "list-item"))
                    xml_node_child_add(list_item, text)

                    if detail:
                        self._detail_add(list_item, item, item_commit_list)

        self._backpatch_render(release, release_section, version, commit_list, remaining_list, commit_map, check)

        # Every commit of the release is claimed by an item when version >= 2.60 and claim is true
        if detail and check and self.claim and len(remaining_list) > 0:
            self._commit_error("release %s has commits that no item claims" % version, remaining_list, commit_map)

    ################################################################################################################################
    def _backpatch_render(self, release, release_section, version, commit_list, remaining_list, commit_map, check):
        """Render the commits of a release that were back-patched to a patch release, grouped by the version of the patch release.

        The commits are claimed on every build and only listed on a development build."""

        backpatch_list = xml_node_child(release, "release-backpatch-list")

        if backpatch_list is None:
            return

        list_node = None

        if self.dev:
            heading = xml_node_add(release_section, "p")
            xml_node_content_add(xml_node_add(xml_node_text_add(heading), "b"), "Back-Patched:")
            list_node = xml_node_add(release_section, "list")

        for backpatch in xml_node_child_list(backpatch_list, "release-backpatch"):
            backpatch_version = xml_node_attribute(backpatch, "version", True)

            if backpatch_version not in self.version_list or _VERSION_BUG_FIX_EXP.match(backpatch_version) is None:
                raise ToolError("release %s back-patch version %s is not a patch release" % (version, backpatch_version))

            if len(xml_node_child_list(backpatch, "commit")) == 0:
                raise ToolError("release %s back-patch version %s has no commits" % (version, backpatch_version))

            item_commit_list = self._item_commit(backpatch, None, version, commit_list, remaining_list, commit_map, check)

            if list_node is not None:
                list_item = xml_node_text_add(xml_node_add(list_node, "list-item"))

                xml_node_content_add(list_item, "Back-patched to ")
                xml_node_content_add(xml_node_add(list_item, "link", {"url": "#%s" % backpatch_version}), "v%s" % backpatch_version)
                xml_node_content_add(list_item, ".")

                self._detail_add(list_item, backpatch, item_commit_list, True)


####################################################################################################################################
def release_render(root, path_doc, dev, claim=False):
    """Build the release document from the release list.

    When claim is true every commit of a release with version >= 2.60 must be claimed by an item."""

    return _Release(root, path_doc, dev, claim).render_document()
