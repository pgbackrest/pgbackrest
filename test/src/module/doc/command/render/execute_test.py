"""Test Document Execution.

No container is started here. The host is stood in for, so what is checked is the command that would run, the key it is cached
under, and what a reader would be shown of what it wrote."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

import command.render.execute as execute_module
from common.error import *
from common.storage import file_read, file_write
from common.var_store import VarStore
from common.xml import xml_node_child, xml_node_child_list, xml_node_normalize, xml_parse
from command.render.execute import CacheInvalidError, DocExecute, doc_user

# What a stood-in host was asked to do, and what it should write when it is asked
HOST_LIST = []
RESULT = {}


####################################################################################################################################
class _Host:
    """Stands in for a host the documentation runs commands on."""

    def __init__(self, name, container, image, user, mount_list=None, option=None, param=None, host_update=True):
        self.name = name
        self.image = image
        self.ip = "172.17.0.9"

        HOST_LIST.append(("add", name, image, mount_list, option, param))

    def execute(self, command, **kwargs):
        HOST_LIST.append(("execute", command, kwargs))

        return 0, RESULT.get("output", ""), RESULT.get("error", "")

    def copy_to(self, source, destination, owner=None, mode=None):
        HOST_LIST.append(("copy_to", source, destination, owner, mode))

    def copy_from(self, source, destination):
        HOST_LIST.append(("copy_from", source, destination))

        file_write(destination, RESULT.get("pg_config", "shared_buffers = 128MB\n"))


####################################################################################################################################
class _Group:
    """Stands in for the hosts of a build."""

    def host_add(self, host):
        HOST_LIST.append(("group", host.name))


####################################################################################################################################
class _Manifest:
    """Stands in for the documentation as a whole."""

    def __init__(self, path_doc, cache_only=False, pre=False):
        self.path_doc = path_doc
        self.cache_only = cache_only
        self.pre = pre
        self.var_store = VarStore()
        self.require_list = []
        self.source_map = {}
        self.page_anchor_map = {}
        self.link_list = []

    def evaluate_if(self, node):
        return True

    def source_get(self, key):
        return self.source_map[key]

    def render_out_get(self, type, key):
        return _Out() if key == "user-guide" else None


####################################################################################################################################
class _Out:
    """Stands in for a page of the documentation."""

    def __init__(self):
        self.source = "user-guide"
        self.file = None
        self.menu = None


####################################################################################################################################
class _Source:
    """Stands in for a document of the documentation."""

    def __init__(self, root):
        self.root = root
        self.cache_list = None


####################################################################################################################################
def _doc(content):
    """Parse a document the way the tool reads one."""

    result = xml_parse(content, "test.xml")

    xml_node_normalize(result)

    return result


####################################################################################################################################
def _execute(path, document=None, exe=True, cache_only=False, pre=False, cache_list=None):
    """Build a renderer for a document, with the host stood in for."""

    HOST_LIST.clear()
    RESULT.clear()

    execute_module.Host = _Host
    execute_module.HOST_GROUP = _Group()

    manifest = _Manifest(path, cache_only, pre)
    root = _doc(
        """<doc>
            <section id="one"><title>One</title>
                <execute-list host="repo"><title>Setup</title>
                    <execute><exe-cmd>ls -l</exe-cmd></execute>
                </execute-list>
            </section>
        </doc>"""
        if document is None
        else document
    )

    source = _Source(root)
    source.cache_list = cache_list
    manifest.source_map["user-guide"] = source

    return DocExecute("html", manifest, "user-guide", exe)


####################################################################################################################################
def test_doc_user():
    """A command runs as the user building the documentation unless it says otherwise."""

    from common.user import user_name

    assert_equal(doc_user(), "ubuntu" if user_name() == "root" else user_name())


####################################################################################################################################
def test_execute_key():
    """The key is everything that decides what a command would be, which is what makes the cache trustworthy."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        command = xml_node_child(xml_node_child(render.section_map["/one"], "execute-list"), "execute")

        assert_equal(
            render.execute_key("repo", command),
            {"host": "repo", "cmd": ["ls -l"], "output": False, "run-as-user": None, "load-env": True, "bash-wrap": True},
        )


####################################################################################################################################
def test_execute_key_user():
    """A command that names a user is reached with sudo unless it is showing what running as that user looks like."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)

        key = render.execute_key("repo", _doc('<execute user="postgres"><exe-cmd>psql</exe-cmd></execute>'))

        assert_equal(key["cmd"], ["sudo -u postgres psql"])
        assert_is_none(key["run-as-user"])

        assert_equal(render.execute_key("repo", _doc('<execute user="root"><exe-cmd>id</exe-cmd></execute>'))["cmd"], ["sudo id"])

        # A command that is showing what running as a user looks like runs as that user rather than reaching it with sudo
        key = render.execute_key("repo", _doc('<execute user="postgres" user-force="y"><exe-cmd>psql</exe-cmd></execute>'))

        assert_equal(key["cmd"], ["psql"])
        assert_equal(key["run-as-user"], "postgres")


####################################################################################################################################
def test_execute_key_split():
    """A command written over more than one line is shown that way, with a backslash where it breaks."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)

        key = render.execute_key("repo", _doc("<execute><exe-cmd>pgbackrest backup\n    --stanza=demo</exe-cmd></execute>"))

        assert_equal(key["cmd"], ["pgbackrest backup \\", "    --stanza=demo"])


####################################################################################################################################
def test_execute_key_output():
    """Output is kept when the documentation shows it or takes a variable from it."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)

        assert_false(render.execute_key("repo", _doc("<execute><exe-cmd>ls</exe-cmd></execute>"))["output"])
        assert_true(render.execute_key("repo", _doc('<execute output="y"><exe-cmd>ls</exe-cmd></execute>'))["output"])
        assert_true(render.execute_key("repo", _doc('<execute show="y"><exe-cmd>ls</exe-cmd></execute>'))["output"])
        assert_true(render.execute_key("repo", _doc('<execute variable-key="v"><exe-cmd>ls</exe-cmd></execute>'))["output"])

        # Everything else a command says about how it runs is part of the key as well
        key = render.execute_key(
            "repo",
            _doc(
                '<execute err-expect="1" load-env="n" bash-wrap="n" filter="n">'
                "<exe-cmd>ls</exe-cmd><exe-cmd-extra>-l</exe-cmd-extra><exe-highlight>error</exe-highlight></execute>"
            ),
        )

        assert_equal(key["err-expect"], "1")
        assert_equal(key["cmd-extra"], "-l")
        assert_false(key["load-env"])
        assert_false(key["bash-wrap"])
        assert_true(key["output"])
        assert_equal(key["highlight"], {"filter": False, "filter-context": 2, "list": ["error"]})


####################################################################################################################################
def test_execute():
    """A command runs on the host and what it wrote is what a reader is shown."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]
        command = xml_node_child(xml_node_child(section, "execute-list"), "execute")

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")
        RESULT["output"] = "one\ntwo\n"

        cmd, output = render.execute(section, "repo", command)

        assert_equal(cmd, "ls -l")

        # Output is only kept when the documentation is showing it
        assert_is_none(output)
        assert_equal(HOST_LIST[-1][1], "ls -l")

        # The command is cached so the documentation can be rendered again without running it
        assert_equal(render.source.cache_list[-1]["type"], "exe")


####################################################################################################################################
def test_execute_output():
    """A command whose output is shown has it kept, and a variable can be taken from it."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")
        RESULT["output"] = "\nversion 2.60\n"

        cmd, output = render.execute(section, "repo", _doc('<execute variable-key="ver"><exe-cmd>ls</exe-cmd></execute>'))

        assert_equal(output, "version 2.60")

        # Blank lines at the end are how the output ends rather than part of it, but a run of them is not all dropped
        RESULT["output"] = "a\n\n\n"

        assert_equal(render.execute(section, "repo", _doc('<execute output="y"><exe-cmd>ls</exe-cmd></execute>'))[1], "a\n")
        assert_equal(render.manifest.var_store.get("ver"), "version 2.60")

        # A command that is expected to fail is showing the error, so the error is part of what it wrote
        RESULT["output"] = "out"
        RESULT["error"] = "bad thing"

        _, output = render.execute(section, "repo", _doc('<execute output="y" err-expect="1"><exe-cmd>ls</exe-cmd></execute>'))

        assert_equal(output, "outbad thing")


####################################################################################################################################
def test_execute_no_exe():
    """A build that does not run the commands still says what they are and shows something in place of what they wrote."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path, exe=False)
        section = render.section_map["/one"]

        cmd, output = render.execute(section, "repo", _doc('<execute output="y"><exe-cmd>ls</exe-cmd></execute>'))

        assert_equal(cmd, "ls")
        assert_equal(output, "Output suppressed for testing")

        # A command whose output the documentation does not show has nothing to show in place of it either
        assert_is_none(render.execute(section, "repo", _doc("<execute><exe-cmd>ls</exe-cmd></execute>"))[1])

        # A host is not started either, since there is nothing to run on it
        render.section_child_process(section, _doc('<host-add name="repo" image="image:1" user="v"/>'), 1)

        assert_equal(HOST_LIST, [])

        # A variable the documentation takes from a command still has to have a value
        render.execute(section, "repo", _doc('<execute variable-key="ver"><exe-cmd>ls</exe-cmd></execute>'))

        assert_equal(render.manifest.var_store.get("ver"), "[Test Variable]")


####################################################################################################################################
def test_execute_error():
    """A command that cannot be shown or cannot be run is reported rather than rendered."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        # A line that is too wide to read is an error, since the documentation is showing it to be read
        with assert_raises(ToolError) as raised:
            render.execute(section, "repo", _doc("<execute><exe-cmd>%s</exe-cmd></execute>" % ("x" * 100)))

        assert_in("command has a line > 80 characters", str(raised.exception))

        # A command may raise the limit for itself when it holds something that cannot be broken
        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        render.execute(section, "repo", _doc('<execute cmd-line-len="120"><exe-cmd>%s</exe-cmd></execute>' % ("x" * 100)))

        # A command against a host that was never started
        with assert_raises(ToolError) as raised:
            render.execute(section, "missing", _doc("<execute><exe-cmd>ls</exe-cmd></execute>"))

        assert_equal(str(raised.exception), "cannot execute on host missing because the host does not exist")


####################################################################################################################################
def test_execute_skip():
    """A command that is skipped is shown but not run, which is how a command that ran while the image was built is handled."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        cmd, output = render.execute(section, "repo", _doc('<execute skip="y"><exe-cmd>ls</exe-cmd></execute>'))

        assert_equal(cmd, "ls")
        assert_is_none(output)
        assert_equal(HOST_LIST, [])


####################################################################################################################################
def test_output_filter():
    """A command can write a great deal and the documentation is showing one thing in it."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")
        RESULT["output"] = (
            "\n".join("line %d" % idx for idx in range(20)) + "\nmatch\n" + "\n".join("tail %d" % idx for idx in range(20))
        )

        _, output = render.execute(
            section,
            "repo",
            _doc('<execute output="y"><exe-cmd>ls</exe-cmd><exe-highlight>match</exe-highlight></execute>'),
        )

        # What is kept is the lines that match with a little either side, and how many were dropped is said
        assert_in("[filtered 18 lines of output]", output)
        assert_in("line 18\nline 19\nmatch\ntail 0\ntail 1", output)
        assert_in("[filtered 17 lines of output]", output.split("tail 1")[1])


####################################################################################################################################
def test_output_filter_edge():
    """One dropped line takes as much room to say as to show, so it is shown."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")
        RESULT["output"] = "a\nb\nc\nmatch\nd\ne\nf"

        _, output = render.execute(
            section,
            "repo",
            _doc('<execute output="y"><exe-cmd>ls</exe-cmd><exe-highlight>match</exe-highlight></execute>'),
        )

        assert_equal(output, "a\nb\nc\nmatch\nd\ne")

        # A match at the very start, with nothing before it to drop
        RESULT["output"] = "match\na\nb"

        _, output = render.execute(
            section,
            "repo",
            _doc('<execute output="y"><exe-cmd>ls</exe-cmd><exe-highlight>match</exe-highlight></execute>'),
        )

        assert_equal(output, "match\na\nb")


####################################################################################################################################
def test_cache():
    """The cache is replayed in the order the commands were run, and a key that does not match means it is out of date."""

    with tempfile.TemporaryDirectory() as path:
        cache_list = [
            {
                "key": {
                    "host": "repo",
                    "cmd": ["ls -l"],
                    "output": False,
                    "run-as-user": None,
                    "load-env": True,
                    "bash-wrap": True,
                },
                "type": "exe",
                "value": {"output": ["one"]},
            }
        ]

        render = _execute(path, cache_list=cache_list)
        section = render.section_map["/one"]
        command = xml_node_child(xml_node_child(section, "execute-list"), "execute")

        cmd, output = render.execute(section, "repo", command)

        # Nothing was run, since what the command wrote is already known
        assert_equal(HOST_LIST, [])
        assert_equal(output, "one")

        # A cache that has run out no longer describes the document
        with assert_raises(CacheInvalidError) as raised:
            render.execute(section, "repo", command)

        assert_equal(str(raised.exception), "unable to get index from cache")


####################################################################################################################################
def test_cache_invalid():
    """A cache entry that does not describe what is about to happen means the whole document has to be built again."""

    with tempfile.TemporaryDirectory() as path:
        section_of = lambda render: render.section_map["/one"]

        render = _execute(path, cache_list=[{"key": {"host": "other"}, "type": "exe"}])
        command = xml_node_child(xml_node_child(section_of(render), "execute-list"), "execute")

        with assert_raises(CacheInvalidError) as raised:
            render.execute(section_of(render), "repo", command)

        assert_in("keys at index 0 do not match", str(raised.exception))

        render = _execute(path, cache_list=[{"key": {}, "type": "host"}])

        with assert_raises(CacheInvalidError) as raised:
            render.execute(section_of(render), "repo", command)

        assert_equal(str(raised.exception), "types do not match, cache is invalid")

        render = _execute(path, cache_list=[{"type": "exe"}])

        with assert_raises(CacheInvalidError) as raised:
            render.execute(section_of(render), "repo", command)

        assert_equal(str(raised.exception), "unable to get key or type from cache")


####################################################################################################################################
def test_cache_only():
    """A build that was told to use the cache and nothing else cannot run a command that is not in it."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path, cache_only=True)
        section = render.section_map["/one"]

        with assert_raises(ToolError) as raised:
            render.execute(section, "repo", xml_node_child(xml_node_child(section, "execute-list"), "execute"))

        assert_equal(str(raised.exception), "cache only operation forced by --cache-only option")


####################################################################################################################################
def test_cache_push():
    """A build that is replaying the cache has nothing to add to it."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path, cache_list=[])

        with assert_raises(ToolError) as raised:
            render.cache_push("exe", {}, None)

        assert_equal(str(raised.exception), "cache push should not be called when the cache is already present")


####################################################################################################################################
def test_backrest_config():
    """A configuration file is built up across a document, so a section shows the file with everything added so far and says what
    it changed."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        config = _doc(
            """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>Configure</title>
                <backrest-config-option section="global" key="repo1-path">/var/lib/pgbackrest</backrest-config-option>
                <backrest-config-option section="global" key="log-level-stderr">off</backrest-config-option>
                <backrest-config-option section="global" key="pg1-path" multi="y">/pg1</backrest-config-option>
                <backrest-config-option section="global" key="pg1-path" multi="y">/pg2</backrest-config-option>
            </backrest-config>"""
        )

        file, content, show = render.backrest_config(section, config, 1)

        assert_equal(file, "/etc/pgbackrest.conf")
        assert_true(show)

        # What a reader sees leaves out the options that are only there so the build can watch what happens, and a file the document
        # has not shown before is all new
        assert_equal(
            content,
            [
                ("add", "[global]"),
                ("add", "pg1-path=/pg1"),
                ("add", "pg1-path=/pg2"),
                ("add", "repo1-path=/var/lib/pgbackrest"),
            ],
        )

        # The file that is installed on the host has everything in it
        assert_in("log-level-stderr=off", file_read(os.path.join(path, "output/pgbackrest.conf")))

        # A later section adds to what is already there and only what it added is marked
        config = _doc(
            """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>More</title>
                <backrest-config-option section="demo" key="pg1-path">/pg</backrest-config-option>
            </backrest-config>"""
        )

        _, content, _ = render.backrest_config(section, config, 1)

        assert_equal(
            content,
            [
                ("add", "[demo]"),
                ("add", "pg1-path=/pg"),
                ("add", ""),
                ("same", "[global]"),
                ("same", "pg1-path=/pg1"),
                ("same", "pg1-path=/pg2"),
                ("same", "repo1-path=/var/lib/pgbackrest"),
            ],
        )

        # A file on another host is compared against that host rather than against the file of the same name on this one
        render.host_map["repo2"] = _Host("repo2", "doc-repo2", "image:1", "vagrant")

        _, content, _ = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo2" file="/etc/pgbackrest.conf"><title>Other</title>
                    <backrest-config-option section="demo" key="pg1-path">/pg</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        assert_equal(content, [("add", "[demo]"), ("add", "pg1-path=/pg")])


####################################################################################################################################
def test_backrest_config_remove():
    """An option that is removed leaves nothing behind for a later section to show, and a change that was not shown is marked on the
    next change that is."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="repo1-path">/var/lib</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        _, content, show = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf" show="n"><title>C</title>
                    <backrest-config-option section="global" key="repo1-path" remove="y"/>
                </backrest-config>"""
            ),
            1,
        )

        # A file with nothing left in it is all of what it held marked as gone
        assert_equal(content, [("remove", "[global]"), ("remove", "repo1-path=/var/lib")])
        assert_false(show)

        # A change the reader was not shown is left for the next change that is shown, so the option that was removed is marked here
        # rather than in the section that removed it
        _, content, show = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="repo1-path">/other</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        assert_true(show)
        assert_equal(content, [("same", "[global]"), ("remove", "repo1-path=/var/lib"), ("add", "repo1-path=/other")])

        # A configuration against a host that was never started
        with assert_raises(ToolError) as raised:
            render.backrest_config(
                section, _doc('<backrest-config host="missing" file="/etc/x"><title>C</title></backrest-config>'), 1
            )

        assert_equal(str(raised.exception), "cannot configure backrest on host missing because the host does not exist")


####################################################################################################################################
def test_postgres_config():
    """The PostgreSQL configuration is what the documentation added rather than the whole file."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        config = _doc(
            """<postgres-config host="repo" file="/pg/postgresql.conf"><title>Configure</title>
                <postgres-config-option key="archive_command">pgbackrest archive-push %p</postgres-config-option>
                <postgres-config-option key="archive_mode">on</postgres-config-option>
            </postgres-config>"""
        )

        file, content, show = render.postgres_config(section, config, 1)

        assert_equal(file, "/pg/postgresql.conf")
        assert_true(show)
        assert_equal(content, [("add", "archive_command = pgbackrest archive-push %p"), ("add", "archive_mode = on")])

        # The file installed on the host is what was there plus what the documentation added
        assert_in("shared_buffers = 128MB", file_read(os.path.join(path, "output/postgresql.conf")))
        assert_in("# pgBackRest Configuration", file_read(os.path.join(path, "output/postgresql.conf")))

        # An option that is changed is marked as gone and added back rather than as a line that is somehow both
        _, content, _ = render.postgres_config(
            section,
            _doc(
                """<postgres-config host="repo" file="/pg/postgresql.conf"><title>C</title>
                    <postgres-config-option key="archive_mode">off</postgres-config-option>
                </postgres-config>"""
            ),
            1,
        )

        assert_equal(
            content,
            [
                ("same", "archive_command = pgbackrest archive-push %p"),
                ("remove", "archive_mode = on"),
                ("add", "archive_mode = off"),
            ],
        )

        # An option that is reset leaves nothing behind
        _, content, _ = render.postgres_config(
            section,
            _doc(
                """<postgres-config host="repo" file="/pg/postgresql.conf"><title>C</title>
                    <postgres-config-option key="archive_command"/>
                    <postgres-config-option key="archive_mode"/>
                </postgres-config>"""
            ),
            1,
        )

        assert_equal(
            content,
            [("remove", "archive_command = pgbackrest archive-push %p"), ("remove", "archive_mode = off")],
        )

        with assert_raises(ToolError) as raised:
            render.postgres_config(
                section, _doc('<postgres-config host="missing" file="/pg/x"><title>C</title></postgres-config>'), 1
            )

        assert_equal(str(raised.exception), "cannot configure postgres on host missing because the host does not exist")


####################################################################################################################################
def test_config_no_exe():
    """A build that does not run the commands shows something in place of the configuration as well."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path, exe=False)
        section = render.section_map["/one"]

        _, content, _ = render.backrest_config(
            section, _doc('<backrest-config host="repo" file="/etc/x"><title>C</title></backrest-config>'), 1
        )

        assert_equal(content, [("same", "Config suppressed for testing")])

        _, content, _ = render.postgres_config(
            section, _doc('<postgres-config host="repo" file="/pg/x"><title>C</title></postgres-config>'), 1
        )

        assert_equal(content, [("same", "Config suppressed for testing")])


####################################################################################################################################
def test_config_key():
    """The key is everything that decides what a configuration change would be."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)

        key = render.config_key(
            _doc(
                """<backrest-config host="repo" file="/etc/x" reset="y"><title>C</title>
                    <backrest-config-option section="global" key="a">1</backrest-config-option>
                    <backrest-config-option section="global" key="b" remove="y"/>
                </backrest-config>"""
            )
        )

        assert_equal(key["host"], "repo")
        assert_true(key["reset"])
        assert_equal(key["option"], {"global": {"a": {"value": "1"}, "b": {"remove": True}}})

        # A PostgreSQL option has no section of its own, since the file has none
        key = render.config_key(
            _doc(
                '<postgres-config host="repo" file="/pg/x"><postgres-config-option key="a">1</postgres-config-option></postgres-config>'
            )
        )

        assert_equal(key["option"], {"a": {"value": "1"}})


####################################################################################################################################
def test_host_key():
    """The key is everything that decides what a host would be."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)

        assert_equal(
            render.host_key(_doc('<host-add name="repo" image="image:1" user="vagrant"/>')),
            {"name": "repo", "image": "image:1", "id": "repo", "update-hosts": True},
        )

        key = render.host_key(
            _doc('<host-add id="r" name="repo" image="image:1" user="v" option="-m 1g" param="run" os="u24" update-hosts="n"/>')
        )

        assert_equal(key["id"], "r")
        assert_equal(key["option"], "-m 1g")
        assert_equal(key["param"], "run")
        assert_equal(key["os"], "u24")
        assert_false(key["update-hosts"])


####################################################################################################################################
def test_host_add():
    """A host is started, told about the others, and set up by the commands the documentation gives it."""

    document = """<doc>
        <section id="one"><title>One</title>
            <host-add name="repo" image="pgbackrest/doc:u24" user="vagrant" mount="{[host-repo-path]}/x:/x" option="-v {[host-repo-path]}:/repo">
                <execute><exe-cmd>echo setup</exe-cmd></execute>
            </host-add>
        </section>
    </doc>"""

    with tempfile.TemporaryDirectory() as path:
        os.environ["HOME"] = path

        render = _execute(os.path.join(path, "doc"), document)
        section = render.section_map["/one"]

        render.section_child_process(section, xml_node_child(section, "host-add"), 1)

        # The repository is mounted from where it is on the host running the build
        assert_equal(HOST_LIST[0][0], "add")
        assert_equal(HOST_LIST[0][3], [os.path.join(path, "x:/x")])
        assert_equal(HOST_LIST[0][4], "-v %s:/repo" % path)

        assert_equal(HOST_LIST[1], ("group", "repo"))
        assert_equal(HOST_LIST[2][1], "echo setup")

        # The address of the host is a variable the documentation can use
        assert_equal(render.manifest.var_store.get("host-repo-ip"), "172.17.0.9")

        # A host that is already up cannot be started again
        with assert_raises(ToolError) as raised:
            render.section_child_process(section, xml_node_child(section, "host-add"), 1)

        assert_equal(str(raised.exception), "cannot add host repo because the host already exists")


####################################################################################################################################
def test_host_add_cache():
    """A host that is replayed from the cache is not started, since the address it had is what the documentation showed."""

    document = '<doc><section id="one"><title>One</title><host-add name="repo" image="image:1" user="v"/></section></doc>'

    with tempfile.TemporaryDirectory() as path:
        cache_list = [
            {
                "key": {"name": "repo", "image": "image:1", "id": "repo", "update-hosts": True},
                "type": "host",
                "value": {"ip": "172.17.0.5"},
            }
        ]

        render = _execute(path, document, cache_list=cache_list)
        section = render.section_map["/one"]

        render.section_child_process(section, xml_node_child(section, "host-add"), 1)

        assert_equal(HOST_LIST, [])
        assert_equal(render.manifest.var_store.get("host-repo-ip"), "172.17.0.5")

        # A title is the only other thing a section holds that every renderer handles the same way
        render.section_child_process(section, xml_node_child(section, "title"), 1)

        with assert_raises(ToolError) as raised:
            render.section_child_process(section, _doc("<unknown/>"), 1)

        assert_equal(str(raised.exception), "unable to process child type unknown")


####################################################################################################################################
def test_host_add_pre():
    """Commands marked pre are run while the image is built, so a rebuild of the documentation does not run them again."""

    document = """<doc>
        <section id="one"><title>One</title>
            <host-add name="repo" image="pgbackrest/doc:u24" user="vagrant"/>

            <execute-list host="repo"><title>Setup</title>
                <execute pre="y"><exe-cmd>apt-get update</exe-cmd></execute>
                <execute pre="y" bash-wrap="n"><exe-cmd>true</exe-cmd></execute>
            </execute-list>
        </section>
    </doc>"""

    build_list = []

    def _exec_result(command, **kwargs):
        build_list.append(command)

        return 0, "", ""

    exec_result_real = execute_module.exec_result
    execute_module.exec_result = _exec_result

    try:
        with tempfile.TemporaryDirectory() as path:
            os.environ["HOME"] = path

            render = _execute(os.path.join(path, "doc"), document, pre=True)
            section = render.section_map["/one"]

            render.section_child_process(section, xml_node_child(section, "host-add"), 1)

            # The image the host starts from is the one the pre commands were run into
            assert_equal(HOST_LIST[0][2], "pgbackrest/doc:u24-repo")

            script = file_read(os.path.join(path, "doc/output/doc-host.dockerfile"))

            assert_in("FROM pgbackrest/doc:u24", script)
            assert_in("RUN sudo -u vagrant bash -l -c 'apt-get update'", script)
            assert_in("RUN sudo -u vagrant true", script)
            assert_in("docker build -f", build_list[-1])
    finally:
        execute_module.exec_result = exec_result_real


####################################################################################################################################
def test_config_cache():
    """A configuration that is replayed from the cache is not applied, since the file it left behind is already known."""

    with tempfile.TemporaryDirectory() as path:
        cache_list = [
            {"key": {"host": "repo", "file": "/etc/x", "option": {}}, "type": "cfg-pgbackrest", "value": {"config": ["[global]"]}},
            {"key": {"host": "repo", "file": "/pg/x", "option": {}}, "type": "cfg-postgresql", "value": {"config": None}},
        ]

        render = _execute(path, cache_list=cache_list)
        section = render.section_map["/one"]

        _, content, _ = render.backrest_config(
            section, _doc('<backrest-config host="repo" file="/etc/x"><title>C</title></backrest-config>'), 1
        )

        assert_equal(content, [("add", "[global]")])

        _, content, _ = render.postgres_config(
            section, _doc('<postgres-config host="repo" file="/pg/x"><title>C</title></postgres-config>'), 1
        )

        assert_equal(content, [])
        assert_equal(HOST_LIST, [])


####################################################################################################################################
def test_backrest_config_reset():
    """A configuration that is reset starts from nothing rather than from what an earlier section left, so what is shown of it is
    all new rather than mostly gone."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="repo1-path">/var/lib</backrest-config-option>
                    <backrest-config-option section="demo" key="pg1-path">/pg</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        _, content, _ = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf" reset="y"><title>C</title>
                    <backrest-config-option section="global" key="repo1-path">/other</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        assert_equal(content, [("add", "[global]"), ("add", "repo1-path=/other")])


####################################################################################################################################
def test_backrest_config_hidden():
    """A file that holds nothing but the options the build watches with has nothing for a reader to see."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        _, content, _ = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="log-level-stderr">off</backrest-config-option>
                    <backrest-config-option section="global" key="log-timestamp">n</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        assert_equal(content, [])


####################################################################################################################################
def test_backrest_config_remove_one():
    """An option that is removed leaves the rest of its section behind."""

    with tempfile.TemporaryDirectory() as path:
        render = _execute(path)
        section = render.section_map["/one"]

        render.host_map["repo"] = _Host("repo", "doc-repo", "image:1", "vagrant")

        render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="a">1</backrest-config-option>
                    <backrest-config-option section="global" key="b">2</backrest-config-option>
                </backrest-config>"""
            ),
            1,
        )

        _, content, _ = render.backrest_config(
            section,
            _doc(
                """<backrest-config host="repo" file="/etc/pgbackrest.conf"><title>C</title>
                    <backrest-config-option section="global" key="a" remove="y"/>
                </backrest-config>"""
            ),
            1,
        )

        assert_equal(content, [("same", "[global]"), ("remove", "a=1"), ("same", "b=2")])
