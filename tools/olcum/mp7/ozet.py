#!/usr/bin/env python3
"""W0-MP7a: sure.csv'den rapor tablolarını (Markdown) üretir.

Kullanım: python3 -I tools/olcum/mp7/ozet.py <sure.csv> [<sure_s3.csv>]
"""
import csv
import sys
from collections import defaultdict


def oku(yol):
    with open(yol, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def sayi(x):
    return f"{float(x):.2f}".replace(".", ",")


def main():
    satirlar = oku(sys.argv[1])
    tam = [r for r in satirlar if r["kademe"] == "tam"]
    sade = [r for r in satirlar if r["kademe"] == "sade"]
    pingler = sorted({r["ping"] for r in satirlar})
    print(f"Hücre sayısı: tam {len(tam)}, sade {len(sade)}; ping sayıları: {', '.join(pingler)}\n")

    # Sahne başına kurulum ve bellek
    print("| Sahne | Üçgen | BVH kurulumu (ms) | Sahne RSS artışı (MB) | Embree sahne belleği (MB) | Süreç tepe RSS (MB) |")
    print("|---|---|---|---|---|---|")
    gor = {}
    for r in satirlar:
        k = (r["sahne"], r["menzil_m"])
        gor.setdefault(r["sahne"], []).append(r)
    for s in ["1e4", "1e5", "1e6"]:
        rs = gor.get(s, [])
        if not rs:
            continue
        kur = sorted(float(r["kurulum_ms"]) for r in rs)
        print(
            f"| {s} | {rs[0]['ucgen']} | {sayi(min(kur))}–{sayi(max(kur))} | {sayi(max(float(r['rss_sahne_mb']) for r in rs))} "
            f"| {sayi(max(float(r['embree_sahne_mb']) for r in rs))} | {sayi(max(float(r['rss_tepe_mb']) for r in rs))} |"
        )
    print()

    isler = sorted({int(r["is"]) for r in tam})
    for kademe, rs in (("tam", tam), ("sade", sade)):
        if not rs:
            continue
        isk = sorted({int(r["is"]) for r in rs})
        print(f"#### Kademe: {kademe} (hücre başına p50 / p99 / en büyük, ms)\n")
        print("| Sahne | Menzil | D | N (gerçek) | " + " | ".join(f"{i} iş p." for i in isk) + " | kesişim/ping | görünürlük/ping |")
        print("|---|---|---|---|" + "---|" * len(isk) + "---|---|")
        tab = defaultdict(dict)
        for r in rs:
            tab[(r["sahne"], int(r["menzil_m"]), int(r["D"]), int(r["N_istek"]), r["N_gercek"])][int(r["is"])] = r
        for k in sorted(tab):
            hs = tab[k]
            herhangi = next(iter(hs.values()))
            hucre = []
            for i in isk:
                r = hs.get(i)
                hucre.append("—" if r is None else f"{sayi(r['p50_ms'])} / {sayi(r['p99_ms'])} / {sayi(r['max_ms'])}")
            print(
                f"| {k[0]} | {k[1]} m | {k[2]} | {k[4]} | " + " | ".join(hucre)
                + f" | {float(herhangi['kesisim_ort']):.0f} | {float(herhangi['gorunurluk_ort']):.0f} |"
            )
        print()

    if len(sys.argv) > 2:
        s3 = oku(sys.argv[2])
        print("#### S3 uzun koşusu (1e5, 7 m, D = 2, tek iş parçacığı)\n")
        print("| N (gerçek) | ping | p50 | p99 | en büyük | ortalama (ms) |")
        print("|---|---|---|---|---|---|")
        for r in s3:
            print(f"| {r['N_gercek']} | {r['ping']} | {sayi(r['p50_ms'])} | {sayi(r['p99_ms'])} | {sayi(r['max_ms'])} | {sayi(r['ort_ms'])} |")


if __name__ == "__main__":
    main()
