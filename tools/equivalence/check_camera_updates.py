#!/usr/bin/env python3
"""Run focused 2276-XBE versus freshly compiled VC71 camera cases.

Uses real original math/constructors and deterministic hooks for game-state
accessors. This verifies command/state behavior, not in-game rendering.
Compile observer.c and director.c with vc71_verify.py before running.
"""
import math
import struct
from pathlib import Path

from coff_loader import extract_function
from xbe_image import assert_pristine, load_xbe, map_image
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP

ROOT = Path(__file__).resolve().parents[2]
STATE, CONTROL, RESULT, UNIT, PLAYER, FACING, TABLE = [0x1000000 + x for x in (0, 0x100, 0x200, 0x400, 0x900, 0xa00, 0xb00)]
CODE, RDATA, STACK, STOP = 0x2000000, 0x2200000, 0x3000000, 0x300f000
ADDRS = {
    'following_camera_update': 0x89cd0, 'dead_camera_update': 0x85c80,
    'flying_camera_update': 0x893a0, 'orbiting_camera_update': 0x8cf30,
    'FUN_00086a50': 0x86a50, 'FUN_00086670': 0x86670,
    'display_assert': 0x8d9f0, 'system_exit': 0x8e2f0, 'csprintf': 0x8d9d0,
    'console_printf': 0xff4d0, 'player_control_get_unit_camera_info': 0xb6740,
    'object_get_and_verify_type': 0x13d680, 'object_try_and_get_and_verify_type': 0x13d640,
    'player_control_get_facing_angles': 0xb7e30, 'camera_track_splut': 0x89ab0,
    'object_get_root_location': 0x140070, 'angles_to_vector': 0x10cc40,
    'observer_up_from_forward': 0x8aa80, 'valid_real_normal3d_perpendicular': 0x84a70,
    'real_vector3d_valid': 0x84a10, 'rotate_vector3d_by_sincos': 0x10b6e0,
    'game_time_get_paused': 0xb5c30, 'FUN_00085a40': 0x85a40,
    'FUN_00085ab0': 0x85ab0, 'datum_get': 0x119320, 'game_engine_running': 0xa8e30,
    'following_camera_new': 0x89850, 'first_person_camera_new': 0x88c40,
    'orbiting_camera_new': 0x8cf10, 'FUN_00089350': 0x89350,
    'FUN_000865a0': 0x865a0, 'director_set_local_player_context': 0x86220, '_CIpow': 0x1d9e70, 'game_in_editor': 0x977f0,
}


def u32(uc, p):
    return struct.unpack('<I', uc.mem_read(p, 4))[0]


def put(uc, p, fmt, *values):
    uc.mem_write(p, struct.pack('<' + fmt, *values))


def run(name, case, candidate):
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    raw, sections = load_xbe()
    map_image(uc, raw, sections)
    for base in (STATE, CODE, RDATA, STACK):
        uc.mem_map(base, 0x10000)
    uc.mem_write(STATE, bytes(case['state']))
    uc.mem_write(CONTROL, bytes(case.get('controls', bytes(0x24))))
    put(uc, UNIT + 0x50, '3f', 11, 12, 13)
    put(uc, UNIT + 0x1b8, 'I', case.get('crouched', 0))
    put(uc, PLAYER + 0x34, 'i', case.get('next_unit', 0x1234))
    put(uc, FACING, '2f', *case.get('facing', (0.4, 0.2)))
    put(uc, TABLE, '4h', 4, 2, 0, 1)
    put(uc, 0x3352a8, 'f', case.get('dt', 1/30))
    put(uc, 0x325716, 'h', case.get('reset_ticks', 0))
    if 'director' in case:
        uc.mem_write(0x3352b0, bytes(case['director']))
    if 'defs' in case:
        uc.mem_write(0x2ee5f8, bytes(case['defs']))
    put(uc, 0x5aa6d4, 'I', PLAYER)
    calls = []
    hooks = {ADDRS[n]: n for n in (
        'display_assert', 'system_exit', 'csprintf', 'console_printf',
        'player_control_get_unit_camera_info', 'object_get_and_verify_type',
        'object_try_and_get_and_verify_type', 'player_control_get_facing_angles',
        'camera_track_splut', 'object_get_root_location', 'game_time_get_paused',
        'FUN_00085a40', 'FUN_00085ab0', 'datum_get', 'game_engine_running',
        'FUN_000865a0', 'game_in_editor')}
    # Fresh, default (non --regcall-elide) VC71 object; external calls use
    # normal C arguments. Original mixed-register calls are modeled below.
    if candidate:
        tu = 'director' if name.startswith('FUN_') or name == 'dead_camera_update' else 'observer'
        f = extract_function(str(ROOT / 'build' / 'vc71' / (tu + '.obj')), name)
        code = bytearray(f.code)
        symbols = {}
        cursor = RDATA
        for symbol, data in f.rdata_map.items():
            symbols[symbol] = cursor
            uc.mem_write(cursor, data)
            cursor += (len(data) + 15) & ~15
        for symbol, offset in f.text_symbol_offsets.items():
            symbols[symbol] = CODE + offset - f.section_offset
        symbols['_player_data'] = 0x5aa6d4
        for r in f.relocs:
            symbol = r.symbol_name
            bare = symbol[1:] if symbol.startswith('_') else symbol
            target = symbols.get(symbol, ADDRS.get(bare))
            if target is None:
                raise AssertionError('unhandled relocation ' + symbol)
            addend = struct.unpack_from('<I', code, r.virtual_address)[0]
            value = target + addend
            if r.reloc_type == 20:
                value -= CODE + r.virtual_address + 4
            elif r.reloc_type != 6:
                raise AssertionError('unsupported relocation')
            struct.pack_into('<I', code, r.virtual_address, value & 0xffffffff)
        uc.mem_write(CODE, bytes(code))
        start = CODE
    else:
        start = ADDRS[name]

    def hook(uc, address, size, _):
        callee = hooks.get(address)
        if not callee:
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [u32(uc, esp + 4 + i*4) for i in range(4)]
        result = 0
        if callee in ('display_assert', 'system_exit', 'csprintf'):
            raise AssertionError('unexpected assertion in ' + name + ': ' + callee)
        if callee == 'player_control_get_unit_camera_info':
            put(uc, args[1], 'ihHI3f', case.get('unit', 0x1234), case.get('seat', 1), 0,
                case.get('camera', UNIT), 1, 2, 3)
            calls.append((callee, args[0]))
        elif callee in ('object_get_and_verify_type', 'object_try_and_get_and_verify_type'):
            result = UNIT if case.get('object_exists', True) else 0
            calls.append((callee, args[0], args[1]))
        elif callee == 'player_control_get_facing_angles':
            result = FACING
            calls.append((callee, args[0]))
        elif callee == 'camera_track_splut':
            camera, pitch, out = args[:3] if candidate else (uc.reg_read(UC_X86_REG_ECX), args[0], args[1])
            put(uc, out, '3f', -2, 0.25, 0.75)
            calls.append((callee, camera, pitch))
        elif callee == 'object_get_root_location':
            put(uc, args[1], '3f', 0.1, -0.2, 0.3)
            calls.append((callee, args[0], args[2]))
        elif callee == 'game_time_get_paused':
            result = case.get('paused', 0)
        elif callee == 'FUN_00085a40':
            player = args[0] if candidate else uc.reg_read(UC_X86_REG_EDI)
            calls.append((callee, player))
            result = case.get('allies', 1)
        elif callee == 'FUN_00085ab0':
            values = args[:3] if candidate else [uc.reg_read(UC_X86_REG_EBX) & 255, args[0], args[1]]
            values[0] &= 255  # Only BL / the low byte of the C char argument is defined.
            calls.append((callee, *values))
            result = case.get('next_player', 0x2345) & 0xffffffff
        elif callee == 'datum_get':
            result = PLAYER
            calls.append((callee, args[0], args[1]))
        elif callee == 'game_engine_running':
            result = case.get('multiplayer', 0)
        elif callee == 'game_in_editor':
            result = case.get('editor', 0)
        elif callee == 'FUN_000865a0':
            player, proc, reset = args[:3] if candidate else [uc.reg_read(UC_X86_REG_ESI) & 0xffff, args[0], args[1]]
            calls.append((callee, player, proc, reset))
            base = 0x3352b0 + player * 0xf8
            put(uc, base + 8, 'I', proc)
            put(uc, base + 0xc4, 'f', 1)
            put(uc, base + 0xc0, 'B', 0)
            if reset:
                put(uc, base + 4, 'f', 1)
        elif callee == 'console_printf':
            calls.append((callee, args[0], args[2]))
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, u32(uc, esp))

    # Deliberately import this here so the explicit original-ABI hook is easy
    # to audit alongside the other register inputs.
    from unicorn.x86_const import UC_X86_REG_ECX
    uc.hook_add(UC_HOOK_CODE, hook)
    if name == 'FUN_00086a50':
        args = [0, TABLE, case.get('count', 3)] if candidate else [case.get('count', 3)]
        if not candidate:
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EBX, TABLE)
    elif name == 'FUN_00086670':
        speed = struct.unpack('<I', struct.pack('<f', case.get('speed', 0)))[0]
        args = [0, case.get('mode_flags', 0), speed] if candidate else [case.get('mode_flags', 0), speed]
        if not candidate:
            uc.reg_write(UC_X86_REG_EAX, 0)
    else:
        args = [STATE, CONTROL, RESULT]
    esp = STACK + 0x8000
    put(uc, esp, 'I' * (len(args)+1), STOP, *args)
    uc.reg_write(UC_X86_REG_ESP, esp)
    uc.emu_start(start, STOP, count=100000)
    if uc.reg_read(UC_X86_REG_EIP) != STOP:
        raise AssertionError('instruction limit')
    return bytes(uc.mem_read(STATE, 0x80)), bytes(uc.mem_read(RESULT, 0x68)), bytes(uc.mem_read(0x3352b0, 0xf8)), calls


def same(a, b, float_offsets):
    if len(a) != len(b):
        return False
    mask = bytearray(len(a))
    for offset in float_offsets:
        for i in range(offset, offset+4):
            mask[i] = 1
        x, y = struct.unpack_from('<f', a, offset)[0], struct.unpack_from('<f', b, offset)[0]
        if not math.isclose(x, y, rel_tol=2e-6, abs_tol=2e-7):
            return False
    return all(x == y or mask[i] for i, (x, y) in enumerate(zip(a, b)))


def cases():
    following = bytearray(0x1c)
    struct.pack_into('<ih', following, 8, 0x1234, 1)
    struct.pack_into('<f', following, 0x18, 1)
    controls = bytearray(0x24)
    struct.pack_into('<f', controls, 4, 1/30)
    for label, changes in [
        ('first', {}), ('same_seat', {'initialized': 1}),
        ('enter_vehicle', {'initialized': 1, 'unit': 0x5678}),
        ('change_seat', {'initialized': 1, 'seat': 2}),
        ('crouch', {'crouched': 1}), ('look_active', {'active': 1}),
        ('look_release', {'offset': (0.2, -0.1)}),
        ('pitch_high', {'active': 1, 'facing': (0.4, 1.5)}),
        ('pitch_low', {'active': 1, 'facing': (0.4, -1.5), 'look': (0.1, -0.5)}),
        ('no_unit', {'unit': -1, 'seat': -1, 'camera': 0}),
    ]:
        state = bytearray(following); state[0] = changes.pop('initialized', 0)
        struct.pack_into('<2f', state, 0x10, *changes.pop('offset', (0, 0)))
        action = bytearray(controls); action[2] = changes.pop('active', 0)
        struct.pack_into('<2f', action, 8, *changes.pop('look', (0.1, 0.5)))
        yield 'following_camera_update', label, dict(state=state, controls=action, **changes)
    # Dead state layout is 3f + 2f + distance/fov/timer + 3i + switch timer.
    dead = struct.pack('<8f3if', 1, 2, 3, 0.4, -0.5, 4, 1.22173047, 3, 0x10001, 0x10001, 0x1234, 3)
    for label, changes in [('corpse', {}), ('missing_corpse', {'object_exists': False}),
        ('no_corpse', {'unit': -1}), ('timer_expired', {'timer': -1}),
        ('switch_campaign', {'switch_timer': 0}), ('switch_multiplayer', {'switch_timer': 0, 'multiplayer': 1}),
        ('paused', {'switch_timer': 0, 'paused': 1}), ('no_player_fallback', {'switch_timer': 0, 'next_player': -1}),
        ('same_unit', {'switch_timer': 0, 'next_unit': 0x1234}), ('no_next_unit', {'switch_timer': 0, 'next_unit': -1})]:
        state = bytearray(dead)
        for key, offset, fmt in [('unit', 0x28, 'i'), ('timer', 0x1c, 'f'), ('switch_timer', 0x2c, 'f')]:
            if key in changes:
                struct.pack_into('<'+fmt, state, offset, changes.pop(key))
        yield 'dead_camera_update', label, dict(state=state, controls=controls, **changes)
    for label, changes in [('inactive', {}), ('active', {'active': 1}), ('pitch_clamp', {'active': 1, 'pitch': 2}), ('reset', {'active': 1, 'reset_ticks': 2})]:
        state = struct.pack('<7f', 1, 2, 3, 0.4, changes.pop('pitch', 0.2), 0.3, 1.22173047)
        action = bytearray(controls); action[2] = changes.pop('active', 0)
        struct.pack_into('<6f', action, 8, 0.1, 0.2, 0.1, 1, -0.5, 0.25)
        yield 'flying_camera_update', label, dict(state=state, controls=action, **changes)
    for label, active, unit, wheel in [('inactive', 0, 0x1234, 0), ('active', 1, 0x1234, 3), ('zoom_limit', 1, 0x1234, 90), ('no_unit', 0, -1, 0)]:
        state = struct.pack('<3f', 0.4, 0.2, 4)
        action = bytearray(controls); action[2] = active
        struct.pack_into('<2f', action, 8, 0.1, 2)
        struct.pack_into('<f', action, 0x20, wheel)
        yield 'orbiting_camera_update', label, dict(state=state, controls=action, unit=unit)
    for count in (3, 4):
        for mode in range(count):
            director = bytearray(0xf8)
            struct.pack_into('<h', director, 0, mode)
            struct.pack_into('<3f', director, 0x5c, 1, 2, 3)
            struct.pack_into('<f', director, 0x74, 4)
            struct.pack_into('<3f', director, 0x7c, 1, 0, 0)
            yield 'FUN_00086a50', '%d_modes_from_%d' % (count, mode), dict(state=bytes(0x80), director=director, count=count)
    for label, changes in [('idle', {}), ('forward', {'mode_flags': 1}), ('reverse', {'mode_flags': 2}), ('all_keys', {'mode_flags': 255}), ('editor_stop', {'editor': 1}), ('speed_up', {'speed': 1}), ('speed_down', {'speed': -1}), ('low_clamp', {'scale': 0.001}), ('high_clamp', {'scale': 100}), ('friction_clamp', {'dt': 1})]:
        director = bytearray(0xf8)
        struct.pack_into('<f', director, 0xc4, changes.pop('scale', 2))
        for i in range(4):
            struct.pack_into('<3f', director, 0xc8+i*12, 0.1, -0.3, 0)
        yield 'FUN_00086670', label, dict(state=bytes(0x80), director=director, **changes)


def main():
    assert_pristine()
    total = 0
    for name, label, case in cases():
        oracle = run(name, case, False)
        candidate = run(name, case, True)
        state_floats = {
            'following_camera_update': [0x10, 0x14, 0x18],
            'dead_camera_update': list(range(0, 0x20, 4)) + [0x2c],
            'flying_camera_update': list(range(0, 0x1c, 4)),
            'orbiting_camera_update': [0, 4, 8],
        }.get(name, [])
        command_floats = list(range(4, 0x4c, 4)) + list(range(0x54, 0x68, 4))
        director_floats = ([0xc4] + list(range(0xc8, 0xf8, 4))) if name == 'FUN_00086670' else []
        if not (same(oracle[0], candidate[0], state_floats) and
                same(oracle[1], candidate[1], command_floats) and
                same(oracle[2], candidate[2], director_floats) and oracle[3] == candidate[3]):
            for i, (a, b) in enumerate(zip(oracle, candidate)):
                if a != b:
                    print('DIFF', i, a.hex() if isinstance(a, bytes) else a, b.hex() if isinstance(b, bytes) else b)
            raise AssertionError(name + '/' + label)
        print('PASS', name, label)
        total += 1
    print('%d camera cases passed (target XBE vs VC71; deterministic engine accessors)' % total)


if __name__ == '__main__':
    main()
