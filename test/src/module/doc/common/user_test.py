"""Test User and Group."""

####################################################################################################################################
import grp
import os
import pwd

from harness.test import *

from common.user import *


####################################################################################################################################
def test_user():
    """The user and group are whoever the tests are running as."""

    assert_equal(user_id(), os.getuid())
    assert_equal(user_name(), pwd.getpwuid(os.getuid()).pw_name)

    assert_equal(group_id(), os.getgid())
    assert_equal(group_name(), grp.getgrgid(os.getgid()).gr_name)
