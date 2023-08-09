import argparse
import sys
from pathlib import Path

LINE_STRING_TEMPLATE = 'const char {var_name}[] PROGMEM = R"rawliteral('
LINE_STRING_END = ')rawliteral";'


def apply_sync(target: Path, source: Path, var_name: str):
    lookup_line = LINE_STRING_TEMPLATE.format(var_name=var_name)
    target_content = target.read_text().splitlines()
    lookup_index_beg = target_content.index(lookup_line)
    lookup_index_end = target_content.index(LINE_STRING_END, lookup_index_beg)
    target.write_text(
        "\n".join(
            [
                *target_content[: lookup_index_beg + 1],
                source.read_text(),
                *target_content[lookup_index_end:],
            ]
        )
    )


def apply_cls(target: Path, var_name: str):
    lookup_line = LINE_STRING_TEMPLATE.format(var_name=var_name)
    target_content = target.read_text().splitlines()
    lookup_index_beg = target_content.index(lookup_line)
    lookup_index_end = target_content.index(LINE_STRING_END, lookup_index_beg)
    target.write_text(
        "\n".join(
            [
                *target_content[: lookup_index_beg + 1],
                *target_content[lookup_index_end:],
            ]
        )
    )


def sync_option(sync_data: str):
    path_str, var_name = sync_data.split(":")
    path_obj = Path(path_str)
    assert path_obj.exists()
    assert path_obj.is_file()
    return path_obj, var_name


def args_parse(args):
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("action", choices=["sync", "cls"])
    arg_parser.add_argument("target", type=Path)
    arg_parser.add_argument(
        "-w",
        "--wach",
        action="store_true",
        default=False,
    )
    arg_parser.add_argument(
        "-i",
        "--item",
        action="append",
        dest="sync_options",
        type=sync_option,
        required=True,
    )

    return arg_parser.parse_args(args)


def cli(args):
    options = args_parse(args)
    target: Path = options.target
    print(f"Apply changes on {target.absolute()}")
    for source, var_name in options.sync_options:
        if options.action == "cls":
            apply_cls(target, var_name)
        if options.action == "sync":
            apply_sync(target, source, var_name)


if __name__ == "__main__":
    cli(sys.argv[1:])
