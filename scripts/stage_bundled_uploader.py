#!/usr/bin/env python3

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


BINARY_NAME = "drp_artwork_uploader"
TARGETS = {
    "386": {
        "machine": 0x014C,
        "machine_name": "IMAGE_FILE_MACHINE_I386",
        "build_filename": f"{BINARY_NAME}-windows-386.exe",
        "resource_filename": f"{BINARY_NAME}_windows_386.exe",
    },
    "amd64": {
        "machine": 0x8664,
        "machine_name": "IMAGE_FILE_MACHINE_AMD64",
        "build_filename": f"{BINARY_NAME}-windows-amd64.exe",
        "resource_filename": f"{BINARY_NAME}_windows_amd64.exe",
    },
}


class StageError(Exception):
    pass


def get_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def resolve_default_uploader_dir(root_dir: Path) -> Path:
    env_dir = os.environ.get("DRP_UPLOADER_DIR")
    candidates = []
    if env_dir:
        candidates.append(Path(env_dir))

    candidates.extend(
        [
            root_dir.parent / "drp_artwork_uploader",
            root_dir / "drp_artwork_uploader",
        ]
    )

    for candidate in candidates:
        if (candidate / "go.mod").is_file():
            return candidate

    return candidates[0] if candidates else root_dir.parent / "drp_artwork_uploader"


def read_pe_machine(path: Path) -> int:
    with path.open("rb") as file:
        dos_header = file.read(64)
        if len(dos_header) < 64 or dos_header[:2] != b"MZ":
            raise StageError(f"{path} is not a PE executable: missing MZ header")

        pe_offset = struct.unpack_from("<I", dos_header, 0x3C)[0]
        file.seek(pe_offset)
        pe_header = file.read(6)
        if len(pe_header) < 6 or pe_header[:4] != b"PE\0\0":
            raise StageError(f"{path} is not a PE executable: missing PE header")

        return struct.unpack_from("<H", pe_header, 4)[0]


def validate_pe_machine(path: Path, arch: str) -> None:
    if not path.is_file():
        raise StageError(f"Expected uploader binary does not exist: {path}")

    expected_machine = TARGETS[arch]["machine"]
    actual_machine = read_pe_machine(path)
    if actual_machine != expected_machine:
        expected_name = TARGETS[arch]["machine_name"]
        raise StageError(
            f"{path} has PE machine 0x{actual_machine:04x}; "
            f"expected {expected_name} (0x{expected_machine:04x})"
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        while True:
            chunk = file.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def run_go_build(go_binary: str, uploader_dir: Path, output_path: Path, goarch: str, package: str) -> None:
    if not (uploader_dir / "go.mod").is_file():
        raise StageError(f"Uploader directory does not look like a Go module: {uploader_dir}")

    output_path.parent.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["CGO_ENABLED"] = "0"
    env["GOOS"] = "windows"
    env["GOARCH"] = goarch

    command = [
        go_binary,
        "build",
        "-trimpath",
        "-ldflags=-s -w",
        "-o",
        str(output_path),
        package,
    ]

    print(f"Building {output_path.name} from {uploader_dir}")
    try:
        subprocess.run(command, cwd=uploader_dir, env=env, check=True)
    except FileNotFoundError as e:
        raise StageError(f"Could not run Go compiler '{go_binary}': {e}") from e
    except subprocess.CalledProcessError as e:
        raise StageError(f"go build failed for GOARCH={goarch} with exit code {e.returncode}") from e


def stage_binary(source_path: Path, destination_path: Path, arch: str, root_dir: Path) -> None:
    validate_pe_machine(source_path, arch)
    destination_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_path, destination_path)
    validate_pe_machine(destination_path, arch)

    try:
        display_path = destination_path.relative_to(root_dir)
    except ValueError:
        display_path = destination_path

    print(
        f"Staged {display_path} "
        f"({arch}, {destination_path.stat().st_size} bytes, sha256 {sha256_file(destination_path)})"
    )


def parse_args() -> argparse.Namespace:
    root_dir = get_repo_root()
    default_uploader_dir = resolve_default_uploader_dir(root_dir)

    parser = argparse.ArgumentParser(
        description="Build and stage bundled drp_artwork_uploader Windows binaries."
    )
    parser.add_argument(
        "--uploader-dir",
        type=Path,
        default=default_uploader_dir,
        help=(
            "Path to the drp_artwork_uploader Go module. "
            "Defaults to DRP_UPLOADER_DIR, ../drp_artwork_uploader, or ./drp_artwork_uploader."
        ),
    )
    parser.add_argument(
        "--from-build-dir",
        type=Path,
        help=(
            "Stage existing binaries instead of running go build. "
            "The directory must contain drp_artwork_uploader-windows-386.exe and "
            "drp_artwork_uploader-windows-amd64.exe."
        ),
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Directory for go build outputs. Defaults to <uploader-dir>/build/component.",
    )
    parser.add_argument(
        "--resources-dir",
        type=Path,
        default=root_dir / "foo_discord_rich" / "resources",
        help="Component resource directory to copy binaries into.",
    )
    parser.add_argument(
        "--go",
        default="go",
        help="Go compiler command to run when --from-build-dir is not used.",
    )
    parser.add_argument(
        "--package",
        default=".",
        help="Go package to build from the uploader module.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root_dir = get_repo_root()

    if args.from_build_dir and args.build_dir:
        raise StageError("--build-dir is only used when building from --uploader-dir")

    if args.from_build_dir:
        source_dir = args.from_build_dir.resolve()
    else:
        uploader_dir = args.uploader_dir.resolve()
        source_dir = (args.build_dir or (uploader_dir / "build" / "component")).resolve()

        for arch, target in TARGETS.items():
            output_path = source_dir / target["build_filename"]
            run_go_build(args.go, uploader_dir, output_path, arch, args.package)

    resources_dir = args.resources_dir.resolve()
    for arch, target in TARGETS.items():
        source_path = source_dir / target["build_filename"]
        destination_path = resources_dir / target["resource_filename"]
        stage_binary(source_path, destination_path, arch, root_dir)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except StageError as e:
        print(f"error: {e}", file=sys.stderr)
        raise SystemExit(1)
