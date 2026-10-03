"""Capture both xemu builds' rendered backbuffers at one Halo 2276 tick.

Windows driver; requires WSL GDB, reachable QMP, and Pillow. --align-carousel
sets the saved two-player Carousel poses and resets first-person presentation
state through existing game routines. See tools/xbox/CAPTURE_MATCHING_FRAME.md.
"""
import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/xbox'))
from xemu_qmp import connect_qmp

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--align-carousel', action='store_true',
                    help='Apply the saved poses and reset first-person weapon presentation state')
parser.add_argument('--output', type=Path)
parser.add_argument('--lead-ticks', type=int, default=90)
parser.add_argument('--client-qmp', type=int, default=4444)
parser.add_argument('--host-qmp', type=int, default=4446)
args = parser.parse_args()
if args.lead_ticks < 30:
    parser.error('--lead-ticks must be at least 30')
repo = Path(__file__).resolve().parents[2]
root = (args.output or repo / 'tmp/frame-comparison-captures' /
        time.strftime('%Y%m%d-%H%M%S')).resolve()
root.mkdir(parents=True, exist_ok=False)

def wsl_path(path):
    resolved = path.resolve()
    return '/mnt/' + resolved.drive[0].lower() + resolved.as_posix()[2:]

def shell_quote(value):
    return "'" + str(value).replace("'", "'\\''") + "'"

wsl_root = wsl_path(root)
wsl_worker = wsl_path(Path(__file__).with_name('_capture_matching_frame_gdb.py'))
workers = []
clients = []

def wait_results(suffix, seconds=45):
    deadline = time.monotonic() + seconds
    paths = [root / (label + suffix) for label in ('client', 'host')]
    while not all(p.exists() for p in paths):
        errors = list(root.glob('*-error.json'))
        if errors:
            raise RuntimeError(errors[0].read_text())
        if any(p.poll() is not None for p in workers):
            raise RuntimeError('GDB worker exited before writing ' + suffix)
        if time.monotonic() > deadline:
            raise RuntimeError('Timeout waiting for ' + suffix)
        time.sleep(.05)
    return [json.loads(p.read_text()) for p in paths]

try:
    for qmp_port, label in ((args.client_qmp, 'client'), (args.host_qmp, 'host')):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            gdb_port = reservation.getsockname()[1]
        client = connect_qmp('127.0.0.1', qmp_port, 10)
        clients.append(client)
        client.command('stop')
        reply = client.command('human-monitor-command', {
            'command-line': 'gdbserver tcp:127.0.0.1:' + str(gdb_port)})
        if 'Failed' in reply or 'error' in reply.lower():
            raise RuntimeError(reply)
        script = root / (label + '.gdb')
        script.write_text('set pagination off\nset confirm off\nset architecture i386\n'
                          'maint set target-async off\nset remotetimeout 10\n'
                          'target remote 127.0.0.1:%d\n' % gdb_port +
                          'python\nexec(compile(open(' + repr(wsl_worker) +
                          ', encoding="utf-8").read(), ' + repr(wsl_worker) + ', "exec"))\nend\n'
                          'quit\n')
        command = ('HALO_ALIGN_POSES=' + ('1' if args.align_carousel else '0') +
                   ' HALO_BARRIER_DIR=' + shell_quote(wsl_root) + ' HALO_BARRIER_LABEL=' + label +
                   ' rtk timeout -s INT 60 gdb -batch -nx -x ' +
                   shell_quote(wsl_root + '/' + script.name))
        workers.append(subprocess.Popen(['rtk', 'wsl', '--exec', 'bash', '-lc', command],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT))
    initial = wait_results('-initial.json')
    if initial[0]['map'] != initial[1]['map'] or 'carousel' not in initial[0]['map']:
        raise RuntimeError('Both games must be running Carousel: ' + repr(initial))
    if initial[0]['variant'] != initial[1]['variant']:
        raise RuntimeError('Game variants differ')
    target = (max(p['tick'] for p in initial) + args.lead_ticks + 1) & ~1
    (root / 'target.json').write_text(json.dumps({'tick': target}))
    print('Target tick:', target, flush=True)
    results = wait_results('-boundary.json')
    left, right = [r['before']['local_players'] for r in results]
    if len(left) != 2 or len(right) != 2:
        raise RuntimeError('Exactly two local players are required on each instance')
    pose_match = all(a['position'] == b['position'] and a['yaw_pitch'] == b['yaw_pitch']
                     for a, b in zip(left, right))
    animation_match = all(bytes.fromhex(a['fp_header'])[0x16:0x28] ==
                          bytes.fromhex(b['fp_header'])[0x16:0x28] and
                          bytes.fromhex(a['fp_header'])[0xc:0xe] ==
                          bytes.fromhex(b['fp_header'])[0xc:0xe] for a, b in zip(left, right))
    rifle_match = all(p['active_weapon_tag'] == 'weapons\\assault rifle\\assault rifle'
                      for p in left + right)
    if args.align_carousel and not (pose_match and animation_match and rifle_match):
        raise RuntimeError('Aligned capture failed pose/animation/active-weapon verification')
    for label in ('client', 'host'):
        raw = root / (label + '-rendered.bin')
        frame = Image.frombytes('RGB', (640, 480), raw.read_bytes(), 'raw', 'BGRX', 2560)
        frame.save(root / (label + '.png'))
        frame.crop((0, 0, 640, 240)).save(root / (label + '-player1.png'))
        frame.crop((0, 240, 640, 480)).save(root / (label + '-player2.png'))
    manifest = {'target_tick': target, 'initial': initial, 'boundaries': results,
                'same_tick_and_present_boundary_verified': True,
                'matching_player_poses_verified': pose_match,
                'matching_first_person_animation_record_verified': animation_match,
                'matching_active_assault_rifles_verified': rifle_match,
                'source': 'rendered backbuffer before Present; D3D LockRect + debug memory readback',
                'test_frame': 'first rendered frame after pose alignment and weapon presentation reset',
                'presentation_reset_applied': args.align_carousel,
                'whole_game_state_equivalence_verified': False,
                'pixel_identity_required': False}
    (root / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    print(json.dumps({'directory': str(root), 'target_tick': target,
                      'ticks': [r['after']['tick'] for r in results],
                      'present_deltas': [r['after']['present_counter'] - r['before']['present_counter']
                                         for r in results],
                      'matching_player_poses_verified': pose_match,
                      'matching_animation_record_verified': animation_match,
                      'matching_active_assault_rifles_verified': rifle_match}), flush=True)
finally:
    (root / 'release.json').write_text('{}')
    # Wake a worker blocked in GDB continue if its peer failed or timed out.
    for client in clients:
        try:
            client.command('stop')
        except Exception as error:
            print('Cleanup stop failed:', error, flush=True)
    for worker in workers:
        try:
            output = worker.communicate(timeout=12)[0]
        except subprocess.TimeoutExpired:
            worker.terminate()
            output = worker.communicate(timeout=5)[0]
        print(output.decode(errors='replace')[-1400:], flush=True)
    for client in clients:
        try:
            client.command('human-monitor-command', {'command-line': 'gdbserver none'})
            client.command('cont')
        finally:
            client.close()
