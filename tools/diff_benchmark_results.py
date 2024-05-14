#! /usr/bin/env python3

import sys

def read_table(filename):
    with open(filename, 'r') as f:
        tables = []
        while True:
            h = f.readline()
            if len(h) == 0:
                return tables
            h = h.split('\u2502')
            for i, x in enumerate(h):
                h[i] = x.strip()
            if len(f.readline()) == 0:
                raise RuntimeError()
            lines = []
            while True:
                l = f.readline()
                if l == '\n' or len(l) == 0:
                    break
                l = l.split('\u2502')
                s = l[0].strip()
                v = l[1:]
                for i, x in enumerate(v):
                    v[i] = float(x.strip().split()[0])
                lines.append((s, v))
            tables.append((h, lines))


def print_table_diff(table1, table2):
    h1, t1 = table1
    h2, t2 = table2
    if h1 != h2:
        raise RuntimeError()
    if len(t1) != len(t2):
        raise RuntimeError()
    print(f" {h1[0]:12.12s}", end='')
    for s in h1[1:]:
        print(f" \u2502 {s.center(12):12.12s}", end='')
    print()
    for i in range(len(h1) * 15 - 1):
        if i % 15 == 14:
            print("\u253c", end='')
        else:
            print("\u2500", end='')
    print()
    for l1, l2 in zip(t1, t2):
        s1, v1 = l1
        s2, v2 = l2
        if s1 != s2:
            raise RuntimeError()
        print(f" {s1:12.12s}", end='')
        for x1, x2 in zip(v1, v2):
            d = 100 * (x2 / x1 - 1)
            print(" \u2502", end='')
            if d <= -5:
                print("\033[31m", end='')
            elif d <= -1:
                print("\033[33m", end='')
            elif d >= 5:
                print("\033[32m", end='')
            elif d >= 1:
                print("\033[36m", end='')
            print(f"{d:+11.2f} %", end='')
            print("\033[0m", end='');
        print()


tables1 = read_table(sys.argv[1])
tables2 = read_table(sys.argv[2])

if len(tables1) != len(tables2):
    raise RuntimeError()

for t1, t2 in zip(tables1, tables2):
    print_table_diff(t1, t2)
    print()
