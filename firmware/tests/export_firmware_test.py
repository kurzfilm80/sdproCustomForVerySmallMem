"""Verify independent exports, incremental replacement and OTA rejection."""
from pathlib import Path
import runpy
import tempfile

script = Path(__file__).resolve().parents[1] / 'scripts/export_firmware.py'
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    project = root / 'firmware'
    project.mkdir()
    class Environment:
        name = 'sdpro-weather'
        def subst(self, value):
            return {'$PROJECT_DIR': str(project), '$PIOENV': self.name,
                    '$BUILD_DIR/${PROGNAME}.bin': str(root / (self.name + '.bin'))}[value]
        def AddPostAction(self, target, callback):
            assert target == 'buildprog'
            self.callback = callback
        def AlwaysBuild(self, target):
            assert target == 'buildprog'
    env = Environment()
    runpy.run_path(str(script), init_globals={'env': env, 'Import': lambda _: None})
    (root / 'sdpro-weather.bin').write_bytes(b'weather')
    env.callback(None, None, env)
    env.name = 'sdpro-ticker'
    (root / 'sdpro-ticker.bin').write_bytes(b'ticker')
    env.callback(None, None, env)
    assert (root / 'dist/sdpro-weather.bin').read_bytes() == b'weather'
    assert (root / 'dist/sdpro-ticker.bin').read_bytes() == b'ticker'
    (root / 'sdpro-ticker.bin').write_bytes(b'incremental')
    env.callback(None, None, env)
    assert (root / 'dist/sdpro-ticker.bin').read_bytes() == b'incremental'
    (root / 'sdpro-ticker.bin').write_bytes(bytes(1044465))
    try:
        env.callback(None, None, env)
        raise AssertionError('Oversized image accepted')
    except RuntimeError:
        pass
    assert (root / 'dist/sdpro-ticker.bin').read_bytes() == b'incremental'
print('PASS: export paths, distinct images, incremental refresh, OTA size rejection')
