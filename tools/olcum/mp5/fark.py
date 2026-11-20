#!/usr/bin/env python3
"""W0-MP5a: iki kaydın ortak serilerinde en büyük (en büyük değer) farkını ve göreli farkını basar.

  python3 -I fark.py <kayit1> <kimlik1> <kayit2> <kimlik2>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ozet  # noqa: E402


def main():
    k1, i1, k2, i2 = sys.argv[1:5]
    _, _, _, s1 = ozet.denetle(ozet.oku(k1, i1), None)
    _, _, _, s2 = ozet.denetle(ozet.oku(k2, i2), None)
    a = {ozet.anahtar(x["kv"]): x for x in s1}
    b = {ozet.anahtar(x["kv"]): x for x in s2}
    ortak = [k for k in a if k in b]
    en = {}
    for k in ortak:
        d = max(b[k]["dag"]) - max(a[k]["dag"])
        ob = k[5]
        if ob not in en or abs(d) > abs(en[ob][0]):
            en[ob] = (d, k, max(a[k]["dag"]))
    print(f"ortak seri {len(ortak)}")
    for ob, (d, k, v) in en.items():
        print(f"{ob}: en büyük fark {d} çevrim ({d / v * 100:+.3f} %), {' '.join(k)}")


if __name__ == "__main__":
    main()
