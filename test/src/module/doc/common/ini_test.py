"""Test Ini Rendering."""

####################################################################################################################################
from harness.test import *

from common.ini import *


####################################################################################################################################
def test_ini_render():
    """Sections and their options are written in the order a reader would look for them."""

    assert_equal(
        ini_render({"global": {"repo1-path": "/var/lib/pgbackrest", "log-level-file": "detail"}, "demo": {"pg1-path": "/pg"}}),
        "[demo]\npg1-path=/pg\n\n[global]\nlog-level-file=detail\nrepo1-path=/var/lib/pgbackrest\n",
    )

    # Nothing to write is nothing rather than an empty section
    assert_equal(ini_render({}), "")


####################################################################################################################################
def test_ini_render_multi():
    """An option that may be given more than once is written once per value."""

    assert_equal(
        ini_render({"global": {"repo1-host": "repo", "pg1-path": ["/pg1", "/pg2"]}}),
        "[global]\npg1-path=/pg1\npg1-path=/pg2\nrepo1-host=repo\n",
    )
