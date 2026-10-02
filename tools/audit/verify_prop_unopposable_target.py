#!/usr/bin/env python3
"""Check 2276 @0x2fc20 against an offset-based, target-derived model.

Consumes a local XBE; emits counts only. Does not consult external corpora or
publish target bytes. This checks the target/model and write ordering, not a
compiled candidate or live-game behavior. See the provenance ledger.
Requires unicorn. The model was derived from the named target's instructions.
"""
import argparse
import itertools
import hashlib
import json
import struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

EXPECTED_MD5 = 'c7869590a1c64ad034e49a5ee0c02465'
ACTOR, PROP, STACK, STOP = 0x1000000, 0x1001000, 0x2000000, 0x3000000
ACTOR_HANDLE, PROP_HANDLE = 0x10001, 0x20002


def check(path):
    data = Path(path).read_bytes()
    if hashlib.md5(data).hexdigest() != EXPECTED_MD5:
        raise ValueError('Input is not the recorded 2276 debug target')
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, table = struct.unpack_from('<II', data, 0x11c)
    sections = [struct.unpack_from('<IIII', data, table-base+i*56+4)
                for i in range(count)]
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    end = max(v+size for v,size,_,_ in sections)
    uc.mem_map(base, (end-base+4095) & ~4095)
    for va, size, raw, raw_size in sections:
        uc.mem_write(va, data[raw:raw+raw_size])
    uc.mem_map(ACTOR, 0x2000)
    uc.mem_map(STACK, 0x2000)
    uc.mem_map(STOP, 0x1000)
    calls, writes, visited = [], [], set()

    def datum_get(emulator, address, size, user):
        esp = emulator.reg_read(UC_X86_REG_ESP)
        ret, pool, handle = struct.unpack('<III', emulator.mem_read(esp, 12))
        calls.append(handle)
        if handle not in (ACTOR_HANDLE, PROP_HANDLE):
            raise AssertionError('Unexpected datum_get handle')
        emulator.reg_write(UC_X86_REG_EAX, ACTOR if handle==ACTOR_HANDLE else PROP)
        emulator.reg_write(UC_X86_REG_ESP, esp+4)
        emulator.reg_write(UC_X86_REG_EIP, ret)

    def record_write(emulator, access, address, size, value, user):
        if ACTOR <= address < ACTOR+0x400 or PROP <= address < PROP+0x138:
            writes.append((address, size, value & ((1 << (size*8))-1)))

    uc.hook_add(UC_HOOK_CODE, datum_get, begin=0x119320, end=0x119320)
    uc.hook_add(UC_HOOK_CODE, lambda u,a,s,d: visited.add(a), begin=0x2fc20, end=0x2fd0d)
    uc.hook_add(UC_HOOK_MEM_WRITE, record_write)
    cases = 0
    for state in (-1, 0, 1, 2, 3, 4):
        for bits in itertools.product((0, 1), repeat=13):
            enemy, blocked, word9c, match270, byte1ed, byte135, byte136, byte161, byte202, type15, previous, positive, match3ac = bits
            actor, prop = bytearray(0x400), bytearray(0x138)
            struct.pack_into('<I', actor, 0x270, PROP_HANDLE if match270 else 7)
            actor[0x1ed], actor[0x161], actor[0x202] = byte1ed, byte161, byte202
            struct.pack_into('<h', actor, 0x3a8, 1 if positive else -1)
            struct.pack_into('<I', actor, 0x3ac, PROP_HANDLE if match3ac else 7)
            struct.pack_into('<h', prop, 0x24, state)
            prop[0x60], prop[0x127] = enemy, blocked
            struct.pack_into('<h', prop, 0x9c, word9c)
            prop[0x135], prop[0x136], prop[0xa4] = byte135, byte136, previous
            struct.pack_into('<h', prop, 0x10, 15 if type15 else 0)
            for offset in (0xaa, 0xac, 0xae): struct.pack_into('<h', prop, offset, 123)
            result = int(2 <= state <= 3 and enemy and not blocked and (
                (word9c and (match270 or not byte1ed)) or
                ((byte135 or byte136) and not byte161 and not byte202) or type15))
            expected_actor, expected_prop = bytearray(actor), bytearray(prop)
            expected_writes = []
            if previous and not result:
                for offset in (0xaa, 0xae, 0xac):
                    struct.pack_into('<h', expected_prop, offset, 0)
                    expected_writes.append((PROP+offset, 2, 0))
            if 2 <= state <= 3 and not result and positive and match3ac:
                struct.pack_into('<h', expected_actor, 0x3a8, 0)
                struct.pack_into('<I', expected_actor, 0x3ac, 0xffffffff)
                expected_writes.extend(((ACTOR+0x3a8,2,0),(ACTOR+0x3ac,4,0xffffffff)))
            expected_prop[0xa4] = result
            expected_writes.append((PROP+0xa4,1,result))
            uc.mem_write(ACTOR, bytes(actor)); uc.mem_write(PROP, bytes(prop))
            esp = STACK+0x1000
            uc.mem_write(esp, struct.pack('<III', STOP, ACTOR_HANDLE, PROP_HANDLE))
            uc.reg_write(UC_X86_REG_ESP, esp)
            calls.clear(); writes.clear()
            uc.emu_start(0x2fc20, STOP, count=1000)
            assert uc.reg_read(UC_X86_REG_EIP)==STOP, (state,bits,'did not return')
            assert calls==[ACTOR_HANDLE,PROP_HANDLE], (state,bits,calls)
            assert uc.reg_read(UC_X86_REG_EAX)&255==result, (state,bits,'return')
            assert bytes(uc.mem_read(ACTOR,0x400))==expected_actor, (state,bits,'actor writes')
            assert bytes(uc.mem_read(PROP,0x138))==expected_prop, (state,bits,'prop writes')
            assert writes==expected_writes, (state,bits,writes,expected_writes)
            cases += 1
    return {'target_md5':EXPECTED_MD5,'address':'0x2fc20','cases':cases,
            'executed_instruction_addresses':len(visited),
            'checks':['AL return','datum_get handles/order','complete synthetic actor/prop state','write widths/order'],
            'limitation':'Target versus target-derived model; no compiled-candidate or live-runtime equivalence'}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe',help='Local pristine debug 2276 XBE')
    print(json.dumps(check(parser.parse_args().xbe),indent=2))
