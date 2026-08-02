"""Test Date.

A date is written the way it sorts and shown the way it reads, so what is checked is the reading of it."""

####################################################################################################################################
from harness.test import *

from common.date import *


####################################################################################################################################
def test_date_render():
    """A date written as it sorts is rendered as the month, the day, and the year."""

    assert_equal(date_render("2026-07-20"), "July 20, 2026")

    # A day is rendered without the zero it is padded with to sort, and every month is named
    assert_equal(date_render("2026-05-04"), "May 4, 2026")
    assert_equal(date_render("2015-01-31"), "January 31, 2015")
    assert_equal(date_render("2015-12-01"), "December 1, 2015")
