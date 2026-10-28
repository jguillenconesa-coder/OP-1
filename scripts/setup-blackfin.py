#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Build pinned GNU Blackfin tools locally, without firmware or root access.

Downloads public GNU sources (or uses explicitly supplied archives), verifies
SHA256, and retains separate stock/patched GDB sources and builds. Nothing is
installed system-wide. The output must be a new directory outside the checkout.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
SOURCES={
    'binutils':('binutils-2.44.tar.xz','https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz','ce2017e059d63e67ddb9240e9d4ec49c2893605035cd60e92ad53177f4377237'),
    'gdb':('gdb-13.1.tar.xz','https://ftp.gnu.org/gnu/gdb/gdb-13.1.tar.xz','115ad5c18d69a6be2ab15882d365dda2a2211c14f480b3502c6eba576e2e95a0'),
}
def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()
def archive(name,local,out):
    filename,url,expected=SOURCES[name]
    dest=out/'downloads'/filename
    if local:
        if sha(local)!=expected:raise ValueError('Archive SHA256 differs: '+str(local))
        shutil.copyfile(local,dest)
    else:
        print('Downloading verified-profile source: '+url,flush=True)
        # Standard urllib preserves inherited proxy/CA configuration. No tokens,
        # proxy overrides, TLS bypasses or application credentials are needed.
        size=0
        with urllib.request.urlopen(url,timeout=60) as response,dest.open('xb') as f:
            while block:=response.read(1024*1024):
                size+=len(block)
                if size>64*1024*1024:raise ValueError('GNU archive exceeds 64 MiB')
                f.write(block)
    if sha(dest)!=expected:raise ValueError('Downloaded archive SHA256 differs: '+filename)
    return dest
def unpack(path,dest):
    dest.mkdir()
    with tarfile.open(path,'r:xz') as source:source.extractall(dest,filter='data')
def check_models(out):
    probe=out/'model-check';probe.mkdir()
    testutils=out/'src-stock/gdb-13.1/sim/testsuite/bfin'
    subprocess.run([str(out/'bin/bfin-elf-as'),'-I',str(testutils),'-o',str(probe/'probe.o'),str(ROOT/'research/blackfin/parallel-dreg.s')],check=True,timeout=60)
    subprocess.run([str(out/'bin/bfin-elf-ld'),'-T',str(ROOT/'research/blackfin/flat.ld'),'-o',str(probe/'probe.elf'),str(probe/'probe.o')],check=True,timeout=60)
    expected=json.loads((ROOT/'docs/firmware/adsr246-validation.json').read_text())['microprobes']['models']
    result={}
    for name,binary in [('stock','run-stock'),('dreg_deferred','run-parallel')]:
        with (probe/(name+'.bin')).open('wb') as stdout,(probe/(name+'.log')).open('wb') as stderr:
            subprocess.run([str(out/'bin'/binary),'--model','bf524','--memory-size','64M','--watch-cycles-int','9000000',str(probe/'probe.elf')],stdout=stdout,stderr=stderr,check=True,timeout=60)
        values=struct.unpack('<10I',(probe/(name+'.bin')).read_bytes())
        if list(values)!=[int(x,16) for x in expected[name]]:raise RuntimeError('Unexpected parallel packet model: '+name)
        result[name]=[hex(x) for x in values]
    print('PASS: five own parallel-packet probes in both simulator models',flush=True)
    return result
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--jobs',type=int,default=2)
    p.add_argument('--binutils-archive',type=Path)
    p.add_argument('--gdb-archive',type=Path)
    args=p.parse_args();out=args.output.resolve()
    if not 1<=args.jobs<=16:raise ValueError('Use 1..16 jobs')
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW output directory outside the checkout')
    if not hasattr(tarfile,'data_filter'):raise RuntimeError('Use Python with tarfile.data_filter (Python 3.12+ or an updated 3.11)')
    missing=[tool for tool in ['cc','c++','make','ar','ranlib','patch'] if not shutil.which(tool)]
    if missing:raise RuntimeError('Missing build tools: '+', '.join(missing))
    if os.name!='posix':raise RuntimeError('Run in a POSIX build environment, e.g. the Linux Codex cloud environment')
    out.mkdir(parents=True)
    for directory in ['downloads','bin','logs']:(out/directory).mkdir()
    binutils=archive('binutils',args.binutils_archive,out);gdb=archive('gdb',args.gdb_archive,out)
    unpack(binutils,out/'src-binutils');unpack(gdb,out/'src-stock');unpack(gdb,out/'src-patched')
    def run(label,command,cwd):
        log=out/'logs'/(label+'.log')
        print(label+'; log: '+str(log),flush=True)
        with log.open('w') as f:
            result=subprocess.run([str(x) for x in command],cwd=cwd,stdout=f,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError(label+' failed; inspect '+str(log))
    patch=ROOT/'research/blackfin/gnu-13.1-dreg-writeback.patch'
    run('patch-gdb',['patch','--batch','--forward','-p1','-i',patch],out/'src-patched/gdb-13.1')
    configurations=[
        ('binutils',out/'src-binutils/binutils-2.44',['--target=bfin-elf','--disable-nls','--disable-werror','--disable-gdb','--enable-gas','--enable-ld','--disable-gprof','--disable-gold','--without-zstd'],['all-gas','all-ld','all-binutils']),
        ('stock',out/'src-stock/gdb-13.1',['--target=bfin-elf','--disable-gdb','--disable-gdbserver','--disable-binutils','--disable-gas','--disable-ld','--disable-gprof','--disable-nls','--enable-sim','--disable-werror','--without-zstd'],['all-sim']),
        ('patched',out/'src-patched/gdb-13.1',['--target=bfin-elf','--disable-gdb','--disable-gdbserver','--disable-binutils','--disable-gas','--disable-ld','--disable-gprof','--disable-nls','--enable-sim','--disable-werror','--without-zstd'],['all-sim']),
    ]
    for name,source,flags,targets in configurations:
        build=out/('build-'+name);build.mkdir()
        run('configure-'+name,[source/'configure',*flags],build)
        run('build-'+name,['make','-j'+str(args.jobs),'MAKEINFO=true',*targets],build)
    binaries={
        'bfin-elf-as':out/'build-binutils/gas/as-new',
        'bfin-elf-ld':out/'build-binutils/ld/ld-new',
        'bfin-elf-objdump':out/'build-binutils/binutils/objdump',
        'run-stock':out/'build-stock/sim/bfin/run',
        'run-parallel':out/'build-patched/sim/bfin/run',
    }
    for name,source in binaries.items():shutil.copy2(source,out/'bin'/name)
    variables={
        'POCKET_BFIN_AS':out/'bin/bfin-elf-as','POCKET_BFIN_LD':out/'bin/bfin-elf-ld',
        'POCKET_BFIN_OBJDUMP':out/'bin/bfin-elf-objdump','POCKET_BFIN_SIM':out/'bin/run-parallel',
        'POCKET_BFIN_STOCK':out/'bin/run-stock','POCKET_BFIN_TESTUTILS':out/'src-stock/gdb-13.1/sim/testsuite/bfin',
    }
    (out/'blackfin-env.sh').write_text('# Source this file in the current shell. No credentials or firmware.\n'+''.join('export '+name+'='+shlex.quote(str(value))+'\n' for name,value in variables.items()))
    report={'sources':{name:{'url':url,'sha256':digest} for name,(_,url,digest) in SOURCES.items()},
            'patch_sha256':sha(patch),'binaries':{name:sha(out/'bin'/name) for name in binaries},
            'parallel_model_probes':check_models(out),
            'scope':'GNU research tools only; no original firmware, boot, model/hardware equivalence or plugin integration validation.'}
    (out/'build-evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: Blackfin tools built. Source '+str(out/'blackfin-env.sh'),flush=True)
if __name__=='__main__':main()
