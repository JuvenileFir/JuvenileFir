#!/usr/bin/env python3

import numpy as np
import os
import stat
from multiprocessing import Pool

def alpha1(id):
    zipf1_data = np.random.zipf(1.000001, int(1e7))
    zipf1_data = (zipf1_data - 1) % np.iinfo(np.uint32).max
    zipf1_data.astype("int32").tofile(f"../data/zipf_1_32_1e9_{id}.data")

def alpha2(id):
  zipf2_data = np.random.zipf(2, int(1e7))
  zipf2_data = (zipf2_data - 1) % np.iinfo(np.uint32).max
  zipf2_data.astype("int32").tofile(f"../data/zipf_2_32_1e9_{id}.data")

def merge(alpha):
    files = [f'../data/zipf_{alpha}_32_1e9_{id}.data' for id in range(100)]
    out_data = b''
    for fn in files:
      with open(fn, 'rb') as fp:
        out_data += fp.read()
    with open(f'../data/zipf_{alpha}_32_1e9.data', 'wb') as fp:
      fp.write(out_data)

    for fn in files:
      os.remove(fn)

with Pool(40) as p:
    p.map(alpha1, [i for i in range(100)])
    p.map(alpha2, [i for i in range(100)])

merge(1)
merge(2)


def get_selc(alpha, method):
    a = np.fromfile(f'../data/zipf_{alpha}_32_1e9.data', dtype=np.uint32)
    sorted_a = np.sort(a)
    if method == 'byteSlice':
        indexes = [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
    elif method == 'zoneMaps':
        indexes = [0, 1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 99, 100]
    else:
        raise Exception("not a valid method")
    res = [ sorted_a[int((1e9 - 1) * i / 100)] + 1 for i in indexes ]
    res[0] = 0
    return res


def replace(method):
    with open(f'../{method}/plot_9.sh', 'w') as fout:
        with open(f'../{method}/plot_9.sh.in') as fin:
            for line in fin.readlines():
                if 'REPLACE_1' in line:
                    fout.write(line.replace('REPLACE_1', str(get_selc(1, method))[1:-1]))
                elif 'REPLACE_2' in line:
                    fout.write(line.replace('REPLACE_2', str(get_selc(2, method))[1:-1]))
                else:
                    fout.write(line)
    st = os.stat(f'../{method}/plot_9.sh')
    os.chmod(f'../{method}/plot_9.sh', st.st_mode | stat.S_IEXEC)

replace('byteSlice')
replace('zoneMaps')
