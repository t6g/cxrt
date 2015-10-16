import numpy as np
import math  as math
import matplotlib.pyplot as plt
import h5py as h5py
from sphere_diffusion_linear_fixedconc import sdl_analytical


def readh5file(h5file, nx, nr):
  length = 5.0
  radius = 0.028
  cc  = []
  tt  = []
  t0  = 0.0

  f0  = h5py.File(h5file, 'r')

  j = 0 
  for i in f0.keys():
    if j == 0:
      cc = f0[i][:, 0]
      j = j + 1
    else:
      cc = np.column_stack((cc, f0[i][:, 0]))
    t = i.split()
    tt.append(t0 + float(t[2]))
  f0.close()

  res = {}
  res['t'] = np.asarray(tt)
  res['c'] = np.asarray(cc)
  res['x'] = np.linspace(0, length, nx)
  res['r'] = np.linspace(0, radius, nr) * 10.0

  return res


r1 = readh5file('sde_5_10.h5', 5, 10)
r2 = readh5file('sde_5_20.h5', 5, 20)
r3 = readh5file('sde_5_100.h5', 5, 100)
cin = 1.0 #1.9941e-07;
nr = 100

lx = 0.08
ly = 0.90

it1 = 0
it2 = 4 
it3 = 9 
it4 = 19
itt =  [r1['t'][it1], r1['t'][it2], r1['t'][it3], r1['t'][it4]] 
rrr = r1['r'][1:nr]/10.0

c4 = sdl_analytical(rrr, itt, 0.028, 1.0e-6, 1.0e-50, 100) 
rx = (0.028 - rrr) * 10.0

ax1 = plt.subplot(2, 2, 1)
i = it1
nx = 5
nr = 10
c1 = np.asarray(r1['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 20
c2 = np.asarray(r2['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 100
c3 = np.asarray(r3['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]

plt.plot(r1['r'], c1/cin, 'b-', r2['r'], c2/cin, 'r-', r3['r'], c3/cin, 'g-', rx, c4[:, 0], 'mo')
plt.ylabel('c/c$_0$')
plt.xlim([0, 0.28])
plt.ylim([0, 1.2])
plt.text(lx, ly, '(a) t = %d s' % (r1['t'][i]), transform=ax1.transAxes)
plt.setp(ax1.get_xticklabels(), visible=False)

ax2 = plt.subplot(2, 2, 2)
i = it2
nr = 10
c1 = np.asarray(r1['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 20
c2 = np.asarray(r2['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 100
c3 = np.asarray(r3['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
plt.plot(r1['r'], c1/cin, 'b-', r2['r'], c2/cin, 'r-', r3['r'], c3/cin, 'g-', rx, c4[:, 1], 'mo')
plt.xlim([0, 0.28])
plt.ylim([0, 1.2])
plt.setp(ax2.get_yticklabels(), visible=False)
plt.text(lx, ly, '(b) t = %d s' % (r1['t'][i]), transform=ax2.transAxes)
plt.setp(ax2.get_xticklabels(), visible=False)

ax3 = plt.subplot(2, 2, 3)
i = it3
nr = 10
c1 = np.asarray(r1['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 20
c2 = np.asarray(r2['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 100
c3 = np.asarray(r3['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
plt.plot(r1['r'], c1/cin, 'b-', r2['r'], c2/cin, 'r-', r3['r'], c3/cin, 'g-', rx, c4[:, 2], 'mo')
plt.xlim([0, 0.28])
plt.ylim([0, 1.2])
plt.text(lx, ly, '(c) t = %d s' % (r1['t'][i]), transform=ax3.transAxes)
plt.xlabel('r (mm)')
plt.ylabel('c/c$_0$')

ax4 = plt.subplot(2, 2, 4)
i = it4
nr = 10
c1 = np.asarray(r1['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 20
c2 = np.asarray(r2['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
nr = 100
c3 = np.asarray(r3['c'][:, i].reshape(nr+1, nx))[1:nr+1, 0]
plt.plot(r1['r'], c1/cin, 'b-', r2['r'], c2/cin, 'r-', r3['r'], c3/cin, 'g-', rx, c4[:, 3], 'mo')
plt.xlim([0, 0.28])
plt.ylim([0, 1.2])
plt.text(lx, ly, '(d) t = %d s' % (r1['t'][i]), transform=ax4.transAxes)
plt.xlabel('r (mm)')
plt.setp(ax4.get_yticklabels(), visible=False)
lgd = plt.legend(('nr = 10', 'nr = 20', 'nr = 100', 'Analytical'),loc=3)
lgd.draw_frame(False)
#txt = lgd.get_texts()
#plt.setp(txt, fontsize='small') 

fig = plt.gcf()
fig.subplots_adjust(left=0.08, right=0.95, wspace=0.05, hspace=0.08)
fig.set_size_inches(8, 6)
plt.savefig('prof.pdf')
plt.show()
