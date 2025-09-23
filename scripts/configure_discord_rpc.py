#!/usr/bin/env python3

import shutil
import subprocess
from pathlib import Path

import call_wrapper
from apply_patch import patch

def is_submodule_clean(submodule_path):
    try:
        result = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=submodule_path,
            capture_output=True,
            text=True,
            check=True
        )
        return not result.stdout.strip()
    except subprocess.CalledProcessError:
        return False

def clean_submodule(submodule_path):
    print(f"Cleaning dirty submodule: {submodule_path}")

    # reset changes
    subprocess.run(
        ["git", "reset", "--hard", "HEAD"],
        cwd=submodule_path,
        check=True
    )

    # get rid of untracked crap
    subprocess.run(
        ["git", "clean", "-fd"],
        cwd=submodule_path,
        check=True
    )

def configure():
    cur_dir = Path(__file__).parent.absolute()
    root_dir = cur_dir.parent
    discord_dir = root_dir/"submodules"/"discord-rpc"
    assert(discord_dir.exists() and discord_dir.is_dir())

    # is it clean? failure to patch will not clean after itself, so it's easier to clean it here
    if not is_submodule_clean(discord_dir):
        clean_submodule(discord_dir)

    shutil.copy2(cur_dir/"additional_files"/"discord-rpc.vcxproj", str(discord_dir/"src") + '/')

    # apply patches to the ancient, deprecated, discord-rpc library that's already outlived its replacement
    patches_dir = cur_dir/"additional_files"
    patch_files = [
        "discord-rpc.patch",
        "discord-rpc-cmake.patch"
    ]

    patches_to_apply = []
    for patch_file in patch_files:
        patch_path = patches_dir / patch_file
        if patch_path.exists():
            patches_to_apply.append(patch_path)

    if patches_to_apply:
        patch(patches_to_apply)

if __name__ == '__main__':
    call_wrapper.final_call_decorator(
        "Configuring Discord RPC",
        "Configuring Discord RPC: success",
        "Configuring Discord RPC: failure!"
    )(configure)()
