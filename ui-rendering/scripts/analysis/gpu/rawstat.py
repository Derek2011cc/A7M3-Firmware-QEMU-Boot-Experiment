import sys, collections
import numpy as np
path = sys.argv[1]
d = open(path, 'rb').read()
a = np.frombuffer(d[:len(d)//2*2], dtype='<u2')
print('bytes', len(d), 'words', len(a), 'min', int(a.min()), 'max', int(a.max()),
      'unique', len(np.unique(a)))
print('top', collections.Counter(a.tolist()).most_common(10))
print('first 16', ' '.join('%04x' % x for x in a[:16]))
