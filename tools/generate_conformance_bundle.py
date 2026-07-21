#!/usr/bin/env python3
"""Build the deterministic FeedForge ITCH 5.0 conformance bundle."""

from __future__ import annotations

import argparse
import binascii
from datetime import date
import hashlib
import io
import json
import math
import os
from pathlib import Path
import shutil
import struct
import tarfile
import tempfile
import tomllib
from typing import Any, Iterable
import zipfile


BUNDLE_NAME = "feedforge-itch50-conformance-v1"
ARCHIVE_EPOCH = (1980, 1, 1, 0, 0, 0)
EXPECTED_FIXTURE_COUNT = 23
CANONICAL_SCHEMA_NAME = "nasdaq_totalview_itch_5_0"
EXPECTED_REVIEW_STATUS = "approved"
EXPECTED_REVIEWER = "independent-line-by-line-protocol-review"
EXPECTED_BYTE_SOURCE = "hand-authored from the cited official field table; not schema-generated"
LICENSE_SHA256 = "1525f5b9e493146a721e1c96960518edd3a7a06499684d28745010ac00f919df"
BUILTIN_UINT_WIDTHS = {"u8": 1, "u16": 2, "u32": 4, "u48": 6, "u64": 8}
REQUIRED_SOURCE_FIELDS = {
    "document_revision",
    "document_version",
    "id",
    "notes",
    "pdf_creation_date",
    "retrieved_date",
    "sha256",
    "size_bytes",
    "status",
    "title",
    "url",
}
REQUIRED_SOURCES = {
    "nasdaq_binaryfile_1_00": {
        "document_revision": "2010-03-30",
        "document_version": "1.00",
        "sha256": "a1f443400728b3ce44953e9ae263e4846fe6ad68420e7a635829872aefdfff60",
        "size_bytes": 84384,
        "url": (
            "https://www.nasdaqtrader.com/content/technicalsupport/specifications/"
            "dataproducts/binaryfile.pdf"
        ),
    },
    "nasdaq_totalview_itch_5_0": {
        "document_revision": "2023-04-28",
        "document_version": "5.0",
        "sha256": "45e0531d1b4b3beb886e9618b2ab824a5aa9bda3a99c0dff03509306e68aacc3",
        "size_bytes": 1200722,
        "url": (
            "https://www.nasdaqtrader.com/content/technicalsupport/specifications/"
            "dataproducts/NQTVITCHSpecification.pdf"
        ),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: Any) -> bytes:
    return (
        json.dumps(value, allow_nan=False, ensure_ascii=True, indent=2, sort_keys=True) + "\n"
    ).encode("ascii")


def parse_payload(raw_hex: Any, fixture_path: Path) -> bytes:
    if not isinstance(raw_hex, str):
        raise ValueError(f"{fixture_path}: raw_hex must be a string")
    try:
        return bytes.fromhex(raw_hex)
    except ValueError as error:
        raise ValueError(f"{fixture_path}: raw_hex is not valid hexadecimal") from error


def require(condition: bool, fixture_path: Path, description: str) -> None:
    if not condition:
        raise ValueError(f"{fixture_path}: {description}")


def validate_json_value(value: Any, source_path: Path, description: str) -> None:
    if value is None or isinstance(value, (bool, str)) or type(value) is int:
        return
    if type(value) is float:
        require(math.isfinite(value), source_path, f"{description} contains a non-finite float")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            validate_json_value(item, source_path, f"{description}[{index}]")
        return
    if isinstance(value, dict):
        require(all(isinstance(key, str) for key in value), source_path,
                f"{description} contains a non-string key")
        for key, item in value.items():
            validate_json_value(item, source_path, f"{description}.{key}")
        return
    raise ValueError(
        f"{source_path}: {description} contains unsupported value type {type(value).__name__}"
    )


def require_iso_date(value: Any, source_path: Path, description: str) -> None:
    require(isinstance(value, str) and value, source_path,
            f"{description} must be a non-empty ISO date")
    try:
        parsed = date.fromisoformat(value)
    except ValueError as error:
        raise ValueError(f"{source_path}: {description} must be a valid ISO date") from error
    require(parsed.isoformat() == value and 2000 <= parsed.year <= 2100, source_path,
            f"{description} must be a sensible canonical ISO date")


def load_toml(source_path: Path) -> dict[str, Any]:
    with source_path.open("rb") as source_file:
        return tomllib.load(source_file)


def load_schema(schema_path: Path) -> list[dict[str, Any]]:
    schema = load_toml(schema_path)
    require(type(schema.get("format_version")) is int and schema["format_version"] == 1,
            schema_path, "format_version must be 1")
    require(schema.get("name") == CANONICAL_SCHEMA_NAME, schema_path,
            "schema name is not the canonical ITCH 5.0 schema")
    require(schema.get("protocol_version") == "5.0", schema_path,
            "protocol_version must be 5.0")
    require(schema.get("wire_endian") == "big", schema_path, "wire_endian must be big")
    require(schema.get("discriminator_offset") == 0, schema_path,
            "discriminator_offset must be 0")
    require(schema.get("discriminator_width") == 1, schema_path,
            "discriminator_width must be 1")

    declared_types = schema.get("types")
    require(isinstance(declared_types, list), schema_path, "types must be an array")
    custom_uint_widths: dict[str, int] = {}
    for index, declared_type in enumerate(declared_types, start=1):
        description = f"types[{index}]"
        require(isinstance(declared_type, dict), schema_path,
                f"{description} must be a table")
        type_name = declared_type.get("name")
        type_width = declared_type.get("width")
        require(isinstance(type_name, str) and type_name, schema_path,
                f"{description}.name must be non-empty")
        require(type_name not in custom_uint_widths and type_name not in BUILTIN_UINT_WIDTHS
                and type_name not in {"alpha", "reserved"}, schema_path,
                f"{description}.name is duplicated or reserved")
        require(declared_type.get("kind") == "uint", schema_path,
                f"{description}.kind must be uint")
        require(type(type_width) is int and type_width > 0, schema_path,
                f"{description}.width must be positive")
        custom_uint_widths[type_name] = type_width

    messages = schema.get("messages")
    require(isinstance(messages, list) and len(messages) == EXPECTED_FIXTURE_COUNT,
            schema_path, f"schema must contain exactly {EXPECTED_FIXTURE_COUNT} messages")
    normalized: list[dict[str, Any]] = []
    message_names: set[str] = set()
    message_types: set[str] = set()
    for index, message in enumerate(messages, start=1):
        description = f"messages[{index}]"
        require(isinstance(message, dict), schema_path, f"{description} must be a table")
        message_name = message.get("name")
        message_type = message.get("type")
        message_size = message.get("size")
        message_spec_section = message.get("spec_section")
        message_spec_page = message.get("spec_page")
        require(isinstance(message_name, str) and message_name, schema_path,
                f"{description}.name must be non-empty")
        require(isinstance(message_type, str) and len(message_type) == 1
                and 0x21 <= ord(message_type) <= 0x7E,
                schema_path, f"{description}.type must be one printable ASCII byte")
        require(type(message_size) is int and message_size > 1, schema_path,
                f"{description}.size must be a positive integer")
        require(isinstance(message_spec_section, str) and message_spec_section, schema_path,
                f"{description}.spec_section must be non-empty")
        require(type(message_spec_page) is int and message_spec_page > 0, schema_path,
                f"{description}.spec_page must be a positive integer")
        require(message_name not in message_names, schema_path,
                f"{description}.name is duplicated")
        require(message_type not in message_types, schema_path,
                f"{description}.type is duplicated")

        fields = message.get("fields")
        require(isinstance(fields, list) and fields, schema_path,
                f"{description}.fields must be non-empty")
        field_names: list[str] = []
        projectable_fields: list[str] = []
        wire_fields: list[dict[str, Any]] = []
        spec_pages = [message_spec_page]
        next_offset = 0
        for field_index, field in enumerate(fields, start=1):
            field_description = f"{description}.fields[{field_index}]"
            require(isinstance(field, dict), schema_path,
                    f"{field_description} must be a table")
            field_name = field.get("name")
            field_type = field.get("type")
            offset = field.get("offset")
            width = field.get("width")
            field_spec_section = field.get("spec_section")
            field_spec_page = field.get("spec_page")
            require(isinstance(field_name, str) and field_name, schema_path,
                    f"{field_description}.name must be non-empty")
            require(field_name not in field_names, schema_path,
                    f"{field_description}.name is duplicated")
            require(isinstance(field_type, str) and field_type, schema_path,
                    f"{field_description}.type must be non-empty")
            require(type(offset) is int and offset == next_offset, schema_path,
                    f"{field_description}.offset must preserve contiguous wire order")
            require(type(width) is int and width > 0, schema_path,
                    f"{field_description}.width must be positive")
            require(field_spec_section == message_spec_section, schema_path,
                    f"{field_description}.spec_section must match its message")
            require(type(field_spec_page) is int and field_spec_page > 0, schema_path,
                    f"{field_description}.spec_page must be a positive integer")
            if field_type in {"alpha", "reserved"}:
                pass
            elif field_type in BUILTIN_UINT_WIDTHS:
                require(width == BUILTIN_UINT_WIDTHS[field_type], schema_path,
                        f"{field_description}.width does not match {field_type}")
            elif field_type in custom_uint_widths:
                require(width == custom_uint_widths[field_type], schema_path,
                        f"{field_description}.width does not match {field_type}")
            else:
                raise ValueError(
                    f"{schema_path}: {field_description}.type {field_type!r} is unsupported"
                )

            if field_index == 1:
                require(field_name == "message_type" and field_type == "alpha"
                        and offset == 0 and width == 1
                        and field.get("role") == "discriminator"
                        and field.get("value") == message_type,
                        schema_path, f"{description} has an invalid discriminator field")
            else:
                require(field.get("role") is None, schema_path,
                        f"{field_description}.role is not projectable schema metadata")
                if field_type != "reserved":
                    projectable_fields.append(field_name)

            field_names.append(field_name)
            wire_fields.append({
                "name": field_name,
                "offset": offset,
                "spec_page": field_spec_page,
                "spec_section": field_spec_section,
                "type": field_type,
                "width": width,
            })
            if field_spec_page not in spec_pages:
                spec_pages.append(field_spec_page)
            next_offset += width

        require(next_offset == message_size, schema_path,
                f"{description}.fields do not cover the declared message size")
        require(projectable_fields, schema_path,
                f"{description} has no projectable fields")
        message_names.add(message_name)
        message_types.add(message_type)
        normalized.append({
            "fields": field_names,
            "name": message_name,
            "projectable_fields": projectable_fields,
            "size": message_size,
            "spec_page": message_spec_page,
            "spec_pages": spec_pages,
            "spec_section": message_spec_section,
            "type": message_type,
            "wire_fields": wire_fields,
        })
    return normalized


def load_pipeline(pipeline_path: Path, schema_messages: list[dict[str, Any]],
                  expected_name: str, expected_namespace: str) -> dict[str, dict[str, Any]]:
    pipeline = load_toml(pipeline_path)
    require(type(pipeline.get("format_version")) is int and pipeline["format_version"] == 1,
            pipeline_path, "format_version must be 1")
    require(pipeline.get("schema") == CANONICAL_SCHEMA_NAME, pipeline_path,
            "pipeline must reference the canonical ITCH 5.0 schema")
    require(pipeline.get("name") == expected_name, pipeline_path,
            f"pipeline name must be {expected_name}")
    require(pipeline.get("namespace") == expected_namespace, pipeline_path,
            f"pipeline namespace must be {expected_namespace}")
    require(pipeline.get("profile") == "portable_checked", pipeline_path,
            "pipeline profile must be portable_checked")
    require(pipeline.get("unknown_messages") == "error", pipeline_path,
            "unknown_messages must be error")
    require(pipeline.get("unselected_messages") == "skip", pipeline_path,
            "unselected_messages must be skip")

    messages_by_type = {message["type"]: message for message in schema_messages}
    decisions = {
        message_type: {"result": "skip", "event": "", "fields": []}
        for message_type in messages_by_type
    }
    emits = pipeline.get("emit")
    require(isinstance(emits, list) and emits, pipeline_path,
            "pipeline must contain at least one emit")
    emitted_sources: set[str] = set()
    emitted_events: set[str] = set()
    for index, emit in enumerate(emits, start=1):
        description = f"emit[{index}]"
        require(isinstance(emit, dict), pipeline_path, f"{description} must be a table")
        source = emit.get("source")
        event = emit.get("event")
        fields = emit.get("fields")
        require(isinstance(source, str) and source in messages_by_type, pipeline_path,
                f"{description}.source must name a schema message")
        require(source not in emitted_sources, pipeline_path,
                f"{description}.source is duplicated")
        require(isinstance(event, str) and event, pipeline_path,
                f"{description}.event must be non-empty")
        require(event == messages_by_type[source]["name"], pipeline_path,
                f"{description}.event must match the canonical schema message name")
        require(event not in emitted_events, pipeline_path,
                f"{description}.event is duplicated")
        require(isinstance(fields, list) and fields
                and all(isinstance(field, str) for field in fields),
                pipeline_path, f"{description}.fields must be a non-empty string array")

        projectable = messages_by_type[source]["projectable_fields"]
        if fields == ["*"]:
            resolved_fields = list(projectable)
        else:
            require("*" not in fields, pipeline_path,
                    f"{description}.fields mixes wildcard and explicit fields")
            require(len(fields) == len(set(fields)), pipeline_path,
                    f"{description}.fields contains duplicates")
            require(all(field in projectable for field in fields), pipeline_path,
                    f"{description}.fields contains a non-projectable field")
            resolved_fields = list(fields)

        decisions[source] = {"result": "emit", "event": event, "fields": resolved_fields}
        emitted_sources.add(source)
        emitted_events.add(event)
    if expected_name == "itch50_all":
        require(emitted_sources == set(messages_by_type), pipeline_path,
                "the canonical all-messages pipeline must emit every schema message")
    return decisions


def load_source_lock(source_lock: Path) -> dict[str, Any]:
    metadata = load_toml(source_lock)
    require(type(metadata.get("format_version")) is int and metadata["format_version"] == 1,
            source_lock, "format_version must be 1")
    require_iso_date(metadata.get("retrieved_date"), source_lock, "retrieved_date")
    sources = metadata.get("sources")
    require(isinstance(sources, list) and len(sources) == len(REQUIRED_SOURCES),
            source_lock, "sources must contain exactly the required official documents")
    source_ids: set[str] = set()
    for index, source in enumerate(sources, start=1):
        description = f"sources[{index}]"
        require(isinstance(source, dict), source_lock, f"{description} must be a table")
        require(REQUIRED_SOURCE_FIELDS <= set(source), source_lock,
                f"{description} is missing required metadata")
        source_id = source.get("id")
        require(isinstance(source_id, str) and source_id in REQUIRED_SOURCES,
                source_lock, f"{description}.id is not a required source")
        require(source_id not in source_ids, source_lock, f"{description}.id is duplicated")
        for field in (
            "document_revision", "document_version", "notes", "pdf_creation_date", "title"
        ):
            require(isinstance(source.get(field), str) and source[field], source_lock,
                    f"{description}.{field} must be non-empty")
        require_iso_date(source.get("retrieved_date"), source_lock,
                         f"{description}.retrieved_date")
        require(source.get("status") == "retrieved", source_lock,
                f"{description}.status must be retrieved")
        required_source = REQUIRED_SOURCES[source_id]
        require(source.get("url") == required_source["url"], source_lock,
                f"{description}.url is not the canonical source URL")
        digest = source.get("sha256")
        require(isinstance(digest, str) and len(digest) == 64
                and all(character in "0123456789abcdef" for character in digest),
                source_lock, f"{description}.sha256 must be a lowercase SHA-256 digest")
        require(digest == required_source["sha256"], source_lock,
                f"{description}.sha256 does not match the pinned official source")
        for field in ("document_revision", "document_version", "size_bytes"):
            require(source.get(field) == required_source[field], source_lock,
                    f"{description}.{field} does not match the pinned official source")
        source_ids.add(source_id)
    require(source_ids == set(REQUIRED_SOURCES), source_lock,
            "sources do not contain the exact required IDs")
    validate_json_value(metadata, source_lock, "source-lock metadata")
    return metadata


def load_license(license_path: Path) -> bytes:
    license_data = license_path.read_bytes()
    require(sha256_bytes(license_data) == LICENSE_SHA256, license_path,
            "license is not the pinned Apache License 2.0 text")
    return license_data


def decode_payload_fields(payload: bytes, schema_message: dict[str, Any],
                          fixture_path: Path) -> dict[str, Any]:
    decoded: dict[str, Any] = {}
    for field in schema_message["wire_fields"]:
        offset = field["offset"]
        raw_value = payload[offset:offset + field["width"]]
        if field["type"] in {"alpha", "reserved"}:
            try:
                value: Any = raw_value.decode("ascii")
            except UnicodeDecodeError as error:
                raise ValueError(
                    f"{fixture_path}: field {field['name']} is not valid fixed-width ASCII"
                ) from error
        else:
            value = int.from_bytes(raw_value, "big", signed=False)
        decoded[field["name"]] = value
    return decoded


def load_fixtures(fixtures_dir: Path, schema_messages: list[dict[str, Any]],
                  all_messages_decisions: dict[str, dict[str, Any]],
                  order_events_decisions: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    fixture_paths = sorted(fixtures_dir.glob("*.toml"))
    if len(fixture_paths) != EXPECTED_FIXTURE_COUNT:
        raise ValueError(
            f"{fixtures_dir}: expected {EXPECTED_FIXTURE_COUNT} fixtures, found {len(fixture_paths)}"
        )

    loaded: list[dict[str, Any]] = []
    message_types: set[str] = set()
    message_names: set[str] = set()
    for ordinal, (fixture_path, schema_message) in enumerate(
        zip(fixture_paths, schema_messages, strict=True), start=1
    ):
        with fixture_path.open("rb") as fixture_file:
            fixture = tomllib.load(fixture_file)

        message_type = fixture.get("message_type")
        message_name = fixture.get("message_name")
        payload = parse_payload(fixture.get("raw_hex"), fixture_path)
        expected_prefix = f"{ordinal:02d}_"

        require(fixture_path.name.startswith(expected_prefix), fixture_path, "fixture order is not contiguous")
        require(type(fixture.get("format_version")) is int and fixture["format_version"] == 1,
                fixture_path, "format_version must be 1")
        require(fixture.get("review_status") == EXPECTED_REVIEW_STATUS, fixture_path,
                "review_status is not the required approved marker")
        require(fixture.get("reviewer") == EXPECTED_REVIEWER, fixture_path,
                "reviewer is not the required independent review marker")
        require(fixture.get("byte_source") == EXPECTED_BYTE_SOURCE, fixture_path,
                "byte_source is not the required hand-authored provenance marker")
        require_iso_date(fixture.get("review_date"), fixture_path, "review_date")
        require(isinstance(message_type, str) and len(message_type) == 1, fixture_path,
                "message_type must contain one byte")
        require(isinstance(message_name, str) and message_name, fixture_path,
                "message_name must be non-empty")
        require(fixture_path.stem == f"{ordinal:02d}_{message_name}", fixture_path,
                "file name does not match message_name")
        require(message_type not in message_types, fixture_path, "duplicate message_type")
        require(message_name not in message_names, fixture_path, "duplicate message_name")
        require(message_type == schema_message["type"]
                and message_name == schema_message["name"]
                and fixture.get("raw_size") == schema_message["size"], fixture_path,
                "message identity or size does not match the canonical schema")
        require(fixture.get("spec_section") == schema_message["spec_section"], fixture_path,
                "spec_section does not match the canonical schema")
        fixture_spec_pages = fixture.get("spec_pages")
        require(isinstance(fixture_spec_pages, list)
                and all(type(page) is int for page in fixture_spec_pages), fixture_path,
                "spec_pages must be an integer array")
        require(fixture_spec_pages == schema_message["spec_pages"], fixture_path,
                "spec_pages do not match the canonical schema citations")
        require(fixture.get("raw_size") == len(payload), fixture_path,
                "raw_size does not match raw_hex")
        require(payload[:1] == message_type.encode("ascii"), fixture_path,
                "payload discriminator does not match message_type")

        fields = fixture.get("expected_fields")
        all_messages = fixture.get("expected_all_messages")
        order_events = fixture.get("expected_order_events")
        negative = fixture.get("negative")
        require(isinstance(fields, dict) and fields.get("message_type") == message_type,
                fixture_path, "expected_fields.message_type does not match")
        require(list(fields) == schema_message["fields"], fixture_path,
                "expected_fields do not match canonical schema field order")
        validate_json_value(fields, fixture_path, "expected_fields")
        decoded_fields = decode_payload_fields(payload, schema_message, fixture_path)
        for field_name, decoded_value in decoded_fields.items():
            expected_value = fields[field_name]
            require(type(expected_value) is type(decoded_value)
                    and expected_value == decoded_value, fixture_path,
                    f"expected_fields.{field_name} does not match decoded raw payload")
        validate_json_value(all_messages, fixture_path, "expected_all_messages")
        validate_json_value(order_events, fixture_path, "expected_order_events")
        require(all_messages == all_messages_decisions[message_type], fixture_path,
                "expected_all_messages does not match the all-messages pipeline")
        require(order_events == order_events_decisions[message_type], fixture_path,
                "expected_order_events does not match the order-events pipeline")
        require(isinstance(negative, dict), fixture_path, "negative cases are missing")
        require(negative.get("size_minus_one", {}).get("size") == len(payload) - 1,
                fixture_path, "size_minus_one metadata is inconsistent")
        require(negative.get("size_plus_one", {}).get("size") == len(payload) + 1,
                fixture_path, "size_plus_one metadata is inconsistent")
        require(negative.get("size_minus_one", {}).get("expected_error") ==
                "invalid_message_size", fixture_path, "size_minus_one error is inconsistent")
        require(negative.get("size_plus_one", {}).get("expected_error") ==
                "invalid_message_size", fixture_path, "size_plus_one error is inconsistent")

        message_types.add(message_type)
        message_names.add(message_name)
        loaded.append({"path": fixture_path, "data": fixture, "payload": payload})
    return loaded


def binary_file(payloads: Iterable[bytes], *, complete: bool) -> bytes:
    output = bytearray()
    for payload in payloads:
        if len(payload) > 0xFFFF:
            raise ValueError("BinaryFILE payload exceeds the 16-bit length limit")
        output.extend(len(payload).to_bytes(2, "big"))
        output.extend(payload)
    if complete:
        output.extend(b"\x00\x00")
    return bytes(output)


def bundle_readme() -> bytes:
    return b"""# FeedForge synthetic ITCH 5.0 conformance bundle

This bundle contains deterministic synthetic payloads derived from FeedForge's
23 independently reviewed Nasdaq TotalView-ITCH 5.0 fixture manifests. It is a
software conformance aid, not exchange certification and not captured market
data.

## Layout

- `payloads/`: one valid raw ITCH payload for every reviewed message type.
- `streams/all-messages.binaryfile`: all 23 payloads in fixture order, framed
  with two-byte big-endian lengths and a zero-length end-of-session marker.
- `expected/`: normalized JSON outcomes for positive, decode-negative, and
  BinaryFILE cases. Fixed-width ASCII strings retain trailing spaces.
- `negative/payloads/`: empty, unknown-type, and size-boundary payloads.
- `negative/framing/`: complete, incomplete, malformed, and decode-error
  BinaryFILE inputs.
- `PROVENANCE.json`: fixture review, canonical schema/pipeline hashes, and
  locked official-source metadata.
- `manifest.json`: byte sizes and SHA-256 hashes for every other bundle file.
- `LICENSE.txt`: the FeedForge Apache License 2.0 text.

All paths are relative to this versioned directory. JSON is UTF-8 with sorted
object keys and a final newline. Archive member order, modes, owners, and
timestamps are fixed. The ZIP uses stored entries; the tar.gz uses deterministic
stored DEFLATE blocks.

Verify a file by hashing its bytes with SHA-256 and comparing the result with
`manifest.json`. The manifest intentionally omits itself to avoid a recursive
hash.
"""


def write_file(root: Path, relative_path: str, data: bytes) -> None:
    destination = root / relative_path
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def normalized_message(fixture: dict[str, Any], ordinal: int) -> dict[str, Any]:
    data = fixture["data"]
    return {
        "expected_all_messages": data["expected_all_messages"],
        "expected_fields": data["expected_fields"],
        "expected_order_events": data["expected_order_events"],
        "fixture": fixture["path"].name,
        "message_name": data["message_name"],
        "message_type": data["message_type"],
        "payload": f"payloads/{fixture['path'].stem}.bin",
        "raw_size": len(fixture["payload"]),
        "spec_pages": data["spec_pages"],
        "spec_section": data["spec_section"],
        "ordinal": ordinal,
    }


def build_tree(root: Path, fixtures: list[dict[str, Any]], source_metadata: dict[str, Any],
               source_lock: Path, license_data: bytes, generator_path: Path,
               schema_path: Path, all_messages_pipeline: Path,
               order_events_pipeline: Path) -> None:
    write_file(root, "README.md", bundle_readme())
    write_file(root, "LICENSE.txt", license_data)

    messages: list[dict[str, Any]] = []
    negative_payload_cases: list[dict[str, Any]] = []
    payloads: list[bytes] = []
    for ordinal, fixture in enumerate(fixtures, start=1):
        payload = fixture["payload"]
        stem = fixture["path"].stem
        payload_path = f"payloads/{stem}.bin"
        write_file(root, payload_path, payload)
        payloads.append(payload)
        messages.append(normalized_message(fixture, ordinal))

        variants = (
            ("size-minus-one", payload[:-1]),
            ("size-plus-one", payload + b"\x00"),
        )
        for variant_name, variant_payload in variants:
            case_path = f"negative/payloads/{stem}.{variant_name}.bin"
            write_file(root, case_path, variant_payload)
            negative_payload_cases.append({
                "actual_size": len(variant_payload),
                "expected_size": len(payload),
                "message_name": fixture["data"]["message_name"],
                "message_type": fixture["data"]["message_type"],
                "path": case_path,
                "status": "invalid_message_size",
            })

    direct_negative_cases = (
        ("empty-payload", b"", "empty_payload"),
        ("unknown-message-type", b"?", "unknown_message_type"),
    )
    for case_id, payload, status in direct_negative_cases:
        case_path = f"negative/payloads/{case_id}.bin"
        write_file(root, case_path, payload)
        negative_payload_cases.append({
            "actual_size": len(payload),
            "expected_size": 0,
            "message_name": "",
            "message_type": "" if not payload else "?",
            "path": case_path,
            "status": status,
        })

    complete_stream = binary_file(payloads, complete=True)
    incomplete_stream = binary_file(payloads, complete=False)
    write_file(root, "streams/all-messages.binaryfile", complete_stream)
    write_file(root, "expected/messages.json", json_bytes({
        "format_version": 1,
        "messages": messages,
        "stream": {
            "frame_count": len(messages),
            "path": "streams/all-messages.binaryfile",
            "status": "complete",
        },
    }))
    write_file(root, "expected/negative.json", json_bytes({
        "cases": negative_payload_cases,
        "format_version": 1,
    }))

    first_payload = payloads[0]
    framing_inputs = {
        "complete-empty": b"\x00\x00",
        "incomplete-all-messages": incomplete_stream,
        "truncated-length-prefix": b"\x00",
        "truncated-payload": b"\x00\x02S",
        "trailing-data-after-end-marker": b"\x00\x00\xff",
        "unknown-message-type": binary_file([b"?"], complete=True),
        "invalid-message-size": binary_file([first_payload[:-1]], complete=True),
    }
    framing_expectations = {
        "complete-empty": {"bytes_consumed": 2, "frames_seen": 0, "status": "complete"},
        "incomplete-all-messages": {
            "bytes_consumed": len(incomplete_stream), "frames_seen": len(payloads),
            "status": "incomplete",
        },
        "truncated-length-prefix": {
            "bytes_consumed": 0, "error_offset": 0, "frames_seen": 0,
            "framing_error": "truncated_length_prefix", "status": "framing_error",
        },
        "truncated-payload": {
            "bytes_consumed": 0, "error_offset": 0, "frames_seen": 0,
            "framing_error": "truncated_payload", "status": "framing_error",
        },
        "trailing-data-after-end-marker": {
            "bytes_consumed": 2, "error_offset": 2, "frames_seen": 0,
            "framing_error": "trailing_data_after_end_marker", "status": "framing_error",
        },
        "unknown-message-type": {
            "bytes_consumed": 3, "decode_error": "unknown_message_type", "error_offset": 2,
            "frames_seen": 1, "status": "decode_error",
        },
        "invalid-message-size": {
            "bytes_consumed": 13, "decode_error": "invalid_message_size", "error_offset": 2,
            "frames_seen": 1, "status": "decode_error",
        },
    }
    framing_cases: list[dict[str, Any]] = []
    for case_id in sorted(framing_inputs):
        case_path = f"negative/framing/{case_id}.binaryfile"
        write_file(root, case_path, framing_inputs[case_id])
        framing_cases.append({
            "expected": framing_expectations[case_id],
            "id": case_id,
            "path": case_path,
        })
    write_file(root, "expected/framing.json", json_bytes({
        "cases": framing_cases,
        "format_version": 1,
    }))

    fixture_provenance = [
        {
            "byte_source": fixture["data"]["byte_source"],
            "fixture": fixture["path"].name,
            "review_date": fixture["data"]["review_date"],
            "review_status": fixture["data"]["review_status"],
            "reviewer": fixture["data"]["reviewer"],
            "sha256": sha256_bytes(fixture["path"].read_bytes()),
        }
        for fixture in fixtures
    ]
    provenance = {
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "generator": {
            "path": "tools/generate_conformance_bundle.py",
            "sha256": sha256_bytes(generator_path.read_bytes()),
        },
        "notice": (
            "Synthetic reviewed test vectors; not exchange certification and not captured market data."
        ),
        "reviewed_fixtures": fixture_provenance,
        "schema": {
            "path": "schemas/nasdaq/totalview_itch_5_0.toml",
            "sha256": sha256_bytes(schema_path.read_bytes()),
        },
        "pipelines": {
            "all_messages": {
                "path": "pipelines/all_messages.toml",
                "sha256": sha256_bytes(all_messages_pipeline.read_bytes()),
            },
            "order_events": {
                "path": "pipelines/order_events.toml",
                "sha256": sha256_bytes(order_events_pipeline.read_bytes()),
            },
        },
        "source_lock": {
            "path": "schemas/sources.lock.toml",
            "sha256": sha256_bytes(source_lock.read_bytes()),
        },
        "sources": source_metadata,
    }
    write_file(root, "PROVENANCE.json", json_bytes(provenance))

    manifest_files = []
    files = (path for path in root.rglob("*") if path.is_file())
    for path in sorted(files, key=lambda item: item.relative_to(root).as_posix()):
        data = path.read_bytes()
        manifest_files.append({
            "bytes": len(data),
            "path": path.relative_to(root).as_posix(),
            "sha256": sha256_bytes(data),
        })
    write_file(root, "manifest.json", json_bytes({
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "files": manifest_files,
        "hash_algorithm": "sha256",
    }))


def archive_entries(root: Path) -> list[tuple[str, Path | None]]:
    entries: list[tuple[str, Path | None]] = [(f"{BUNDLE_NAME}/", None)]
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        archive_name = f"{BUNDLE_NAME}/{relative}"
        entries.append((archive_name + "/" if path.is_dir() else archive_name,
                        None if path.is_dir() else path))
    return entries


def deterministic_tar(root: Path) -> bytes:
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        for name, path in archive_entries(root):
            info = tarfile.TarInfo(name=name)
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            info.mtime = 0
            if path is None:
                info.type = tarfile.DIRTYPE
                info.mode = 0o755
                info.size = 0
                archive.addfile(info)
            else:
                data = path.read_bytes()
                info.mode = 0o644
                info.size = len(data)
                archive.addfile(info, io.BytesIO(data))
    return output.getvalue()


def deterministic_gzip(data: bytes) -> bytes:
    compressed = bytearray(b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff")
    if not data:
        compressed.extend(b"\x01\x00\x00\xff\xff")
    else:
        position = 0
        while position < len(data):
            block = data[position:position + 0xFFFF]
            position += len(block)
            compressed.append(1 if position == len(data) else 0)
            compressed.extend(struct.pack("<HH", len(block), 0xFFFF - len(block)))
            compressed.extend(block)
    compressed.extend(struct.pack("<II", binascii.crc32(data) & 0xFFFFFFFF,
                                  len(data) & 0xFFFFFFFF))
    return bytes(compressed)


def write_zip(root: Path, destination: Path) -> None:
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, path in archive_entries(root):
            info = zipfile.ZipInfo(name, date_time=ARCHIVE_EPOCH)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_STORED
            if path is None:
                info.external_attr = (0o40755 << 16) | 0x10
                data = b""
            else:
                info.external_attr = 0o100644 << 16
                data = path.read_bytes()
            archive.writestr(info, data)


def remove_existing(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def generate_bundle(fixtures_dir: Path, source_lock: Path, license_path: Path,
                    schema_path: Path, all_messages_pipeline: Path,
                    order_events_pipeline: Path, output_dir: Path,
                    generator_path: Path) -> tuple[Path, Path, Path]:
    schema_messages = load_schema(schema_path)
    all_messages_decisions = load_pipeline(
        all_messages_pipeline,
        schema_messages,
        "itch50_all",
        "feedforge::generated::nasdaq::itch50_all",
    )
    order_events_decisions = load_pipeline(
        order_events_pipeline,
        schema_messages,
        "itch50_order_events",
        "feedforge::generated::nasdaq::itch50_order_events",
    )
    source_metadata = load_source_lock(source_lock)
    license_data = load_license(license_path)
    fixtures = load_fixtures(
        fixtures_dir,
        schema_messages,
        all_messages_decisions,
        order_events_decisions,
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=".feedforge-conformance-", dir=output_dir))
    try:
        staged_root = work / BUNDLE_NAME
        staged_root.mkdir()
        build_tree(
            staged_root,
            fixtures,
            source_metadata,
            source_lock,
            license_data,
            generator_path,
            schema_path,
            all_messages_pipeline,
            order_events_pipeline,
        )

        staged_tar_gz = work / f"{BUNDLE_NAME}.tar.gz"
        staged_zip = work / f"{BUNDLE_NAME}.zip"
        staged_tar_gz.write_bytes(deterministic_gzip(deterministic_tar(staged_root)))
        write_zip(staged_root, staged_zip)

        final_root = output_dir / BUNDLE_NAME
        final_tar_gz = output_dir / staged_tar_gz.name
        final_zip = output_dir / staged_zip.name
        for destination in (final_root, final_tar_gz, final_zip):
            remove_existing(destination)
        os.replace(staged_root, final_root)
        os.replace(staged_tar_gz, final_tar_gz)
        os.replace(staged_zip, final_zip)
        return final_root, final_tar_gz, final_zip
    finally:
        shutil.rmtree(work, ignore_errors=True)


def parse_args() -> argparse.Namespace:
    repository_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures-dir", type=Path,
                        default=repository_root / "tests/fixtures/itch50")
    parser.add_argument("--source-lock", type=Path,
                        default=repository_root / "schemas/sources.lock.toml")
    parser.add_argument("--license", dest="license_path", type=Path,
                        default=repository_root / "LICENSE")
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--all-messages-pipeline", required=True, type=Path)
    parser.add_argument("--order-events-pipeline", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=repository_root / "out/conformance")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generated = generate_bundle(
        args.fixtures_dir.resolve(),
        args.source_lock.resolve(),
        args.license_path.resolve(),
        args.schema.resolve(),
        args.all_messages_pipeline.resolve(),
        args.order_events_pipeline.resolve(),
        args.output_dir.resolve(),
        Path(__file__).resolve(),
    )
    for path in generated:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
