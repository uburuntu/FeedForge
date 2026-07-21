#!/usr/bin/env python3
"""Black-box checks for the synthetic conformance bundle generator."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib
from typing import Any, Iterable
import zipfile


BUNDLE_NAME = "feedforge-itch50-conformance-v1"
LICENSE_SHA256 = "1525f5b9e493146a721e1c96960518edd3a7a06499684d28745010ac00f919df"


def check(condition: bool, description: str) -> None:
    if not condition:
        raise AssertionError(description)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def files_under(root: Path) -> dict[str, bytes]:
    return {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


def directories_under(root: Path) -> set[str]:
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_dir()
    }


def generator_command(args: argparse.Namespace, output_dir: Path, *,
                      fixtures_dir: Path | None = None,
                      source_lock: Path | None = None,
                      license_path: Path | None = None,
                      schema: Path | None = None,
                      all_messages_pipeline: Path | None = None,
                      order_events_pipeline: Path | None = None) -> list[str]:
    return [
        sys.executable,
        str(args.generator),
        "--fixtures-dir",
        str(fixtures_dir or args.fixtures_dir),
        "--source-lock",
        str(source_lock or args.source_lock),
        "--license",
        str(license_path or args.license_path),
        "--schema",
        str(schema or args.schema),
        "--all-messages-pipeline",
        str(all_messages_pipeline or args.all_messages_pipeline),
        "--order-events-pipeline",
        str(order_events_pipeline or args.order_events_pipeline),
        "--output-dir",
        str(output_dir),
    ]


def run_generator(args: argparse.Namespace, output_dir: Path) -> None:
    subprocess.run(
        generator_command(args, output_dir),
        check=True,
        stdout=subprocess.DEVNULL,
    )


def read_json(files: dict[str, bytes], path: str) -> Any:
    data = files[path]
    check(data.endswith(b"\n"), f"JSON final newline: {path}")
    check(data.isascii(), f"JSON ASCII-compatible encoding: {path}")
    value = json.loads(data)
    canonical = (
        json.dumps(value, allow_nan=False, ensure_ascii=True, indent=2, sort_keys=True) + "\n"
    ).encode("ascii")
    check(data == canonical, f"canonical JSON encoding: {path}")
    return value


def pipeline_decisions(pipeline_path: Path,
                       schema_messages: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    with pipeline_path.open("rb") as pipeline_file:
        pipeline = tomllib.load(pipeline_file)
    decisions = {
        message_type: {"result": "skip", "event": "", "fields": []}
        for message_type in schema_messages
    }
    for emit in pipeline["emit"]:
        projectable = schema_messages[emit["source"]]["projectable_fields"]
        fields = projectable if emit["fields"] == ["*"] else emit["fields"]
        decisions[emit["source"]] = {
            "result": "emit",
            "event": emit["event"],
            "fields": fields,
        }
    return decisions


def load_fixture_manifests(args: argparse.Namespace) -> list[dict[str, Any]]:
    fixtures = []
    for path in sorted(args.fixtures_dir.glob("*.toml")):
        with path.open("rb") as fixture_file:
            data = tomllib.load(fixture_file)
        fixtures.append({"path": path, "data": data, "payload": bytes.fromhex(data["raw_hex"])})
    check(len(fixtures) == 23, "source fixture count")

    with args.schema.open("rb") as schema_file:
        schema = tomllib.load(schema_file)
    schema_records = []
    schema_messages = {}
    for message in schema["messages"]:
        field_names = [field["name"] for field in message["fields"]]
        spec_pages = [message["spec_page"]]
        for field in message["fields"]:
            if field["spec_page"] not in spec_pages:
                spec_pages.append(field["spec_page"])
        projectable_fields = [
            field["name"]
            for field in message["fields"]
            if field.get("role") != "discriminator" and field["type"] != "reserved"
        ]
        record = {
            "fields": field_names,
            "name": message["name"],
            "projectable_fields": projectable_fields,
            "size": message["size"],
            "spec_pages": spec_pages,
            "spec_section": message["spec_section"],
            "type": message["type"],
        }
        schema_records.append(record)
        schema_messages[message["type"]] = record
    check(len(schema_records) == len(fixtures), "canonical schema message count")

    all_messages = pipeline_decisions(args.all_messages_pipeline, schema_messages)
    order_events = pipeline_decisions(args.order_events_pipeline, schema_messages)
    for fixture, schema_record in zip(fixtures, schema_records, strict=True):
        data = fixture["data"]
        identity = (data["message_type"], data["message_name"], data["raw_size"])
        expected_identity = (
            schema_record["type"], schema_record["name"], schema_record["size"]
        )
        check(identity == expected_identity, f"canonical schema identity: {fixture['path'].name}")
        check(list(data["expected_fields"]) == schema_record["fields"],
              f"canonical schema field order: {fixture['path'].name}")
        check(data["spec_section"] == schema_record["spec_section"],
              f"canonical schema section citation: {fixture['path'].name}")
        check(all(type(page) is int for page in data["spec_pages"])
              and data["spec_pages"] == schema_record["spec_pages"],
              f"canonical schema page citations: {fixture['path'].name}")
        check(data["expected_all_messages"] == all_messages[data["message_type"]],
              f"canonical all-messages projection: {fixture['path'].name}")
        check(data["expected_order_events"] == order_events[data["message_type"]],
              f"canonical order-events projection: {fixture['path'].name}")
    return fixtures


def binary_file(payloads: Iterable[bytes], *, complete: bool) -> bytes:
    stream = bytearray()
    for payload in payloads:
        stream.extend(len(payload).to_bytes(2, "big"))
        stream.extend(payload)
    if complete:
        stream.extend(b"\x00\x00")
    return bytes(stream)


def check_records(actual: Any, expected: list[dict[str, Any]], description: str) -> None:
    check(isinstance(actual, list), f"{description} is an array")
    check(len(actual) == len(expected), f"{description} count")
    for index, (actual_record, expected_record) in enumerate(
        zip(actual, expected, strict=True), start=1
    ):
        check(actual_record == expected_record, f"{description} record {index}")


def check_manifest(files: dict[str, bytes]) -> None:
    expected_records = [
        {
            "bytes": len(files[path]),
            "path": path,
            "sha256": sha256(files[path]),
        }
        for path in sorted(set(files) - {"manifest.json"})
    ]
    expected_manifest = {
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "files": expected_records,
        "hash_algorithm": "sha256",
    }
    check(read_json(files, "manifest.json") == expected_manifest, "complete manifest contract")


def check_license(args: argparse.Namespace, files: dict[str, bytes]) -> None:
    license_data = args.license_path.read_bytes()
    check(sha256(license_data) == LICENSE_SHA256, "repository Apache-2.0 license identity")
    check(files["LICENSE.txt"] == license_data, "bundled Apache-2.0 license bytes")


def check_message_contract(fixtures: list[dict[str, Any]],
                           files: dict[str, bytes]) -> list[bytes]:
    expected_records = []
    payloads = []
    for ordinal, fixture in enumerate(fixtures, start=1):
        path = fixture["path"]
        data = fixture["data"]
        payload = fixture["payload"]
        payload_path = f"payloads/{path.stem}.bin"
        payloads.append(payload)
        expected_records.append({
            "expected_all_messages": data["expected_all_messages"],
            "expected_fields": data["expected_fields"],
            "expected_order_events": data["expected_order_events"],
            "fixture": path.name,
            "message_name": data["message_name"],
            "message_type": data["message_type"],
            "ordinal": ordinal,
            "payload": payload_path,
            "raw_size": data["raw_size"],
            "spec_pages": data["spec_pages"],
            "spec_section": data["spec_section"],
        })
        check(files[payload_path] == payload, f"positive payload: {path.name}")

    expected_document = {
        "format_version": 1,
        "messages": expected_records,
        "stream": {
            "frame_count": len(fixtures),
            "path": "streams/all-messages.binaryfile",
            "status": "complete",
        },
    }
    actual_document = read_json(files, "expected/messages.json")
    check_records(actual_document.get("messages"), expected_records, "expected message")
    check(actual_document == expected_document, "complete message and stream contract")

    expected_stream = binary_file(payloads, complete=True)
    check(files["streams/all-messages.binaryfile"] == expected_stream,
          "aggregate stream fixture order and framing")
    return payloads


def check_negative_contract(fixtures: list[dict[str, Any]], files: dict[str, bytes]) -> None:
    expected_cases = []
    for fixture in fixtures:
        path = fixture["path"]
        data = fixture["data"]
        payload = fixture["payload"]
        variants = (
            ("size_minus_one", "size-minus-one", payload[:-1]),
            ("size_plus_one", "size-plus-one", payload + b"\x00"),
        )
        for source_name, path_name, variant in variants:
            case_path = f"negative/payloads/{path.stem}.{path_name}.bin"
            source_case = data["negative"][source_name]
            expected_cases.append({
                "actual_size": source_case["size"],
                "expected_size": data["raw_size"],
                "message_name": data["message_name"],
                "message_type": data["message_type"],
                "path": case_path,
                "status": source_case["expected_error"],
            })
            check(files[case_path] == variant, f"negative payload bytes: {path.name} {path_name}")

    direct_cases = (
        ("empty-payload", b"", "empty_payload"),
        ("unknown-message-type", b"?", "unknown_message_type"),
    )
    for case_id, payload, status in direct_cases:
        case_path = f"negative/payloads/{case_id}.bin"
        expected_cases.append({
            "actual_size": len(payload),
            "expected_size": 0,
            "message_name": "",
            "message_type": "" if not payload else "?",
            "path": case_path,
            "status": status,
        })
        check(files[case_path] == payload, f"negative payload bytes: {case_id}")

    expected_document = {"cases": expected_cases, "format_version": 1}
    actual_document = read_json(files, "expected/negative.json")
    check_records(actual_document.get("cases"), expected_cases, "negative payload")
    check(actual_document == expected_document, "complete negative payload contract")


def check_framing_contract(payloads: list[bytes], files: dict[str, bytes]) -> None:
    complete_stream = binary_file(payloads, complete=True)
    incomplete_stream = binary_file(payloads, complete=False)
    first_short_payload = payloads[0][:-1]
    framing_inputs = {
        "complete-empty": b"\x00\x00",
        "incomplete-all-messages": incomplete_stream,
        "invalid-message-size": binary_file([first_short_payload], complete=True),
        "trailing-data-after-end-marker": b"\x00\x00\xff",
        "truncated-length-prefix": b"\x00",
        "truncated-payload": b"\x00\x02S",
        "unknown-message-type": binary_file([b"?"], complete=True),
    }
    framing_expectations = {
        "complete-empty": {"bytes_consumed": 2, "frames_seen": 0, "status": "complete"},
        "incomplete-all-messages": {
            "bytes_consumed": len(incomplete_stream),
            "frames_seen": len(payloads),
            "status": "incomplete",
        },
        "invalid-message-size": {
            "bytes_consumed": 2 + len(first_short_payload),
            "decode_error": "invalid_message_size",
            "error_offset": 2,
            "frames_seen": 1,
            "status": "decode_error",
        },
        "trailing-data-after-end-marker": {
            "bytes_consumed": 2,
            "error_offset": 2,
            "frames_seen": 0,
            "framing_error": "trailing_data_after_end_marker",
            "status": "framing_error",
        },
        "truncated-length-prefix": {
            "bytes_consumed": 0,
            "error_offset": 0,
            "frames_seen": 0,
            "framing_error": "truncated_length_prefix",
            "status": "framing_error",
        },
        "truncated-payload": {
            "bytes_consumed": 0,
            "error_offset": 0,
            "frames_seen": 0,
            "framing_error": "truncated_payload",
            "status": "framing_error",
        },
        "unknown-message-type": {
            "bytes_consumed": 3,
            "decode_error": "unknown_message_type",
            "error_offset": 2,
            "frames_seen": 1,
            "status": "decode_error",
        },
    }
    expected_cases = []
    for case_id in sorted(framing_inputs):
        case_path = f"negative/framing/{case_id}.binaryfile"
        check(files[case_path] == framing_inputs[case_id], f"framing input bytes: {case_id}")
        expected_cases.append({
            "expected": framing_expectations[case_id],
            "id": case_id,
            "path": case_path,
        })

    expected_document = {"cases": expected_cases, "format_version": 1}
    actual_document = read_json(files, "expected/framing.json")
    check_records(actual_document.get("cases"), expected_cases, "framing")
    check(actual_document == expected_document, "complete framing contract")
    check(files["streams/all-messages.binaryfile"] == complete_stream,
          "framing contract aggregate stream")


def check_provenance(args: argparse.Namespace, fixtures: list[dict[str, Any]],
                     files: dict[str, bytes]) -> None:
    with args.source_lock.open("rb") as source_file:
        source_metadata = tomllib.load(source_file)
    expected_reviewed_fixtures = [
        {
            "byte_source": fixture["data"]["byte_source"],
            "fixture": fixture["path"].name,
            "review_date": fixture["data"]["review_date"],
            "review_status": fixture["data"]["review_status"],
            "reviewer": fixture["data"]["reviewer"],
            "sha256": sha256(fixture["path"].read_bytes()),
        }
        for fixture in fixtures
    ]
    expected_generator = {
        "path": "tools/generate_conformance_bundle.py",
        "sha256": sha256(args.generator.read_bytes()),
    }
    expected_source_lock = {
        "path": "schemas/sources.lock.toml",
        "sha256": sha256(args.source_lock.read_bytes()),
    }
    expected_schema = {
        "path": "schemas/nasdaq/totalview_itch_5_0.toml",
        "sha256": sha256(args.schema.read_bytes()),
    }
    expected_pipelines = {
        "all_messages": {
            "path": "pipelines/all_messages.toml",
            "sha256": sha256(args.all_messages_pipeline.read_bytes()),
        },
        "order_events": {
            "path": "pipelines/order_events.toml",
            "sha256": sha256(args.order_events_pipeline.read_bytes()),
        },
    }
    expected_provenance = {
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "generator": expected_generator,
        "notice": (
            "Synthetic reviewed test vectors; not exchange certification and not captured market data."
        ),
        "pipelines": expected_pipelines,
        "reviewed_fixtures": expected_reviewed_fixtures,
        "schema": expected_schema,
        "source_lock": expected_source_lock,
        "sources": source_metadata,
    }

    provenance = read_json(files, "PROVENANCE.json")
    check(provenance.get("generator") == expected_generator, "provenance generator hash")
    check(provenance.get("schema") == expected_schema, "provenance schema hash")
    check(provenance.get("pipelines") == expected_pipelines, "provenance pipeline hashes")
    check(provenance.get("source_lock") == expected_source_lock, "provenance source-lock hash")
    check_records(provenance.get("reviewed_fixtures"), expected_reviewed_fixtures,
                  "provenance fixture")
    check(provenance.get("sources") == source_metadata, "provenance source-lock records")
    check(provenance == expected_provenance, "complete provenance contract")


def replace_once(path: Path, original: str, replacement: str) -> None:
    contents = path.read_text(encoding="utf-8")
    check(contents.count(original) == 1, f"unique test mutation anchor: {path.name}")
    path.write_text(contents.replace(original, replacement), encoding="utf-8")


def replace_projection_table(path: Path, table_name: str,
                             projection: dict[str, Any]) -> None:
    contents = path.read_text(encoding="utf-8")
    header = f"[{table_name}]"
    check(contents.count(header) == 1, f"unique projection table: {path.name} {table_name}")
    start = contents.index(header)
    next_table = contents.find("\n[", start + len(header))
    end = len(contents) if next_table == -1 else next_table + 1
    fields = ", ".join(json.dumps(field) for field in projection["fields"])
    replacement = (
        f"{header}\n"
        f"result = {json.dumps(projection['result'])}\n"
        f"event = {json.dumps(projection['event'])}\n"
        f"fields = [{fields}]\n"
    )
    path.write_text(contents[:start] + replacement + contents[end:], encoding="utf-8")


def expect_generator_failure(args: argparse.Namespace, temporary_path: Path,
                             case_id: str, expected_error: str, **overrides: Path) -> None:
    completed = subprocess.run(
        generator_command(
            args,
            temporary_path / f"invalid-{case_id}-output",
            **overrides,
        ),
        check=False,
        capture_output=True,
        text=True,
    )
    check(completed.returncode != 0, f"reject invalid generator input: {case_id}")
    check(expected_error in completed.stderr, f"invalid-input diagnostic: {case_id}")


def copy_fixtures(args: argparse.Namespace, temporary_path: Path, case_id: str) -> Path:
    destination = temporary_path / f"invalid-{case_id}-fixtures"
    shutil.copytree(args.fixtures_dir, destination)
    return destination


def check_rejects_projection_mismatches(args: argparse.Namespace,
                                        temporary_path: Path) -> None:
    for case_id in ("emit-to-skip", "wrong-fields", "reordered-fields", "missing-fields"):
        fixtures_dir = copy_fixtures(args, temporary_path, case_id)
        fixture_path = fixtures_dir / "11_add_order.toml"
        with fixture_path.open("rb") as fixture_file:
            projection = dict(tomllib.load(fixture_file)["expected_order_events"])
        projection["fields"] = list(projection["fields"])
        if case_id == "emit-to-skip":
            projection = {"result": "skip", "event": "", "fields": []}
        elif case_id == "wrong-fields":
            projection["fields"][0] = "tracking_number"
        elif case_id == "reordered-fields":
            projection["fields"][0:2] = reversed(projection["fields"][0:2])
        else:
            projection["fields"].pop()
        replace_projection_table(fixture_path, "expected_order_events", projection)
        expect_generator_failure(
            args,
            temporary_path,
            case_id,
            "expected_order_events does not match the order-events pipeline",
            fixtures_dir=fixtures_dir,
        )


def check_rejects_review_and_value_mismatches(args: argparse.Namespace,
                                              temporary_path: Path) -> None:
    review_cases = (
        ("review-status", "review_status", "pending", "review_status is not the required"),
        ("reviewer", "reviewer", "self-review", "reviewer is not the required"),
        ("byte-source", "byte_source", "generated", "byte_source is not the required"),
        ("review-date", "review_date", "not-a-date", "review_date must be a valid ISO date"),
    )
    for case_id, key, replacement, expected_error in review_cases:
        fixtures_dir = copy_fixtures(args, temporary_path, case_id)
        fixture_path = fixtures_dir / "01_system_event.toml"
        with fixture_path.open("rb") as fixture_file:
            original = tomllib.load(fixture_file)[key]
        replace_once(
            fixture_path,
            f"{key} = {json.dumps(original)}",
            f"{key} = {json.dumps(replacement)}",
        )
        expect_generator_failure(
            args, temporary_path, case_id, expected_error, fixtures_dir=fixtures_dir
        )

    value_cases = (
        (
            "numeric-string",
            "timestamp = 84281096",
            'timestamp = "84281096"',
            "expected_fields.timestamp does not match decoded raw payload",
        ),
        (
            "wrong-numeric",
            "timestamp = 84281096",
            "timestamp = 84281097",
            "expected_fields.timestamp does not match decoded raw payload",
        ),
        ("non-finite", "timestamp = 84281096", "timestamp = nan", "non-finite float"),
        (
            "unsupported-date",
            'event_code = "O"',
            "event_code = 2026-07-14",
            "unsupported value type date",
        ),
    )
    for case_id, original, replacement, expected_error in value_cases:
        fixtures_dir = copy_fixtures(args, temporary_path, case_id)
        replace_once(fixtures_dir / "01_system_event.toml", original, replacement)
        expect_generator_failure(
            args, temporary_path, case_id, expected_error, fixtures_dir=fixtures_dir
        )


def check_rejects_citation_mismatches(args: argparse.Namespace,
                                      temporary_path: Path) -> None:
    cases = (
        (
            "citation-section",
            "01_system_event.toml",
            'spec_section = "1.1"',
            'spec_section = "bogus"',
            "spec_section does not match the canonical schema",
        ),
        (
            "citation-pages",
            "02_stock_directory.toml",
            "spec_pages = [5, 6, 7, 8]",
            "spec_pages = [5, 6, 7]",
            "spec_pages do not match the canonical schema citations",
        ),
        (
            "citation-page-type",
            "01_system_event.toml",
            "spec_pages = [4]",
            'spec_pages = ["4"]',
            "spec_pages must be an integer array",
        ),
    )
    for case_id, fixture_name, original, replacement, expected_error in cases:
        fixtures_dir = copy_fixtures(args, temporary_path, case_id)
        replace_once(fixtures_dir / fixture_name, original, replacement)
        expect_generator_failure(
            args, temporary_path, case_id, expected_error, fixtures_dir=fixtures_dir
        )


def check_rejects_canonical_input_mismatches(args: argparse.Namespace,
                                             temporary_path: Path) -> None:
    source_cases = (
        ("source-version", "format_version = 1", "format_version = 2", "format_version must be 1"),
        (
            "source-id",
            'id = "nasdaq_totalview_itch_5_0"',
            'id = "unreviewed_source"',
            ".id is not a required source",
        ),
        (
            "source-sha",
            'sha256 = "45e0531d1b4b3beb886e9618b2ab824a5aa9bda3a99c0dff03509306e68aacc3"',
            f'sha256 = "{"0" * 64}"',
            ".sha256 does not match the pinned official source",
        ),
        (
            "source-url",
            "https://www.nasdaqtrader.com/content/technicalsupport/specifications/"
            "dataproducts/NQTVITCHSpecification.pdf",
            "https://example.invalid/itch.pdf",
            ".url is not the canonical source URL",
        ),
    )
    for case_id, original, replacement, expected_error in source_cases:
        source_lock = temporary_path / f"invalid-{case_id}-sources.lock.toml"
        shutil.copy2(args.source_lock, source_lock)
        replace_once(source_lock, original, replacement)
        expect_generator_failure(
            args, temporary_path, case_id, expected_error, source_lock=source_lock
        )

    schema = temporary_path / "invalid-schema-size.toml"
    shutil.copy2(args.schema, schema)
    replace_once(
        schema,
        'name = "system_event"\ntype = "S"\nsize = 12',
        'name = "system_event"\ntype = "S"\nsize = 13',
    )
    expect_generator_failure(
        args, temporary_path, "schema-size", "fields do not cover the declared message size",
        schema=schema
    )

    unsupported_schema = temporary_path / "invalid-schema-type.toml"
    shutil.copy2(args.schema, unsupported_schema)
    replace_once(
        unsupported_schema,
        'name = "timestamp"\nkind = "uint"\nwidth = 6',
        'name = "timestamp"\nkind = "float"\nwidth = 6',
    )
    expect_generator_failure(
        args,
        temporary_path,
        "schema-type",
        "types[1].kind must be uint",
        schema=unsupported_schema,
    )

    all_messages_pipeline = temporary_path / "invalid-all-messages-pipeline.toml"
    shutil.copy2(args.all_messages_pipeline, all_messages_pipeline)
    replace_once(
        all_messages_pipeline,
        'name = "itch50_all"',
        'name = "not_the_canonical_pipeline"',
    )
    expect_generator_failure(
        args,
        temporary_path,
        "pipeline-identity",
        "pipeline name must be itch50_all",
        all_messages_pipeline=all_messages_pipeline,
    )

    license_path = temporary_path / "invalid-LICENSE"
    license_path.write_bytes(args.license_path.read_bytes() + b"\nmodified\n")
    expect_generator_failure(
        args, temporary_path, "license", "license is not the pinned Apache License 2.0 text",
        license_path=license_path
    )


def check_tar(root: Path, archive_path: Path, files: dict[str, bytes], directories: set[str]) -> None:
    raw_archive = archive_path.read_bytes()
    check(raw_archive[:10] == b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff",
          "fixed gzip header")
    expected_names = {BUNDLE_NAME}
    expected_names.update(f"{BUNDLE_NAME}/{path}" for path in directories)
    expected_names.update(f"{BUNDLE_NAME}/{path}" for path in files)
    with tarfile.open(archive_path, "r:gz") as archive:
        members = archive.getmembers()
        check([member.name for member in members] == sorted(expected_names), "tar member order")
        check({member.name for member in members} == expected_names, "tar member inventory")
        for member in members:
            check(member.mtime == 0, f"tar timestamp: {member.name}")
            check(member.uid == 0 and member.gid == 0, f"tar ownership: {member.name}")
            check(member.mode == (0o755 if member.isdir() else 0o644),
                  f"tar mode: {member.name}")
            if member.isfile():
                extracted = archive.extractfile(member)
                check(extracted is not None, f"tar extract: {member.name}")
                relative = member.name.removeprefix(f"{BUNDLE_NAME}/")
                check(extracted.read() == files[relative], f"tar bytes: {relative}")


def check_zip(root: Path, archive_path: Path, files: dict[str, bytes], directories: set[str]) -> None:
    expected_names = {f"{BUNDLE_NAME}/"}
    expected_names.update(f"{BUNDLE_NAME}/{path}/" for path in directories)
    expected_names.update(f"{BUNDLE_NAME}/{path}" for path in files)
    with zipfile.ZipFile(archive_path) as archive:
        members = archive.infolist()
        check([member.filename for member in members] == sorted(expected_names), "zip member order")
        check({member.filename for member in members} == expected_names, "zip member inventory")
        for member in members:
            check(member.date_time == (1980, 1, 1, 0, 0, 0),
                  f"zip timestamp: {member.filename}")
            check(member.compress_type == zipfile.ZIP_STORED,
                  f"zip storage method: {member.filename}")
            expected_mode = 0o40755 if member.is_dir() else 0o100644
            check(member.external_attr >> 16 == expected_mode, f"zip mode: {member.filename}")
            if not member.is_dir():
                relative = member.filename.removeprefix(f"{BUNDLE_NAME}/")
                check(archive.read(member) == files[relative], f"zip bytes: {relative}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generator", required=True, type=Path)
    parser.add_argument("--fixtures-dir", required=True, type=Path)
    parser.add_argument("--source-lock", required=True, type=Path)
    parser.add_argument("--license", dest="license_path", required=True, type=Path)
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--all-messages-pipeline", required=True, type=Path)
    parser.add_argument("--order-events-pipeline", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    with tempfile.TemporaryDirectory(prefix="feedforge-conformance-test-") as temporary:
        temporary_path = Path(temporary)
        first = temporary_path / "first"
        second = temporary_path / "second"
        run_generator(args, first)
        run_generator(args, second)

        first_root = first / BUNDLE_NAME
        second_root = second / BUNDLE_NAME
        first_files = files_under(first_root)
        check(first_files == files_under(second_root), "expanded bundle determinism")
        for archive_name in (f"{BUNDLE_NAME}.tar.gz", f"{BUNDLE_NAME}.zip"):
            check((first / archive_name).read_bytes() == (second / archive_name).read_bytes(),
                  f"archive determinism: {archive_name}")

        fixtures = load_fixture_manifests(args)
        check_manifest(first_files)
        check_license(args, first_files)
        payloads = check_message_contract(fixtures, first_files)
        check_negative_contract(fixtures, first_files)
        check_framing_contract(payloads, first_files)
        check_provenance(args, fixtures, first_files)
        directories = directories_under(first_root)
        check_tar(first_root, first / f"{BUNDLE_NAME}.tar.gz", first_files, directories)
        check_zip(first_root, first / f"{BUNDLE_NAME}.zip", first_files, directories)
        check_rejects_projection_mismatches(args, temporary_path)
        check_rejects_review_and_value_mismatches(args, temporary_path)
        check_rejects_citation_mismatches(args, temporary_path)
        check_rejects_canonical_input_mismatches(args, temporary_path)
        check(not any(path.name.startswith(".feedforge-conformance-") for path in first.iterdir()),
              "temporary generator output was not cleaned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
