from talp_pages.cli.metadata import parse_date


def test_parse_date_gitlab():
    example_string = "2022-01-31T16:47:55Z"
    date = parse_date(example_string)
    assert date.year == 2022
    assert date.month == 1
    assert date.day == 31
    assert date.hour == 16
