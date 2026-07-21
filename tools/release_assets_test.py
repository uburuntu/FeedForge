#!/usr/bin/env python3
"""Focused tests for deterministic source release assets."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

import release_assets


class RepositoryFixture:
    def __init__(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="feedforge-release-test-")
        self.path = Path(self.temporary.name)
        self._git("init", "--quiet")
        self._git("config", "user.name", "FeedForge Test")
        self._git("config", "user.email", "feedforge-test@example.invalid")
        self._write_version("1.2.3", "1.2.3")
        (self.path / "nested").mkdir()
        (self.path / "nested" / "data.txt").write_text("payload\n", encoding="ascii")
        executable = self.path / "run.sh"
        executable.write_text("#!/bin/sh\nexit 0\n", encoding="ascii")
        executable.chmod(0o755)
        try:
            os.symlink("nested/data.txt", self.path / "data-link")
        except (NotImplementedError, OSError):
            pass
        self._commit("initial")
        self._git("tag", "-a", "v1.2.3", "-m", "release 1.2.3")

    def close(self) -> None:
        self.temporary.cleanup()

    def _git(self, *arguments: str) -> str:
        environment = os.environ.copy()
        environment.update(
            {
                "GIT_AUTHOR_DATE": "2024-01-02T03:04:06Z",
                "GIT_COMMITTER_DATE": "2024-01-02T03:04:06Z",
            }
        )
        result = subprocess.run(
            ["git", "-C", str(self.path), *arguments],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment,
        )
        return result.stdout.strip()

    def _write_version(self, project: str, header: str) -> None:
        (self.path / "include" / "feedforge").mkdir(parents=True, exist_ok=True)
        (self.path / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            f"project(FeedForge VERSION {project} LANGUAGES CXX)\n",
            encoding="ascii",
        )
        (self.path / "include" / "feedforge" / "version.hpp").write_text(
            "#include <string_view>\n"
            "namespace feedforge {\n"
            f'inline constexpr std::string_view version_string = "{header}";\n'
            "}\n",
            encoding="ascii",
        )

    def _commit(self, message: str) -> str:
        self._git("add", "--all")
        self._git("commit", "--quiet", "-m", message)
        return self.commit

    @property
    def commit(self) -> str:
        return self._git("rev-parse", "HEAD")


class ReleaseAssetsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.repository = RepositoryFixture()

    def tearDown(self) -> None:
        self.repository.close()

    def test_full_commit_build_is_reproducible_and_excludes_worktree(self) -> None:
        (self.repository.path / "untracked-secret.txt").write_text(
            "not released\n", encoding="ascii"
        )
        (self.repository.path / "nested" / "data.txt").write_text(
            "dirty\n", encoding="ascii"
        )
        with tempfile.TemporaryDirectory() as first_raw:
            with tempfile.TemporaryDirectory() as second_raw:
                first = Path(first_raw)
                second = Path(second_raw)
                snapshot = release_assets.build_assets(
                    self.repository.path, self.repository.commit, first
                )
                release_assets.build_assets(
                    self.repository.path, self.repository.commit, second
                )
                self.assertEqual(snapshot.version, "1.2.3")
                self.assertEqual(
                    {path.name for path in first.iterdir()},
                    {
                        "feedforge-v1.2.3-source.tar.gz",
                        "feedforge-v1.2.3-source.zip",
                        "SHA256SUMS",
                    },
                )
                for path in first.iterdir():
                    self.assertEqual(
                        path.read_bytes(), (second / path.name).read_bytes()
                    )
                sums = (first / "SHA256SUMS").read_text(encoding="ascii")
                self.assertNotIn("SHA256SUMS", sums)
                with zipfile.ZipFile(first / "feedforge-v1.2.3-source.zip") as archive:
                    names = set(archive.namelist())
                    self.assertNotIn(
                        "feedforge-v1.2.3/untracked-secret.txt", names
                    )
                    self.assertEqual(
                        archive.read("feedforge-v1.2.3/nested/data.txt"), b"payload\n"
                    )

    def test_exact_matching_tag_is_supported(self) -> None:
        with tempfile.TemporaryDirectory() as output:
            snapshot = release_assets.build_assets(
                self.repository.path, "v1.2.3", Path(output)
            )
        self.assertEqual(snapshot.commit, self.repository.commit)

    def test_branches_and_abbreviated_commits_are_rejected(self) -> None:
        for revision in ("HEAD", "main", self.repository.commit[:12]):
            with self.subTest(revision=revision):
                with self.assertRaises(release_assets.ReleaseAssetError):
                    release_assets.resolve_revision(self.repository.path, revision)

    def test_mismatched_tag_is_rejected(self) -> None:
        self.repository._git("tag", "-a", "not-a-release", "-m", "wrong tag")
        with self.assertRaisesRegex(
            release_assets.ReleaseAssetError, "does not match committed version"
        ):
            release_assets.load_snapshot(self.repository.path, "not-a-release")

    def test_version_mismatch_is_rejected(self) -> None:
        self.repository._write_version("1.2.4", "1.2.3")
        commit = self.repository._commit("mismatch")
        with self.assertRaisesRegex(release_assets.ReleaseAssetError, "do not match"):
            release_assets.load_snapshot(self.repository.path, commit)

    def test_symlink_cannot_escape_archive_root(self) -> None:
        escape = self.repository.path / "escape"
        try:
            os.symlink("../outside", escape)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable")
        commit = self.repository._commit("escaping symlink")
        with self.assertRaisesRegex(release_assets.ReleaseAssetError, "escapes"):
            release_assets.load_snapshot(self.repository.path, commit)

    def test_nonempty_output_directory_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as output_raw:
            output = Path(output_raw)
            (output / "keep.txt").write_text("keep\n", encoding="ascii")
            with self.assertRaisesRegex(
                release_assets.ReleaseAssetError, "must be empty"
            ):
                release_assets.build_assets(
                    self.repository.path, self.repository.commit, output
                )
            self.assertEqual((output / "keep.txt").read_text(), "keep\n")

    def test_check_builds_byte_identical_assets(self) -> None:
        snapshot = release_assets.check_determinism(
            self.repository.path, self.repository.commit
        )
        self.assertEqual(snapshot.version, "1.2.3")


if __name__ == "__main__":
    unittest.main()
