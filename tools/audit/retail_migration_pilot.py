#!/usr/bin/env python3
"""Compare local 2276 images for a retail migration feasibility pilot.

Emits address/evidence metadata only. Candidate maps never modify the canonical
target, knowledge base or implementations. Requires Capstone.
"""
import argparse
import collections
import hashlib
import json
import re
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM, CS_OP_REG

def image(path):
    data=Path(path).read_bytes()
    assert data[:4]==b'XBEH'
    base=struct.unpack_from('<I',data,0x104)[0]
    count,ptr=struct.unpack_from('<II',data,0x11c)
    secs={}
    for i in range(count):
        fields=struct.unpack_from('<9I',data,ptr-base+i*56)
        off=fields[5]-base
        name=data[off:data.index(b'\0',off)].decode('ascii')
        secs[name]=(fields[1],data[fields[3]:fields[3]+fields[4]],fields[2])
    return data,secs

def extract(secs,va,size):
    for start,data,_ in secs.values():
        if start<=va<start+len(data):return data[va-start:va-start+size]
    return b''

def location(secs,address):
    for name,(start,data,size) in secs.items():
        if start<=address<start+size:return name
    return None

def decode(secs,start,size,require_complete=False):
    raw=extract(secs,start,size)
    cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True
    ins=list(cs.disasm(raw,start))
    if require_complete and (len(raw)!=size or sum(x.size for x in ins)!=size):return []
    while ins and ins[-1].mnemonic in ('nop','int3'):ins.pop()
    return ins

def normalize(secs,ins,start,size):
    cs=Cs(CS_ARCH_X86,CS_MODE_32)
    tokens=[];calls=[];globals=[];args=set();returns=[];edges=[]
    for inst in ins:
        operands=[]
        for op in inst.operands:
            if op.type==CS_OP_REG:operands.append(('r',cs.reg_name(op.reg),op.size))
            elif op.type==CS_OP_IMM:
                if inst.mnemonic.startswith(('j','loop')):
                    if start<=op.imm<start+size:
                        value=('internal',op.imm-start);edges.append((inst.address-start,op.imm-start))
                    else:value='external_branch';calls.append((inst.address-start,op.imm,'tail'))
                elif inst.mnemonic=='call':
                    value='external_call';calls.append((inst.address-start,op.imm,'call'))
                elif inst.mnemonic in ('mov','push') and location(secs,op.imm) in ('.data','.rdata'):
                    value='data_address';globals.append((inst.address-start,op.imm,op.size,'address'))
                else:value=op.imm  # Never mask arithmetic/comparison constants.
                operands.append(('i',value,op.size))
            elif op.type==CS_OP_MEM:
                mem=op.mem;disp=mem.disp
                if not mem.base and not mem.index and location(secs,disp):
                    globals.append((inst.address-start,disp,op.size,'memory'))
                    disp='absolute_address'
                if cs.reg_name(mem.base)=='ebp' and mem.disp>=8:
                    args.add((mem.disp,op.size))
                operands.append(('m',cs.reg_name(mem.segment),cs.reg_name(mem.base),
                                 cs.reg_name(mem.index),mem.scale,disp,op.size))
        tokens.append((inst.mnemonic,tuple(operands)))
        if inst.mnemonic.startswith('ret'):
            returns.append((inst.address-start,inst.mnemonic,
                            inst.operands[0].imm if inst.operands else 0))
    return tokens,calls,globals,sorted(args),returns,edges

def category(name):
    if name.startswith(('actor_','action_','ai_','prop_')):return 'AI'
    if name.startswith(('object_','objects_','unit_','weapon_','glow_','placement_')):return 'objects'
    if name.startswith(('game_','player_','scenario_','update_')):return 'game'
    if name.startswith(('rasterizer_','render_','camera_','texture_')):return 'render'
    if name.startswith(('network_','transport_','hud_','ui_','widget_','scripted_','column_')):return 'network/UI'
    if any(x in name.lower() for x in ('vector','matrix','collision','physics','plane','square','ftol')):return 'math/physics'
    return 'platform/library'

def run(args):
    debug,dsecs=image(args.debug);retail,rsecs=image(args.retail)
    if hashlib.md5(debug).hexdigest()!='c7869590a1c64ad034e49a5ee0c02465':
        raise ValueError('Debug input is not the audited pristine 2276 target')
    if hashlib.sha256(retail).hexdigest()!='ed3a8e962351ad6c4b3b620768fb6a0bda658963390252439ea036d5ede3a3ac':
        raise ValueError('Retail input differs from the audited NTSC retail image')
    bounds=json.loads(Path(args.bounds).read_text(encoding='utf-8'))
    anchors=json.loads(Path(args.anchors).read_text(encoding='utf-8'))
    byref={int(x['reference'],16):x for x in anchors}
    # Deliberately include high-risk/unanchored controls as well as strong matches.
    priorities=[0x1beb0,0x22dc0,0x2fc20,0x2fd10,0x30f50,0x31a90,
                0x441c0,0x64170,0x647c0,0x133750,0x13c430,0x166210]
    selected=list(priorities)
    groups=collections.defaultdict(list)
    for row in anchors:
        name=row['name'] or ''
        if name.startswith('FUN_') or int(row['reference'],16)>=0x1cd000:continue
        groups[category(name)].append(row)
    for group in groups.values():
        group.sort(key=lambda r:(r['unique_window_hits']/max(1,r['window_count']),r['unique_window_hits']),reverse=True)
    while len(selected)<50:
        progressed=False
        for cat in ['AI','objects','game','render','network/UI','math/physics','platform/library']:
            while groups[cat] and int(groups[cat][0]['reference'],16) in selected:groups[cat].pop(0)
            if groups[cat] and len(selected)<50:
                selected.append(int(groups[cat].pop(0)['reference'],16));progressed=True
        if not progressed:break
    print('Selected',len(selected),'functions',flush=True)
    # Whole-body comparison for every anchor gives a conservative callee map.
    records=[];strong={}
    for lo,row in byref.items():
        bound=bounds.get(hex(lo));
        if not bound:continue
        size=int(bound['end'],16)-lo;candidate=int(row['candidate_entry_estimate'],16)
        di=decode(dsecs,lo,size,require_complete=True);ri=decode(rsecs,candidate,size,require_complete=True)
        dn=normalize(dsecs,di,lo,size);rn=normalize(rsecs,ri,candidate,size)
        match=bool(di and ri and dn[0]==rn[0] and sum(x.size for x in di)==sum(x.size for x in ri))
        if match:strong[lo]=candidate
        records.append((lo,candidate,size,di,ri,dn,rn,match))
    recs={r[0]:r for r in records}
    # Candidate direct-call and tail-branch references to retail entry estimates.
    inbound=collections.defaultdict(list);rva,rtext,_=rsecs['.text']
    cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.skipdata=True
    scan_count=0
    for ins in cs.disasm(rtext,rva):
        if ins.id==0:continue
        scan_count+=1
        if ins.mnemonic in ('call','jmp') and ins.op_str.startswith('0x'):
            inbound[int(ins.op_str,16)].append((ins.address,ins.mnemonic))
    rows=[]
    for lo in selected:
        bound=bounds[hex(lo)];label=bound.get('name') or hex(lo)
        if lo not in recs:
            rows.append({'reference':hex(lo),'label':label,'category':category(label),
                         'status':'NO_UNIQUE_WINDOW_ANCHOR','canonical_change':False})
            continue
        lo,entry,size,di,ri,dn,rn,match=recs[lo]
        refs=inbound.get(entry,())
        witnesses=[]
        for site,kind in refs[:4]:
            follow=decode(rsecs,site,20)
            cleanup=None
            if len(follow)>1 and follow[1].mnemonic=='add' and follow[1].op_str.startswith('esp,'):
                cleanup=follow[1].operands[1].imm
            witnesses.append({'site':hex(site),'kind':kind,'immediate_stack_cleanup':cleanup})
        global_checks=[]
        if match:
            for left,right in zip(dn[2],rn[2]):
                _,a,width,kind=left;_,b,_,_=right
                da=extract(dsecs,a,width);ra=extract(rsecs,b,width)
                global_checks.append({'debug':hex(a),'retail':hex(b),'width':width,'use':kind,
                                      'debug_section':location(dsecs,a),'retail_section':location(rsecs,b),
                                      'file_backed_values_equal':bool(da and ra and da==ra),
                                      'note':'Address operands require pointee/role verification' if kind=='address' else 'Load-time bytes only; runtime state unverified'})
        call_checks=[]
        if match:
            for left,right in zip(dn[1],rn[1]):
                offset,a,kind=left;_,b,_=right
                call_checks.append({'offset':offset,'kind':kind,'debug_target':hex(a),'retail_target':hex(b),
                                    'agrees_with_other_full_body_candidate':strong.get(a)==b})
        rows.append({'reference':hex(lo),'label':label,'category':category(label),'retail_estimate':hex(entry),
                     'reference_bytes_without_padding':sum(x.size for x in di),'reference_instruction_count':len(di),
                     'retail_instruction_count_in_reference_span':len(ri),'full_normalized_body_match':match,
                     'internal_cfg_edges_equal':match and dn[5]==rn[5],
                     'frame_arg_reads_equal':match and dn[3]==rn[3],
                     'frame_arg_reads_debug':dn[3],'frame_arg_reads_retail':rn[3],
                     'return_stack_cleanup_debug':[v[2] for v in dn[4]],
                     'return_stack_cleanup_retail':[v[2] for v in rn[4]],
                     'retail_incoming_direct_references':len(refs),'boundary_witnesses':witnesses,
                     'global_checks':global_checks,'call_checks':call_checks,
                     'status':'BODY_MATCH_WITH_SCANNED_ENTRY_REFERENCE' if match and refs else 'BODY_MATCH_BOUNDARY_UNPROVEN' if match else 'PARTIAL_ANCHOR_REQUIRES_RECONSTRUCTION',
                     'canonical_change':False})
    summary={'debug_sha256':hashlib.sha256(debug).hexdigest(),'retail_sha256':hashlib.sha256(retail).hexdigest(),
             'bounds_sha256':hashlib.sha256(Path(args.bounds).read_bytes()).hexdigest(),
             'anchors_sha256':hashlib.sha256(Path(args.anchors).read_bytes()).hexdigest(),
             'mapping_namespace':'migration_hypothesis_only',
             'pilot_functions':len(rows),'categories':dict(collections.Counter(r['category'] for r in rows)),
             'status_counts':dict(collections.Counter(r['status'] for r in rows)),
             'full_body_candidates_across_all_anchors':len(strong),
             'retail_instructions_scanned_for_entry_references':scan_count,
             'limitations':['Selection favors anchors but deliberately includes 12 substantive controls; not an unbiased migration completion estimate',
                            'Full body matches normalize external calls and mapped absolute addresses; pointee roles/callee identity still require independent proof',
                            'Linear text scan references are candidate entry witnesses; embedded text data and complete function boundaries require caller review',
                            'Argument access and cleanup comparisons are partial ABI evidence; register arguments and floating return consumers need caller review',
                            'No source implementation, KB address, target, runtime activation, or canonical binary changed'],
             'functions':rows}
    Path(args.output).write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('functions','limitations')},indent=2),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description='Target-only retail migration pilot. Emits metadata only, never binary bytes.')
    p.add_argument('--debug',required=True);p.add_argument('--retail',required=True)
    p.add_argument('--bounds',required=True);p.add_argument('--anchors',required=True)
    p.add_argument('--output',required=True)
    run(p.parse_args())
