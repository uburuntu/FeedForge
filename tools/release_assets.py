#!/usr/bin/env python3
"""Build deterministic FeedForge source release assets from an exact revision."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import datetime as dt
import gzip
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile


FULL_COMMIT = re.compile(r"[0-9a-fA-F]{40}")
SAFE_TAG = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
PROJECT_VERSION = re.compile(
    r"\bproject\s*\(\s*FeedForge\b.*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    re.DOTALL,
)
HEADER_VERSION = re.compile(r'\bversion_string\s*=\s*"([^"]+)"')


class ReleaseAssetError(RuntimeError):
    """An actionable release-asset validation failure."""


@dataclass(frozen=True)
class ResolvedRevision:
    commit: str
    tag: str | None


@dataclass(frozen=True)
class SourceEntry:
    name: str
    kind: str
    mode: int
    data: bytes = b""
    linkname: str = ""


@dataclass(frozen=True)
class SourceSnapshot:
    commit: str
    epoch: int
    version: str
    entries: tuple[SourceEntry, ...]

    @property
    def prefix(self) -> str:
        return f"feedforge-v{self.version}"


def _run_git(repository: Path, *arguments: str) -> bytes:
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "TZ": "UTC"})
    result = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        command = " ".join(("git", *arguments))
        raise ReleaseAssetError(f"{command} failed: {detail or 'unknown Git error'}")
    return result.stdout


def resolve_revision(repository: Path, revision: str) -> ResolvedRevision:
    """Accept only a full commit object ID or an exact, simple tag name."""

    if FULL_COMMIT.fullmatch(revision):
        commit = _run_git(
            repository, "rev-parse", "--verify", f"{revision}^{{commit}}"
        ).decode("ascii").strip()
        if commit.lower() != revision.lower():
            raise ReleaseAssetError(f"revision is not the exact commit {revision}")
        return ResolvedRevision(commit=commit, tag=None)

    tag = revision.removeprefix("refs/tags/")
    if not SAFE_TAG.fullmatch(tag):
        raise ReleaseAssetError(
            "revision must be a full 40-character commit ID or an exact tag name"
        )
    ref = f"refs/tags/{tag}"
    _run_git(repository, "show-ref", "--verify", "--hash", ref)
    commit = _run_git(
        repository, "rev-parse", "--verify", f"{ref}^{{commit}}"
    ).decode("ascii").strip()
    return ResolvedRevision(commit=commit, tag=tag)


def _tree_modes(repository: Path, commit: str) -> dict[str, int]:
    raw = _run_git(repository, "ls-tree", "-r", "-z", "--full-tree", commit)
    modes: dict[str, int] = {}
    for record in raw.split(b"\0"):
        if not record:
            continue
        metadata, encoded_name = record.split(b"\t", 1)
        encoded_mode, object_type, _object_id = metadata.split(b" ", 2)
        name = encoded_name.decode("utf-8", errors="surrogateescape")
        mode = int(encoded_mode, 8)
        if object_type == b"commit" or mode == 0o160000:
            raise ReleaseAssetError(
                f"source release cannot contain the unexpanded gitlink {name!r}"
            )
        modes[name] = mode
    return modes


def _validate_archive_name(name: str) -> str:
    normalized = name.rstrip("/")
    path = PurePosixPath(normalized)
    if (
        not normalized
        or path.is_absolute()
        or ".." in path.parts
        or "\\" in normalized
    ):
        raise ReleaseAssetError(f"unsafe path in Git archive: {name!r}")
    return normalized


def _validate_linkname(name: str, linkname: str) -> None:
    target = PurePosixPath(linkname)
    if not linkname or target.is_absolute() or "\\" in linkname:
        raise ReleaseAssetError(f"unsafe symlink target for {name!r}: {linkname!r}")
    depth = len(PurePosixPath(name).parent.parts)
    for part in target.parts:
        if part == "..":
            depth -= 1
            if depth < 0:
                raise ReleaseAssetError(
                    f"symlink target escapes the source archive: {name!r}"
                )
        elif part not in ("", "."):
            depth += 1


def _snapshot_entries(repository: Path, commit: str) -> tuple[SourceEntry, ...]:
    modes = _tree_modes(repository, commit)
    archived = _run_git(repository, "archive", "--format=tar", commit)
    entries: list[SourceEntry] = []
    names: set[str] = set()

    with tarfile.open(fileobj=io.BytesIO(archived), mode="r:") as source:
        for member in source.getmembers():
            name = _validate_archive_name(member.name)
            if name in names:
                raise ReleaseAssetError(f"duplicate path in Git archive: {name!r}")
            names.add(name)

            if member.isdir():
                entries.append(SourceEntry(name=name, kind="directory", mode=0o755))
                continue

            tree_mode = modes.get(name)
            if tree_mode is None:
                raise ReleaseAssetError(
                    f"Git archive path is absent from the tree: {name}"
                )
            if member.isfile():
                extracted = source.extractfile(member)
                if extracted is None:
                    raise ReleaseAssetError(f"could not read archived file: {name}")
                mode = 0o755 if tree_mode == 0o100755 else 0o644
                entries.append(
                    SourceEntry(
                        name=name, kind="file", mode=mode, data=extracted.read()
                    )
                )
            elif member.issym():
                _validate_linkname(name, member.linkname)
                entries.append(
                    SourceEntry(
                        name=name,
                        kind="symlink",
                        mode=0o777,
                        linkname=member.linkname,
                    )
                )
            else:
                raise ReleaseAssetError(
                    f"unsupported member type in Git archive: {name!r}"
                )

    return tuple(sorted(entries, key=lambda entry: entry.name))


def _required_file(entries: tuple[SourceEntry, ...], name: str) -> bytes:
    for entry in entries:
        if entry.name == name and entry.kind == "file":
            return entry.data
    raise ReleaseAssetError(f"source revision does not contain required file {name}")


def load_snapshot(repository: Path, revision: str) -> SourceSnapshot:
    repository = repository.resolve()
    resolved = resolve_revision(repository, revision)
    entries = _snapshot_entries(repository, resolved.commit)

    cmake_text = _required_file(entries, "CMakeLists.txt").decode("utf-8")
    header_text = _required_file(entries, "include/feedforge/version.hpp").decode(
        "utf-8"
    )
    project_match = PROJECT_VERSION.search(cmake_text)
    header_match = HEADER_VERSION.search(header_text)
    if project_match is None or header_match is None:
        raise ReleaseAssetError(
            "could not read the project version from committed files"
        )
    version = project_match.group(1)
    if header_match.group(1) != version:
        raise ReleaseAssetError(
            "CMake project version and feedforge::version_string do not match"
        )
    if resolved.tag is not None and resolved.tag != f"v{version}":
        raise ReleaseAssetError(
            f"tag {resolved.tag!r} does not match committed version v{version}"
        )

    epoch_text = _run_git(
        repository, "show", "-s", "--format=%ct", resolved.commit
    ).decode("ascii").strip()
    try:
        epoch = int(epoch_text)
    except ValueError as error:
        raise ReleaseAssetError("Git returned an invalid commit timestamp") from error

    return SourceSnapshot(
        commit=resolved.commit,
        epoch=epoch,
        version=version,
        entries=entries,
    )


def _tar_info(snapshot: SourceSnapshot, entry: SourceEntry) -> tarfile.TarInfo:
    info = tarfile.TarInfo(f"{snapshot.prefix}/{entry.name}")
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mode = entry.mode
    info.mtime = snapshot.epoch
    info.pax_headers = {}
    if entry.kind == "directory":
        info.type = tarfile.DIRTYPE
        info.size = 0
    elif entry.kind == "symlink":
        info.type = tarfile.SYMTYPE
        info.linkname = entry.linkname
        info.size = 0
    else:
        info.type = tarfile.REGTYPE
        info.size = len(entry.data)
    return info


def _write_tar_gz(snapshot: SourceSnapshot, destination: Path) -> None:
    uncompressed = io.BytesIO()
    with tarfile.open(
        fileobj=uncompressed,
        mode="w",
        format=tarfile.PAX_FORMAT,
        pax_headers={"comment": snapshot.commit},
    ) as archive:
        root = SourceEntry(snapshot.prefix, "directory", 0o755)
        root_info = _tar_info(snapshot, root)
        root_info.name = snapshot.prefix
        archive.addfile(root_info)
        for entry in snapshot.entries:
            info = _tar_info(snapshot, entry)
            data = io.BytesIO(entry.data) if entry.kind == "file" else None
            archive.addfile(info, data)

    with destination.open("wb") as output:
        with gzip.GzipFile(
            filename="",
            mode="wb",
            fileobj=output,
            compresslevel=9,
            mtime=snapshot.epoch,
        ) as compressed:
            compressed.write(uncompressed.getvalue())


def _zip_timestamp(epoch: int) -> tuple[int, int, int, int, int, int]:
    value = dt.datetime.fromtimestamp(epoch, tz=dt.timezone.utc)
    if value.year < 1980 or value.year > 2107:
        raise ReleaseAssetError("commit timestamp is outside the ZIP date range")
    return (
        value.year,
        value.month,
        value.day,
        value.hour,
        value.minute,
        value.second & ~1,
    )


def _zip_info(
    snapshot: SourceSnapshot, entry: SourceEntry, *, root: bool = False
) -> zipfile.ZipInfo:
    name = snapshot.prefix if root else f"{snapshot.prefix}/{entry.name}"
    if entry.kind == "directory":
        name += "/"
    info = zipfile.ZipInfo(name, date_time=_zip_timestamp(snapshot.epoch))
    info.create_system = 3
    info.compress_type = zipfile.ZIP_STORED
    if entry.kind == "directory":
        info.external_attr = ((stat.S_IFDIR | entry.mode) << 16) | 0x10
    elif entry.kind == "symlink":
        info.external_attr = (stat.S_IFLNK | entry.mode) << 16
    else:
        info.external_attr = (stat.S_IFREG | entry.mode) << 16
    return info


def _write_zip(snapshot: SourceSnapshot, destination: Path) -> None:
    with zipfile.ZipFile(
        destination, mode="w", compression=zipfile.ZIP_STORED, allowZip64=True
    ) as archive:
        archive.comment = f"feedforge-commit:{snapshot.commit}\n".encode("ascii")
        root = SourceEntry(snapshot.prefix, "directory", 0o755)
        archive.writestr(_zip_info(snapshot, root, root=True), b"")
        for entry in snapshot.entries:
            data = (
                entry.linkname.encode("utf-8", errors="surrogateescape")
                if entry.kind == "symlink"
                else entry.data
            )
            archive.writestr(_zip_info(snapshot, entry), data)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _asset_names(snapshot: SourceSnapshot) -> tuple[str, str, str]:
    stem = snapshot.prefix
    return f"{stem}-source.tar.gz", f"{stem}-source.zip", "SHA256SUMS"


def _prepare_output_directory(output_directory: Path) -> Path:
    output_directory = output_directory.resolve()
    if output_directory.exists():
        if not output_directory.is_dir():
            raise ReleaseAssetError(
                f"output path is not a directory: {output_directory}"
            )
        if any(output_directory.iterdir()):
            raise ReleaseAssetError(
                f"output directory must be empty: {output_directory}"
            )
    else:
        output_directory.mkdir(parents=True)
    return output_directory


def build_assets(
    repository: Path, revision: str, output_directory: Path
) -> SourceSnapshot:
    snapshot = load_snapshot(repository, revision)
    output_directory = _prepare_output_directory(output_directory)
    tar_name, zip_name, checksum_name = _asset_names(snapshot)
    tar_path = output_directory / tar_name
    zip_path = output_directory / zip_name
    _write_tar_gz(snapshot, tar_path)
    _write_zip(snapshot, zip_path)

    checksum_lines = [
        f"{_sha256(path)}  {path.name}" for path in sorted((tar_path, zip_path))
    ]
    (output_directory / checksum_name).write_text(
        "\n".join(checksum_lines) + "\n", encoding="ascii", newline="\n"
    )
    verify_assets(snapshot, output_directory)
    return snapshot


def _expected_records(snapshot: SourceSnapshot) -> dict[str, SourceEntry]:
    records = {
        snapshot.prefix: SourceEntry(snapshot.prefix, "directory", 0o755)
    }
    for entry in snapshot.entries:
        records[f"{snapshot.prefix}/{entry.name}"] = entry
    return records


def _verify_tar(snapshot: SourceSnapshot, path: Path) -> None:
    expected = _expected_records(snapshot)
    actual: dict[str, SourceEntry] = {}
    with tarfile.open(path, mode="r:gz") as archive:
        if archive.pax_headers.get("comment") != snapshot.commit:
            raise ReleaseAssetError("tar archive does not identify the source commit")
        for member in archive.getmembers():
            name = member.name.rstrip("/")
            if name in actual:
                raise ReleaseAssetError(f"duplicate tar member: {name}")
            if int(member.mtime) != snapshot.epoch:
                raise ReleaseAssetError(f"tar member has unstable timestamp: {name}")
            mode = member.mode & 0o777
            if member.isdir():
                actual[name] = SourceEntry(name, "directory", mode)
            elif member.issym():
                actual[name] = SourceEntry(
                    name, "symlink", mode, linkname=member.linkname
                )
            elif member.isfile():
                extracted = archive.extractfile(member)
                if extracted is None:
                    raise ReleaseAssetError(f"could not read tar member: {name}")
                actual[name] = SourceEntry(name, "file", mode, extracted.read())
            else:
                raise ReleaseAssetError(f"unsupported tar member: {name}")

    _compare_records("tar", expected, actual)


def _verify_zip(snapshot: SourceSnapshot, path: Path) -> None:
    expected = _expected_records(snapshot)
    actual: dict[str, SourceEntry] = {}
    expected_comment = f"feedforge-commit:{snapshot.commit}\n".encode("ascii")
    with zipfile.ZipFile(path, mode="r") as archive:
        if archive.comment != expected_comment:
            raise ReleaseAssetError("zip archive does not identify the source commit")
        for info in archive.infolist():
            name = info.filename.rstrip("/")
            if name in actual:
                raise ReleaseAssetError(f"duplicate zip member: {name}")
            if info.date_time != _zip_timestamp(snapshot.epoch):
                raise ReleaseAssetError(f"zip member has unstable timestamp: {name}")
            unix_mode = (info.external_attr >> 16) & 0xFFFF
            mode = stat.S_IMODE(unix_mode)
            data = archive.read(info)
            if stat.S_ISDIR(unix_mode):
                actual[name] = SourceEntry(name, "directory", mode)
            elif stat.S_ISLNK(unix_mode):
                actual[name] = SourceEntry(
                    name,
                    "symlink",
                    mode,
                    linkname=data.decode("utf-8", errors="surrogateescape"),
                )
            elif stat.S_ISREG(unix_mode):
                actual[name] = SourceEntry(name, "file", mode, data)
            else:
                raise ReleaseAssetError(f"unsupported zip member mode: {name}")

    _compare_records("zip", expected, actual)


def _compare_records(
    archive_kind: str,
    expected: dict[str, SourceEntry],
    actual: dict[str, SourceEntry],
) -> None:
    if expected.keys() != actual.keys():
        missing = sorted(expected.keys() - actual.keys())
        extra = sorted(actual.keys() - expected.keys())
        raise ReleaseAssetError(
            f"{archive_kind} member mismatch; missing={missing}, extra={extra}"
        )
    for name, expected_entry in expected.items():
        actual_entry = actual[name]
        if (
            expected_entry.kind,
            expected_entry.mode,
            expected_entry.data,
            expected_entry.linkname,
        ) != (
            actual_entry.kind,
            actual_entry.mode,
            actual_entry.data,
            actual_entry.linkname,
        ):
            raise ReleaseAssetError(f"{archive_kind} member differs: {name}")


def verify_assets(snapshot: SourceSnapshot, output_directory: Path) -> None:
    tar_name, zip_name, checksum_name = _asset_names(snapshot)
    expected_names = {tar_name, zip_name, checksum_name}
    actual_names = {path.name for path in output_directory.iterdir()}
    if actual_names != expected_names:
        raise ReleaseAssetError(
            f"release asset set mismatch; expected={sorted(expected_names)}, "
            f"actual={sorted(actual_names)}"
        )

    checksum_path = output_directory / checksum_name
    lines = checksum_path.read_text(encoding="ascii").splitlines()
    expected_lines = [
        f"{_sha256(output_directory / name)}  {name}"
        for name in sorted((tar_name, zip_name))
    ]
    if lines != expected_lines:
        raise ReleaseAssetError("SHA256SUMS does not match the primary assets")
    if checksum_name in checksum_path.read_text(encoding="ascii"):
        raise ReleaseAssetError("SHA256SUMS must not contain a circular self-hash")

    _verify_tar(snapshot, output_directory / tar_name)
    _verify_zip(snapshot, output_directory / zip_name)


def check_determinism(repository: Path, revision: str) -> SourceSnapshot:
    with tempfile.TemporaryDirectory(prefix="feedforge-release-a-") as first_raw:
        with tempfile.TemporaryDirectory(prefix="feedforge-release-b-") as second_raw:
            first = Path(first_raw)
            second = Path(second_raw)
            first_snapshot = build_assets(repository, revision, first)
            second_snapshot = build_assets(repository, revision, second)
            if first_snapshot != second_snapshot:
                raise ReleaseAssetError("resolved source snapshots differ")
            for name in sorted(path.name for path in first.iterdir()):
                if (first / name).read_bytes() != (second / name).read_bytes():
                    raise ReleaseAssetError(f"repeated build differs: {name}")
            return first_snapshot


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    default_repository = Path(__file__).resolve().parents[1]

    build = subparsers.add_parser("build", help="build and verify release assets")
    build.add_argument("--repository", type=Path, default=default_repository)
    build.add_argument("--revision", required=True)
    build.add_argument("--output-dir", type=Path, required=True)

    check = subparsers.add_parser(
        "check", help="build twice and require byte-identical assets"
    )
    check.add_argument("--repository", type=Path, default=default_repository)
    check.add_argument("--revision", required=True)
    return parser


def main(arguments: list[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "build":
            snapshot = build_assets(
                options.repository, options.revision, options.output_dir
            )
            print(f"built FeedForge v{snapshot.version} assets from {snapshot.commit}")
            for path in sorted(options.output_dir.resolve().iterdir()):
                print(path)
        else:
            snapshot = check_determinism(options.repository, options.revision)
            print(
                f"release assets are deterministic for v{snapshot.version} "
                f"at {snapshot.commit}"
            )
    except (OSError, UnicodeError, ReleaseAssetError) as error:
        print(f"release_assets.py: error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
