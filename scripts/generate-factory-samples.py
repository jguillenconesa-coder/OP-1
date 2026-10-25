#!/usr/bin/env python3
"""Generate original, deterministic Pocket One factory WAVs (no external recordings)."""
from pathlib import Path
import math, random, struct, wave
root = Path(__file__).resolve().parents[1] / 'Assets' / 'Factory'
root.mkdir(parents=True, exist_ok=True)
rate = 32000
names = ['kick','snare','closed_hat','open_hat','tom','clap','crash','rim']
for kit, colour in [('Studio',0),('Circuit',1),('Dust',2)]:
 for slot, name in enumerate(names):
  rng = random.Random(6701 + colour*100 + slot)
  duration = [0.8,0.45,0.18,0.7,0.65,0.4,1.3,0.15][slot]
  phase=0.; previous=0.; data=[]
  for i in range(int(rate*duration)):
   t=i/rate; noise=rng.uniform(-1,1); high=(noise-previous)*.5;previous=noise
   tune=1+colour*.12
   if slot==0:
    phase+=2*math.pi*(45*tune+130*math.exp(-t*38))/rate
    x=math.sin(phase)*math.exp(-t*(7+colour))+noise*math.exp(-t*200)*.15
   elif slot==1:x=(noise*.7+math.sin(2*math.pi*185*t)*.3)*math.exp(-t*15)
   elif slot in (2,3):x=(high*.7+math.sin(2*math.pi*7130*t)*.1)*math.exp(-t*(45 if slot==2 else 8))
   elif slot==4:
    phase+=2*math.pi*(105*tune+85*math.exp(-t*28))/rate
    x=math.sin(phase)*math.exp(-t*9)
   elif slot==5:
    burst=sum(math.exp(-(t-start)*110) for start in (0,.014,.028,.042) if t>=start)
    x=noise*(burst*.5+math.exp(-t*20)*.25)
   elif slot==6:x=(high*.6+sum(math.sin(2*math.pi*f*t) for f in (3127,4583,6911))*.09)*math.exp(-t*4)
   else:x=(math.sin(2*math.pi*820*t)+math.sin(2*math.pi*1790*t)*.5)*math.exp(-t*65)
   if colour==1:x=math.tanh(x*2)*.7
   if colour==2:x=round(x*24)/24*(.8+.2*math.sin(2*math.pi*37*t))
   data.append(x*min(1,i/16)*min(1,(rate*duration-i)/160))
  peak=max(abs(x) for x in data) or 1
  path=root/f'{kit}_{name}.wav'
  with wave.open(str(path),'wb') as w:
   w.setparams((1,2,rate,0,'NONE','not compressed'))
   w.writeframes(struct.pack('<'+'h'*len(data),*(round(x/peak*26000) for x in data)))
for name, kind in [('VelvetKeys',0),('GlassMallet',1),('AirChoir',2),('LowPluck',3)]:
 data=[];rng=random.Random(8400+kind)
 for i in range(rate*2):
  t=i/rate;p=2*math.pi*261.625565*t
  if kind==0:x=(math.sin(p)+.25*math.sin(2*p)+.08*math.sin(3*p))*math.exp(-t*2)
  elif kind==1:x=sum(math.sin(p*f)*a*math.exp(-t*d) for f,a,d in [(1,1,3),(2.76,.5,5),(5.4,.3,8)])
  elif kind==2:x=(math.sin(p)+.4*math.sin(p*2.003)+.25*math.sin(p*3.001))*(1-math.exp(-t*12))*math.exp(-t*1.4)
  else:x=(math.sin(p)+.5*math.sin(2*p)+.2*math.sin(4*p))*math.exp(-t*5)
  data.append(x*min(1,i/64)*min(1,(rate*2-i)/320))
 peak=max(abs(x) for x in data)
 with wave.open(str(root/f'{name}.wav'),'wb') as w:
  w.setparams((1,2,rate,0,'NONE','not compressed'))
  w.writeframes(struct.pack('<'+'h'*len(data),*(round(x/peak*26000) for x in data)))
print('Generated 24 drum one-shots and 4 pitched samples at 32 kHz / PCM16.')
