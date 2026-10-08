"""Export each SD PRO build to a uniquely named firmware image.

The build environment is used as the filename to prevent weather and ticker
images from silently overwriting one another.
"""

from pathlib import Path
import shutil

Import("env")  # noqa: F821 - PlatformIO/SCons build environment


def export_firmware(source, target, env):
    image = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    build_env = env.subst("$PIOENV")
    destination = Path(env.subst("$PROJECT_DIR")).resolve().parents[1] / f"{build_env}.bin"
    shutil.copyfile(image, destination)
    print(f"Firmware exported: {destination} ({destination.stat().st_size} bytes)")


env.AddPostAction("buildprog", export_firmware)  # noqa: F821
env.AlwaysBuild("buildprog")  # noqa: F821 - Refresh the export on incremental builds too.
