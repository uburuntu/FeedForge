#!/usr/bin/env python3
"""Black-box checks for the synthetic conformance bundle generator."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import zipfile


BUNDLE_NAME = "feedforge-itch50-conformance-v1"


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


def run_generator(args: argparse.Namespace, output_dir: Path) -> None:
    subprocess.run(
        [
            sys.executable,
            str(args.generator),
            "--fixtures-dir",
            str(args.fixtures_dir),
            "--source-lock",
            str(args.source_lock),
            "--license",
            str(args.license_path),
            "--output-dir",
            str(output_dir),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def check_manifest(root: Path, files: dict[str, bytes]) -> None:
    manifest = json.loads(files["manifest.json"])
    check(manifest["bundle_name"] == BUNDLE_NAME, "manifest bundle name")
    check(manifest["hash_algorithm"] == "sha256", "manifest hash algorithm")
    records = {record["path"]: record for record in manifest["files"]}
    check("manifest.json" not in records, "manifest must not hash itself")
    check(set(records) == set(files) - {"manifest.json"}, "manifest file inventory")
    for path, record in records.items():
        check(record["bytes"] == len(files[path]), f"manifest byte count: {path}")
        check(record["sha256"] == sha256(files[path]), f"manifest hash: {path}")


def check_fixture_projection(args: argparse.Namespace, root: Path, files: dict[str, bytes]) -> None:
    fixture_paths = sorted(args.fixtures_dir.glob("*.toml"))
    expected = json.loads(files["expected/messages.json"])
    negative = json.loads(files["expected/negative.json"])
    framing = json.loads(files["expected/framing.json"])
    check(len(fixture_paths) == 23, "source fixture count")
    check(len(expected["messages"]) == 23, "normalized message count")
    check(len(negative["cases"]) == 48, "decode-negative case count")
    check(len(framing["cases"]) == 7, "framing case count")

    payloads = []
    for fixture_path, normalized in zip(fixture_paths, expected["messages"], strict=True):
        with fixture_path.open("rb") as fixture_file:
            fixture = tomllib.load(fixture_file)
        payload = bytes.fromhex(fixture["raw_hex"])
        payloads.append(payload)
        check(normalized["fixture"] == fixture_path.name, f"fixture name: {fixture_path.name}")
        check(normalized["expected_fields"] == fixture["expected_fields"],
              f"normalized fields: {fixture_path.name}")
        check(files[normalized["payload"]] == payload, f"positive payload: {fixture_path.name}")

        stem = fixture_path.stem
        check(files[f"negative/payloads/{stem}.size-minus-one.bin"] == payload[:-1],
              f"short negative: {fixture_path.name}")
        check(files[f"negative/payloads/{stem}.size-plus-one.bin"] == payload + b"\x00",
              f"long negative: {fixture_path.name}")

    stream = files["streams/all-messages.binaryfile"]
    position = 0
    decoded = []
    while True:
        check(position + 2 <= len(stream), "aggregate stream length prefix")
        payload_size = int.from_bytes(stream[position:position + 2], "big")
        position += 2
        if payload_size == 0:
            break
        check(position + payload_size <= len(stream), "aggregate stream payload")
        decoded.append(stream[position:position + payload_size])
        position += payload_size
    check(position == len(stream), "aggregate stream trailing bytes")
    check(decoded == payloads, "aggregate stream fixture order")

    check(files["negative/framing/complete-empty.binaryfile"] == b"\x00\x00",
          "complete empty framing case")
    check(files["negative/framing/incomplete-all-messages.binaryfile"] == stream[:-2],
          "incomplete aggregate framing case")
    check(files["negative/framing/truncated-length-prefix.binaryfile"] == b"\x00",
          "truncated prefix framing case")
    check(files["negative/framing/truncated-payload.binaryfile"] == b"\x00\x02S",
          "truncated payload framing case")
    check(files["negative/framing/trailing-data-after-end-marker.binaryfile"] == b"\x00\x00\xff",
          "trailing data framing case")


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

        check_manifest(first_root, first_files)
        check_fixture_projection(args, first_root, first_files)
        directories = directories_under(first_root)
        check_tar(first_root, first / f"{BUNDLE_NAME}.tar.gz", first_files, directories)
        check_zip(first_root, first / f"{BUNDLE_NAME}.zip", first_files, directories)
        check(not any(path.name.startswith(".feedforge-conformance-") for path in first.iterdir()),
              "temporary generator output was not cleaned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
