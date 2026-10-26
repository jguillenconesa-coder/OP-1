#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Record named/numeric preset observations from an operator-supplied rev246 DB.
Read-only; no samples, database image, firmware code or executable input is copied.
This is an inventory, not a claim that every engine or complete preset is playable.
"""
import argparse
from collections import Counter
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3

EXPECTED='be4572298fe9dc83b106d5c4eaf1cc972280d97f028f3eaebc0c76dc68ac9cf0'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    source=a.database.resolve();before=hashlib.sha256(source.read_bytes()).hexdigest()
    if before!=EXPECTED:raise ValueError('This profile only supports the inventoried rev246 factory DB')
    records=[]
    with closing(sqlite3.connect(source.as_uri()+'?mode=ro',uri=True)) as c:
        c.execute('PRAGMA query_only=ON');c.execute('PRAGMA trusted_schema=OFF')
        for table in ['synth_presets','drum_presets']:
            for identifier,raw,folder in c.execute('SELECT id,patch,folder FROM '+table+' ORDER BY id'):
                patch=json.loads(raw)
                if not isinstance(patch,dict):raise ValueError('Unexpected preset record')
                row={'id':identifier,'table':table,'folder':folder}
                for key in ['name','type','synth_version','knobs','adsr','octave','fx_type','fx_active','fx_params','lfo_type','lfo_active','lfo_params','base_freq','start','end']:
                    if key in patch:row[key]=patch[key]
                records.append(row)
    if hashlib.sha256(source.read_bytes()).hexdigest()!=before:raise RuntimeError('Source database changed during inventory')
    result={'schema':1,'database_sha256':before,'scope':'Named/numeric stored fields only. Engine/FX/LFO availability and complete sound fidelity must be validated separately. No audio, original database or code included.','counts_by_table':dict(Counter(x['table'] for x in records)),'synth_counts_by_type':dict(sorted(Counter(x.get('type','unknown') for x in records if x['table']=='synth_presets').items())),'presets':records}
    if a.output.exists():raise ValueError('Choose a new output file; existing evidence is not overwritten')
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['counts_by_table']))
if __name__=='__main__':main()
