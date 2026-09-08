Import("env")

from pathlib import Path
import os
import subprocess

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
python_exe = env.subst("$PYTHONEXE")
uploader = env.subst("$UPLOADER")

chip = env.GetProjectOption("custom_chip")
boot_offset = env.GetProjectOption("custom_boot_offset")
output_rel = env.GetProjectOption("custom_output")
output_path = project_dir / output_rel

framework_dir = Path(
    env.PioPlatform().get_package_dir(
        "framework-arduinoespressif32"
    )
)

boot_app0 = framework_dir / "tools" / "partitions" / "boot_app0.bin"

def merge_firmware(source, target, env):
    output_path.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"

    required = [
        bootloader,
        partitions,
        firmware,
        boot_app0,
    ]

    missing = [
        str(path)
        for path in required
        if not path.exists()
    ]

    if missing:
        raise RuntimeError(
            "Missing firmware build files: " +
            ", ".join(missing)
        )

    common_parts = [
        boot_offset,
        str(bootloader),
        "0x8000",
        str(partitions),
        "0xe000",
        str(boot_app0),
        "0x10000",
        str(firmware),
    ]

    # esptool v5 uses hyphenated command/flags. Older releases used
    # underscore-style names. Try the modern syntax first and fall back.
    modern_cmd = [
        python_exe,
        uploader,
        "--chip",
        chip,
        "merge-bin",
        "-o",
        str(output_path),
        "--flash-mode",
        "dio",
        "--flash-freq",
        "keep",
        "--flash-size",
        "keep",
    ] + common_parts

    legacy_cmd = [
        python_exe,
        uploader,
        "--chip",
        chip,
        "merge_bin",
        "-o",
        str(output_path),
        "--flash_mode",
        "dio",
        "--flash_freq",
        "keep",
        "--flash_size",
        "keep",
    ] + common_parts

    print(
        "Creating ESP Web Tools image:",
        output_path
    )

    try:
        subprocess.check_call(modern_cmd)
    except subprocess.CalledProcessError:
        print("Modern esptool syntax failed; retrying legacy syntax.")
        subprocess.check_call(legacy_cmd)

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.bin",
    merge_firmware
)
