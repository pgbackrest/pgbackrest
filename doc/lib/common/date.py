"""Date.

A date is written the way it sorts, i.e. YYYY-MM-DD, so that the documentation reads in order wherever it is dated, and it is shown
the way it reads."""

####################################################################################################################################
_MONTH_LIST = (
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
)


####################################################################################################################################
def date_render(date):
    """Render a date written as it sorts as the month, the day, and the year."""

    return "%s %d, %s" % (_MONTH_LIST[int(date[5:7]) - 1], int(date[8:10]), date[0:4])
