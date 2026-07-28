#!/usr/bin/env python3
"""Mutation tests for the frozen benchmark evidence contract."""

from __future__ import annotations

import copy
import csv
import io
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

import benchmark


SOURCE_A = "a" * 40
SOURCE_B = "b" * 40
EXECUTABLE_A = "1" * 64
EXECUTABLE_B = "2" * 64


def fixture_corpus() -> dict[str, object]:
    fixtures = []
    for frozen in benchmark.FROZEN_FIXTURES:
        file_name, message_name, message_type, selected, digest, size = frozen
        fixtures.append(
            {
                "byte_source": benchmark.FROZEN_BYTE_SOURCE,
                "file": file_name,
                "message_name": message_name,
                "message_type": message_type,
                "order_events_selected": selected,
                "review_status": benchmark.FROZEN_REVIEW_STATUS,
                "reviewer": benchmark.FROZEN_REVIEWER,
                "sha256": digest,
                "size": size,
            }
        )
    return {
        "fixture_count": 23,
        "fixtures": fixtures,
        "sha256": benchmark.CORPUS_SHA256,
        "source": "independently reviewed tests/fixtures/itch50 raw_hex",
    }


def make_run(
    *,
    source_id: str = SOURCE_A,
    run_offset: int = 0,
    speed: float = 1.0,
    samples: int = benchmark.QUALIFIED_SAMPLES,
    warmup: int = benchmark.QUALIFIED_WARMUP,
    batch: int = benchmark.QUALIFIED_BATCH,
    minimum_time_ms: float = benchmark.QUALIFIED_MIN_TIME_MS,
    smoke: bool = False,
    run_name: str = "run-01",
    output_directory: str = "build/bench/results/qualified",
) -> dict[str, object]:
    config = {
        "batch": batch,
        "clock": "std::chrono::steady_clock",
        "clock_is_steady": True,
        "minimum_time_ms": minimum_time_ms,
        "samples": samples,
        "smoke": smoke,
        "timer_resolution_ns": 10,
        "warmup": warmup,
    }
    cases = []
    for case_index, frozen in enumerate(benchmark.FROZEN_CASES, start=1):
        item = copy.deepcopy(frozen)
        rounds = batch * 4
        case_samples = []
        for sample_index in range(samples):
            elapsed = int(
                (60_000_000 + case_index * 100_000 + run_offset + sample_index * 100)
                * speed
            )
            case_samples.append(
                {
                    "bytes": frozen["bytes_per_round"] * rounds,
                    "checksum": f"0x{case_index + 100:x}",
                    "elapsed_ns": elapsed,
                    "events": frozen["events_per_round"] * rounds,
                    "finish_calls": frozen["finish_calls_per_round"] * rounds,
                    "messages": frozen["messages_per_round"] * rounds,
                    "pushes": frozen["pushes_per_round"] * rounds,
                    "rounds": rounds,
                }
            )
        item["anti_elision_checksum"] = case_samples[0]["checksum"]
        item["rounds_per_sample"] = rounds
        item["samples"] = case_samples
        statistics, quality = benchmark._derive_case(item, config)
        item["statistics"] = statistics
        item["quality"] = quality
        cases.append(item)
    executable = "build/bench/benchmarks/feedforge_benchmark"
    arguments = benchmark._expected_benchmark_base(executable, config) + [
        "--json",
        f"{output_directory}/{run_name}.json",
        "--csv",
        f"{output_directory}/{run_name}.csv",
    ]
    return {
        "benchmarks": cases,
        "build": {
            "build_type": "Release",
            "compiler_builtin": "test compiler",
            "compiler_id": "Clang",
            "compiler_path": "/usr/bin/clang++",
            "compiler_version": "21.0.0",
            "config_flags": "-O3 -DNDEBUG",
            "cxx_standard": 202002,
            "feedforge_version": "1.0.0",
            "generator": "Ninja",
            "interprocedural_optimization": "OFF",
            "pipeline_fingerprints": copy.deepcopy(benchmark.PIPELINE_FINGERPRINTS),
            "schema_fingerprint": benchmark.SCHEMA_FINGERPRINT,
            "source_dirty": False,
            "source_revision": source_id,
            "target_flags": "-Wall -Wextra warnings-as-errors",
        },
        "command": {
            "argv": arguments,
            "joined": benchmark.join_command(arguments),
            "working_directory": ".",
        },
        "config": config,
        "contract_version": benchmark.CONTRACT_VERSION,
        "corpus": fixture_corpus(),
        "correctness": copy.deepcopy(benchmark.CORRECTNESS),
        "host": {
            "architecture": "arm64",
            "cpu_affinity": "scheduler-controlled",
            "cpu_governor": "unavailable",
            "cpu_model": "Test CPU",
            "kernel": "Test 1.0",
            "limitations": ["synthetic unit-test host"],
            "logical_cpus": 8,
            "machine_model": "Test Machine",
            "memory_bytes": 16 * 1024 * 1024 * 1024,
            "os": "TestOS",
            "physical_cpus": 8,
            "turbo_state": "unavailable",
        },
        "publishable": False,
        "schema_version": benchmark.RESULT_SCHEMA_VERSION,
        "timestamp_utc": "2026-07-28T12:00:00Z",
        "warnings": ["single process run is diagnostic only"],
    }


def raw_csv(run: dict[str, object]) -> str:
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(
        stream, fieldnames=benchmark.RAW_CSV_FIELDS, lineterminator="\n"
    )
    writer.writeheader()
    for item in run["benchmarks"]:
        writer.writerow(benchmark._raw_csv_expected(run, item))
    return stream.getvalue()


def write_series(
    directory: Path,
    *,
    source_id: str = SOURCE_A,
    executable_sha256: str = EXECUTABLE_A,
    speed: float = 1.0,
    diagnostic_only: bool = False,
    label: str = "qualified",
) -> dict[str, object]:
    directory.mkdir(parents=True, exist_ok=True)
    correctness_path = directory / "correctness.txt"
    benchmark.atomic_text(correctness_path, "correctness passed\n")
    runs = []
    records = []
    for index in range(1, benchmark.QUALIFIED_REPEATS + 1):
        stem = f"run-{index:02d}"
        run = make_run(
            source_id=source_id,
            run_offset=index * 1_000,
            speed=speed,
            run_name=stem,
        )
        benchmark.canonical_json(directory / f"{stem}.json", run)
        benchmark.atomic_text(directory / f"{stem}.csv", raw_csv(run))
        benchmark.atomic_text(directory / f"{stem}.txt", f"run {index} passed\n")
        runs.append(run)
        records.append(benchmark._raw_record(directory, stem))
    correctness = {
        "command": copy.deepcopy(benchmark.FROZEN_CORRECTNESS_COMMAND),
        "exit_code": 0,
        "log": "correctness.txt",
        "log_sha256": benchmark.sha256_file(correctness_path),
    }
    series = benchmark.build_series(
        runs,
        records,
        label=label,
        command=benchmark._expected_benchmark_base(
            "build/bench/benchmarks/feedforge_benchmark", runs[0]["config"]
        ),
        correctness=correctness,
        executable="build/bench/benchmarks/feedforge_benchmark",
        executable_sha256_before=executable_sha256,
        executable_sha256_after=executable_sha256,
        source_id=source_id,
        cooldown_seconds=benchmark.QUALIFIED_COOLDOWN_SECONDS,
        diagnostic_only=diagnostic_only,
        timestamp_utc="2026-07-28T13:00:00Z",
    )
    benchmark.canonical_json(directory / "series.json", series)
    benchmark.atomic_text(directory / "series.csv", benchmark.series_csv(series))
    return series


class RunValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.run = make_run()
        self.path = Path("synthetic-run.json")

    def assert_invalid(self, run: dict[str, object], message: str) -> None:
        with self.assertRaisesRegex(ValueError, message):
            benchmark.validate_run(run, self.path)

    def test_valid_exact_contract(self) -> None:
        benchmark.validate_run(
            self.run,
            self.path,
            expected_source_id=SOURCE_A,
            require_clean_source=True,
        )

    def test_exact_case_order_and_metadata_are_pinned(self) -> None:
        swapped = copy.deepcopy(self.run)
        swapped["benchmarks"][0], swapped["benchmarks"][1] = (
            swapped["benchmarks"][1],
            swapped["benchmarks"][0],
        )
        self.assert_invalid(swapped, "changed from the frozen contract")

        schedule = copy.deepcopy(self.run)
        schedule["benchmarks"][8]["schedule_sha256"] = "0" * 64
        self.assert_invalid(schedule, "schedule_sha256 changed")

        workload = copy.deepcopy(self.run)
        workload["benchmarks"][0]["workload_sha256"] = "0" * 64
        self.assert_invalid(workload, "workload_sha256 changed")

    def test_sample_counters_and_checksum_are_derived(self) -> None:
        counter = copy.deepcopy(self.run)
        counter["benchmarks"][10]["samples"][0]["pushes"] += 1
        self.assert_invalid(counter, "sample pushes changed")

        checksum = copy.deepcopy(self.run)
        checksum["benchmarks"][0]["samples"][1]["checksum"] = "0xbeef"
        self.assert_invalid(checksum, "anti-elision checksum changed")

        zero = copy.deepcopy(self.run)
        for sample in zero["benchmarks"][0]["samples"]:
            sample["checksum"] = "0x0"
        zero["benchmarks"][0]["anti_elision_checksum"] = "0x0"
        self.assert_invalid(zero, "checksum is zero")

    def test_statistics_and_quality_are_recomputed_from_samples(self) -> None:
        statistics = copy.deepcopy(self.run)
        statistics["benchmarks"][0]["statistics"]["ns_per_message"]["median"] += 1
        self.assert_invalid(statistics, "does not match derived")

        quality = copy.deepcopy(self.run)
        quality["benchmarks"][0]["quality"]["noisy"] = True
        self.assert_invalid(quality, "derived quality.noisy changed")

        implausible = copy.deepcopy(self.run)
        for sample in implausible["benchmarks"][0]["samples"]:
            sample["elapsed_ns"] = 49_000_000
        stats, derived_quality = benchmark._derive_case(
            implausible["benchmarks"][0], implausible["config"]
        )
        implausible["benchmarks"][0]["statistics"] = stats
        implausible["benchmarks"][0]["quality"] = derived_quality
        self.assert_invalid(implausible, "is implausible")

    def test_build_source_and_isa_guards(self) -> None:
        dirty = copy.deepcopy(self.run)
        dirty["build"]["source_dirty"] = True
        with self.assertRaisesRegex(ValueError, "dirty source tree"):
            benchmark.validate_run(
                dirty, self.path, expected_source_id=SOURCE_A,
                require_clean_source=True,
            )

        wrong_source = copy.deepcopy(self.run)
        wrong_source["build"]["source_revision"] = SOURCE_B
        with self.assertRaisesRegex(ValueError, "requested SHA"):
            benchmark.validate_run(
                wrong_source, self.path, expected_source_id=SOURCE_A,
                require_clean_source=True,
            )

        forbidden_flags = (
            "-march=native", "-mcpu=apple-m4", "-mtune=native", "-mavx2",
            "-mavx512f", "-mno-sse2", "-msse4.2", "-msve2", "/arch:AVX512",
            "-Xclang -target-feature +sve", "-Wa,-march=armv9-a", "-qarch=pwr10",
            "-flto", "-flto=thin", "/GL", "/LTCG", "-ipo",
        )
        for flag in forbidden_flags:
            with self.subTest(flag=flag):
                changed = copy.deepcopy(self.run)
                changed["build"]["target_flags"] += f" {flag}"
                self.assert_invalid(changed, "explicit LTO flag is forbidden")
        no_lto = copy.deepcopy(self.run)
        no_lto["build"]["target_flags"] += " -fno-lto /LTCG:OFF"
        benchmark.validate_run(no_lto, self.path)

        cxx23 = copy.deepcopy(self.run)
        cxx23["build"]["cxx_standard"] = 202302
        self.assert_invalid(cxx23, r"frozen C\+\+20 mode")

    def test_corpus_command_and_numeric_encoding_are_strict(self) -> None:
        corpus = copy.deepcopy(self.run)
        corpus["corpus"]["sha256"] = "0" * 64
        self.assert_invalid(corpus, "frozen corpus hash changed")

        absolute = copy.deepcopy(self.run)
        absolute["command"]["argv"][0] = "/tmp/feedforge_benchmark"
        absolute["command"]["joined"] = benchmark.join_command(
            absolute["command"]["argv"]
        )
        self.assert_invalid(absolute, "path must remain relative")

        for executable in ("C:outside.exe", "."):
            with self.subTest(executable=executable):
                unsafe = copy.deepcopy(self.run)
                unsafe["command"]["argv"][0] = executable
                unsafe["command"]["joined"] = benchmark.join_command(
                    unsafe["command"]["argv"]
                )
                self.assert_invalid(unsafe, "path must remain relative")

        joined = copy.deepcopy(self.run)
        joined["command"]["joined"] += " --forged"
        self.assert_invalid(joined, "does not match argv")

        review = copy.deepcopy(self.run)
        review["corpus"]["fixtures"][0]["reviewer"] = "someone-else"
        self.assert_invalid(review, "review metadata changed")

        for timestamp in (
            "2026-07-28T12:00:00+00:00",
            "2026-02-30T12:00:00Z",
            "2026-07-28Z",
        ):
            with self.subTest(timestamp=timestamp):
                changed = copy.deepcopy(self.run)
                changed["timestamp_utc"] = timestamp
                self.assert_invalid(changed, "RFC3339 UTC")

        boolean_counter = copy.deepcopy(self.run)
        boolean_counter["benchmarks"][0]["samples"][0]["elapsed_ns"] = True
        self.assert_invalid(boolean_counter, "expected an integer")

    def test_run_argv_is_frozen_and_bound_to_config(self) -> None:
        changed = copy.deepcopy(self.run)
        sample_value = changed["command"]["argv"].index("--samples") + 1
        changed["command"]["argv"][sample_value] = "16"
        changed["command"]["joined"] = benchmark.join_command(
            changed["command"]["argv"]
        )
        self.assert_invalid(changed, "frozen canonical order")

        removed = copy.deepcopy(self.run)
        warmup = removed["command"]["argv"].index("--warmup")
        del removed["command"]["argv"][warmup:warmup + 2]
        removed["command"]["joined"] = benchmark.join_command(
            removed["command"]["argv"]
        )
        self.assert_invalid(removed, "option set does not match config")

        duplicate = copy.deepcopy(self.run)
        duplicate["command"]["argv"].extend(["--batch", "256"])
        duplicate["command"]["joined"] = benchmark.join_command(
            duplicate["command"]["argv"]
        )
        self.assert_invalid(duplicate, "duplicate benchmark option --batch")

        config_drift = copy.deepcopy(self.run)
        config_drift["config"]["batch"] = 512
        self.assert_invalid(config_drift, "frozen canonical order")

    def test_canonical_object_key_sets_reject_extensions(self) -> None:
        mutations = (
            ((), "top-level result fields changed"),
            (("build",), "build fields changed"),
            (("config",), "config fields changed"),
            (("corpus",), "corpus fields changed"),
            (("host",), "host fields changed"),
            (("command",), "command fields changed"),
        )
        for path, message in mutations:
            with self.subTest(path=path):
                changed = copy.deepcopy(self.run)
                target = changed
                for element in path:
                    target = target[element]
                target["unexpected_extension"] = True
                self.assert_invalid(changed, message)

    def test_nonfinite_json_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "nan.json"
            path.write_text('{"value":NaN}\n', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-finite JSON"):
                benchmark.load_json(path)

    def test_duplicate_json_keys_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "duplicate.json"
            path.write_text(
                '{"schema_version":1,"nested":{"id":1,"id":2}}\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: id"):
                benchmark.load_json(path)

    def test_validate_artifact_rejects_a_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            target = directory / "target.json"
            benchmark.canonical_json(target, self.run)
            artifact = directory / "artifact.json"
            artifact.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "symlinked JSON evidence"):
                benchmark.validate_artifact(SimpleNamespace(artifact=artifact))


class RawCsvValidationTest(unittest.TestCase):
    def test_valid_csv_and_mutated_projection(self) -> None:
        run = make_run()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "run.csv"
            benchmark.atomic_text(path, raw_csv(run))
            benchmark.validate_raw_csv(path, run)
            contents = path.read_text(encoding="utf-8")
            benchmark.atomic_text(path, contents.replace(",694,23,23,", ",695,23,23,", 1))
            with self.assertRaisesRegex(ValueError, "does not match raw JSON"):
                benchmark.validate_raw_csv(path, run)

    def test_extra_trailing_csv_cell_is_rejected(self) -> None:
        run = make_run()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "run.csv"
            lines = raw_csv(run).splitlines()
            lines[1] += ",forged"
            benchmark.atomic_text(path, "\n".join(lines) + "\n")
            with self.assertRaisesRegex(ValueError, "extra or missing cells"):
                benchmark.validate_raw_csv(path, run)

    def test_validate_cli_checks_the_direct_csv_pair_and_rejects_a_symlink(self) -> None:
        run = make_run()
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            artifact = directory / "run.json"
            csv_path = directory / "run.csv"
            benchmark.canonical_json(artifact, run)
            benchmark.atomic_text(csv_path, raw_csv(run))
            self.assertEqual(
                benchmark.validate_artifact(
                    SimpleNamespace(artifact=artifact, csv=csv_path)
                ),
                0,
            )
            target = directory / "csv-target.csv"
            csv_path.rename(target)
            csv_path.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "symlinked CSV evidence"):
                benchmark.validate_artifact(
                    SimpleNamespace(artifact=artifact, csv=csv_path)
                )


class SeriesValidationTest(unittest.TestCase):
    def test_valid_series_rebuilds_from_all_raw_runs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            self.assertTrue(series["qualification"]["qualified"])
            loaded = benchmark.validate_series_path(
                directory / "series.json", directory, verify_current_checkout=False
            )
            self.assertEqual(loaded, series)

    def test_portable_validation_needs_no_checkout_or_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            self.assertFalse((directory / "build/bench/benchmarks/feedforge_benchmark").exists())
            result = benchmark.validate_series_command(
                SimpleNamespace(
                    series=directory / "series.json",
                    runs_dir=directory,
                    verify_checkout=False,
                    cwd=Path.cwd(),
                )
            )
            self.assertEqual(result, 0)
            with self.assertRaises(ValueError):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=True,
                )

    def test_explicit_checkout_root_is_used_for_local_verification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "checkout"
            executable = root / "build/bench/benchmarks/feedforge_benchmark"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"frozen executable")
            directory = root / "evidence"
            write_series(
                directory,
                executable_sha256=benchmark.sha256_file(executable),
            )
            with (
                mock.patch.object(
                    benchmark, "repository_root", return_value=root
                ) as repository_root,
                mock.patch.object(benchmark, "verify_checkout") as verify_checkout,
            ):
                benchmark.validate_series_path(
                    directory / "series.json",
                    directory,
                    verify_current_checkout=True,
                    checkout_root=root,
                )
            repository_root.assert_called_once_with(root)
            verify_checkout.assert_called_once_with(root, SOURCE_A)

    def test_portable_validation_requires_hash_bound_sanitized_logs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            path = directory / "run-01.txt"
            benchmark.atomic_text(
                path,
                "Authorization: Bearer "
                "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0."
                "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c\n",
            )
            series["run_files"][0]["log_sha256"] = benchmark.sha256_file(path)
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "credential-like"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_raw_json_hash_and_unexpected_run_reject_selection(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            with (directory / "run-01.json").open("a", encoding="utf-8") as output:
                output.write(" \n")
            with self.assertRaisesRegex(ValueError, "artifact hash changed"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_raw_command_is_bound_to_series_and_exact_artifact_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            path = directory / "run-01.json"
            run = benchmark.load_json(path)
            json_value = run["command"]["argv"].index("--json") + 1
            csv_value = run["command"]["argv"].index("--csv") + 1
            run["command"]["argv"][json_value] = (
                "build/bench/results/qualified/run-99.json"
            )
            run["command"]["argv"][csv_value] = (
                "build/bench/results/qualified/run-99.csv"
            )
            run["command"]["joined"] = benchmark.join_command(
                run["command"]["argv"]
            )
            benchmark.canonical_json(path, run)
            series["run_files"][0]["json_sha256"] = benchmark.sha256_file(path)
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "series/raw paths"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_duplicate_series_keys_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            path = directory / "series.json"
            contents = path.read_text(encoding="utf-8").rstrip()
            benchmark.atomic_text(path, contents[:-1] + ',"schema_version":1}\n')
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: schema_version"):
                benchmark.validate_series_path(
                    path, directory, verify_current_checkout=False
                )

    def test_symlinked_evidence_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            raw = directory / "run-01.json"
            target = directory / "raw-target.json"
            raw.rename(target)
            raw.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "symlinked evidence"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            series = directory / "series.json"
            target = directory / "series-target.json"
            series.rename(target)
            series.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "must not be a symlink"):
                benchmark.validate_series_path(
                    series, directory, verify_current_checkout=False
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            write_series(directory)
            benchmark.atomic_text(directory / "run-08.json", "{}\n")
            with self.assertRaisesRegex(ValueError, "raw run set changed"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_csv_is_cross_checked_even_if_its_hash_is_rebound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            path = directory / "run-01.csv"
            text = path.read_text(encoding="utf-8")
            benchmark.atomic_text(path, text.replace(",694,23,23,", ",695,23,23,", 1))
            series["run_files"][0]["csv_sha256"] = benchmark.sha256_file(path)
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "does not match raw JSON"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_aggregate_and_correctness_claim_mutations_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            series["benchmarks"][0]["normalized_mad"] = 0.5
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "aggregate does not rebuild"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            series["identity"] = None
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "identity: expected an object"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            series["unexpected_extension"] = True
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "top-level series fields changed"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory)
            series["correctness"]["exit_code"] = 1
            benchmark.canonical_json(directory / "series.json", series)
            with self.assertRaisesRegex(ValueError, "did not pass"):
                benchmark.validate_series_path(
                    directory / "series.json", directory,
                    verify_current_checkout=False,
                )

    def test_diagnostic_escape_is_never_qualified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            series = write_series(directory, diagnostic_only=True)
            self.assertFalse(series["comparison_ready"])
            self.assertFalse(series["qualification"]["qualified"])
            validated = benchmark.validate_series_path(
                directory / "series.json", directory, verify_current_checkout=False
            )
            self.assertTrue(validated["diagnostic_only"])

    def test_frozen_cooldown_and_correctness_command_gate_readiness(self) -> None:
        runs = [
            make_run(run_name=f"run-{index:02d}")
            for index in range(1, benchmark.QUALIFIED_REPEATS + 1)
        ]
        records = [
            {
                "json": f"run-{index:02d}.json", "json_sha256": "1" * 64,
                "csv": f"run-{index:02d}.csv", "csv_sha256": "2" * 64,
                "log": f"run-{index:02d}.txt", "log_sha256": "3" * 64,
            }
            for index in range(1, benchmark.QUALIFIED_REPEATS + 1)
        ]
        series = benchmark.build_series(
            runs, records, label="bad-cooldown",
            command=benchmark._expected_benchmark_base(
                "build/bench/benchmarks/feedforge_benchmark", runs[0]["config"]
            ),
            correctness={
                "command": ["ctest"], "exit_code": 0,
                "log": "correctness.txt", "log_sha256": "4" * 64,
            },
            executable="build/bench/benchmarks/feedforge_benchmark",
            executable_sha256_before=EXECUTABLE_A,
            executable_sha256_after=EXECUTABLE_A,
            source_id=SOURCE_A,
            cooldown_seconds=119,
            diagnostic_only=False,
        )
        self.assertFalse(series["comparison_ready"])
        self.assertFalse(series["qualification"]["checks"]["cooldown_exact"])
        self.assertFalse(
            series["qualification"]["checks"]["correctness_command_exact"]
        )

    def test_correctness_environment_removes_make_overrides(self) -> None:
        environment = {
            "PATH": "/usr/bin",
            "CUSTOM": "preserved",
            "MAKEFLAGS": "-n CTEST=true",
            "MFLAGS": "-n",
            "GNUMAKEFLAGS": "-n",
            "MAKEFILES": "/tmp/injected.mk",
            "CTEST": "true",
            "CTEST_ARGS": "-R one-test",
            "CMAKE_ARGS": "-DBUILD_TESTING=OFF",
            "PRESET": "fast",
        }
        sanitized = benchmark._correctness_environment(environment)
        self.assertEqual(sanitized, {"PATH": "/usr/bin", "CUSTOM": "preserved"})
        self.assertIn("MAKEFLAGS", environment)


class ComparisonTest(unittest.TestCase):
    def arguments(self, baseline: Path, candidate: Path) -> SimpleNamespace:
        return SimpleNamespace(
            baseline=baseline,
            candidate=candidate,
            target=[benchmark.FROZEN_CASES[0]["id"]],
            output_json=None,
            output_csv=None,
        )

    def test_comparator_validates_both_series_and_distinct_shas(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline_dir = root / "baseline"
            candidate_dir = root / "candidate"
            write_series(
                baseline_dir,
                source_id=SOURCE_A,
                executable_sha256=EXECUTABLE_A,
                label="baseline-label",
            )
            write_series(
                candidate_dir,
                source_id=SOURCE_B,
                executable_sha256=EXECUTABLE_B,
                speed=0.90,
                label="candidate-label",
            )
            arguments = self.arguments(
                baseline_dir / "series.json", candidate_dir / "series.json"
            )
            arguments.output_json = root / "comparison.json"
            result = benchmark.compare_series(arguments)
            self.assertEqual(result, 0)
            comparison = benchmark.load_json(arguments.output_json)
            self.assertEqual(comparison["baseline"], f"baseline-label@{SOURCE_A}")
            self.assertEqual(comparison["candidate"], f"candidate-label@{SOURCE_B}")
            self.assertEqual(comparison["baseline_file"], "series.json")
            self.assertEqual(comparison["candidate_file"], "series.json")
            self.assertEqual(comparison["baseline_label"], "baseline-label")
            self.assertEqual(comparison["candidate_label"], "candidate-label")

            with (baseline_dir / "run-01.json").open("a", encoding="utf-8") as output:
                output.write(" \n")
            with self.assertRaisesRegex(ValueError, "artifact hash changed"):
                benchmark.compare_series(
                    self.arguments(
                        baseline_dir / "series.json", candidate_dir / "series.json"
                    )
                )

    def test_same_source_or_executable_sha_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline_dir = root / "baseline"
            candidate_dir = root / "candidate"
            write_series(baseline_dir, source_id=SOURCE_A, executable_sha256=EXECUTABLE_A)
            write_series(candidate_dir, source_id=SOURCE_A, executable_sha256=EXECUTABLE_B)
            with self.assertRaisesRegex(ValueError, "source SHAs must be distinct"):
                benchmark.compare_series(
                    self.arguments(
                        baseline_dir / "series.json", candidate_dir / "series.json"
                    )
                )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline_dir = root / "baseline"
            candidate_dir = root / "candidate"
            write_series(baseline_dir, source_id=SOURCE_A, executable_sha256=EXECUTABLE_A)
            write_series(candidate_dir, source_id=SOURCE_B, executable_sha256=EXECUTABLE_A)
            with self.assertRaisesRegex(ValueError, "executable SHAs must be distinct"):
                benchmark.compare_series(
                    self.arguments(
                        baseline_dir / "series.json", candidate_dir / "series.json"
                    )
                )

    def test_parser_requires_explicit_target(self) -> None:
        parser = benchmark.make_parser()
        with self.assertRaises(SystemExit):
            parser.parse_args(
                ["compare", "--baseline", "a.json", "--candidate", "b.json"]
            )
        validate_args = parser.parse_args(
            ["validate-series", "--series", "series.json", "--runs-dir", "."]
        )
        self.assertFalse(validate_args.verify_checkout)


class RedactionTest(unittest.TestCase):
    def test_redaction_writes_a_separate_review_copy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            root = directory / "checkout" / "FeedForge"
            root.mkdir(parents=True)
            source = directory / "correctness.txt"
            output = directory / "correctness.public.txt"
            secret = "ghp_ABCDEFGHIJKLMNOPQRSTUVWXYZ123456"
            fine_grained = "github_pat_ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
            jwt = (
                "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0."
                "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"
            )
            private_key = (
                "-----BEGIN PRIVATE KEY-----\n"
                "c2Vuc2l0aXZlLWtleS1tYXRlcmlhbA==\n"
                "-----END PRIVATE KEY-----"
            )
            original = (
                f"build: {root}/build/dev/output.txt\n"
                f"home: {Path.home()}/private.txt\n"
                "foreign mac: /Users/another-user/project/output.txt\n"
                "foreign linux: /home/another-user/project/output.txt\n"
                "temp: /private/tmp/feedforge/repro.txt\n"
                "\x1b]0;private terminal title\x07"
                "\x9dc1 private terminal title\x9c"
                "\x9b31mc1 color\x9b0m\n"
                "control:\x00hidden\x1ftext\n"
                f"token={secret}\n"
                f"fine={fine_grained}\n"
                f"Authorization: Bearer {jwt}\n"
                f"{private_key}\n"
            )
            benchmark.atomic_text(source, original)
            result = benchmark.redact_log(
                SimpleNamespace(input=source, output=output, source_root=root)
            )
            self.assertEqual(result, 0)
            self.assertEqual(source.read_text(encoding="utf-8"), original)
            public = output.read_text(encoding="utf-8")
            self.assertIn("<SOURCE_ROOT>", public)
            self.assertIn("<HOME>", public)
            self.assertIn("<TEMP_PATH>", public)
            self.assertIn("token=<REDACTED>", public)
            self.assertIn("Authorization: <REDACTED>", public)
            self.assertIn("<REDACTED PRIVATE KEY>", public)
            self.assertNotIn(secret, public)
            self.assertNotIn(fine_grained, public)
            self.assertNotIn(jwt, public)
            self.assertNotIn("c2Vuc2l0aXZlLWtleS1tYXRlcmlhbA", public)
            self.assertNotIn("another-user", public)
            self.assertNotIn("private terminal title", public)
            self.assertNotIn("c1 private terminal title", public)
            self.assertNotIn("\x00", public)
            self.assertNotIn("\x1f", public)
            self.assertNotIn("\x9b", public)
            self.assertNotIn("\x9d", public)
            self.assertNotIn("\x9c", public)
            self.assertNotIn(str(root), public)
            with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
                benchmark.redact_log(
                    SimpleNamespace(input=source, output=output, source_root=root)
                )

    def test_redact_log_self_validates_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / "raw.txt"
            output = directory / "public.txt"
            benchmark.atomic_text(source, "ordinary log\n")
            with mock.patch.object(
                benchmark, "redact_text", return_value="/home/leaked/secret\n"
            ):
                with self.assertRaisesRegex(ValueError, "user home path"):
                    benchmark.redact_log(
                        SimpleNamespace(
                            input=source,
                            output=output,
                            source_root=directory,
                        )
                    )
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
