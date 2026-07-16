#!/usr/bin/env python3
"""Dependency-free FeedForge benchmark repeat runner and comparator."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from typing import Any, Iterable


MIN_REPEATS = 7
MIN_SAMPLES = 11
MIN_SAMPLE_TIME_MS = 50.0
MIN_MEDIAN_IMPROVEMENT = 0.05
MAX_CROSS_RUN_NORMALIZED_MAD = 0.03
MIN_ROBUST_MARGIN = 0.03
MAX_UNTARGETED_REGRESSION = 0.02
MAD_SCALE = 1.4826


def percentile(values: Iterable[float], quantile: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot summarize an empty sequence")
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


def atomic_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent, text=True
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write(text)
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
        json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            separators=(",", ":"),
            sort_keys=True,
        )
        + "\n",
    )


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as source:
        value = json.load(source)
    if not isinstance(value, dict):
        raise ValueError(f"{path} does not contain a JSON object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_identity(run: dict[str, Any]) -> dict[str, Any]:
    benchmarks = []
    for item in run["benchmarks"]:
        benchmarks.append(
            {
                "bytes_per_round": item["bytes_per_round"],
                "events_per_round": item["events_per_round"],
                "id": item["id"],
                "messages_per_round": item["messages_per_round"],
                "operation": item["operation"],
                "pipeline": item["pipeline"],
                "workload": item["workload"],
                "workload_sha256": item["workload_sha256"],
            }
        )
    config = run["config"]
    return {
        "benchmarks": benchmarks,
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


def validate_run(run: dict[str, Any], path: Path) -> None:
    if run.get("schema_version") != 1:
        raise ValueError(f"{path}: unsupported result schema")
    if run.get("contract_version") != "1.0.0":
        raise ValueError(f"{path}: unsupported benchmark contract")
    if run.get("publishable") is not False:
        raise ValueError(f"{path}: single-run artifact must be non-publishable")
    if not run.get("correctness", {}).get("verified"):
        raise ValueError(f"{path}: pre-timing correctness was not verified")
    if run.get("config", {}).get("clock_is_steady") is not True:
        raise ValueError(f"{path}: steady clock was not available")
    benchmarks = run.get("benchmarks")
    if not isinstance(benchmarks, list) or len(benchmarks) != 8:
        raise ValueError(f"{path}: expected the frozen eight benchmark cases")
    identifiers = [item["id"] for item in benchmarks]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError(f"{path}: duplicate benchmark identifiers")
    for item in benchmarks:
        if item["quality"]["implausible"]:
            raise ValueError(f"{path}: {item['id']} is implausible")
        samples = item.get("samples", [])
        if len(samples) != run["config"]["samples"]:
            raise ValueError(f"{path}: {item['id']} sample count mismatch")
        if len({sample["checksum"] for sample in samples}) != 1:
            raise ValueError(f"{path}: {item['id']} anti-elision checksum changed")
        for sample in samples:
            if sample["messages"] <= 0 or sample["bytes"] <= 0:
                raise ValueError(f"{path}: {item['id']} has an empty timed sample")


def series_csv(series: dict[str, Any]) -> str:
    fields = [
        "benchmark_id",
        "median_ns_per_message",
        "p05_ns_per_message",
        "p95_ns_per_message",
        "mad_ns_per_message",
        "normalized_mad",
        "comparison_ready",
        "repeat_count",
        "corpus_sha256",
        "compiler",
        "build_type",
        "os",
        "architecture",
        "cpu_model",
    ]
    stream = __import__("io").StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    identity = series["identity"]
    for item in series["benchmarks"]:
        writer.writerow(
            {
                "benchmark_id": item["id"],
                "median_ns_per_message": format(
                    item["ns_per_message"]["median"], ".17g"
                ),
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
            }
        )
    return stream.getvalue()


def build_series(
    runs: list[dict[str, Any]],
    run_files: list[str],
    label: str,
    command: list[str],
    correctness_command: list[str] | None,
    executable: Path,
    source_id: str,
) -> dict[str, Any]:
    identity = artifact_identity(runs[0])
    for index, run in enumerate(runs[1:], start=2):
        if artifact_identity(run) != identity:
            raise ValueError(
                f"run {index} changed corpus, correctness, build, host, config, or cases"
            )

    aggregate = []
    warnings: list[str] = []
    all_within_run_quiet = True
    for benchmark_index, template in enumerate(runs[0]["benchmarks"]):
        medians = [
            run["benchmarks"][benchmark_index]["statistics"]["ns_per_message"][
                "median"
            ]
            for run in runs
        ]
        summary = distribution(medians)
        normalized_mad = summary["mad"] / summary["median"]
        noisy_runs = [
            index + 1
            for index, run in enumerate(runs)
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
                f"{normalized_mad:.2%} exceeds "
                f"{MAX_CROSS_RUN_NORMALIZED_MAD:.0%}"
            )
        aggregate.append(
            {
                "id": template["id"],
                "normalized_mad": normalized_mad,
                "ns_per_message": summary,
                "run_medians_ns_per_message": medians,
            }
        )

    config = identity["config"]
    threshold_ready = (
        len(runs) >= MIN_REPEATS
        and config["samples"] >= MIN_SAMPLES
        and config["minimum_time_ms"] >= MIN_SAMPLE_TIME_MS
        and config["smoke"] is False
        and identity["build"]["build_type"] == "Release"
    )
    cross_run_quiet = all(
        item["normalized_mad"] <= MAX_CROSS_RUN_NORMALIZED_MAD
        for item in aggregate
    )
    comparison_ready = threshold_ready and cross_run_quiet and all_within_run_quiet
    if len(runs) < MIN_REPEATS:
        warnings.append(
            f"repeat count {len(runs)} is below the frozen minimum {MIN_REPEATS}"
        )
    if config["samples"] < MIN_SAMPLES:
        warnings.append(
            f"sample count {config['samples']} is below the frozen minimum "
            f"{MIN_SAMPLES}"
        )
    if config["minimum_time_ms"] < MIN_SAMPLE_TIME_MS:
        warnings.append(
            f"sample time {config['minimum_time_ms']} ms is below the frozen minimum "
            f"{MIN_SAMPLE_TIME_MS:g} ms"
        )
    warnings.append(
        "a baseline series alone is not a performance claim; compare a "
        "correctness-equivalent candidate"
    )

    return {
        "benchmarks": aggregate,
        "command": command,
        "comparison_ready": comparison_ready,
        "correctness_command": correctness_command,
        "executable": str(executable),
        "executable_sha256": sha256_file(executable),
        "identity": identity,
        "label": label,
        "repeat_count": len(runs),
        "run_files": run_files,
        "schema_version": 1,
        "source_id": source_id,
        "thresholds": {
            "max_cross_run_normalized_mad": MAX_CROSS_RUN_NORMALIZED_MAD,
            "max_untargeted_regression": MAX_UNTARGETED_REGRESSION,
            "min_median_improvement": MIN_MEDIAN_IMPROVEMENT,
            "min_repeats": MIN_REPEATS,
            "min_robust_margin": MIN_ROBUST_MARGIN,
            "min_sample_time_ms": MIN_SAMPLE_TIME_MS,
            "min_samples": MIN_SAMPLES,
        },
        "timestamp_utc": dt.datetime.now(dt.timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
        "warnings": warnings,
    }


def run_series(args: argparse.Namespace) -> int:
    executable = args.executable.resolve()
    if not executable.is_file():
        raise ValueError(f"benchmark executable does not exist: {executable}")
    if args.repeats <= 0:
        raise ValueError("--repeats must be positive")
    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ValueError(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    correctness_command = (
        shlex.split(args.correctness_command) if args.correctness_command else None
    )
    if correctness_command:
        print("running external correctness command:", shlex.join(correctness_command))
        completed = subprocess.run(
            correctness_command,
            cwd=args.cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        atomic_text(output_dir / "correctness.txt", completed.stdout)
        if completed.returncode != 0:
            raise RuntimeError(
                f"correctness command failed with exit code {completed.returncode}"
            )

    common = [
        str(executable),
        "--samples",
        str(args.samples),
        "--warmup",
        str(args.warmup),
        "--batch",
        str(args.batch),
        "--min-time-ms",
        format(args.min_time_ms, "g"),
    ]
    runs: list[dict[str, Any]] = []
    run_files: list[str] = []
    for index in range(1, args.repeats + 1):
        stem = f"run-{index:02d}"
        json_path = output_dir / f"{stem}.json"
        csv_path = output_dir / f"{stem}.csv"
        command = common + ["--json", str(json_path), "--csv", str(csv_path)]
        print(f"[{index}/{args.repeats}] {shlex.join(command)}")
        completed = subprocess.run(
            command,
            cwd=args.cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        atomic_text(output_dir / f"{stem}.txt", completed.stdout)
        if completed.returncode != 0:
            raise RuntimeError(
                f"{stem} failed with exit code {completed.returncode}; "
                f"see {output_dir / f'{stem}.txt'}"
            )
        run = load_json(json_path)
        validate_run(run, json_path)
        runs.append(run)
        run_files.append(json_path.name)

    series = build_series(
        runs,
        run_files,
        args.label,
        common,
        correctness_command,
        executable,
        args.source_id,
    )
    canonical_json(output_dir / "series.json", series)
    atomic_text(output_dir / "series.csv", series_csv(series))
    print()
    print(
        f"{args.label}: {len(runs)} repeats; "
        f"comparison_ready={str(series['comparison_ready']).lower()}"
    )
    for item in series["benchmarks"]:
        print(
            f"  {item['id']}: {item['ns_per_message']['median']:.3f} ns/message "
            f"(cross-run MAD {item['normalized_mad']:.2%})"
        )
    for warning in series["warnings"]:
        print("warning:", warning)
    print("series:", output_dir / "series.json")
    return 0


def comparable_identity(identity: dict[str, Any]) -> dict[str, Any]:
    return {
        "benchmarks": identity["benchmarks"],
        "build": identity["build"],
        "config": identity["config"],
        "contract_version": identity["contract_version"],
        "corpus_sha256": identity["corpus_sha256"],
        "correctness": identity["correctness"],
        "host": identity["host"],
        "schema_version": identity["schema_version"],
    }


def comparison_csv(comparison: dict[str, Any]) -> str:
    fields = [
        "benchmark_id",
        "targeted",
        "baseline_median_ns_per_message",
        "candidate_median_ns_per_message",
        "median_improvement",
        "noise_bound",
        "robust_margin",
        "regression",
        "passes_target",
        "passes_regression",
        "overall_win",
    ]
    stream = __import__("io").StringIO(newline="")
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
    baseline_path = args.baseline.resolve()
    candidate_path = args.candidate.resolve()
    baseline = load_json(baseline_path)
    candidate = load_json(candidate_path)
    if not baseline.get("comparison_ready"):
        raise ValueError(f"baseline series is not comparison-ready: {baseline_path}")
    if not candidate.get("comparison_ready"):
        raise ValueError(f"candidate series is not comparison-ready: {candidate_path}")
    if baseline.get("thresholds") != candidate.get("thresholds"):
        raise ValueError("baseline and candidate threshold contracts differ")
    if comparable_identity(baseline["identity"]) != comparable_identity(
        candidate["identity"]
    ):
        raise ValueError(
            "baseline and candidate differ in build, host, config, corpus, "
            "correctness, or benchmark contract"
        )
    if baseline.get("executable_sha256") == candidate.get("executable_sha256"):
        raise ValueError("baseline and candidate benchmark executables are identical")

    baseline_cases = {item["id"]: item for item in baseline["benchmarks"]}
    candidate_cases = {item["id"]: item for item in candidate["benchmarks"]}
    if baseline_cases.keys() != candidate_cases.keys():
        raise ValueError("baseline and candidate benchmark case sets differ")
    targets = set(args.target or baseline_cases.keys())
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
                and max(
                    before["normalized_mad"], after["normalized_mad"]
                )
                <= MAX_CROSS_RUN_NORMALIZED_MAD
                and robust_margin >= MIN_ROBUST_MARGIN
            )
        )
        passes_regression = regression <= MAX_UNTARGETED_REGRESSION
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
        "baseline": str(baseline_path),
        "baseline_executable_sha256": baseline["executable_sha256"],
        "baseline_source_id": baseline["source_id"],
        "benchmarks": rows,
        "candidate": str(candidate_path),
        "candidate_executable_sha256": candidate["executable_sha256"],
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
            f"  [{marker}] {item['id']}: "
            f"improvement={item['median_improvement']:.2%} "
            f"robust_margin={item['robust_margin']:.2%} "
            f"regression={item['regression']:.2%}"
        )
    return 0 if optimization_win else 1


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run and compare the frozen FeedForge benchmark contract"
    )
    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    run_parser = subparsers.add_parser(
        "run", help="collect an independent repeated benchmark series"
    )
    run_parser.add_argument("--executable", required=True, type=Path)
    run_parser.add_argument("--output-dir", required=True, type=Path)
    run_parser.add_argument("--label", default="baseline")
    run_parser.add_argument(
        "--source-id",
        default="uncommitted-working-tree",
        help="commit, patch, or other immutable source identifier",
    )
    run_parser.add_argument("--repeats", type=int, default=MIN_REPEATS)
    run_parser.add_argument("--samples", type=int, default=15)
    run_parser.add_argument("--warmup", type=int, default=5)
    run_parser.add_argument("--batch", type=int, default=256)
    run_parser.add_argument("--min-time-ms", type=float, default=50.0)
    run_parser.add_argument(
        "--correctness-command",
        help="external command run once before timing, for example 'ctest --preset bench'",
    )
    run_parser.add_argument(
        "--cwd",
        type=Path,
        default=Path.cwd(),
        help="working directory for correctness and benchmark commands",
    )
    run_parser.set_defaults(function=run_series)

    compare_parser = subparsers.add_parser(
        "compare", help="compare baseline and candidate series"
    )
    compare_parser.add_argument("--baseline", required=True, type=Path)
    compare_parser.add_argument("--candidate", required=True, type=Path)
    compare_parser.add_argument(
        "--target",
        action="append",
        help="benchmark ID declared as an optimization target; default is all",
    )
    compare_parser.add_argument("--output-json", type=Path)
    compare_parser.add_argument("--output-csv", type=Path)
    compare_parser.set_defaults(function=compare_series)
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
