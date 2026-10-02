#!/usr/bin/env python3
"""Read-only census of every locally stored Git blob and historical path alias.

Fetch authorized public/fork/PR refs into a separate audit repository first.
Output is metadata only. Candidates require review; this tool does not choose
a purge set, rewrite history, fetch, delete, or publish binary contents.
"""
import argparse
import collections
import json
import re
import struct
import subprocess
from pathlib import Path

EXTENSIONS = {'.obj','.xbe','.xex','.pdb','.dbg','.sym','.bin','.raw','.ram',
              '.dump','.dmp','.core','.savestate','.halorec','.map','.iso',
              '.xiso','.dat','.zip','.7z','.rar','.gz','.xz','.exe','.dll'}


def census(repo, output):
    def git(*args, data=None):
        return subprocess.check_output(['git',*args],cwd=repo,input=data)
    meta=[line.split() for line in git('cat-file','--batch-all-objects',
          '--batch-check=%(objectname) %(objecttype) %(objectsize)').splitlines()]
    commits=[r[0] for r in meta if r[1]==b'commit']
    raw=git('log','--stdin','--no-walk=unsorted','--raw','--no-abbrev',
            '--no-renames','--diff-merges=separate','--root','--format=COMMIT %H',
            '-z',data=b'\n'.join(commits)+b'\n')
    paths=collections.defaultdict(set); changes=collections.defaultdict(set)
    tokens=raw.split(b'\0'); i=0; commit=None
    while i<len(tokens):
        token=tokens[i].strip(b'\n')
        if token.startswith(b'COMMIT '): commit=token.split()[1].decode()
        elif token.startswith(b':'):
            fields=token.split(); path=tokens[i+1].decode('utf-8','surrogateescape')
            for oid in fields[2:4]:
                if oid!=b'0'*40: paths[oid.decode()].add(path)
            changes[path].add((commit,fields[4].decode(),fields[2].decode(),fields[3].decode()))
            i+=1
        i+=1
    snapshots=[]
    ref_rows=git('for-each-ref','--format=%(refname)\t%(objectname)\t%(*objectname)\t%(objecttype)\t%(*objecttype)').decode().splitlines()
    for row in ref_rows:
        ref,oid,peeled,kind,peeled_kind=row.split('\t')
        if (peeled_kind or kind)!='tree': continue
        tree=peeled or oid
        for entry in git('ls-tree','-r','-z',tree).split(b'\0'):
            if not entry: continue
            header,name=entry.split(b'\t',1); fields=header.split()
            if fields[1]!=b'blob': continue
            path=name.decode('utf-8','surrogateescape'); blob=fields[2].decode()
            paths[blob].add(path)
            snapshots.append({'ref':ref,'tree':tree,'path':path,'blob':blob})
    batch=subprocess.Popen(['git','cat-file','--batch'],cwd=repo,
                           stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    inventory=[]; candidates=[]
    for oid,kind,size in meta:
        if kind!=b'blob': continue
        batch.stdin.write(oid+b'\n'); batch.stdin.flush()
        header=batch.stdout.readline().split()
        content=batch.stdout.read(int(header[2]))
        assert batch.stdout.read(1)==b'\n'
        names=sorted(paths.get(oid.decode(),())); reasons=[]
        if content.startswith(b'XBEH'): reasons.append('XBE signature')
        if content.startswith(b'Microsoft C/C++'): reasons.append('PDB signature')
        if content.startswith(b'MZ'): reasons.append('executable signature: review origin')
        if content.startswith((b'MDMP',b'HMRC')): reasons.append('dump/recording signature')
        if len(content)>=20:
            machine,sections,_,symbols,_,optional,_=struct.unpack_from('<HHIIIHH',content)
            if machine in (0x14c,0x8664) and 0<sections<200 and 20+optional+40*sections<=len(content) and symbols<=len(content):
                reasons.append('COFF signature: review origin')
        if any(Path(n).suffix.lower() in EXTENSIONS for n in names):
            reasons.append('artifact extension: review origin')
        if re.search(rb'[0-9a-fA-F]{4096}',content) and any(
                marker in content for marker in (b'"regions"',b'"memory"',b'bytes.fromhex',b'"hex"')):
            reasons.append('hex corpus: distinguish constructed from captured')
        if len(content)>=262144 and len(re.findall(rb'\b(?:FUN|LAB|sub|loc)_[0-9A-Fa-f]{6,16}\b',content))>=10000:
            reasons.append('large generated export')
        row={'oid':oid.decode(),'size':int(size),'paths':names,'review_reasons':reasons}
        inventory.append(row)
        if reasons: candidates.append(row)
    batch.stdin.close(); assert batch.wait()==0; batch.stdout.close()
    output.mkdir(parents=True,exist_ok=True)
    for name,value in (('inventory.json',inventory),('candidates.json',candidates),
                       ('changes.json',{p:sorted(v) for p,v in changes.items()}),
                       ('tree_snapshots.json',snapshots)):
        (output/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
    refs=git('for-each-ref','--format=%(refname)\t%(objectname)\t%(*objectname)')
    (output/'refs.tsv').write_bytes(refs)
    print(json.dumps({'stored_commits':len(commits),'stored_blobs':len(inventory),
                      'historical_paths':len(changes),'blob_bytes_scanned':sum(r['size'] for r in inventory),
                      'candidate_blobs':len(candidates)},indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output',type=Path,required=True,help='Private metadata output directory')
    args=parser.parse_args()
    census(args.repo,args.output)
