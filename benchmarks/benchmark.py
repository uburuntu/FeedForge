#!/usr/bin/env python3
"""Validate, collect, and compare FeedForge benchmark contract evidence."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import io
import json
import math
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shlex
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable, Sequence


CONTRACT_VERSION = "2.0.0"
RESULT_SCHEMA_VERSION = 1
SERIES_SCHEMA_VERSION = 1
QUALIFIED_REPEATS = 7
QUALIFIED_SAMPLES = 15
QUALIFIED_WARMUP = 5
QUALIFIED_BATCH = 256
QUALIFIED_MIN_TIME_MS = 50.0
QUALIFIED_COOLDOWN_SECONDS = 120
FROZEN_CORRECTNESS_COMMAND = ["make", "bench-correctness"]
CORRECTNESS_ENVIRONMENT_BLOCKLIST = frozenset(
    {
        "BUILD_ARGS",
        "CMAKE",
        "CMAKE_ARGS",
        "CMAKE_BUILD",
        "CTEST",
        "CTEST_ARGS",
        "GENERATE_PRESET",
        "GIT",
        "GNUMAKEFLAGS",
        "MAKE",
        "MAKEFILES",
        "MAKEFLAGS",
        "MAKELEVEL",
        "MAKEOVERRIDES",
        "MFLAGS",
        "PARALLEL",
        "PRESET",
        "PYTHON",
        "SHELL",
    }
)
MIN_MEDIAN_IMPROVEMENT = 0.05
MAX_CROSS_RUN_NORMALIZED_MAD = 0.03
MIN_ROBUST_MARGIN = 0.03
MAX_UNTARGETED_REGRESSION = 0.02
MAD_SCALE = 1.4826
CORPUS_SHA256 = "1737425a359d1759ec010dd56a2e12e920e34c028820c4301bad7d75fa839bd0"
CORRECTNESS = {
    "checksum": "ff938ee3464956dde7deb4940a9caf93bfa9c6951f85a687b7059fad8311a583",
    "chunked_replay_cases": 8,
    "fixture_decodes": 46,
    "sink_order_verified": True,
    "strict_replay": True,
    "verified": True,
}
PIPELINE_FINGERPRINTS = {
    "itch50_all": "42f71275830db9a05233c775df3b25b889a5fb13fc72485fa7819201b6a9c5ca",
    "itch50_order_events": "5091875ac081f047b55ccf8c8231b7aca268e4163d28e59956685944b1403ec1",
}
SCHEMA_FINGERPRINT = "5caf2a24f113157cc5e74069339801fd332582dea234e66dd34148a8f12b938a"
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
REVISION_RE = re.compile(r"[0-9a-f]{40}\Z")
LABEL_RE = re.compile(r"[A-Za-z0-9._-]+\Z")
CHECKSUM_RE = re.compile(r"0x[0-9a-f]+\Z")
UTC_RE = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\Z")
RUN_KEYS = {
    "benchmarks", "build", "command", "config", "contract_version", "corpus",
    "correctness", "host", "publishable", "schema_version", "timestamp_utc",
    "warnings",
}
BUILD_KEYS = {
    "build_type", "compiler_builtin", "compiler_id", "compiler_path",
    "compiler_version", "config_flags", "cxx_standard", "feedforge_version",
    "generator", "interprocedural_optimization", "pipeline_fingerprints",
    "schema_fingerprint", "source_dirty", "source_revision", "target_flags",
}
CONFIG_KEYS = {
    "batch", "clock", "clock_is_steady", "minimum_time_ms", "samples", "smoke",
    "timer_resolution_ns", "warmup",
}
CORPUS_KEYS = {"fixture_count", "fixtures", "sha256", "source"}
FIXTURE_KEYS = {
    "byte_source", "file", "message_name", "message_type",
    "order_events_selected", "review_status", "reviewer", "sha256", "size",
}
HOST_KEYS = {
    "architecture", "cpu_affinity", "cpu_governor", "cpu_model", "kernel",
    "limitations", "logical_cpus", "machine_model", "memory_bytes", "os",
    "physical_cpus", "turbo_state",
}
COMMAND_KEYS = {"argv", "joined", "working_directory"}
SERIES_KEYS = {
    "benchmarks", "command", "command_output_directory", "comparison_ready", "contract_version",
    "cooldown_seconds", "correctness", "diagnostic_only", "executable",
    "executable_sha256", "executable_sha256_after", "executable_sha256_before",
    "identity", "label", "qualification", "repeat_count", "run_files",
    "schema_version", "source_id", "source_root", "thresholds", "timestamp_utc",
    "warnings",
}
IDENTITY_KEYS = {
    "benchmarks", "build", "config", "contract_version", "corpus_sha256",
    "correctness", "host", "schema_version",
}
IDENTITY_CONFIG_KEYS = {
    "batch", "clock", "clock_is_steady", "minimum_time_ms", "samples", "smoke",
    "warmup",
}
FROZEN_REVIEWER = "independent-line-by-line-protocol-review"
FROZEN_REVIEW_STATUS = "approved"
FROZEN_BYTE_SOURCE = "hand-authored from the cited official field table; not schema-generated"
FROZEN_FIXTURES = (
    ("01_system_event.toml", "system_event", "S", False, "4518bcf87627f16ecd98ba40b4839be8f23e55df95b79fd97bb7c59c99f25ced", 12),
    ("02_stock_directory.toml", "stock_directory", "R", False, "f3220f7e0e59bc7b5c7acc451d986bc9359b08db605a807d7043cb1f3e37b182", 39),
    ("03_stock_trading_action.toml", "stock_trading_action", "H", False, "0a34e58508645a63fa737996259842ef14e5152fadd26a38165fe41710bbe2db", 25),
    ("04_reg_sho_restriction.toml", "reg_sho_restriction", "Y", False, "8a5a3a23f33734af03481f35eb72ef33e67e554c910e30be45cbfaafecc66001", 20),
    ("05_market_participant_position.toml", "market_participant_position", "L", False, "c957f930baf7436fec759e8d07af19dd9b90f3edb4aa76ab2ce06502c9a59b63", 26),
    ("06_mwcb_decline_level.toml", "mwcb_decline_level", "V", False, "4b7be692ce5c44added70d3f4b5b399fbe70259c37c6fe095176632dcf035c99", 35),
    ("07_mwcb_status.toml", "mwcb_status", "W", False, "e934ec3d72a69db9fd30d37a6762b1a39aae92e8977451f56c90863fb70321d6", 12),
    ("08_ipo_quoting_period_update.toml", "ipo_quoting_period_update", "K", False, "1393a91cb0cd826845b3cd62f3cefc1a2c8666823d56239e81c52730e64e787e", 28),
    ("09_luld_auction_collar.toml", "luld_auction_collar", "J", False, "fcbf7aac3d7e13395e657937e94623054a705b8a0639d6bd6f10ba06254a9b2a", 35),
    ("10_operational_halt.toml", "operational_halt", "h", False, "9276cb5ca72437ecc0383077a0671ea035ab7ae19c46344b8ae45e3350288f93", 21),
    ("11_add_order.toml", "add_order", "A", True, "c570f28d4a11703971c42c430f5601f57d2beccd48edd91c6ce795d054a61346", 36),
    ("12_add_order_mpid.toml", "add_order_mpid", "F", True, "1d9aec218b29b59155ed1b2d037129837f699d65cc1a51b1726906bceb45bef5", 40),
    ("13_order_executed.toml", "order_executed", "E", True, "872809dd86966116e1a55880d6b94d0d96c29ab0e5329512d33712c46224b1c7", 31),
    ("14_order_executed_with_price.toml", "order_executed_with_price", "C", True, "4031eeca1bb2f8e9ae3f74a49ccc13a7cbb775d83338d15897548bbb176bccf6", 36),
    ("15_order_cancel.toml", "order_cancel", "X", True, "de401784533ea228602b2b3ca8d515e3141f2b07a6cd4983f4c5c49df45b574c", 23),
    ("16_order_delete.toml", "order_delete", "D", True, "1d6d0020bbfc468dea4ee0bd6b243d91e4231c3e6773294f171bc1ab4d2593ea", 19),
    ("17_order_replace.toml", "order_replace", "U", True, "ea62148c2591932844ecedd3d85f61cefe0afcae4d328ecc252726d5a9a1765a", 35),
    ("18_trade.toml", "trade", "P", True, "d1546edc619ae239974736264684fbbef8474948d2eaf00a2c0b9853014053a7", 44),
    ("19_cross_trade.toml", "cross_trade", "Q", False, "b18502cee513cd26336ebc9a0994d8259df9705b088ab7612ec5aa799ec41d3e", 40),
    ("20_broken_trade.toml", "broken_trade", "B", False, "4dffefbd1c977615b47f7392e38539580468bcc64128784457cbfa418c610c3c", 19),
    ("21_net_order_imbalance_indicator.toml", "net_order_imbalance_indicator", "I", False, "92dd216354057b0417148e15ed84a8bb8829b497aea5ab328f6ff79ea10aa0e4", 50),
    ("22_retail_price_improvement_indicator.toml", "retail_price_improvement_indicator", "N", False, "7224c96373031b8f1bb8f3d205ea4c13570f159528952776c4ff0da57a3722cf", 20),
    ("23_direct_listing_with_capital_raise.toml", "direct_listing_with_capital_raise", "O", False, "fdf315dcd2fc4558ea1e0936139726485f9419e9571a419eceed4aff03910352", 48),
)


def _case(
    identifier: str,
    operation: str,
    pipeline: str,
    schedule: str | None,
    schedule_sha256: str | None,
    workload: str,
    workload_sha256: str,
    bytes_per_round: int,
    messages_per_round: int,
    events_per_round: int,
    pushes_per_round: int,
    finish_calls_per_round: int,
) -> dict[str, Any]:
    return {
        "bytes_per_round": bytes_per_round,
        "events_per_round": events_per_round,
        "finish_calls_per_round": finish_calls_per_round,
        "id": identifier,
        "messages_per_round": messages_per_round,
        "operation": operation,
        "pipeline": pipeline,
        "pushes_per_round": pushes_per_round,
        "schedule": schedule,
        "schedule_sha256": schedule_sha256,
        "workload": workload,
        "workload_sha256": workload_sha256,
    }


ALL_DECODE = "be2e5b33f12ce0bc3a28d1181c5de7023c29121ec0350a05538474f1000b2b8f"
ALL_REPLAY = "80e748f580a51f3a671229787b340bc3d512a3e2587723e92a1a76f72fa9ad03"
SELECTED_DECODE = "7769824c046be46900b67ae9903ddabdaec43a4c05e6e12d097f0c8b8d3d0951"
SELECTED_REPLAY = "a533c7046b5ac5c06be48b3620ce51859c65173b838eb133f904871f7dab31a1"
UNSELECTED_DECODE = "b73ed867fa094dcbb196620a915426a9584d470fa2c175d4f8d49a5fc3936705"
UNSELECTED_REPLAY = "7019f05855331f57352d859c34fbd1b046bbe047652d2c8cfa62701ebfeb62a8"
MIXED_DECODE = "bc99dee6ed0216ff3fa36cd6c0d194b5dfe3ce99764b38d35db231347f77d6be"
FRAME_ALL = "e16b5b699e985c2dd9916830120cb2974d9edd646b33c724ac24e49b70e405a2"
BYTE_ALL = "f8c85fa1537c901f33832b51b118a62b54fe9083b582f2ca6782d20992b0291a"
FRAME_SELECTED = "4670b435e031d0860a7015cc87535c4b2abf8e5cc2911527c3cf636782b3a6e4"
BYTE_SELECTED = "fd2182be180800be95aa07f898bafacc7f015294956251bd0e21f3d7f6da2cd6"
FRAME_UNSELECTED = "bed1b9d35994d9f6125c47509a134ff110f201d9439546cad9a1c0c9e0d5b42d"
BYTE_UNSELECTED = "3ca01326df49e2161991b3edb812c10ba8784e8c7c0199fc09dba34a331671e2"


FROZEN_CASES = (
    _case("decode_one/itch50_all/all_types", "decode_one", "itch50_all", None, None,
          "all_types", ALL_DECODE, 694, 23, 23, 0, 0),
    _case("replay_binary_file/itch50_all/all_types", "replay_binary_file",
          "itch50_all", None, None, "all_types", ALL_REPLAY, 742, 23, 23, 0, 0),
    _case("decode_one/itch50_order_events/selected", "decode_one",
          "itch50_order_events", None, None, "selected", SELECTED_DECODE,
          264, 8, 8, 0, 0),
    _case("replay_binary_file/itch50_order_events/selected", "replay_binary_file",
          "itch50_order_events", None, None, "selected", SELECTED_REPLAY,
          282, 8, 8, 0, 0),
    _case("decode_one/itch50_order_events/unselected", "decode_one",
          "itch50_order_events", None, None, "unselected", UNSELECTED_DECODE,
          430, 15, 0, 0, 0),
    _case("replay_binary_file/itch50_order_events/unselected", "replay_binary_file",
          "itch50_order_events", None, None, "unselected", UNSELECTED_REPLAY,
          462, 15, 0, 0, 0),
    _case("decode_one/itch50_order_events/mixed", "decode_one",
          "itch50_order_events", None, None, "mixed", MIXED_DECODE,
          694, 23, 8, 0, 0),
    _case("replay_binary_file/itch50_order_events/mixed", "replay_binary_file",
          "itch50_order_events", None, None, "mixed", ALL_REPLAY,
          742, 23, 8, 0, 0),
    _case("chunked_replay/frame_aligned/itch50_all/all_types", "chunked_replay",
          "itch50_all", "frame_aligned", FRAME_ALL, "all_types", ALL_REPLAY,
          742, 23, 23, 24, 1),
    _case("chunked_replay/one_byte/itch50_all/all_types", "chunked_replay",
          "itch50_all", "one_byte", BYTE_ALL, "all_types", ALL_REPLAY,
          742, 23, 23, 742, 1),
    _case("chunked_replay/frame_aligned/itch50_order_events/selected",
          "chunked_replay", "itch50_order_events", "frame_aligned",
          FRAME_SELECTED, "selected", SELECTED_REPLAY, 282, 8, 8, 9, 1),
    _case("chunked_replay/one_byte/itch50_order_events/selected",
          "chunked_replay", "itch50_order_events", "one_byte", BYTE_SELECTED,
          "selected", SELECTED_REPLAY, 282, 8, 8, 282, 1),
    _case("chunked_replay/frame_aligned/itch50_order_events/unselected",
          "chunked_replay", "itch50_order_events", "frame_aligned",
          FRAME_UNSELECTED, "unselected", UNSELECTED_REPLAY, 462, 15, 0, 16, 1),
    _case("chunked_replay/one_byte/itch50_order_events/unselected",
          "chunked_replay", "itch50_order_events", "one_byte", BYTE_UNSELECTED,
          "unselected", UNSELECTED_REPLAY, 462, 15, 0, 462, 1),
    _case("chunked_replay/frame_aligned/itch50_order_events/mixed",
          "chunked_replay", "itch50_order_events", "frame_aligned", FRAME_ALL,
          "mixed", ALL_REPLAY, 742, 23, 8, 24, 1),
    _case("chunked_replay/one_byte/itch50_order_events/mixed", "chunked_replay",
          "itch50_order_events", "one_byte", BYTE_ALL, "mixed", ALL_REPLAY,
          742, 23, 8, 742, 1),
)


RAW_CSV_FIELDS = (
    "schema_version", "contract_version", "timestamp_utc", "benchmark_id",
    "operation", "pipeline", "schedule", "schedule_sha256", "workload",
    "workload_sha256", "corpus_sha256", "bytes_per_round",
    "messages_per_round", "events_per_round", "pushes_per_round",
    "finish_calls_per_round", "rounds_per_sample", "samples", "warmup",
    "minimum_time_ms", "median_ns_per_message", "p05_ns_per_message",
    "p95_ns_per_message", "mad_ns_per_message", "median_ns_per_event",
    "median_ns_per_push", "median_bytes_per_second",
    "median_messages_per_second", "median_events_per_second", "relative_mad",
    "relative_p95_p05_spread", "noisy", "implausible",
    "anti_elision_checksum", "feedforge_version", "source_revision",
    "source_dirty", "build_type", "compiler_id", "compiler_version",
    "compiler_path", "config_flags", "target_flags", "os", "kernel",
    "architecture", "cpu_model", "machine_model", "logical_cpus",
    "physical_cpus", "memory_bytes", "cpu_affinity", "cpu_governor",
    "turbo_state", "correctness_checksum", "command",
)


def percentile(values: Iterable[float], quantile: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot summarize an empty sequence")
    if any(not math.isfinite(value) for value in ordered):
        raise ValueError("cannot summarize non-finite values")
    position = quantile * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    fraction = position - lower
    return ordered[lower] + (ordered[upper] - ordered[lower]) * fraction


def distribution(values: Iterable[float]) -> dict[str, float]:
    ordered = list(values)
    median = percentile(ordered, 0.5)
    deviations = [abs(value - median) for value in ordered]
    return {
        "mad": percentile(deviations, 0.5),
        "maximum": max(ordered),
        "median": median,
        "minimum": min(ordered),
        "p05": percentile(ordered, 0.05),
        "p95": percentile(ordered, 0.95),
    }


def atomic_text(path: Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent, text=True
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write(contents)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def canonical_json(path: Path, value: Any) -> None:
    atomic_text(
        path,
        json.dumps(value, ensure_ascii=False, allow_nan=False,
                   separators=(",", ":"), sort_keys=True) + "\n",
    )


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON number: {value}")


def _reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, Any]:
    if path.is_symlink():
        raise ValueError(f"{path}: symlinked JSON evidence is forbidden")
    with path.open("r", encoding="utf-8") as source:
        value = json.load(
            source,
            parse_constant=_reject_json_constant,
            object_pairs_hook=_reject_duplicate_json_keys,
        )
    if not isinstance(value, dict):
        raise ValueError(f"{path} does not contain a JSON object")
    return value


def sha256_file(path: Path) -> str:
    if path.is_symlink():
        raise ValueError(f"{path}: symlinked evidence is forbidden")
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _integer(value: Any, context: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ValueError(f"{context}: expected an integer >= {minimum}")
    return value


def _number(value: Any, context: str, *, positive: bool = False) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{context}: expected a finite number")
    result = float(value)
    if not math.isfinite(result) or (positive and result <= 0.0):
        raise ValueError(f"{context}: expected a finite positive number")
    return result


def _boolean(value: Any, context: str) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{context}: expected a boolean")
    return value


def _mapping(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{context}: expected an object")
    return value


def _sequence(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValueError(f"{context}: expected an array")
    return value


def _sha256(value: Any, context: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise ValueError(f"{context}: expected a lowercase SHA-256")
    return value


def _revision(value: Any, context: str) -> str:
    if not isinstance(value, str) or not REVISION_RE.fullmatch(value):
        raise ValueError(f"{context}: expected a lowercase 40-character Git SHA")
    return value


def _close(actual: Any, expected: float, context: str) -> None:
    value = _number(actual, context)
    if not math.isclose(value, expected, rel_tol=2e-12, abs_tol=1e-9):
        raise ValueError(f"{context}: {value!r} does not match derived {expected!r}")


def _safe_relative(
    value: Any, context: str, *, allow_current_directory: bool = False
) -> str:
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError(f"{context}: expected a nonempty POSIX relative path")
    path = Path(value)
    windows_path = PureWindowsPath(value)
    if (
        path.is_absolute()
        or windows_path.is_absolute()
        or windows_path.drive
        or ".." in path.parts
        or (path.as_posix() == "." and not allow_current_directory)
    ):
        raise ValueError(f"{context}: path must remain relative and contained")
    return path.as_posix()


def join_command(arguments: Sequence[str]) -> str:
    simple = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./:-=")
    output = []
    for argument in arguments:
        if argument and all(character in simple for character in argument):
            output.append(argument)
        else:
            output.append("'" + argument.replace("'", "'\\''") + "'")
    return " ".join(output)


def _expected_benchmark_base(
    executable: str, config: dict[str, Any]
) -> list[str]:
    if config["smoke"]:
        if (
            config["samples"] != 3
            or config["warmup"] != 1
            or config["batch"] != 1
            or config["minimum_time_ms"] != 2
        ):
            raise ValueError("smoke argv does not match the frozen smoke config")
        return [executable, "--smoke"]
    return [
        executable,
        "--samples",
        str(config["samples"]),
        "--warmup",
        str(config["warmup"]),
        "--batch",
        str(config["batch"]),
        "--min-time-ms",
        format(config["minimum_time_ms"], "g"),
    ]


def _parse_benchmark_argv(
    arguments: Sequence[str], config: dict[str, Any], context: str
) -> dict[str, Any]:
    if not arguments or any(not isinstance(item, str) or not item for item in arguments):
        raise ValueError(f"{context}: command argv is empty or malformed")
    executable = _safe_relative(arguments[0], f"{context}: argv[0]")
    value_options = {
        "--samples", "--warmup", "--batch", "--min-time-ms", "--json", "--csv"
    }
    seen: dict[str, str | bool] = {}
    index = 1
    while index < len(arguments):
        option = arguments[index]
        if option in seen:
            raise ValueError(f"{context}: duplicate benchmark option {option}")
        if option == "--smoke":
            seen[option] = True
            index += 1
            continue
        if option not in value_options:
            raise ValueError(f"{context}: unknown benchmark option {option}")
        if index + 1 >= len(arguments):
            raise ValueError(f"{context}: {option} requires a value")
        seen[option] = arguments[index + 1]
        index += 2

    expected_options = (
        {"--smoke", "--json", "--csv"}
        if config["smoke"]
        else value_options
    )
    if set(seen) != expected_options:
        raise ValueError(f"{context}: benchmark option set does not match config")
    json_path = _safe_relative(seen["--json"], f"{context}: --json")
    csv_path = _safe_relative(seen["--csv"], f"{context}: --csv")
    json_pure = PurePosixPath(json_path)
    csv_pure = PurePosixPath(csv_path)
    if (
        json_pure.suffix != ".json"
        or csv_pure.suffix != ".csv"
        or json_pure.parent != csv_pure.parent
        or json_pure.stem != csv_pure.stem
    ):
        raise ValueError(f"{context}: JSON and CSV command paths do not form one run pair")
    base = _expected_benchmark_base(executable, config)
    expected = base + ["--json", json_path, "--csv", csv_path]
    if list(arguments) != expected:
        raise ValueError(f"{context}: benchmark argv is not in frozen canonical order")
    return {
        "base": base,
        "csv": csv_path,
        "json": json_path,
        "output_directory": json_pure.parent.as_posix(),
    }


def _utc_timestamp(value: Any, context: str) -> str:
    if not isinstance(value, str) or not UTC_RE.fullmatch(value):
        raise ValueError(f"{context}: expected RFC3339 UTC seconds ending in Z")
    try:
        dt.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise ValueError(f"{context}: invalid RFC3339 UTC timestamp") from error
    return value


def _relative_path(path: Path, root: Path, context: str) -> str:
    try:
        relative = path.resolve().relative_to(root.resolve())
    except ValueError as error:
        raise ValueError(f"{context} must be inside the repository root") from error
    return _safe_relative(relative.as_posix(), context)


def _forbidden_isa_flags(build: dict[str, Any]) -> list[str]:
    text = f"{build.get('config_flags', '')} {build.get('target_flags', '')}"
    try:
        tokens = shlex.split(text)
    except ValueError:
        tokens = text.split()
    prefixes = (
        "-march", "-mcpu", "-mtune", "-mattr", "-target-cpu",
        "-target-feature", "--target-feature", "/arch:", "/favor:",
        "-xarch", "-xhost", "-qarch", "-qtune", "-tp=",
    )
    feature_re = re.compile(
        r"-m(?:no-)?(?:3dnow|adx|aes|altivec|avx|bf16|bmi|cldemote|clflush|"
        r"clwb|crc|crypto|cx16|dotprod|f16c|fma|fp16|fsgsbase|gfni|hle|i8mm|"
        r"lwp|lzcnt|mmx|movbe|movdir|mpx|neon|outline-atomics|pclmul|pconfig|"
        r"pku|popcnt|power|prefetch|prfchw|ptwrite|ras|rcpc|rdpid|rdrnd|rdseed|"
        r"rtm|rvv|serialize|sgx|sha|shstk|simd|sse|sve|tbm|tsx|uintr|vaes|"
        r"vector|vpclmul|vsx|waitpkg|wbnoinvd|xop|xsave|zvector)(?:[0-9._=+-].*)?\Z",
        re.IGNORECASE,
    )
    rejected: list[str] = []
    lowered = [token.lower() for token in tokens]
    for index, token in enumerate(lowered):
        if token.startswith(prefixes) or feature_re.fullmatch(token):
            rejected.append(tokens[index])
        if (
            token.startswith(("-flto", "-qipa", "-xipo"))
            or (token.startswith("-ipo") and token not in {"-ipo-", "-no-ipo"})
            or token == "/gl"
            or (token.startswith("/ltcg") and token != "/ltcg:off")
            or (token.startswith("-wl,") and ("-flto" in token or "-plugin-opt" in token))
        ):
            rejected.append(tokens[index])
        if token in {"-mllvm", "-xclang"} and index + 1 < len(tokens):
            following = lowered[index + 1]
            if any(word in following for word in ("target-cpu", "target-feature", "mattr")):
                rejected.extend(tokens[index:index + 2])
        if token.startswith("-wa,") and any(
            word in token for word in ("march", "mcpu", "mtune")
        ):
            rejected.append(tokens[index])
    return list(dict.fromkeys(rejected))


def _qualification_config(config: dict[str, Any]) -> bool:
    return (
        config.get("batch") == QUALIFIED_BATCH
        and config.get("samples") == QUALIFIED_SAMPLES
        and config.get("warmup") == QUALIFIED_WARMUP
        and config.get("minimum_time_ms") == QUALIFIED_MIN_TIME_MS
        and config.get("smoke") is False
    )


def _derive_case(item: dict[str, Any], config: dict[str, Any]) -> tuple[
    dict[str, dict[str, float] | None], dict[str, Any]
]:
    samples = _sequence(item.get("samples"), f"{item.get('id', 'case')}.samples")
    elapsed = [float(_integer(sample.get("elapsed_ns"), "sample.elapsed_ns", minimum=1))
               for sample in samples]
    messages = [float(_integer(sample.get("messages"), "sample.messages", minimum=1))
                for sample in samples]
    byte_counts = [float(_integer(sample.get("bytes"), "sample.bytes", minimum=1))
                   for sample in samples]
    event_counts = [float(_integer(sample.get("events"), "sample.events"))
                    for sample in samples]
    push_counts = [float(_integer(sample.get("pushes"), "sample.pushes"))
                   for sample in samples]
    ns_per_message = [duration / count for duration, count in zip(elapsed, messages)]
    statistics: dict[str, dict[str, float] | None] = {
        "bytes_per_second": distribution(
            count / (duration / 1_000_000_000.0)
            for duration, count in zip(elapsed, byte_counts)
        ),
        "events_per_second": None,
        "messages_per_second": distribution(
            count / (duration / 1_000_000_000.0)
            for duration, count in zip(elapsed, messages)
        ),
        "ns_per_event": None,
        "ns_per_message": distribution(ns_per_message),
        "ns_per_push": None,
        "sample_time_ns": distribution(elapsed),
    }
    if any(event_counts):
        statistics["ns_per_event"] = distribution(
            duration / count for duration, count in zip(elapsed, event_counts)
        )
        statistics["events_per_second"] = distribution(
            count / (duration / 1_000_000_000.0)
            for duration, count in zip(elapsed, event_counts)
        )
    if any(push_counts):
        statistics["ns_per_push"] = distribution(
            duration / count for duration, count in zip(elapsed, push_counts)
        )
    message_stats = statistics["ns_per_message"]
    sample_stats = statistics["sample_time_ns"]
    assert message_stats is not None and sample_stats is not None
    relative_mad = message_stats["mad"] / message_stats["median"]
    relative_spread = (
        message_stats["p95"] - message_stats["p05"]
    ) / message_stats["median"]
    noisy = relative_mad > 0.05 or relative_spread > 0.20
    target_ns = _number(config.get("minimum_time_ms"), "config.minimum_time_ms") * 1_000_000.0
    implausible = (
        sample_stats["minimum"] < target_ns
        or message_stats["median"] < 0.01
        or sample_stats["minimum"] <= 0.0
    )
    warnings = []
    if noisy:
        warnings.append(
            "sample dispersion exceeds the 5% MAD or 20% p95-p05 diagnostic bound"
        )
    if implausible:
        warnings.append(
            "sample duration or per-message timing is implausible; discard this run"
        )
    return statistics, {
        "implausible": implausible,
        "noisy": noisy,
        "relative_mad": relative_mad,
        "relative_p95_p05_spread": relative_spread,
        "warnings": warnings,
    }


def _validate_distribution(actual: Any, expected: dict[str, float], context: str) -> None:
    mapping = _mapping(actual, context)
    if set(mapping) != set(expected):
        raise ValueError(f"{context}: distribution fields changed")
    for name, value in expected.items():
        _close(mapping.get(name), value, f"{context}.{name}")


def _validate_build(
    build_value: Any,
    path: Path,
    expected_source_id: str | None,
    require_clean_source: bool,
) -> dict[str, Any]:
    build = _mapping(build_value, f"{path}: build")
    if set(build) != BUILD_KEYS:
        raise ValueError(f"{path}: build fields changed")
    required_text = (
        "build_type", "compiler_builtin", "compiler_id", "compiler_path",
        "compiler_version", "config_flags", "feedforge_version", "generator",
        "interprocedural_optimization", "schema_fingerprint", "source_revision",
        "target_flags",
    )
    for key in required_text:
        if not isinstance(build.get(key), str) or not build[key]:
            raise ValueError(f"{path}: build.{key} is missing")
    cxx_standard = _integer(
        build.get("cxx_standard"), f"{path}: build.cxx_standard", minimum=202002
    )
    if cxx_standard != 202002:
        raise ValueError(f"{path}: benchmark build must use the frozen C++20 mode")
    _boolean(build.get("source_dirty"), f"{path}: build.source_dirty")
    _revision(build.get("source_revision"), f"{path}: build.source_revision")
    if build.get("build_type") != "Release":
        raise ValueError(f"{path}: benchmark build is not Release")
    if build.get("feedforge_version") != "0.6.0":
        raise ValueError(f"{path}: benchmark executable is not FeedForge 0.6.0")
    if build.get("interprocedural_optimization") != "OFF":
        raise ValueError(f"{path}: interprocedural optimization must be OFF")
    if build.get("schema_fingerprint") != SCHEMA_FINGERPRINT:
        raise ValueError(f"{path}: schema fingerprint changed")
    if build.get("pipeline_fingerprints") != PIPELINE_FINGERPRINTS:
        raise ValueError(f"{path}: generated pipeline fingerprints changed")
    rejected = _forbidden_isa_flags(build)
    if rejected:
        raise ValueError(
            f"{path}: CPU-specific ISA/tuning or explicit LTO flag is forbidden: "
            + " ".join(rejected)
        )
    if expected_source_id is not None and build.get("source_revision") != expected_source_id:
        raise ValueError(f"{path}: executable source revision does not match requested SHA")
    if require_clean_source and build.get("source_dirty") is not False:
        raise ValueError(f"{path}: executable was configured from a dirty source tree")
    return build


def _validate_corpus(corpus_value: Any, path: Path) -> None:
    corpus = _mapping(corpus_value, f"{path}: corpus")
    if set(corpus) != CORPUS_KEYS:
        raise ValueError(f"{path}: corpus fields changed")
    if corpus.get("sha256") != CORPUS_SHA256:
        raise ValueError(f"{path}: frozen corpus hash changed")
    if corpus.get("fixture_count") != 23:
        raise ValueError(f"{path}: frozen corpus must contain 23 fixtures")
    if corpus.get("source") != "independently reviewed tests/fixtures/itch50 raw_hex":
        raise ValueError(f"{path}: corpus source statement changed")
    fixtures = _sequence(corpus.get("fixtures"), f"{path}: corpus.fixtures")
    if len(fixtures) != 23:
        raise ValueError(f"{path}: corpus fixture list must contain 23 entries")
    for index, (fixture_value, frozen) in enumerate(
        zip(fixtures, FROZEN_FIXTURES), start=1
    ):
        fixture = _mapping(fixture_value, f"{path}: fixture {index}")
        if set(fixture) != FIXTURE_KEYS:
            raise ValueError(f"{path}: fixture {index} fields changed")
        file_name, message_name, message_type, selected, digest, size = frozen
        expected = {
            "byte_source": FROZEN_BYTE_SOURCE,
            "file": file_name,
            "message_name": message_name,
            "message_type": message_type,
            "order_events_selected": selected,
            "review_status": FROZEN_REVIEW_STATUS,
            "reviewer": FROZEN_REVIEWER,
            "sha256": digest,
            "size": size,
        }
        if fixture != expected:
            raise ValueError(f"{path}: frozen fixture {index} review metadata changed")


def _validate_command(
    command_value: Any, config: dict[str, Any], path: Path
) -> dict[str, Any]:
    command = _mapping(command_value, f"{path}: command")
    if set(command) != COMMAND_KEYS:
        raise ValueError(f"{path}: command fields changed")
    arguments = _sequence(command.get("argv"), f"{path}: command.argv")
    if command.get("working_directory") != ".":
        raise ValueError(f"{path}: benchmark working directory must be relative '.'")
    if not isinstance(command.get("joined"), str) or not command["joined"]:
        raise ValueError(f"{path}: joined benchmark command is missing")
    if command["joined"] != join_command(arguments):
        raise ValueError(f"{path}: joined benchmark command does not match argv")
    return _parse_benchmark_argv(arguments, config, f"{path}: command")


def validate_run(
    run: dict[str, Any],
    path: Path,
    *,
    expected_source_id: str | None = None,
    require_clean_source: bool = False,
) -> None:
    if set(run) != RUN_KEYS:
        raise ValueError(f"{path}: top-level result fields changed")
    if run.get("schema_version") != RESULT_SCHEMA_VERSION:
        raise ValueError(f"{path}: unsupported result schema")
    if run.get("contract_version") != CONTRACT_VERSION:
        raise ValueError(f"{path}: unsupported benchmark contract")
    if run.get("publishable") is not False:
        raise ValueError(f"{path}: a single-process artifact must be non-publishable")
    if run.get("correctness") != CORRECTNESS:
        raise ValueError(f"{path}: pre-timing correctness contract changed")
    _validate_corpus(run.get("corpus"), path)
    build = _validate_build(
        run.get("build"), path, expected_source_id, require_clean_source
    )
    config = _mapping(run.get("config"), f"{path}: config")
    if set(config) != CONFIG_KEYS:
        raise ValueError(f"{path}: config fields changed")
    if config.get("clock") != "std::chrono::steady_clock":
        raise ValueError(f"{path}: unexpected benchmark clock")
    if config.get("clock_is_steady") is not True:
        raise ValueError(f"{path}: steady clock was not available")
    batch = _integer(config.get("batch"), f"{path}: config.batch", minimum=1)
    samples_count = _integer(config.get("samples"), f"{path}: config.samples", minimum=1)
    _integer(config.get("warmup"), f"{path}: config.warmup", minimum=1)
    _number(config.get("minimum_time_ms"), f"{path}: config.minimum_time_ms", positive=True)
    _boolean(config.get("smoke"), f"{path}: config.smoke")
    timer_resolution = _number(
        config.get("timer_resolution_ns"), f"{path}: config.timer_resolution_ns", positive=True
    )
    if timer_resolution > 100_000.0:
        raise ValueError(f"{path}: steady-clock resolution is too coarse")
    _validate_command(run.get("command"), config, path)
    host = _mapping(run.get("host"), f"{path}: host")
    if set(host) != HOST_KEYS:
        raise ValueError(f"{path}: host fields changed")
    for field in ("architecture", "cpu_affinity", "cpu_governor", "cpu_model",
                  "kernel", "machine_model", "os", "turbo_state"):
        if not isinstance(host.get(field), str) or not host[field]:
            raise ValueError(f"{path}: host.{field} is missing")
    _integer(host.get("logical_cpus"), f"{path}: host.logical_cpus", minimum=1)
    for field in ("physical_cpus", "memory_bytes"):
        _integer(host.get(field), f"{path}: host.{field}")
    limitations = _sequence(host.get("limitations"), f"{path}: host.limitations")
    if any(not isinstance(item, str) or not item for item in limitations):
        raise ValueError(f"{path}: host limitations are malformed")
    _utc_timestamp(run.get("timestamp_utc"), f"{path}: timestamp_utc")
    warnings = _sequence(run.get("warnings"), f"{path}: warnings")
    if any(not isinstance(item, str) or not item for item in warnings):
        raise ValueError(f"{path}: top-level warnings are malformed")

    benchmarks = _sequence(run.get("benchmarks"), f"{path}: benchmarks")
    if len(benchmarks) != len(FROZEN_CASES):
        raise ValueError(f"{path}: expected the frozen 16 benchmark cases")
    for index, (item_value, expected) in enumerate(zip(benchmarks, FROZEN_CASES)):
        item = _mapping(item_value, f"{path}: benchmark {index + 1}")
        identifier = expected["id"]
        if set(item) != set(expected) | {
            "anti_elision_checksum", "quality", "rounds_per_sample", "samples",
            "statistics",
        }:
            raise ValueError(f"{path}: {identifier} benchmark fields changed")
        for field, expected_value in expected.items():
            if item.get(field) != expected_value:
                raise ValueError(
                    f"{path}: {identifier}.{field} changed from the frozen contract"
                )
        rounds = _integer(
            item.get("rounds_per_sample"), f"{path}: {identifier}.rounds_per_sample",
            minimum=batch,
        )
        if rounds % batch != 0:
            raise ValueError(f"{path}: {identifier} rounds are not a batch multiple")
        case_samples = _sequence(item.get("samples"), f"{path}: {identifier}.samples")
        if len(case_samples) != samples_count:
            raise ValueError(f"{path}: {identifier} sample count mismatch")
        checksums: set[str] = set()
        for sample_index, sample_value in enumerate(case_samples, start=1):
            sample = _mapping(sample_value, f"{path}: {identifier} sample {sample_index}")
            if set(sample) != {
                "bytes", "checksum", "elapsed_ns", "events", "finish_calls",
                "messages", "pushes", "rounds",
            }:
                raise ValueError(f"{path}: {identifier} sample fields changed")
            if sample.get("rounds") != rounds:
                raise ValueError(f"{path}: {identifier} sample rounds changed")
            for field, case_field in (
                ("bytes", "bytes_per_round"),
                ("messages", "messages_per_round"),
                ("events", "events_per_round"),
                ("pushes", "pushes_per_round"),
                ("finish_calls", "finish_calls_per_round"),
            ):
                if sample.get(field) != expected[case_field] * rounds:
                    raise ValueError(f"{path}: {identifier} sample {field} changed")
            _integer(sample.get("elapsed_ns"), f"{path}: {identifier}.elapsed_ns", minimum=1)
            checksum = sample.get("checksum")
            if not isinstance(checksum, str) or not CHECKSUM_RE.fullmatch(checksum):
                raise ValueError(f"{path}: {identifier} checksum is malformed")
            if int(checksum, 16) == 0:
                raise ValueError(f"{path}: {identifier} checksum is zero")
            checksums.add(checksum)
        if len(checksums) != 1 or item.get("anti_elision_checksum") not in checksums:
            raise ValueError(f"{path}: {identifier} anti-elision checksum changed")
        expected_statistics, expected_quality = _derive_case(item, config)
        statistics = _mapping(item.get("statistics"), f"{path}: {identifier}.statistics")
        if set(statistics) != set(expected_statistics):
            raise ValueError(f"{path}: {identifier} statistics fields changed")
        for name, derived in expected_statistics.items():
            actual = statistics.get(name)
            if derived is None:
                if actual is not None:
                    raise ValueError(f"{path}: {identifier}.{name} must be null")
            else:
                _validate_distribution(actual, derived, f"{path}: {identifier}.{name}")
        quality = _mapping(item.get("quality"), f"{path}: {identifier}.quality")
        if set(quality) != set(expected_quality):
            raise ValueError(f"{path}: {identifier} quality fields changed")
        for name in ("implausible", "noisy", "warnings"):
            if quality.get(name) != expected_quality[name]:
                raise ValueError(f"{path}: {identifier} derived quality.{name} changed")
        _close(quality.get("relative_mad"), expected_quality["relative_mad"],
               f"{path}: {identifier}.quality.relative_mad")
        _close(quality.get("relative_p95_p05_spread"),
               expected_quality["relative_p95_p05_spread"],
               f"{path}: {identifier}.quality.relative_p95_p05_spread")
        if expected_quality["implausible"]:
            raise ValueError(f"{path}: {identifier} is implausible")

    if expected_source_id is not None and build["source_revision"] != expected_source_id:
        raise ValueError(f"{path}: source revision drifted")


def artifact_identity(run: dict[str, Any]) -> dict[str, Any]:
    cases = []
    for item in run["benchmarks"]:
        cases.append({key: item[key] for key in FROZEN_CASES[0]})
    config = run["config"]
    return {
        "benchmarks": cases,
        "build": run["build"],
        "config": {
            "batch": config["batch"],
            "clock": config["clock"],
            "clock_is_steady": config["clock_is_steady"],
            "minimum_time_ms": config["minimum_time_ms"],
            "samples": config["samples"],
            "smoke": config["smoke"],
            "warmup": config["warmup"],
        },
        "contract_version": run["contract_version"],
        "corpus_sha256": run["corpus"]["sha256"],
        "correctness": run["correctness"],
        "host": run["host"],
        "schema_version": run["schema_version"],
    }


def _raw_csv_expected(run: dict[str, Any], item: dict[str, Any]) -> dict[str, Any]:
    stats = item["statistics"]
    build = run["build"]
    host = run["host"]
    return {
        "schema_version": run["schema_version"],
        "contract_version": run["contract_version"],
        "timestamp_utc": run["timestamp_utc"],
        "benchmark_id": item["id"],
        "operation": item["operation"],
        "pipeline": item["pipeline"],
        "schedule": item["schedule"] or "",
        "schedule_sha256": item["schedule_sha256"] or "",
        "workload": item["workload"],
        "workload_sha256": item["workload_sha256"],
        "corpus_sha256": run["corpus"]["sha256"],
        "bytes_per_round": item["bytes_per_round"],
        "messages_per_round": item["messages_per_round"],
        "events_per_round": item["events_per_round"],
        "pushes_per_round": item["pushes_per_round"],
        "finish_calls_per_round": item["finish_calls_per_round"],
        "rounds_per_sample": item["rounds_per_sample"],
        "samples": run["config"]["samples"],
        "warmup": run["config"]["warmup"],
        "minimum_time_ms": run["config"]["minimum_time_ms"],
        "median_ns_per_message": stats["ns_per_message"]["median"],
        "p05_ns_per_message": stats["ns_per_message"]["p05"],
        "p95_ns_per_message": stats["ns_per_message"]["p95"],
        "mad_ns_per_message": stats["ns_per_message"]["mad"],
        "median_ns_per_event": "" if stats["ns_per_event"] is None
        else stats["ns_per_event"]["median"],
        "median_ns_per_push": "" if stats["ns_per_push"] is None
        else stats["ns_per_push"]["median"],
        "median_bytes_per_second": stats["bytes_per_second"]["median"],
        "median_messages_per_second": stats["messages_per_second"]["median"],
        "median_events_per_second": "" if stats["events_per_second"] is None
        else stats["events_per_second"]["median"],
        "relative_mad": item["quality"]["relative_mad"],
        "relative_p95_p05_spread": item["quality"]["relative_p95_p05_spread"],
        "noisy": str(item["quality"]["noisy"]).lower(),
        "implausible": str(item["quality"]["implausible"]).lower(),
        "anti_elision_checksum": item["anti_elision_checksum"],
        "feedforge_version": build["feedforge_version"],
        "source_revision": build["source_revision"],
        "source_dirty": str(build["source_dirty"]).lower(),
        "build_type": build["build_type"],
        "compiler_id": build["compiler_id"],
        "compiler_version": build["compiler_version"],
        "compiler_path": build["compiler_path"],
        "config_flags": build["config_flags"],
        "target_flags": build["target_flags"],
        "os": host["os"],
        "kernel": host["kernel"],
        "architecture": host["architecture"],
        "cpu_model": host["cpu_model"],
        "machine_model": host["machine_model"],
        "logical_cpus": host["logical_cpus"],
        "physical_cpus": host["physical_cpus"],
        "memory_bytes": host["memory_bytes"],
        "cpu_affinity": host["cpu_affinity"],
        "cpu_governor": host["cpu_governor"],
        "turbo_state": host["turbo_state"],
        "correctness_checksum": run["correctness"]["checksum"],
        "command": run["command"]["joined"],
    }


def validate_raw_csv(path: Path, run: dict[str, Any]) -> None:
    if path.is_symlink():
        raise ValueError(f"{path}: symlinked CSV evidence is forbidden")
    with path.open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != RAW_CSV_FIELDS:
            raise ValueError(f"{path}: raw CSV header changed")
        rows = list(reader)
    if len(rows) != len(FROZEN_CASES):
        raise ValueError(f"{path}: raw CSV must contain 16 rows")
    numeric_float = {
        "minimum_time_ms", "median_ns_per_message", "p05_ns_per_message",
        "p95_ns_per_message", "mad_ns_per_message", "median_ns_per_event",
        "median_ns_per_push", "median_bytes_per_second",
        "median_messages_per_second", "median_events_per_second", "relative_mad",
        "relative_p95_p05_spread",
    }
    for index, (row, item) in enumerate(zip(rows, run["benchmarks"]), start=1):
        if None in row or set(row) != set(RAW_CSV_FIELDS):
            raise ValueError(f"{path}: row {index} has extra or missing cells")
        expected = _raw_csv_expected(run, item)
        for field in RAW_CSV_FIELDS:
            actual = row[field]
            wanted = expected[field]
            context = f"{path}: row {index} {field}"
            if field in numeric_float and wanted != "":
                try:
                    parsed = float(actual)
                except ValueError as error:
                    raise ValueError(f"{context}: expected a number") from error
                _close(parsed, float(wanted), context)
            elif actual != str(wanted):
                raise ValueError(f"{context}: does not match raw JSON")


def series_csv(series: dict[str, Any]) -> str:
    fields = [
        "benchmark_id", "median_ns_per_message", "p05_ns_per_message",
        "p95_ns_per_message", "mad_ns_per_message", "normalized_mad",
        "comparison_ready", "repeat_count", "corpus_sha256", "compiler",
        "build_type", "os", "architecture", "cpu_model", "source_id",
        "executable_sha256",
    ]
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    identity = series["identity"]
    for item in series["benchmarks"]:
        writer.writerow(
            {
                "benchmark_id": item["id"],
                "median_ns_per_message": format(item["ns_per_message"]["median"], ".17g"),
                "p05_ns_per_message": format(item["ns_per_message"]["p05"], ".17g"),
                "p95_ns_per_message": format(item["ns_per_message"]["p95"], ".17g"),
                "mad_ns_per_message": format(item["ns_per_message"]["mad"], ".17g"),
                "normalized_mad": format(item["normalized_mad"], ".17g"),
                "comparison_ready": str(series["comparison_ready"]).lower(),
                "repeat_count": series["repeat_count"],
                "corpus_sha256": identity["corpus_sha256"],
                "compiler": (
                    f"{identity['build']['compiler_id']} "
                    f"{identity['build']['compiler_version']}"
                ),
                "build_type": identity["build"]["build_type"],
                "os": identity["host"]["os"],
                "architecture": identity["host"]["architecture"],
                "cpu_model": identity["host"]["cpu_model"],
                "source_id": series["source_id"],
                "executable_sha256": series["executable_sha256"],
            }
        )
    return stream.getvalue()


def _thresholds() -> dict[str, Any]:
    return {
        "batch": QUALIFIED_BATCH,
        "cooldown_seconds": QUALIFIED_COOLDOWN_SECONDS,
        "max_cross_run_normalized_mad": MAX_CROSS_RUN_NORMALIZED_MAD,
        "max_untargeted_regression": MAX_UNTARGETED_REGRESSION,
        "min_median_improvement": MIN_MEDIAN_IMPROVEMENT,
        "min_repeats": QUALIFIED_REPEATS,
        "min_robust_margin": MIN_ROBUST_MARGIN,
        "min_sample_time_ms": QUALIFIED_MIN_TIME_MS,
        "samples": QUALIFIED_SAMPLES,
        "warmup": QUALIFIED_WARMUP,
    }


def _command_artifact_path(directory: str, name: str) -> str:
    return name if directory == "." else f"{directory}/{name}"


def build_series(
    runs: list[dict[str, Any]],
    raw_runs: list[dict[str, str]],
    *,
    label: str,
    command: list[str],
    correctness: dict[str, Any],
    executable: str,
    executable_sha256_before: str,
    executable_sha256_after: str,
    source_id: str,
    cooldown_seconds: int,
    diagnostic_only: bool,
    timestamp_utc: str | None = None,
) -> dict[str, Any]:
    if not runs:
        raise ValueError("cannot build a benchmark series without raw runs")
    identity = artifact_identity(runs[0])
    for index, run in enumerate(runs[1:], start=2):
        if artifact_identity(run) != identity:
            raise ValueError(
                f"run {index} changed corpus, correctness, build, host, config, or cases"
            )

    if len(raw_runs) != len(runs):
        raise ValueError("raw run descriptors do not match the loaded run count")
    command_output_directory: str | None = None
    for index, (run, record) in enumerate(zip(runs, raw_runs), start=1):
        parsed = _parse_benchmark_argv(
            run["command"]["argv"], run["config"], f"run {index} command"
        )
        if parsed["base"] != command:
            raise ValueError(f"run {index} command does not match the series base command")
        if command_output_directory is None:
            command_output_directory = parsed["output_directory"]
        elif parsed["output_directory"] != command_output_directory:
            raise ValueError(f"run {index} command output directory changed")
        expected_json = _command_artifact_path(
            command_output_directory, record["json"]
        )
        expected_csv = _command_artifact_path(
            command_output_directory, record["csv"]
        )
        if parsed["json"] != expected_json or parsed["csv"] != expected_csv:
            raise ValueError(f"run {index} command paths do not match its raw files")
    assert command_output_directory is not None

    aggregate = []
    warnings: list[str] = []
    all_within_run_quiet = True
    for benchmark_index, template in enumerate(runs[0]["benchmarks"]):
        medians = [
            run["benchmarks"][benchmark_index]["statistics"]["ns_per_message"]["median"]
            for run in runs
        ]
        summary = distribution(medians)
        normalized_mad = summary["mad"] / summary["median"]
        noisy_runs = [
            index + 1 for index, run in enumerate(runs)
            if run["benchmarks"][benchmark_index]["quality"]["noisy"]
        ]
        if noisy_runs:
            all_within_run_quiet = False
            warnings.append(
                f"{template['id']}: noisy within-run samples in repeats "
                + ",".join(map(str, noisy_runs))
            )
        if normalized_mad > MAX_CROSS_RUN_NORMALIZED_MAD:
            warnings.append(
                f"{template['id']}: cross-run normalized MAD "
                f"{normalized_mad:.2%} exceeds {MAX_CROSS_RUN_NORMALIZED_MAD:.0%}"
            )
        aggregate.append(
            {
                "id": template["id"],
                "normalized_mad": normalized_mad,
                "ns_per_message": summary,
                "run_medians_ns_per_message": medians,
            }
        )

    config_exact = _qualification_config(identity["config"])
    repeat_count_exact = len(runs) == QUALIFIED_REPEATS
    cross_run_quiet = all(
        item["normalized_mad"] <= MAX_CROSS_RUN_NORMALIZED_MAD
        for item in aggregate
    )
    sources_exact = all(
        run["build"]["source_revision"] == source_id
        and run["build"]["source_dirty"] is False
        for run in runs
    )
    executable_unchanged = (
        executable_sha256_before == executable_sha256_after
        and SHA256_RE.fullmatch(executable_sha256_before) is not None
    )
    correctness_exact = correctness.get("command") == FROZEN_CORRECTNESS_COMMAND
    cooldown_exact = cooldown_seconds == QUALIFIED_COOLDOWN_SECONDS
    raw_count_exact = len(raw_runs) == QUALIFIED_REPEATS
    qualified = all(
        (
            config_exact,
            repeat_count_exact,
            cross_run_quiet,
            all_within_run_quiet,
            sources_exact,
            executable_unchanged,
            correctness_exact,
            cooldown_exact,
            raw_count_exact,
            not diagnostic_only,
        )
    )
    checks = {
        "all_runs_quiet": all_within_run_quiet,
        "config_exact": config_exact,
        "cooldown_exact": cooldown_exact,
        "correctness_command_exact": correctness_exact,
        "cross_run_quiet": cross_run_quiet,
        "executable_unchanged": executable_unchanged,
        "raw_count_exact": raw_count_exact,
        "repeat_count_exact": repeat_count_exact,
        "sources_exact": sources_exact,
    }
    for name, passed in checks.items():
        if not passed:
            warnings.append(f"qualification check failed: {name}")
    if diagnostic_only:
        warnings.append("--allow-unqualified was used; this series is diagnostic only")
    warnings.append(
        "a contract 2.0 baseline is not an optimization, feed-latency, or production-throughput claim"
    )
    timestamp = timestamp_utc or (
        dt.datetime.now(dt.timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )
    return {
        "benchmarks": aggregate,
        "command": command,
        "command_output_directory": command_output_directory,
        "comparison_ready": qualified,
        "contract_version": CONTRACT_VERSION,
        "cooldown_seconds": cooldown_seconds,
        "correctness": correctness,
        "diagnostic_only": diagnostic_only,
        "executable": executable,
        "executable_sha256": executable_sha256_before,
        "executable_sha256_after": executable_sha256_after,
        "executable_sha256_before": executable_sha256_before,
        "identity": identity,
        "label": label,
        "qualification": {"checks": checks, "qualified": qualified},
        "repeat_count": len(runs),
        "run_files": raw_runs,
        "schema_version": SERIES_SCHEMA_VERSION,
        "source_id": source_id,
        "source_root": ".",
        "thresholds": _thresholds(),
        "timestamp_utc": timestamp,
        "warnings": warnings,
    }


def _git(root: Path, *arguments: str) -> str:
    try:
        completed = subprocess.run(
            ["git", "-C", str(root), *arguments],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as error:
        raise ValueError(f"unable to execute git: {error}") from error
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise ValueError(f"git {' '.join(arguments)} failed: {detail}")
    return completed.stdout.strip()


def repository_root(path: Path) -> Path:
    resolved = path.resolve()
    root = Path(_git(resolved, "rev-parse", "--show-toplevel")).resolve()
    if root != resolved:
        raise ValueError(f"benchmark cwd must be the exact Git root: {root}")
    return root


def verify_checkout(root: Path, source_id: str) -> None:
    _revision(source_id, "source id")
    head = _git(root, "rev-parse", "HEAD")
    if head != source_id:
        raise ValueError(f"Git HEAD {head} does not match source id {source_id}")
    status = _git(root, "status", "--porcelain", "--untracked-files=no")
    if status:
        raise ValueError("tracked Git worktree is not clean")


def _resolve_from_root(value: Path, root: Path) -> Path:
    return value.resolve() if value.is_absolute() else (root / value).resolve()


def _correctness_environment(
    environment: dict[str, str] | None = None,
) -> dict[str, str]:
    sanitized = dict(os.environ if environment is None else environment)
    for name in CORRECTNESS_ENVIRONMENT_BLOCKLIST:
        sanitized.pop(name, None)
    return sanitized


def _raw_record(output_dir: Path, stem: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for kind, suffix in (("json", ".json"), ("csv", ".csv"), ("log", ".txt")):
        name = f"{stem}{suffix}"
        result[kind] = name
        result[f"{kind}_sha256"] = sha256_file(output_dir / name)
    return result


def run_series(args: argparse.Namespace) -> int:
    root = repository_root(args.cwd)
    source_id = _revision(args.source_id, "--source-id")
    if not LABEL_RE.fullmatch(args.label):
        raise ValueError("--label must match [A-Za-z0-9._-]+")
    if os.environ.get("CI"):
        raise ValueError("benchmark timing under CI is forbidden")
    verify_checkout(root, source_id)

    executable = _resolve_from_root(args.executable, root)
    if not executable.is_file():
        raise ValueError(f"benchmark executable does not exist: {executable}")
    executable_relative = _relative_path(executable, root, "benchmark executable")
    output_dir = _resolve_from_root(args.output_dir, root)
    output_relative = _relative_path(output_dir, root, "benchmark output directory")
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ValueError(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.repeats <= 0 or args.samples <= 0 or args.warmup <= 0 or args.batch <= 0:
        raise ValueError("repeat, sample, warmup, and batch counts must be positive")
    if args.min_time_ms <= 0.0 or args.cooldown_seconds < 0:
        raise ValueError("sample time must be positive and cooldown must be nonnegative")
    requested_contract_exact = (
        args.repeats == QUALIFIED_REPEATS
        and args.samples == QUALIFIED_SAMPLES
        and args.warmup == QUALIFIED_WARMUP
        and args.batch == QUALIFIED_BATCH
        and args.min_time_ms == QUALIFIED_MIN_TIME_MS
        and args.cooldown_seconds == QUALIFIED_COOLDOWN_SECONDS
    )
    if not requested_contract_exact and not args.allow_unqualified:
        raise ValueError(
            "non-frozen measurement settings require --allow-unqualified"
        )
    correctness_command = shlex.split(args.correctness_command)
    if correctness_command != FROZEN_CORRECTNESS_COMMAND:
        raise ValueError("--correctness-command is frozen as 'make bench-correctness'")

    executable_before = sha256_file(executable)
    print("running frozen correctness command:", shlex.join(correctness_command))
    completed = subprocess.run(
        correctness_command,
        cwd=root,
        env=_correctness_environment(),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    correctness_path = output_dir / "correctness.txt"
    atomic_text(correctness_path, redact_text(completed.stdout, root))
    if completed.returncode != 0:
        raise RuntimeError(
            f"correctness command failed with exit code {completed.returncode}; "
            f"see {correctness_path}"
        )
    verify_checkout(root, source_id)
    if sha256_file(executable) != executable_before:
        raise ValueError("benchmark executable changed during correctness validation")
    correctness = {
        "command": correctness_command,
        "exit_code": completed.returncode,
        "log": "correctness.txt",
        "log_sha256": sha256_file(correctness_path),
    }

    print(f"cooldown: {args.cooldown_seconds} seconds")
    time.sleep(args.cooldown_seconds)
    common = [
        executable_relative,
        "--samples", str(args.samples),
        "--warmup", str(args.warmup),
        "--batch", str(args.batch),
        "--min-time-ms", format(args.min_time_ms, "g"),
    ]
    runs: list[dict[str, Any]] = []
    raw_runs: list[dict[str, str]] = []
    for index in range(1, args.repeats + 1):
        stem = f"run-{index:02d}"
        json_relative = f"{output_relative}/{stem}.json"
        csv_relative = f"{output_relative}/{stem}.csv"
        command = common + ["--json", json_relative, "--csv", csv_relative]
        print(f"[{index}/{args.repeats}] {shlex.join(command)}")
        completed = subprocess.run(
            command,
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        log_path = output_dir / f"{stem}.txt"
        atomic_text(log_path, redact_text(completed.stdout, root))
        if completed.returncode != 0:
            raise RuntimeError(
                f"{stem} failed with exit code {completed.returncode}; see {log_path}"
            )
        if sha256_file(executable) != executable_before:
            raise ValueError(f"benchmark executable changed during {stem}")
        json_path = output_dir / f"{stem}.json"
        csv_path = output_dir / f"{stem}.csv"
        run = load_json(json_path)
        validate_run(
            run, json_path, expected_source_id=source_id, require_clean_source=True
        )
        validate_raw_csv(csv_path, run)
        runs.append(run)
        raw_runs.append(_raw_record(output_dir, stem))

    verify_checkout(root, source_id)
    executable_after = sha256_file(executable)
    series = build_series(
        runs,
        raw_runs,
        label=args.label,
        command=common,
        correctness=correctness,
        executable=executable_relative,
        executable_sha256_before=executable_before,
        executable_sha256_after=executable_after,
        source_id=source_id,
        cooldown_seconds=args.cooldown_seconds,
        diagnostic_only=args.allow_unqualified,
    )
    canonical_json(output_dir / "series.json", series)
    atomic_text(output_dir / "series.csv", series_csv(series))
    validate_series_path(
        output_dir / "series.json",
        output_dir,
        verify_current_checkout=True,
        checkout_root=root,
    )
    print()
    print(
        f"{args.label}: {len(runs)} repeats; "
        f"qualified={str(series['qualification']['qualified']).lower()}"
    )
    for item in series["benchmarks"]:
        print(
            f"  {item['id']}: {item['ns_per_message']['median']:.3f} ns/message "
            f"(cross-run MAD {item['normalized_mad']:.2%})"
        )
    for warning in series["warnings"]:
        print("warning:", warning)
    print("series:", output_dir / "series.json")
    if series["qualification"]["qualified"] or args.allow_unqualified:
        return 0
    return 1


def _validate_raw_record(record_value: Any, index: int) -> dict[str, str]:
    record = _mapping(record_value, f"run_files[{index - 1}]")
    expected_keys = {
        "json", "json_sha256", "csv", "csv_sha256", "log", "log_sha256"
    }
    if set(record) != expected_keys:
        raise ValueError(f"run_files[{index - 1}]: fields changed")
    stem = f"run-{index:02d}"
    for kind, suffix in (("json", ".json"), ("csv", ".csv"), ("log", ".txt")):
        if record.get(kind) != f"{stem}{suffix}":
            raise ValueError(f"run_files[{index - 1}].{kind}: unexpected name")
        _safe_relative(record[kind], f"run_files[{index - 1}].{kind}")
        _sha256(record.get(f"{kind}_sha256"),
                f"run_files[{index - 1}].{kind}_sha256")
    return record


def _verify_series_shape(series: dict[str, Any], path: Path) -> None:
    if set(series) != SERIES_KEYS:
        raise ValueError(f"{path}: top-level series fields changed")
    if series.get("schema_version") != SERIES_SCHEMA_VERSION:
        raise ValueError(f"{path}: unsupported series schema")
    if series.get("contract_version") != CONTRACT_VERSION:
        raise ValueError(f"{path}: unsupported series contract")
    if not isinstance(series.get("label"), str) or not LABEL_RE.fullmatch(series["label"]):
        raise ValueError(f"{path}: invalid series label")
    _revision(series.get("source_id"), f"{path}: source_id")
    if series.get("source_root") != ".":
        raise ValueError(f"{path}: source_root must be relative '.'")
    _safe_relative(series.get("executable"), f"{path}: executable")
    for field in (
        "executable_sha256", "executable_sha256_before", "executable_sha256_after"
    ):
        _sha256(series.get(field), f"{path}: {field}")
    if series.get("executable_sha256") != series.get("executable_sha256_before"):
        raise ValueError(f"{path}: primary executable hash differs from pre-series hash")
    _integer(series.get("repeat_count"), f"{path}: repeat_count", minimum=1)
    _integer(series.get("cooldown_seconds"), f"{path}: cooldown_seconds")
    _boolean(series.get("diagnostic_only"), f"{path}: diagnostic_only")
    _boolean(series.get("comparison_ready"), f"{path}: comparison_ready")
    command = _sequence(series.get("command"), f"{path}: command")
    if not command or any(not isinstance(value, str) or not value for value in command):
        raise ValueError(f"{path}: series command is malformed")
    _safe_relative(command[0], f"{path}: command executable")
    command_output_directory = _safe_relative(
        series.get("command_output_directory"),
        f"{path}: command_output_directory",
        allow_current_directory=True,
    )
    if command_output_directory != series["command_output_directory"]:
        raise ValueError(f"{path}: command output directory is not canonical")
    identity = _mapping(series.get("identity"), f"{path}: identity")
    if set(identity) != IDENTITY_KEYS:
        raise ValueError(f"{path}: identity fields changed")
    identity_config = _mapping(identity.get("config"), f"{path}: identity.config")
    if set(identity_config) != IDENTITY_CONFIG_KEYS:
        raise ValueError(f"{path}: identity config fields changed")
    for name in ("batch", "samples", "warmup"):
        _integer(
            identity_config.get(name),
            f"{path}: identity.config.{name}",
            minimum=1,
        )
    _number(
        identity_config.get("minimum_time_ms"),
        f"{path}: identity.config.minimum_time_ms",
        positive=True,
    )
    _boolean(identity_config.get("smoke"), f"{path}: identity.config.smoke")
    expected_base = _expected_benchmark_base(
        series["executable"], identity_config
    )
    if command != expected_base:
        raise ValueError(f"{path}: series command does not match identity config")
    if series.get("thresholds") != _thresholds():
        raise ValueError(f"{path}: frozen series thresholds changed")
    _utc_timestamp(series.get("timestamp_utc"), f"{path}: timestamp_utc")


def _direct_evidence_file(directory: Path, name: str, context: str) -> Path:
    candidate = directory / name
    if candidate.is_symlink():
        raise ValueError(f"{context}: symlinked evidence is forbidden")
    if not candidate.is_file():
        raise ValueError(f"{context}: evidence file is missing")
    resolved = candidate.resolve(strict=True)
    if resolved.parent != directory or resolved != candidate:
        raise ValueError(f"{context}: evidence file is not directly contained")
    return candidate


def validate_series_path(
    series_path: Path,
    runs_dir: Path,
    *,
    verify_current_checkout: bool,
    checkout_root: Path | None = None,
) -> dict[str, Any]:
    lexical_series = Path(os.path.abspath(series_path))
    lexical_runs_dir = Path(os.path.abspath(runs_dir))
    if lexical_runs_dir.is_symlink():
        raise ValueError("--runs-dir must not be a symlink")
    if not lexical_runs_dir.is_dir():
        raise ValueError("--runs-dir is not a directory")
    if lexical_series.is_symlink():
        raise ValueError("--series must not be a symlink")
    if not lexical_series.is_file():
        raise ValueError("--series is not a file")
    if lexical_series.parent != lexical_runs_dir:
        raise ValueError("--series must be directly inside --runs-dir")
    runs_dir = lexical_runs_dir.resolve(strict=True)
    series_path = lexical_series.resolve(strict=True)
    if series_path.parent != runs_dir:
        raise ValueError("--series is not directly contained in --runs-dir")
    series = load_json(series_path)
    _verify_series_shape(series, series_path)
    records_value = _sequence(series.get("run_files"), f"{series_path}: run_files")
    if len(records_value) != series["repeat_count"]:
        raise ValueError(f"{series_path}: run file count differs from repeat count")
    records = [
        _validate_raw_record(value, index)
        for index, value in enumerate(records_value, start=1)
    ]
    expected_names = {
        record[kind] for record in records for kind in ("json", "csv", "log")
    }
    actual_names = {
        path.name for path in runs_dir.iterdir()
        if re.fullmatch(r"run-[0-9]+\.(?:json|csv|txt)", path.name)
    }
    if actual_names != expected_names:
        missing = sorted(expected_names - actual_names)
        unexpected = sorted(actual_names - expected_names)
        raise ValueError(
            f"{runs_dir}: raw run set changed; missing={missing}, unexpected={unexpected}"
        )

    runs: list[dict[str, Any]] = []
    for record in records:
        for kind in ("json", "csv", "log"):
            artifact = _direct_evidence_file(
                runs_dir,
                record[kind],
                f"{series_path}: run {record['json']} {kind}",
            )
            if sha256_file(artifact) != record[f"{kind}_sha256"]:
                raise ValueError(f"raw benchmark artifact hash changed: {artifact}")
        _validate_publishable_log(runs_dir / record["log"])
        json_path = runs_dir / record["json"]
        run = load_json(json_path)
        validate_run(
            run,
            json_path,
            expected_source_id=series["source_id"],
            require_clean_source=True,
        )
        expected_argv = series["command"] + [
            "--json",
            _command_artifact_path(
                series["command_output_directory"], record["json"]
            ),
            "--csv",
            _command_artifact_path(
                series["command_output_directory"], record["csv"]
            ),
        ]
        if run["command"]["argv"] != expected_argv:
            raise ValueError(f"{json_path}: command does not match series/raw paths")
        validate_raw_csv(runs_dir / record["csv"], run)
        runs.append(run)

    correctness = _mapping(series.get("correctness"), f"{series_path}: correctness")
    if set(correctness) != {"command", "exit_code", "log", "log_sha256"}:
        raise ValueError(f"{series_path}: correctness evidence fields changed")
    if correctness.get("command") != FROZEN_CORRECTNESS_COMMAND:
        raise ValueError(f"{series_path}: correctness command is not frozen")
    if correctness.get("exit_code") != 0 or correctness.get("log") != "correctness.txt":
        raise ValueError(f"{series_path}: correctness command did not pass")
    _sha256(correctness.get("log_sha256"), f"{series_path}: correctness log hash")
    correctness_path = _direct_evidence_file(
        runs_dir, "correctness.txt", f"{series_path}: correctness log"
    )
    if sha256_file(correctness_path) != correctness["log_sha256"]:
        raise ValueError(f"{series_path}: correctness log is missing or changed")
    _validate_publishable_log(correctness_path)

    rebuilt = build_series(
        runs,
        records,
        label=series["label"],
        command=series["command"],
        correctness=correctness,
        executable=series["executable"],
        executable_sha256_before=series["executable_sha256_before"],
        executable_sha256_after=series["executable_sha256_after"],
        source_id=series["source_id"],
        cooldown_seconds=series["cooldown_seconds"],
        diagnostic_only=series["diagnostic_only"],
        timestamp_utc=series["timestamp_utc"],
    )
    if rebuilt != series:
        raise ValueError(f"{series_path}: aggregate does not rebuild from raw runs")
    series_csv_path = _direct_evidence_file(
        runs_dir, "series.csv", f"{series_path}: series.csv"
    )
    if series_csv_path.read_text(encoding="utf-8") != series_csv(series):
        raise ValueError(f"{series_path}: series.csv does not match rebuilt aggregate")

    if verify_current_checkout:
        root = repository_root(checkout_root or Path.cwd())
        verify_checkout(root, series["source_id"])
        executable = root / series["executable"]
        if not executable.is_file():
            raise ValueError(f"{series_path}: benchmark executable is missing: {executable}")
        if sha256_file(executable) != series["executable_sha256"]:
            raise ValueError(f"{series_path}: benchmark executable hash changed")
    return series


def validate_series_command(args: argparse.Namespace) -> int:
    series = validate_series_path(
        args.series,
        args.runs_dir,
        verify_current_checkout=args.verify_checkout,
        checkout_root=args.cwd,
    )
    print(
        f"validated {series['repeat_count']} raw runs; "
        f"qualified={str(series['qualification']['qualified']).lower()}"
    )
    return 0 if series["qualification"]["qualified"] else 1


def validate_artifact(args: argparse.Namespace) -> int:
    path = Path(os.path.abspath(args.artifact))
    run = load_json(path)
    validate_run(run, path)
    csv_argument = getattr(args, "csv", None)
    if csv_argument is not None:
        csv_path = Path(os.path.abspath(csv_argument))
        if csv_path.parent != path.parent or csv_path.stem != path.stem:
            raise ValueError("--csv must be the direct sibling of --artifact with one stem")
        validate_raw_csv(csv_path, run)
        print(
            f"validated benchmark contract {CONTRACT_VERSION}: {path} and {csv_path}"
        )
    else:
        print(f"validated benchmark contract {CONTRACT_VERSION}: {path}")
    return 0


def _comparable_identity(identity: dict[str, Any]) -> dict[str, Any]:
    build = dict(identity["build"])
    build.pop("source_revision", None)
    build.pop("source_dirty", None)
    return {
        "benchmarks": identity["benchmarks"],
        "build": build,
        "config": identity["config"],
        "contract_version": identity["contract_version"],
        "corpus_sha256": identity["corpus_sha256"],
        "correctness": identity["correctness"],
        "host": identity["host"],
        "schema_version": identity["schema_version"],
    }


def comparison_csv(comparison: dict[str, Any]) -> str:
    fields = [
        "benchmark_id", "targeted", "baseline_median_ns_per_message",
        "candidate_median_ns_per_message", "median_improvement", "noise_bound",
        "robust_margin", "regression", "passes_target", "passes_regression",
        "overall_win",
    ]
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    for item in comparison["benchmarks"]:
        writer.writerow(
            {
                "benchmark_id": item["id"],
                "targeted": str(item["targeted"]).lower(),
                "baseline_median_ns_per_message": format(
                    item["baseline_median_ns_per_message"], ".17g"
                ),
                "candidate_median_ns_per_message": format(
                    item["candidate_median_ns_per_message"], ".17g"
                ),
                "median_improvement": format(item["median_improvement"], ".17g"),
                "noise_bound": format(item["noise_bound"], ".17g"),
                "robust_margin": format(item["robust_margin"], ".17g"),
                "regression": format(item["regression"], ".17g"),
                "passes_target": str(item["passes_target"]).lower(),
                "passes_regression": str(item["passes_regression"]).lower(),
                "overall_win": str(comparison["optimization_win"]).lower(),
            }
        )
    return stream.getvalue()


def compare_series(args: argparse.Namespace) -> int:
    baseline_path = Path(os.path.abspath(args.baseline))
    candidate_path = Path(os.path.abspath(args.candidate))
    baseline = validate_series_path(
        baseline_path, baseline_path.parent, verify_current_checkout=False
    )
    candidate = validate_series_path(
        candidate_path, candidate_path.parent, verify_current_checkout=False
    )
    if not baseline.get("comparison_ready") or not candidate.get("comparison_ready"):
        raise ValueError("baseline and candidate must both be qualified series")
    if baseline.get("thresholds") != candidate.get("thresholds"):
        raise ValueError("baseline and candidate threshold contracts differ")
    if baseline["source_id"] == candidate["source_id"]:
        raise ValueError("baseline and candidate source SHAs must be distinct")
    if baseline["executable_sha256"] == candidate["executable_sha256"]:
        raise ValueError("baseline and candidate benchmark executable SHAs must be distinct")
    if _comparable_identity(baseline["identity"]) != _comparable_identity(
        candidate["identity"]
    ):
        raise ValueError(
            "baseline and candidate differ in build, host, config, corpus, "
            "correctness, or benchmark contract"
        )

    baseline_cases = {item["id"]: item for item in baseline["benchmarks"]}
    candidate_cases = {item["id"]: item for item in candidate["benchmarks"]}
    if baseline_cases.keys() != candidate_cases.keys():
        raise ValueError("baseline and candidate benchmark case sets differ")
    targets = set(args.target or ())
    if not targets:
        raise ValueError("at least one explicit --target is required")
    unknown_targets = targets - baseline_cases.keys()
    if unknown_targets:
        raise ValueError(
            "unknown target benchmark(s): " + ", ".join(sorted(unknown_targets))
        )

    rows = []
    all_targets_pass = True
    all_regressions_pass = True
    for identifier in baseline_cases:
        before = baseline_cases[identifier]
        after = candidate_cases[identifier]
        baseline_median = before["ns_per_message"]["median"]
        candidate_median = after["ns_per_message"]["median"]
        improvement = (baseline_median - candidate_median) / baseline_median
        regression = (candidate_median - baseline_median) / baseline_median
        noise_bound = MAD_SCALE * (
            before["normalized_mad"] + after["normalized_mad"]
        )
        robust_margin = improvement - noise_bound
        targeted = identifier in targets
        passes_target = (
            not targeted
            or (
                improvement >= MIN_MEDIAN_IMPROVEMENT
                and max(before["normalized_mad"], after["normalized_mad"])
                <= MAX_CROSS_RUN_NORMALIZED_MAD
                and robust_margin >= MIN_ROBUST_MARGIN
            )
        )
        passes_regression = targeted or regression <= MAX_UNTARGETED_REGRESSION
        all_targets_pass = all_targets_pass and passes_target
        all_regressions_pass = all_regressions_pass and passes_regression
        rows.append(
            {
                "baseline_median_ns_per_message": baseline_median,
                "candidate_median_ns_per_message": candidate_median,
                "id": identifier,
                "median_improvement": improvement,
                "noise_bound": noise_bound,
                "passes_regression": passes_regression,
                "passes_target": passes_target,
                "regression": regression,
                "robust_margin": robust_margin,
                "targeted": targeted,
            }
        )
    optimization_win = all_targets_pass and all_regressions_pass
    comparison = {
        "baseline": f"{baseline['label']}@{baseline['source_id']}",
        "baseline_executable_sha256": baseline["executable_sha256"],
        "baseline_file": baseline_path.name,
        "baseline_label": baseline["label"],
        "baseline_source_id": baseline["source_id"],
        "benchmarks": rows,
        "candidate": f"{candidate['label']}@{candidate['source_id']}",
        "candidate_executable_sha256": candidate["executable_sha256"],
        "candidate_file": candidate_path.name,
        "candidate_label": candidate["label"],
        "candidate_source_id": candidate["source_id"],
        "correctness_equivalent": True,
        "optimization_win": optimization_win,
        "schema_version": 1,
        "targets": sorted(targets),
        "thresholds": baseline["thresholds"],
        "timestamp_utc": dt.datetime.now(dt.timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
    }
    if args.output_json:
        canonical_json(args.output_json.resolve(), comparison)
    if args.output_csv:
        atomic_text(args.output_csv.resolve(), comparison_csv(comparison))
    print(f"optimization_win={str(optimization_win).lower()}")
    for item in rows:
        marker = "target" if item["targeted"] else "guard"
        print(
            f"  [{marker}] {item['id']}: improvement={item['median_improvement']:.2%} "
            f"robust_margin={item['robust_margin']:.2%} "
            f"regression={item['regression']:.2%}"
        )
    return 0 if optimization_win else 1


_ANSI_RE = re.compile(r"(?:\x1b\[|\x9b)[0-?]*[ -/]*[@-~]")
_OSC_RE = re.compile(r"(?:\x1b\]|\x9d).*?(?:\x07|\x1b\\|\x9c)", re.DOTALL)
_CONTROL_RE = re.compile(r"[\x00-\x08\x0b-\x1f\x7f-\x9f]")
_SECRET_LITERAL_PATTERNS = (
    re.compile(r"\bgh[pousr]_[A-Za-z0-9_]{20,}\b"),
    re.compile(r"\bgithub_pat_[A-Za-z0-9_]{20,}\b"),
    re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(
        r"\beyJ[A-Za-z0-9_-]{7,}\.[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\b"
    ),
)
_AUTHORIZATION_RE = re.compile(r"(?im)^(\s*authorization\s*:\s*).*$")
_KEY_VALUE_SECRET_RE = re.compile(
    r"(?i)\b([A-Za-z0-9_]*(?:api[_-]?key|password|secret|token)[A-Za-z0-9_]*)\b"
    r"(\s*[:=]\s*)([^\s,;]+)"
)
_PRIVATE_KEY_RE = re.compile(
    r"-----BEGIN (?P<label>(?:[A-Z0-9]+ )*PRIVATE KEY)-----.*?"
    r"-----END (?P=label)-----",
    re.DOTALL,
)
_TEMP_PATH_RE = re.compile(
    r"(?<![A-Za-z0-9:])(?:/private/tmp|/tmp|/var/folders)/[^\s'\";,)]*"
)
_WINDOWS_HOME_RE = re.compile(r"(?i)[A-Z]:\\Users\\[^\\\s]+")
_UNIX_HOME_RE = re.compile(r"/(?:Users|home)/[^/\s]+")


def _validate_publishable_text(contents: str, context: str) -> None:
    if _ANSI_RE.search(contents) or _OSC_RE.search(contents) or _CONTROL_RE.search(contents):
        raise ValueError(f"{context}: log still contains terminal control sequences")
    if _TEMP_PATH_RE.search(contents) or _WINDOWS_HOME_RE.search(contents):
        raise ValueError(f"{context}: log still contains a sensitive absolute path")
    if _UNIX_HOME_RE.search(contents):
        raise ValueError(f"{context}: log still contains a user home path")
    sentinel_root = Path("/__feedforge_redaction_source_root_sentinel__")
    if redact_text(contents, sentinel_root) != contents:
        raise ValueError(f"{context}: log still contains credential-like text")


def _validate_publishable_log(path: Path) -> None:
    contents = path.read_text(encoding="utf-8", errors="replace")
    _validate_publishable_text(contents, str(path))


def redact_text(text: str, source_root: Path) -> str:
    redacted = _OSC_RE.sub("", text)
    redacted = _ANSI_RE.sub("", redacted)
    redacted = _CONTROL_RE.sub("", redacted)
    replacements = {
        str(source_root): "<SOURCE_ROOT>",
        source_root.as_posix(): "<SOURCE_ROOT>",
        str(source_root.resolve()): "<SOURCE_ROOT>",
        source_root.resolve().as_posix(): "<SOURCE_ROOT>",
        str(Path.home().resolve()): "<HOME>",
        Path.home().resolve().as_posix(): "<HOME>",
    }
    resolved_root = source_root.resolve().as_posix()
    if resolved_root.startswith("/private/"):
        replacements[resolved_root[len("/private"):]] = "<SOURCE_ROOT>"
    for value in sorted((item for item in replacements if item), key=len, reverse=True):
        redacted = redacted.replace(value, replacements[value])
        redacted = redacted.replace(value.replace("/", "\\"), replacements[value])
    redacted = _WINDOWS_HOME_RE.sub("<HOME>", redacted)
    redacted = _UNIX_HOME_RE.sub("<HOME>", redacted)
    redacted = _TEMP_PATH_RE.sub("<TEMP_PATH>", redacted)
    redacted = _PRIVATE_KEY_RE.sub("<REDACTED PRIVATE KEY>", redacted)
    redacted = _AUTHORIZATION_RE.sub(
        lambda match: f"{match.group(1)}<REDACTED>", redacted
    )
    for pattern in _SECRET_LITERAL_PATTERNS:
        redacted = pattern.sub("<REDACTED>", redacted)
    redacted = _KEY_VALUE_SECRET_RE.sub(
        lambda match: f"{match.group(1)}{match.group(2)}<REDACTED>", redacted
    )
    return redacted


def redact_log(args: argparse.Namespace) -> int:
    source = args.input.resolve()
    output = args.output.resolve()
    root = args.source_root.resolve()
    if source == output:
        raise ValueError("redacted output must be separate from the raw input")
    if not source.is_file():
        raise ValueError(f"raw log does not exist: {source}")
    if output.exists():
        raise ValueError(f"refusing to overwrite redacted log: {output}")
    original = source.read_text(encoding="utf-8", errors="replace")
    redacted = redact_text(original, root)
    for sensitive in (str(root), root.as_posix(), str(Path.home()), Path.home().as_posix()):
        if sensitive and sensitive in redacted:
            raise ValueError("redaction left a sensitive absolute path in the output")
    header = (
        "# FeedForge benchmark log: mechanically redacted copy\n"
        "# Mechanical checks are not exhaustive; review manually before publication.\n"
    )
    public_contents = header + redacted
    _validate_publishable_text(public_contents, str(output))
    atomic_text(output, public_contents)
    print(f"redacted log: {output}")
    return 0


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate, run, and compare FeedForge benchmark contract 2.0"
    )
    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    run_parser = subparsers.add_parser(
        "run", help="collect the frozen seven-process benchmark series"
    )
    run_parser.add_argument("--executable", required=True, type=Path)
    run_parser.add_argument("--output-dir", required=True, type=Path)
    run_parser.add_argument("--label", required=True)
    run_parser.add_argument("--source-id", required=True)
    run_parser.add_argument("--repeats", type=int, default=QUALIFIED_REPEATS)
    run_parser.add_argument("--samples", type=int, default=QUALIFIED_SAMPLES)
    run_parser.add_argument("--warmup", type=int, default=QUALIFIED_WARMUP)
    run_parser.add_argument("--batch", type=int, default=QUALIFIED_BATCH)
    run_parser.add_argument("--min-time-ms", type=float, default=QUALIFIED_MIN_TIME_MS)
    run_parser.add_argument(
        "--cooldown-seconds", type=int, default=QUALIFIED_COOLDOWN_SECONDS
    )
    run_parser.add_argument(
        "--correctness-command", default=shlex.join(FROZEN_CORRECTNESS_COMMAND)
    )
    run_parser.add_argument("--cwd", type=Path, default=Path.cwd())
    run_parser.add_argument(
        "--allow-unqualified",
        action="store_true",
        help="return success for diagnostic output; marks the series non-releasable",
    )
    run_parser.set_defaults(function=run_series)

    validate_parser = subparsers.add_parser(
        "validate", help="validate one raw benchmark artifact"
    )
    validate_parser.add_argument("--artifact", required=True, type=Path)
    validate_parser.add_argument("--csv", type=Path)
    validate_parser.set_defaults(function=validate_artifact)

    series_parser = subparsers.add_parser(
        "validate-series", help="rebuild a series from its hash-bound raw runs"
    )
    series_parser.add_argument("--series", required=True, type=Path)
    series_parser.add_argument("--runs-dir", required=True, type=Path)
    series_parser.add_argument("--cwd", type=Path, default=Path.cwd())
    series_parser.add_argument(
        "--verify-checkout",
        action="store_true",
        help="also require the current clean HEAD and local executable to match",
    )
    series_parser.set_defaults(function=validate_series_command)

    compare_parser = subparsers.add_parser(
        "compare", help="compare two independently validated qualified series"
    )
    compare_parser.add_argument("--baseline", required=True, type=Path)
    compare_parser.add_argument("--candidate", required=True, type=Path)
    compare_parser.add_argument("--target", action="append", required=True)
    compare_parser.add_argument("--output-json", type=Path)
    compare_parser.add_argument("--output-csv", type=Path)
    compare_parser.set_defaults(function=compare_series)

    redact_parser = subparsers.add_parser(
        "redact-log", help="write a separate mechanically redacted public log copy"
    )
    redact_parser.add_argument("--input", required=True, type=Path)
    redact_parser.add_argument("--output", required=True, type=Path)
    redact_parser.add_argument("--source-root", required=True, type=Path)
    redact_parser.set_defaults(function=redact_log)
    return parser


def main() -> int:
    try:
        args = make_parser().parse_args()
        return int(args.function(args))
    except (OSError, RuntimeError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"benchmark.py: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
