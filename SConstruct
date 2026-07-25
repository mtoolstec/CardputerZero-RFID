from pathlib import Path
import os
import platform
import subprocess
import shutil
import sys

arch = platform.machine()
homebrew_toolchain = "/opt/homebrew/bin"
has_homebrew_aarch64 = os.path.exists(os.path.join(homebrew_toolchain, "aarch64-linux-gnu-gcc"))

version = "v0.0.3"
static_lib = "build/static_lib"
local_path = Path(os.getcwd())
sdk_path = local_path.parent.parent / "SDK"
dependency_dir = sdk_path / "github_source" / f"static_lib_{version}"
dependency_archive = sdk_path / "github_source" / f"static_lib_{version}.tar.gz"

if "CardputerZero" in os.environ:
    if not os.path.exists("build/config/config_tmp.mk"):
        os.makedirs("build/config", exist_ok=True)
        with open("build/config/config_tmp.mk", "w") as f:
            f.write("CONFIG_V9_5_LV_USE_LINUX_FBDEV=y\n")
            f.write("CONFIG_V9_5_LV_USE_SDL=n\n")
            f.write("CONFIG_V9_5_LV_DRAW_SW_ASM_NEON=y\n")
            f.write("CONFIG_V9_5_LV_USE_DRAW_SW_ASM=1\n")
            f.write("CONFIG_V9_5_LV_USE_EVDEV=y\n")
            if has_homebrew_aarch64:
                f.write(f'CONFIG_TOOLCHAIN_PATH="{homebrew_toolchain}"\n')
            f.write('CONFIG_TOOLCHAIN_PREFIX="aarch64-linux-gnu-"\n')
            f.write(f'''CONFIG_TOOLCHAIN_SYSROOT="{os.path.join(sys.path[0], "build", "static_lib")}"\n''')
            f.write(f'''CONFIG_TOOLCHAIN_FLAGS="-I{os.path.join(sys.path[0], "build", "static_lib", "usr", "include", "aarch64-linux-gnu")}"\n''')
            f.write(f'''CONFIG_TOOLCHAIN_FLAGS="-I{os.path.join(sys.path[0], "static_lib", "usr", "include", "aarch64-linux-gnu")}"\n''')
elif arch != "aarch64":
    if not os.path.exists("build/config/config_tmp.mk"):
        os.makedirs("build/config", exist_ok=True)
        with open("build/config/config_tmp.mk", "w") as f:
            f.write("CONFIG_V9_5_LV_USE_SDL=y\n")
else:
    if not os.path.exists("build/config/config_tmp.mk"):
        os.makedirs("build/config", exist_ok=True)
        with open("build/config/config_tmp.mk", "w") as f:
            f.write("CONFIG_V9_5_LV_USE_LINUX_FBDEV=y\n")
            f.write("CONFIG_V9_5_LV_DRAW_SW_ASM_NEON=y\n")
            f.write("CONFIG_V9_5_LV_USE_DRAW_SW_ASM=1\n")
            f.write("CONFIG_V9_5_LV_USE_EVDEV=y\n")

os.environ["SDK_PATH"] = str(sdk_path)
os.environ["EXT_COMPONENTS_PATH"] = str(sdk_path.parent / "ext_components")

env = SConscript(
    str(sdk_path / "tools" / "scons" / "project.py"),
    variant_dir=os.getcwd(),
    duplicate=0,
)

# ── Inject git commit hash as compile-time define for AppInfo modal ──────────
try:
    import subprocess
    import datetime
    git_hash = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        text=True, cwd=os.getcwd()
    ).strip()
    # Always include current date/time so AppInfo shows when it was built
    build_label = "{}_{}Z".format(git_hash, datetime.datetime.utcnow().strftime("%m%dT%H%M"))
    env.Append(CPPDEFINES={"RFID_BUILD_HASH": '"{}"'.format(build_label)})
    print("[RFID] Build version: {}".format(build_label))
except Exception:
    pass  # fallback: code uses __DATE__ __TIME__

dependency_ready = False
if dependency_dir.exists():
    try:
        with open(dependency_dir / "version", "r") as f:
            dependency_ready = f.read().strip() == version
    except Exception:
        dependency_ready = False

if not dependency_ready and "CardputerZero" in os.environ:
    with open(env["PROJECT_TOOL_S"]) as f:
        exec(f.read())
    down_url = "https://github.com/dianjixz/M5CardputerZero-UserDemo/releases/download/{}/sdk_bsp.tar.gz".format(
        version
    )
    if not dependency_archive.exists():
        dependency_archive.parent.mkdir(parents=True, exist_ok=True)
        subprocess.check_call(["curl", "-L", down_url, "-o", str(dependency_archive)])
    if dependency_dir.exists():
        shutil.rmtree(dependency_dir)
    dependency_dir.mkdir(parents=True, exist_ok=True)
    subprocess.check_call(["tar", "-xzf", str(dependency_archive), "-C", str(dependency_dir)])
    with open(dependency_dir / "version", "w") as f:
        f.write(version)

if dependency_dir.exists():
    static_lib_path = Path(static_lib)
    if static_lib_path.is_symlink() or static_lib_path.exists():
        if static_lib_path.is_dir() and not static_lib_path.is_symlink():
            shutil.rmtree(static_lib_path)
        else:
            static_lib_path.unlink()
    static_lib_path.parent.mkdir(parents=True, exist_ok=True)
    static_lib_path.symlink_to(dependency_dir, target_is_directory=True)
