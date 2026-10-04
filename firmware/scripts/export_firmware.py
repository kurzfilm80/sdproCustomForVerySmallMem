"""Export the completed SD PRO image beside the project directory."""

from pathlib import Path
import shutil

Import("env")  # noqa: F821 - PlatformIO/SCons build environment


def export_firmware(source, target, env):
    image = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    destination = Path(env.subst("$PROJECT_DIR")).resolve().parents[1] / "SDP-SeoulWeather.bin"
    shutil.copyfile(image, destination)
    print(f"Firmware exported: {destination} ({destination.stat().st_size} bytes)")


env.AddPostAction("buildprog", export_firmware)  # noqa: F821
env.AlwaysBuild("buildprog")  # noqa: F821 - Refresh the export on incremental builds too.
