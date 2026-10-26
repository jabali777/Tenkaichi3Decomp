"""Record one local session with isolated saves, timings and controller input."""
import csv
import datetime
import json
import os
import shutil
import signal
import subprocess
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROFILE_KEYS = {
    'BT3_GS', 'BT3_GPU_API', 'BT3_SCALE', 'BT3_WINDOW', 'BT3_FULLSCREEN',
    'BT3_INTERP', 'BT3_GS_THREAD', 'BT3_FX_OFF', 'BT3_GLOW', 'BT3_GLARE',
    'BT3_FRAME_SKIP', 'BT3_METER', 'BT3_2D_SMOOTH', 'BT3_SOMBREADO_SIMPLE',
    'BT3_OPTIMIZACION_GL', 'BT3_SEED', 'BT3_NOMOVIE', 'BT3_GS_VERBOSE',
    'BT3_BAJA_RESOLUCION', 'BT3_TEX_THREADS',
    'BT3_DRAW_LOG', 'BT3_UPDATE_CHECK', 'vblank_mode', 'mesa_glthread', 'LD_PRELOAD',
}


def write_json(path, data):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')
    temporary.replace(path)


def prepare_session(root):
    game = root / 'Tenkaichi3Decomp'
    recordings = root / 'performance-diagnostics'
    recordings.mkdir(exist_ok=True)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    session = Path(tempfile.mkdtemp(prefix='ataques-' + stamp + '-', dir=recordings))
    if (game / 'saves').is_dir():
        shutil.copytree(game / 'saves', session / 'saves-initial')
    else:
        (session / 'saves-initial').mkdir()
    shutil.copytree(session / 'saves-initial', session / 'saves-test')
    initial = session / 'settings-initial.txt'
    settings = game / 'bt3_settings.txt'
    initial.write_bytes(settings.read_bytes() if settings.exists() else b'')
    shutil.copyfile(initial, session / 'settings-test.txt')
    if (game / 'bt3_pipelines.txt').exists():
        shutil.copyfile(game / 'bt3_pipelines.txt', session / 'pipelines-test.txt')
    shutil.copyfile(root / 'optimization/libbt3_opengl_fast.so', session / 'adapter-initial.so')
    lowres = root / 'optimization/libbt3_resolution.so'
    if lowres.exists():
        shutil.copyfile(lowres, session / 'resolution-initial.so')
    shutil.copyfile(root / 'play-fast.sh', session / 'launcher-initial.sh')
    write_json(session / 'state.json', {'estado': 'preparado', 'inicio': datetime.datetime.now().isoformat()})
    return session


def recording_environment(session):
    env = os.environ.copy()
    for key in ('BT3_DEMO', 'BT3_REPLAY', 'BT3_PAD_PLAY', 'BT3_PAD_TABLE',
                'BT3_UNCAPPED', 'BT3_SHOT', 'BT3_SHOT_FROM', 'BT3_SHOT_TO',
                'BT3_GL_WINSHOT', 'BT3_SKIP_FORCE', 'BT3_FX_SKIP'):
        env.pop(key, None)
    env.update(BT3_SEED='1', BT3_GS_VERBOSE='1', BT3_DRAW_LOG='33', BT3_METER='1',
               BT3_UPDATE_CHECK='0', BT3_PAD_REC=str(session / 'controls.pad'),
               BT3_SAVES=str(session / 'saves-test'),
               BT3_SETTINGS=str(session / 'settings-test.txt'),
               BT3_PIPELINES=str(session / 'pipelines-test.txt'))
    return env


def running_game(executable):
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if os.readlink(entry / 'exe') == str(executable):
                return int(entry.name)
        except OSError:
            pass
    return None


def read_number(path):
    try:
        return int(Path(path).read_text().strip())
    except (OSError, ValueError):
        return ''


def sample_hardware(session, stop, started):
    sensors = []
    for folder in Path('/sys/class/hwmon').glob('hwmon*'):
        try:
            if folder.joinpath('name').read_text().strip() == 'coretemp':
                sensors.extend(folder.glob('temp*_input'))
        except OSError:
            pass
    with (session / 'hardware.csv').open('w', newline='') as output:
        writer = csv.writer(output)
        writer.writerow(['segundos', 'gpu_mhz', 'cpu0_khz', 'cpu1_khz', 'cpu_max_miligrados'])
        while not stop.is_set():
            temperatures = [read_number(path) for path in sensors]
            temperatures = [value for value in temperatures if isinstance(value, int)]
            writer.writerow([
                round(time.monotonic() - started, 3),
                read_number('/sys/class/drm/card0/gt_act_freq_mhz'),
                read_number('/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq'),
                read_number('/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq'),
                max(temperatures) if temperatures else '',
            ])
            output.flush()
            stop.wait(1)


def main():
    game = ROOT / 'Tenkaichi3Decomp/Tenkaichi3Decomp'
    required = (game, ROOT / 'optimization/libbt3_opengl_fast.so',
                ROOT / 'optimization/libbt3_resolution.so')
    missing = [str(path.relative_to(ROOT)) for path in required if not path.is_file()]
    if missing:
        print('Missing components: ' + ', '.join(missing), flush=True)
        print('Install the port and run: sh optimization/build.sh', flush=True)
        return 2
    pid = running_game(game)
    if pid is not None:
        print('Close the game before starting diagnostics; your game was not closed.', flush=True)
        return 2
    session = prepare_session(ROOT)
    (ROOT / 'performance-diagnostics/latest-session.txt').write_text(str(session) + '\n')
    print('Diagnostics:', session, flush=True)
    print('Play a LOCAL fight for 30–60 seconds with ultimate attacks, then close this copy.', flush=True)
    print('A copy of your saves is used; saves from this test do not replace the originals.', flush=True)
    started = time.monotonic()
    stop = threading.Event()
    sampler = threading.Thread(target=sample_hardware, args=(session, stop, started), daemon=True)
    state = {'estado': 'iniciando', 'inicio': datetime.datetime.now().isoformat(), 'sesion': str(session)}
    child = None
    sampler.start()
    try:
        with (session / 'performance.log').open('w') as log:
            child = subprocess.Popen([str(ROOT / 'play-fast.sh')], cwd=ROOT,
                                     env=recording_environment(session), stdin=subprocess.DEVNULL,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, errors='replace', bufsize=1)
            profile = {}
            for _ in range(20):
                try:
                    if os.readlink(f'/proc/{child.pid}/exe') == str(game):
                        raw = Path(f'/proc/{child.pid}/environ').read_bytes()
                        for item in raw.split(b'\0'):
                            key, separator, value = item.partition(b'=')
                            name = key.decode(errors='replace')
                            if separator and name in PROFILE_KEYS:
                                profile[name] = value.decode(errors='replace')
                        break
                except OSError:
                    pass
                if child.poll() is not None:
                    break
                time.sleep(.05)
            write_json(session / 'profile.json', profile)
            state.update(estado='grabando', pid=child.pid)
            write_json(session / 'state.json', state)
            flushed = time.monotonic()
            for line in child.stdout:
                now = time.monotonic()
                log.write(f'{now - started:.6f} {line}')
                if now - flushed >= .5:
                    log.flush()
                    flushed = now
            result = child.wait()
            state.update(estado='finalizado', codigo_salida=result)
    except BaseException as error:
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        state.update(estado='interrumpido', error=type(error).__name__)
        raise
    finally:
        stop.set()
        sampler.join(timeout=2)
        controls = session / 'controls.pad'
        state.update(segundos=round(time.monotonic() - started, 3),
                     bytes_controles=controls.stat().st_size if controls.exists() else 0)
        write_json(session / 'state.json', state)
    print('Recording finished:', session, flush=True)
    return state.get('codigo_salida', 1)


if __name__ == '__main__':
    raise SystemExit(main())
