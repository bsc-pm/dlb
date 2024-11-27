import talp_pages.cli.metadata as m
import pytest


class FakeArgs:
    pass


def test_main_emtpy_args():
    with pytest.raises(AttributeError):
        m.main({})


def test_main_path():
    args = FakeArgs()
    args.log_level = "DEBUG"
    # m.main(args)
