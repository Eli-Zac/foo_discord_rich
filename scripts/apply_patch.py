import subprocess
from pathlib import Path


def _can_apply(root_dir: Path, patch_file: Path, reverse: bool = False) -> bool:
    cmd = ["git", "apply", "--ignore-whitespace", "--check"]
    if reverse:
        cmd.append("--reverse")
    cmd.append(str(patch_file))

    proc = subprocess.run(
        cmd,
        cwd=root_dir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return proc.returncode == 0


def patch(patches):
    cur_dir = Path(__file__).parent.absolute()
    root_dir = cur_dir.parent

    for p in patches:
        assert p.exists() and p.is_file()

    for patch_file in patches:
        if _can_apply(root_dir, patch_file):
            subprocess.check_call(
                ["git", "apply", "--ignore-whitespace", str(patch_file)],
                cwd=root_dir,
            )
        elif _can_apply(root_dir, patch_file, reverse=True):
            print(f"Patch already applied, skipping: {patch_file.name}")
        else:
            raise RuntimeError(
                f"Patch does not apply cleanly and is not already applied: {patch_file}"
            )
