#!/usr/bin/env python3

import sys

def extract(arr):
    return [ s.split(',')[1] for s in arr ]

if len(sys.argv) <= 1:
    print('Usage: {} <filename>'.format(sys.argv[0]))
    exit(1)

fans = open('../data/ans.csv').read().strip().split('\n')
fsubm = open(sys.argv[1]).read().strip().split('\n')

ans = extract(fans)
subm = extract(fsubm)

if len(ans) != len(subm):
    print('Length of ans ({}) != subm({})'.format(len(ans), len(subm)))

correctCnt = 0
type1err = 0
type2err = 0
for i, d in enumerate(zip(ans, subm)):
  if i == 0: continue
  if d[0] != d[1]:
    if d[0] == '1':
        # yellow
        type1err += 1
        print('\033[93mx\033[0m', end='')
    else:
        # red
        type2err += 1
        print('\033[31mx\033[0m', end='')
    # if d[0] == '1':
    #     print('\033[93m', end='')
    # else:
    #     print('\033[31m', end='')
    # print('Line {}: ans={}, submitted={}'.format(i, d[0], d[1]), end='')
    # print('\033[0m')
  else:
    if d[0] == '1':
        # blue
        print('\033[34mo\033[0m', end='')
    else:
        # green
        print('\033[32mo\033[0m', end='')
    correctCnt += 1

print()

wrongCnt = len(ans) - correctCnt

print('Result: \
\033[01m\033[32m{} correct\033[0m, \
\033[01m\033[31m{} wrong\033[0m, \
rate=\033[01m\033[34m{:.2f}%\033[0m'.format(
    correctCnt,
    wrongCnt,
    correctCnt / (correctCnt + wrongCnt) * 100
))
print('Errors: {} type 1, {} type 2'.format(type1err, type2err))
