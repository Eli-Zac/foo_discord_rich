# Build Instructions

## Requirements
- Visual Studio 2022 (MSVC C++ v142 and v143 toolchain with respective ATL; Windows 10 SDK)
- CMake 3.5+
- Python with semver package

## Building

0. Clone repository resolving submodules
   ```bash
   git clone --recursive https://github.com/wsnrq/foo_discord_rich
   ```

1. Setup
   ```bash
   python scripts/setup.py
   ```

2. Build
   ```bash
   MSBuild.exe workspaces/foo_discord_rich.sln -p:Configuration=Release -p:Platform=Win32
   MSBuild.exe workspaces/foo_discord_rich.sln -p:Configuration=Release -p:Platform=x64
   ```
   
3. Package:
   ```bash
   python scripts/pack_component.py
   ```

### Notes

If you've made any changes and bumped the version, use the following to regenerate headers without setting project up again, before the build process:
```cd scripts && python ../submodules/fb2k_utils/scripts/generate_version_header.py --output_dir ../_result/AllPlatforms/generated --component_prefix DRP```

**SourceLink:**
- Disabled by default, to enable run build with `/p:EnableSourceLink=true`