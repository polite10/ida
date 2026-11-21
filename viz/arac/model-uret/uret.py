#!/usr/bin/env python3
"""İDA-0, hedef ve deniz tabanı varlıklarını basit geometriden üretir (W3-viz kapsam md. 3).

Çıktı: assets/acik_su/{manifest.json, ida0.glb, hedef_samandira.glb, deniz_tabani.glb}.
A7 sözleşmesine göre kaynak .glb dosyaları ve manifest depoda durur (A7 §3.1, K47: üretim
derlemenin parçası değildir). Yalnız Python standart kitaplığı kullanılır.

Kullanım:  python3 -I viz/arac/model-uret/uret.py [--cikti assets/acik_su] [--denetle]
  --denetle  dosya yazmaz; üretilenin depodakiyle bayt bayt aynı olduğunu denetler.

Geometri gövde çerçevesinde yazılır (x ileri, y sancak, z aşağı; iskelet §3.1) ve glTF'e
A7-K14'teki M'nin tersiyle çevrilir: (X, Y, Z)_gltf = (−y, −z, x). M bir dönüş olduğundan
üçgen yönleri korunur. REF düğümü glTF orijinindedir; bütün düğümler sahne köküdür.

Oranlar: BlueROV2 Heavy sınıfı açık çerçeve (uzunluk 457 mm, genişlik 575 mm, gövde yüksekliği
254 mm; üretici belgesindeki dış ölçüler, doğrulanmadı) ve A5 araç dosyasındaki
(scenarios/ida0.toml) itici, tüp ve sensör konumları. A5 konumları yer tutucu [T]/[MV]
değerlerdir; çakıştıkları yerde (yatay iticiler ile torpido tüpleri) tüp boyu kısaltıldı.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import random
import struct
import sys
import zlib

# --------------------------------------------------------------------------------------
# Küçük vektör yardımcıları
# --------------------------------------------------------------------------------------


def ekle(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def cikar(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def olc(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def nokta(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def capraz(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def boy(a):
    return math.sqrt(nokta(a, a))


def birim(a):
    n = boy(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 0 else (0.0, 0.0, 1.0)


def dik_taban(eksen):
    """Eksene dik iki birim vektör (u, v) ve eksen: (u, v, eksen) sağ ellidir."""
    a = birim(eksen)
    yardimci = (0.0, 0.0, 1.0) if abs(a[2]) < 0.9 else (1.0, 0.0, 0.0)
    u = birim(capraz(yardimci, a))
    v = capraz(a, u)
    return u, v, a


def govde_gltf(p):
    """Gövde çerçevesi → glTF: (X, Y, Z) = (−y, −z, x)."""
    return (-p[1], -p[2], p[0])


# --------------------------------------------------------------------------------------
# Ağ parçaları (gövde çerçevesinde)
# --------------------------------------------------------------------------------------


class Parca:
    """Tek malzemeli üçgen listesi: konum, normal, isteğe bağlı UV, indeks."""

    def __init__(self):
        self.p = []
        self.n = []
        self.uv = []
        self.i = []

    def kose(self, p, n, uv=None):
        self.p.append(p)
        self.n.append(n)
        if uv is not None:
            self.uv.append(uv)
        return len(self.p) - 1

    def ucgen(self, a, b, c):
        self.i.extend((a, b, c))

    def birlestir(self, o):
        k = len(self.p)
        self.p.extend(o.p)
        self.n.extend(o.n)
        self.uv.extend(o.uv)
        self.i.extend(x + k for x in o.i)
        return self


def cokgen(parca, noktalar, merkez_ipucu):
    """Düz gölgeli dışbükey çokgen; normal merkez ipucundan dışa doğru yönlenir."""
    n = birim(capraz(cikar(noktalar[1], noktalar[0]), cikar(noktalar[2], noktalar[0])))
    orta = olc(noktalar[0], 0.0)
    for q in noktalar:
        orta = ekle(orta, q)
    orta = olc(orta, 1.0 / len(noktalar))
    if nokta(n, cikar(orta, merkez_ipucu)) < 0:
        noktalar = list(reversed(noktalar))
        n = olc(n, -1.0)
    ilk = None
    idx = [parca.kose(q, n) for q in noktalar]
    ilk = idx[0]
    for k in range(1, len(idx) - 1):
        parca.ucgen(ilk, idx[k], idx[k + 1])


def pahli_kutu(merkez, yari, pah=0.0, eksenler=None):
    """Pahlı (kenarları kesik) kutu. yari: yarı boyutlar; eksenler: (ex, ey, ez) dönüş."""
    ex, ey, ez = eksenler or ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
    hx, hy, hz = yari
    c = min(pah, hx * 0.45, hy * 0.45, hz * 0.45)

    def nk(x, y, z):
        return ekle(merkez, ekle(ekle(olc(ex, x), olc(ey, y)), olc(ez, z)))

    parca = Parca()
    if c <= 0.0:
        for eks, h1, h2, h3 in ((0, hx, hy, hz), (1, hy, hx, hz), (2, hz, hx, hy)):
            for s in (-1, 1):
                kos = []
                for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    if eks == 0:
                        kos.append(nk(s * hx, a * hy, b * hz))
                    elif eks == 1:
                        kos.append(nk(a * hx, s * hy, b * hz))
                    else:
                        kos.append(nk(a * hx, b * hy, s * hz))
                cokgen(parca, kos, merkez)
        return parca

    def P(eks, sx, sy, sz):
        if eks == 0:
            return nk(sx * hx, sy * (hy - c), sz * (hz - c))
        if eks == 1:
            return nk(sx * (hx - c), sy * hy, sz * (hz - c))
        return nk(sx * (hx - c), sy * (hy - c), sz * hz)

    halka = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    for s in (-1, 1):  # ana yüzler
        cokgen(parca, [P(0, s, a, b) for a, b in halka], merkez)
        cokgen(parca, [P(1, a, s, b) for a, b in halka], merkez)
        cokgen(parca, [P(2, a, b, s) for a, b in halka], merkez)
    for s1 in (-1, 1):  # kenar pahları
        for s2 in (-1, 1):
            cokgen(parca, [P(0, s1, s2, -1), P(0, s1, s2, 1), P(1, s1, s2, 1), P(1, s1, s2, -1)], merkez)
            cokgen(parca, [P(0, s1, -1, s2), P(0, s1, 1, s2), P(2, s1, 1, s2), P(2, s1, -1, s2)], merkez)
            cokgen(parca, [P(1, -1, s1, s2), P(1, 1, s1, s2), P(2, 1, s1, s2), P(2, -1, s1, s2)], merkez)
    for sx in (-1, 1):  # köşe üçgenleri
        for sy in (-1, 1):
            for sz in (-1, 1):
                cokgen(parca, [P(0, sx, sy, sz), P(1, sx, sy, sz), P(2, sx, sy, sz)], merkez)
    return parca


def kutu_min_max(mn, mx, pah=0.0):
    m = olc(ekle(mn, mx), 0.5)
    y = olc(cikar(mx, mn), 0.5)
    return pahli_kutu(m, y, pah)


def silindir(p0, p1, r, n=32, kapak0=True, kapak1=True, r1=None, yumusak=True):
    """p0'dan p1'e silindir (r1 verilirse koni kesiği)."""
    r1 = r if r1 is None else r1
    u, v, a = dik_taban(cikar(p1, p0))
    L = boy(cikar(p1, p0))
    parca = Parca()
    egim = (r - r1) / L if L > 0 else 0.0
    alt, ust = [], []
    for k in range(n + 1):
        t = 2.0 * math.pi * k / n
        d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
        nrm = birim(ekle(d, olc(a, egim)))
        alt.append(parca.kose(ekle(p0, olc(d, r)), nrm))
        ust.append(parca.kose(ekle(p1, olc(d, r1)), nrm))
    for k in range(n):
        parca.ucgen(alt[k], alt[k + 1], ust[k + 1])
        parca.ucgen(alt[k], ust[k + 1], ust[k])
    for kapak, p, rr, yon in ((kapak0, p0, r, olc(a, -1.0)), (kapak1, p1, r1, a)):
        if not kapak or rr <= 0:
            continue
        m = parca.kose(p, yon)
        halka = []
        for k in range(n):
            t = 2.0 * math.pi * k / n
            d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
            halka.append(parca.kose(ekle(p, olc(d, rr)), yon))
        for k in range(n):
            a1, b1 = halka[k], halka[(k + 1) % n]
            if yon is a:
                parca.ucgen(m, a1, b1)
            else:
                parca.ucgen(m, b1, a1)
    return parca


def boru(p0, p1, r_dis, r_ic, n=40):
    """İçi boş silindir (halka kesitli): dış ve iç yüzey, iki uç halkası."""
    u, v, a = dik_taban(cikar(p1, p0))
    parca = Parca()
    for r, ic in ((r_dis, False), (r_ic, True)):
        alt, ust = [], []
        for k in range(n + 1):
            t = 2.0 * math.pi * k / n
            d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
            nrm = olc(d, -1.0) if ic else d
            alt.append(parca.kose(ekle(p0, olc(d, r)), nrm))
            ust.append(parca.kose(ekle(p1, olc(d, r)), nrm))
        for k in range(n):
            if ic:
                parca.ucgen(alt[k], ust[k + 1], alt[k + 1])
                parca.ucgen(alt[k], ust[k], ust[k + 1])
            else:
                parca.ucgen(alt[k], alt[k + 1], ust[k + 1])
                parca.ucgen(alt[k], ust[k + 1], ust[k])
    for p, yon in ((p0, olc(a, -1.0)), (p1, a)):
        dis, ic = [], []
        for k in range(n + 1):
            t = 2.0 * math.pi * k / n
            d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
            dis.append(parca.kose(ekle(p, olc(d, r_dis)), yon))
            ic.append(parca.kose(ekle(p, olc(d, r_ic)), yon))
        for k in range(n):
            if yon is a:
                parca.ucgen(ic[k], dis[k], dis[k + 1])
                parca.ucgen(ic[k], dis[k + 1], ic[k + 1])
            else:
                parca.ucgen(ic[k], dis[k + 1], dis[k])
                parca.ucgen(ic[k], ic[k + 1], dis[k + 1])
    return parca


def kubbe(merkez, eksen, r, n=32, m=12, yukseklik_orani=1.0):
    """Yarım küre (eksen yönünde şişkin), taban açık."""
    u, v, a = dik_taban(eksen)
    parca = Parca()
    satirlar = []
    for j in range(m + 1):
        phi = (math.pi / 2.0) * j / m
        satir = []
        for k in range(n + 1):
            t = 2.0 * math.pi * k / n
            d = ekle(ekle(olc(u, math.cos(t) * math.cos(phi)), olc(v, math.sin(t) * math.cos(phi))),
                     olc(a, math.sin(phi)))
            p = ekle(merkez, ekle(ekle(olc(u, r * math.cos(t) * math.cos(phi)), olc(v, r * math.sin(t) * math.cos(phi))),
                                  olc(a, r * yukseklik_orani * math.sin(phi))))
            satir.append(parca.kose(p, birim(d)))
        satirlar.append(satir)
    for j in range(m):
        for k in range(n):
            a0, a1 = satirlar[j][k], satirlar[j][k + 1]
            b0, b1 = satirlar[j + 1][k], satirlar[j + 1][k + 1]
            parca.ucgen(a0, a1, b1)
            parca.ucgen(a0, b1, b0)
    return parca


def sferoid_kapali(merkez, eksen, yari_boy, yari_cap, n=32, m=16):
    """Kapalı, manifold uzun sferoid (paylaşılan köşeler; yüzerlik ağı için de uygun)."""
    u, v, a = dik_taban(eksen)
    parca = Parca()
    kuzey = parca.kose(ekle(merkez, olc(a, yari_boy)), a)
    guney = parca.kose(ekle(merkez, olc(a, -yari_boy)), olc(a, -1.0))
    halkalar = []
    for j in range(1, m):
        th = math.pi * j / m
        z = yari_boy * math.cos(th)
        rr = yari_cap * math.sin(th)
        halka = []
        for k in range(n):
            t = 2.0 * math.pi * k / n
            d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
            p = ekle(merkez, ekle(olc(d, rr), olc(a, z)))
            nrm = birim(ekle(olc(d, math.sin(th) / yari_cap), olc(a, math.cos(th) / yari_boy)))
            halka.append(parca.kose(p, nrm))
        halkalar.append(halka)
    for k in range(n):
        parca.ucgen(kuzey, halkalar[0][k], halkalar[0][(k + 1) % n])
        parca.ucgen(guney, halkalar[-1][(k + 1) % n], halkalar[-1][k])
    for j in range(len(halkalar) - 1):  # halkalar kuzeyden güneye: dışa bakan yön için ters sıra
        for k in range(n):
            a0, a1 = halkalar[j][k], halkalar[j][(k + 1) % n]
            b0, b1 = halkalar[j + 1][k], halkalar[j + 1][(k + 1) % n]
            parca.ucgen(a0, b1, a1)
            parca.ucgen(a0, b0, b1)
    return parca


def kutu_kapali(mn, mx):
    """8 köşeli kapalı manifold kutu (yüzerlik ve çarpışma ağları)."""
    parca = Parca()
    k = []
    for x in (mn[0], mx[0]):
        for y in (mn[1], mx[1]):
            for z in (mn[2], mx[2]):
                k.append(parca.kose((x, y, z), (0.0, 0.0, 0.0)))
    # indeks: x*4 + y*2 + z
    yuzler = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    m = olc(ekle(mn, mx), 0.5)
    for f in yuzler:
        a, b, c, d = (parca.p[x] for x in f)
        n = capraz(cikar(b, a), cikar(c, a))
        orta = olc(ekle(ekle(a, b), ekle(c, d)), 0.25)
        if nokta(n, cikar(orta, m)) < 0:
            f = tuple(reversed(f))
        parca.ucgen(k[f[0]], k[f[1]], k[f[2]])
        parca.ucgen(k[f[0]], k[f[2]], k[f[3]])
    return parca


def ikosfer(merkez, r, bozulma, rnd, alt_bolme=1, olcek=(1.0, 1.0, 1.0)):
    """Kaya: bozulmuş ikosfer, düz gölgeli."""
    t = (1.0 + math.sqrt(5.0)) / 2.0
    ks = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
          (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    ks = [birim(k) for k in ks]
    yz = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
          (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10),
          (8, 6, 7), (9, 8, 1)]
    for _ in range(alt_bolme):
        orta = {}

        def om(i, j):
            key = (min(i, j), max(i, j))
            if key not in orta:
                ks.append(birim(olc(ekle(ks[i], ks[j]), 0.5)))
                orta[key] = len(ks) - 1
            return orta[key]

        yeni = []
        for a, b, c in yz:
            ab, bc, ca = om(a, b), om(b, c), om(c, a)
            yeni += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        yz = yeni
    pk = []
    for k in ks:
        s = 1.0 + bozulma * (rnd.random() * 2.0 - 1.0)
        pk.append(ekle(merkez, (k[0] * r * s * olcek[0], k[1] * r * s * olcek[1], k[2] * r * s * olcek[2])))
    parca = Parca()
    for a, b, c in yz:
        cokgen(parca, [pk[a], pk[b], pk[c]], merkez)
    return parca


# --------------------------------------------------------------------------------------
# Hacim, merkez, kenar, dışbükeylik (A7-K25, K26)
# --------------------------------------------------------------------------------------


def hacim_merkez(parca):
    V = 0.0
    c = (0.0, 0.0, 0.0)
    for k in range(0, len(parca.i), 3):
        a, b, d = (parca.p[parca.i[k + j]] for j in range(3))
        v6 = nokta(a, capraz(b, d))
        V += v6
        c = ekle(c, olc(ekle(ekle(a, b), d), v6))
    V /= 6.0
    c = olc(c, 1.0 / (24.0 * V))
    return V, c


def en_uzun_kenar(parca):
    e = 0.0
    for k in range(0, len(parca.i), 3):
        t = [parca.p[parca.i[k + j]] for j in range(3)]
        for j in range(3):
            e = max(e, boy(cikar(t[j], t[(j + 1) % 3])))
    return e


def manifold_mu(parca):
    kenarlar = {}
    for k in range(0, len(parca.i), 3):
        t = parca.i[k:k + 3]
        for j in range(3):
            e = (t[j], t[(j + 1) % 3])
            kenarlar[e] = kenarlar.get(e, 0) + 1
    for (a, b), say in kenarlar.items():
        if say != 1 or kenarlar.get((b, a), 0) != 1:
            return False
    return True


def disbukey_mu(parca):
    for k in range(0, len(parca.i), 3):
        a, b, c = (parca.p[parca.i[k + j]] for j in range(3))
        n = birim(capraz(cikar(b, a), cikar(c, a)))
        for p in parca.p:
            if nokta(n, cikar(p, a)) > 1e-6:
                return False
    return True


# --------------------------------------------------------------------------------------
# PNG (doku) ve GLB yazımı
# --------------------------------------------------------------------------------------


def png_yaz(gen, yuk, pikseller):
    """8 bit RGB PNG; filtre 0. pikseller: satır satır (r, g, b) baytları."""
    ham = bytearray()
    for y in range(yuk):
        ham.append(0)
        ham.extend(pikseller[y * gen * 3:(y + 1) * gen * 3])

    def obek(tur, veri):
        return struct.pack(">I", len(veri)) + tur + veri + struct.pack(">I", zlib.crc32(tur + veri) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + obek(b"IHDR", struct.pack(">IIBBBBB", gen, yuk, 8, 2, 0, 0, 0)) +
            obek(b"IDAT", zlib.compress(bytes(ham), 9)) + obek(b"IEND", b""))


class Glb:
    def __init__(self):
        self.ikili = bytearray()
        self.gorunumler = []
        self.erisimciler = []
        self.agler = []
        self.malzemeler = []
        self.dugumler = []
        self.isiklar = []
        self.goruntuler = []
        self.dokular = []
        self.ornekleyiciler = []
        self.uzantilar = set()

    def _gorunum(self, veri, hedef=None):
        while len(self.ikili) % 4:
            self.ikili.append(0)
        g = {"buffer": 0, "byteOffset": len(self.ikili), "byteLength": len(veri)}
        if hedef is not None:
            g["target"] = hedef
        self.ikili.extend(veri)
        self.gorunumler.append(g)
        return len(self.gorunumler) - 1

    def _erisimci(self, veri, bilesen, tur, sayi, mn=None, mx=None, hedef=None):
        e = {"bufferView": self._gorunum(veri, hedef), "componentType": bilesen, "count": sayi, "type": tur}
        if mn is not None:
            e["min"] = mn
            e["max"] = mx
        self.erisimciler.append(e)
        return len(self.erisimciler) - 1

    def malzeme(self, ad, renk, metal=0.0, puruz=0.5, alfa=None, isima=None, isima_gucu=None, doku=None,
                cift_yuz=False):
        m = {"name": ad, "pbrMetallicRoughness": {"baseColorFactor": [round(x, 6) for x in renk],
                                                  "metallicFactor": metal, "roughnessFactor": puruz}}
        if doku is not None:
            m["pbrMetallicRoughness"]["baseColorTexture"] = {"index": doku}
        if alfa is not None:
            m["alphaMode"] = "BLEND"
        if isima is not None:
            m["emissiveFactor"] = list(isima)
            if isima_gucu is not None:
                m["extensions"] = {"KHR_materials_emissive_strength": {"emissiveStrength": isima_gucu}}
                self.uzantilar.add("KHR_materials_emissive_strength")
        if cift_yuz:
            m["doubleSided"] = True
        self.malzemeler.append(m)
        return len(self.malzemeler) - 1

    def doku_png(self, png):
        gv = self._gorunum(png)
        self.goruntuler.append({"bufferView": gv, "mimeType": "image/png"})
        if not self.ornekleyiciler:
            # 10497 REPEAT, 9987 LINEAR_MIPMAP_LINEAR, 9729 LINEAR
            self.ornekleyiciler.append({"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497})
        self.dokular.append({"sampler": 0, "source": len(self.goruntuler) - 1})
        return len(self.dokular) - 1

    def ag(self, ad, parcalar):
        """parcalar: [(malzeme_indeksi, Parca)] — gövde çerçevesindeki koordinatlar."""
        temel = []
        for mi, p in parcalar:
            if not p.i:
                continue
            kon = [govde_gltf(q) for q in p.p]
            nrm = [birim(govde_gltf(q)) if boy(q) > 0 else (0.0, 1.0, 0.0) for q in p.n]
            mn = [min(q[j] for q in kon) for j in range(3)]
            mx = [max(q[j] for q in kon) for j in range(3)]
            kb = b"".join(struct.pack("<3f", *q) for q in kon)
            nb = b"".join(struct.pack("<3f", *q) for q in nrm)
            pk = {"POSITION": self._erisimci(kb, 5126, "VEC3", len(kon), mn, mx, 34962),
                  "NORMAL": self._erisimci(nb, 5126, "VEC3", len(nrm), hedef=34962)}
            if p.uv:
                ub = b"".join(struct.pack("<2f", *q) for q in p.uv)
                pk["TEXCOORD_0"] = self._erisimci(ub, 5126, "VEC2", len(p.uv), hedef=34962)
            if len(kon) < 65536:
                ib = struct.pack("<%dH" % len(p.i), *p.i)
                ie = self._erisimci(ib, 5123, "SCALAR", len(p.i), hedef=34963)
            else:
                ib = struct.pack("<%dI" % len(p.i), *p.i)
                ie = self._erisimci(ib, 5125, "SCALAR", len(p.i), hedef=34963)
            temel.append({"attributes": pk, "indices": ie, "material": mi, "mode": 4})
        self.agler.append({"name": ad, "primitives": temel})
        return len(self.agler) - 1

    def dugum(self, ad, ag=None, konum_govde=None, donus=None, isik=None):
        d = {"name": ad}
        if ag is not None:
            d["mesh"] = ag
        if konum_govde is not None and any(abs(x) > 0 for x in konum_govde):
            d["translation"] = list(govde_gltf(konum_govde))
        if donus is not None:
            d["rotation"] = list(donus)
        if isik is not None:
            d["extensions"] = {"KHR_lights_punctual": {"light": isik}}
            self.uzantilar.add("KHR_lights_punctual")
        self.dugumler.append(d)
        return len(self.dugumler) - 1

    def spot_isik(self, ad, renk, siddet, menzil, ic, dis):
        self.isiklar.append({"name": ad, "type": "spot", "color": list(renk), "intensity": siddet, "range": menzil,
                             "spot": {"innerConeAngle": ic, "outerConeAngle": dis}})
        return len(self.isiklar) - 1

    def yaz(self, degistir=None):
        j = {"asset": {"version": "2.0", "generator": "İDA viz/arac/model-uret/uret.py"},
             "scene": 0, "scenes": [{"nodes": list(range(len(self.dugumler)))}],
             "nodes": self.dugumler, "meshes": self.agler, "materials": self.malzemeler,
             "accessors": self.erisimciler, "bufferViews": self.gorunumler,
             "buffers": [{"byteLength": len(self.ikili)}]}
        if self.goruntuler:
            j["images"] = self.goruntuler
            j["textures"] = self.dokular
            j["samplers"] = self.ornekleyiciler
        if self.isiklar:
            j.setdefault("extensions", {})["KHR_lights_punctual"] = {"lights": self.isiklar}
        if self.uzantilar:
            j["extensionsUsed"] = sorted(self.uzantilar)  # A7-K11: izinli uzantılar
        if degistir is not None:  # yalnız olumsuz sınama kümeleri için
            degistir(j)
        js = json.dumps(j, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        while len(js) % 4:
            js += b" "
        while len(self.ikili) % 4:
            self.ikili.append(0)
        toplam = 12 + 8 + len(js) + 8 + len(self.ikili)
        return (struct.pack("<III", 0x46546C67, 2, toplam) + struct.pack("<II", len(js), 0x4E4F534A) + js +
                struct.pack("<II", len(self.ikili), 0x004E4942) + bytes(self.ikili))


# --------------------------------------------------------------------------------------
# Varlık: İDA-0 (BlueROV2 Heavy sınıfı açık çerçeve + iki torpido tüpü + sonar başı)
# --------------------------------------------------------------------------------------

S2 = math.sqrt(0.5)
# A5 (scenarios/ida0.toml) itici tablosu: konum, pozitif komutun itki yönü.
ITICILER = [
    ((0.156, 0.111, 0.085), (S2, -S2, 0.0)), ((0.156, -0.111, 0.085), (S2, S2, 0.0)),
    ((-0.156, 0.111, 0.085), (S2, S2, 0.0)), ((-0.156, -0.111, 0.085), (S2, -S2, 0.0)),
    ((0.120, 0.218, 0.0), (0.0, 0.0, 1.0)), ((0.120, -0.218, 0.0), (0.0, 0.0, 1.0)),
    ((-0.120, 0.218, 0.0), (0.0, 0.0, 1.0)), ((-0.120, -0.218, 0.0), (0.0, 0.0, 1.0)),
]
TUPLER = [(0.0, 0.10, 0.10), (0.0, -0.10, 0.10)]  # A5 firlaticilar.f1/f2.tup_konumu
TORPIDO_BOY, TORPIDO_CAP = 0.20, 0.04              # A5 arac.torpido
KAMERA = (0.21, 0.0, -0.02)                        # A5 kamera0 montajı
PING360 = (0.12, 0.0, -0.20)
DVL = (0.05, 0.0, 0.14)


def itici(merkez, yon, m):
    """T200 benzeri itici: nozul, motor gövdesi, 3 destek, 3 kanatlı pervane."""
    u, v, a = dik_taban(yon)
    nozul, motor, pervane, metal = m
    ps = {nozul: Parca(), motor: Parca(), pervane: Parca(), metal: Parca()}
    L = 0.075
    ps[nozul].birlestir(boru(ekle(merkez, olc(a, -L / 2)), ekle(merkez, olc(a, L / 2)), 0.050, 0.0455, 40))
    # Nozul ağzında hafif genişleme halkası
    ps[nozul].birlestir(silindir(ekle(merkez, olc(a, L / 2)), ekle(merkez, olc(a, L / 2 + 0.006)), 0.0505, 32,
                                 False, False, r1=0.0475))
    # Motor gövdesi (arkada, nozulun içinden geçer) ve burun konisi
    ps[motor].birlestir(silindir(ekle(merkez, olc(a, -0.055)), ekle(merkez, olc(a, 0.010)), 0.024, 28))
    ps[motor].birlestir(silindir(ekle(merkez, olc(a, -0.072)), ekle(merkez, olc(a, -0.055)), 0.012, 20, True, False,
                                 r1=0.024))
    ps[metal].birlestir(silindir(ekle(merkez, olc(a, 0.010)), ekle(merkez, olc(a, 0.030)), 0.013, 20, False, True,
                                 r1=0.008))
    # Destek kolları (motor → nozul)
    for k in range(3):
        t = 2.0 * math.pi * k / 3.0 + 0.3
        d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
        e = capraz(a, d)
        orta = ekle(merkez, ekle(olc(a, -0.030), olc(d, 0.035)))
        ps[motor].birlestir(pahli_kutu(orta, (0.0115, 0.003, 0.009), 0.0, (d, e, a)))
    # Pervane: 3 kanat, 28° hatve açısı
    for k in range(3):
        t = 2.0 * math.pi * k / 3.0
        d = ekle(olc(u, math.cos(t)), olc(v, math.sin(t)))
        e = capraz(a, d)
        aci = math.radians(28.0)
        e2 = ekle(olc(e, math.cos(aci)), olc(a, math.sin(aci)))
        a2 = capraz(d, e2)
        orta = ekle(merkez, ekle(olc(a, 0.016), olc(d, 0.028)))
        ps[pervane].birlestir(pahli_kutu(orta, (0.016, 0.011, 0.0012), 0.0, (d, e2, a2)))
    return ps


def ida0(glb):
    mat = {
        "cerceve": glb.malzeme("cerceve_hdpe", (0.012, 0.012, 0.013, 1.0), 0.0, 0.55),
        "kopuk": glb.malzeme("yuzdurucu_kopuk", (0.010, 0.085, 0.42, 1.0), 0.0, 0.8),
        "akrilik": glb.malzeme("akrilik_tup", (0.85, 0.92, 0.95, 0.16), 0.0, 0.04, alfa=True),
        "alu_siyah": glb.malzeme("eloksal_siyah", (0.02, 0.02, 0.022, 1.0), 0.85, 0.32),
        "alu": glb.malzeme("aluminyum", (0.62, 0.63, 0.65, 1.0), 0.95, 0.28),
        "kart": glb.malzeme("elektronik_kart", (0.008, 0.10, 0.03, 1.0), 0.1, 0.5),
        "nozul": glb.malzeme("itici_nozul", (0.015, 0.015, 0.017, 1.0), 0.0, 0.42),
        "motor": glb.malzeme("itici_motor", (0.03, 0.03, 0.035, 1.0), 0.3, 0.45),
        "pervane": glb.malzeme("itici_pervane", (0.04, 0.12, 0.38, 1.0), 0.0, 0.35),
        "tup": glb.malzeme("firlatma_tupu", (0.20, 0.22, 0.20, 1.0), 0.6, 0.45),
        "torpido": glb.malzeme("torpido_govde", (0.85, 0.42, 0.02, 1.0), 0.1, 0.35),
        "kanat": glb.malzeme("torpido_kanat", (0.03, 0.03, 0.03, 1.0), 0.2, 0.5),
        "sonar": glb.malzeme("sonar_govde", (0.025, 0.025, 0.028, 1.0), 0.1, 0.4),
        "sonar_kubbe": glb.malzeme("sonar_kubbe", (0.03, 0.035, 0.04, 1.0), 0.0, 0.12),
        "anten": glb.malzeme("anten", (0.80, 0.80, 0.78, 1.0), 0.0, 0.5),
        "lens": glb.malzeme("isik_lensi", (0.9, 0.9, 0.9, 1.0), 0.0, 0.1, isima=(1.0, 0.94, 0.82), isima_gucu=6.0),
        "cam": glb.malzeme("kamera_lensi", (0.005, 0.005, 0.008, 1.0), 0.0, 0.05),
    }
    gov = {k: Parca() for k in mat}

    # ---- çerçeve: iki yan panel (çubuklardan, açık), merkez plaka, ağır kanatlar ----
    P = 0.003
    for s in (1, -1):
        y0, t = 0.155 * s, 0.00635
        yk = lambda a, b: (min(a, b), max(a, b))
        Y = yk(y0 - t, y0 + t)
        gov["cerceve"].birlestir(kutu_min_max((-0.2285, Y[0], -0.127), (0.2285, Y[1], -0.100), P))   # üst ray
        gov["cerceve"].birlestir(kutu_min_max((-0.080, Y[0], 0.100), (0.080, Y[1], 0.127), P))       # kızak
        for x0, x1 in ((0.195, 0.2285), (-0.2285, -0.195)):                                          # uç dikmeler
            gov["cerceve"].birlestir(kutu_min_max((x0, Y[0], -0.127), (x1, Y[1], 0.020), P))
        gov["cerceve"].birlestir(kutu_min_max((-0.022, Y[0], -0.127), (0.022, Y[1], 0.127), P))      # orta dikme
        for xs in (1, -1):  # çapraz dikmeler (kızaktan orta raya)
            orta = (0.062 * xs, y0, 0.010)
            yon = birim((0.035 * xs, 0.0, 0.10))
            gov["cerceve"].birlestir(pahli_kutu(orta, (0.010, t, 0.060), 0.0, (birim(capraz((0, 1, 0), yon)), (0, 1, 0), yon)))
        # Ağır kanat (dikey iticiler için dış çerçeve), z ortada
        Ys = yk(0.161 * s, 0.2875 * s)
        gov["cerceve"].birlestir(kutu_min_max((-0.195, yk(0.272 * s, 0.2875 * s)[0], -0.006),
                                              (0.195, yk(0.272 * s, 0.2875 * s)[1], 0.006), P))
        for x0, x1 in ((0.175, 0.195), (-0.195, -0.175), (-0.010, 0.010)):
            gov["cerceve"].birlestir(kutu_min_max((x0, Ys[0], -0.006), (x1, Ys[1], 0.006), P))
    gov["cerceve"].birlestir(kutu_min_max((-0.19, -0.1486, -0.006), (0.19, 0.1486, 0.004), P))     # merkez plaka
    for x in (0.12, -0.05):  # üst köprüler (sonar ve anten taşıyıcı)
        gov["cerceve"].birlestir(kutu_min_max((x - 0.012, -0.062, -0.140), (x + 0.012, 0.062, -0.128), P))

    # ---- yüzdürücü köpük blokları ----
    for s in (1, -1):
        yy = sorted((0.062 * s, 0.148 * s))
        gov["kopuk"].birlestir(kutu_min_max((-0.19, yy[0], -0.140), (0.19, yy[1], -0.064), 0.010))

    # ---- elektronik tüpü (4"), uç kapaklar, kubbe, içerik ----
    ez = -0.065
    gov["akrilik"].birlestir(boru((-0.130, 0.0, ez), (0.160, 0.0, ez), 0.057, 0.0535, 48))
    gov["akrilik"].birlestir(kubbe((0.172, 0.0, ez), (1.0, 0.0, 0.0), 0.057, 48, 14))
    gov["alu_siyah"].birlestir(silindir((-0.148, 0.0, ez), (-0.130, 0.0, ez), 0.0595, 48))
    gov["alu_siyah"].birlestir(boru((0.160, 0.0, ez), (0.172, 0.0, ez), 0.0595, 0.054, 48))
    for k in range(6):  # arka kapak geçişleri
        t = 2.0 * math.pi * k / 6.0
        p0 = (-0.148, 0.032 * math.cos(t), ez + 0.032 * math.sin(t))
        gov["alu"].birlestir(silindir(p0, ekle(p0, (-0.018, 0.0, 0.0)), 0.0055, 14))
    gov["kart"].birlestir(kutu_min_max((-0.12, -0.040, ez - 0.003), (0.15, 0.040, ez + 0.003)))
    rnd = random.Random(7)
    for _ in range(9):  # kart üstü bileşenler
        x = rnd.uniform(-0.10, 0.13)
        y = rnd.uniform(-0.03, 0.03)
        w = rnd.uniform(0.008, 0.022)
        gov["alu_siyah"].birlestir(kutu_min_max((x - w, y - w * 0.7, ez - 0.003 - rnd.uniform(0.004, 0.012)),
                                                (x + w, y + w * 0.7, ez - 0.003)))
    # Kamera, tüp ekseninde (A5 montajı z = −0,02 tüp eksenine yakın; görsel modelde eksende)
    gov["sonar"].birlestir(kutu_min_max((0.185, -0.016, ez - 0.016), (0.200, 0.016, ez + 0.016)))
    gov["cam"].birlestir(silindir((0.200, 0.0, ez), (0.212, 0.0, ez), 0.008, 20))

    # ---- batarya tüpü (3", alüminyum) ----
    bz = 0.050
    gov["alu"].birlestir(silindir((-0.140, 0.0, bz), (0.130, 0.0, bz), 0.0445, 40, False, False))
    gov["alu_siyah"].birlestir(silindir((-0.152, 0.0, bz), (-0.140, 0.0, bz), 0.047, 40))
    gov["alu_siyah"].birlestir(silindir((0.130, 0.0, bz), (0.142, 0.0, bz), 0.047, 40))
    for x in (-0.09, 0.09):  # kelepçeler
        gov["cerceve"].birlestir(kutu_min_max((x - 0.008, -0.050, 0.004), (x + 0.008, 0.050, 0.012), 0.002))

    # ---- iticiler ----
    for merkez, yon in ITICILER:
        for k, p in itici(merkez, yon, ("nozul", "motor", "pervane", "alu")).items():
            gov[k].birlestir(p)

    # ---- ışıklar (ön üst köşeler) ----
    isik_konumlari = []
    for s in (1, -1):
        p0 = (0.190, 0.12 * s, -0.092)
        gov["alu_siyah"].birlestir(silindir(p0, (0.226, 0.12 * s, -0.092), 0.0175, 28))
        gov["lens"].birlestir(silindir((0.226, 0.12 * s, -0.092), (0.229, 0.12 * s, -0.092), 0.0145, 28, False, True))
        isik_konumlari.append((0.232, 0.12 * s, -0.092))

    # ---- Ping360 benzeri sonar başı (MNT_ping360_0 bu başın içinde) ----
    gov["alu_siyah"].birlestir(silindir((PING360[0], 0.0, -0.140), (PING360[0], 0.0, -0.160), 0.012, 20))
    gov["sonar"].birlestir(silindir((PING360[0], 0.0, -0.160), (PING360[0], 0.0, -0.205), 0.0395, 40))
    gov["sonar_kubbe"].birlestir(kubbe((PING360[0], 0.0, -0.205), (0.0, 0.0, -1.0), 0.037, 40, 10, 0.75))

    # ---- GNSS anten direği (arka üst) ----
    gov["alu"].birlestir(silindir((-0.05, 0.0, -0.140), (-0.05, 0.0, -0.208), 0.006, 14))
    gov["anten"].birlestir(silindir((-0.05, 0.0, -0.208), (-0.05, 0.0, -0.222), 0.028, 32))

    # ---- DVL (alt, aşağı bakar) ----
    gov["sonar"].birlestir(silindir((DVL[0], 0.0, 0.097), (DVL[0], 0.0, DVL[2]), 0.033, 36))
    for k in range(4):
        t = math.pi / 4 + k * math.pi / 2
        p = (DVL[0] + 0.019 * math.cos(t), 0.019 * math.sin(t), DVL[2])
        gov["cam"].birlestir(silindir(p, ekle(p, (0.0, 0.0, 0.002)), 0.009, 16, False, True))

    # ---- fırlatma tüpleri (yatay iticilerle çakışmasın diye 0,16 m; A5 tüp merkezleri) ----
    for t in TUPLER:
        gov["tup"].birlestir(boru((t[0] - 0.080, t[1], t[2]), (t[0] + 0.080, t[1], t[2]), 0.026, 0.0215, 40))
        for x in (-0.05, 0.05):  # askılar
            gov["tup"].birlestir(kutu_min_max((x - 0.007, t[1] - 0.010, 0.004), (x + 0.007, t[1] + 0.010, t[2] - 0.024)))
        gov["alu"].birlestir(boru((t[0] + 0.080, t[1], t[2]), (t[0] + 0.086, t[1], t[2]), 0.0285, 0.0215, 40))

    govde_ag = glb.ag("govde", [(mat[k], p) for k, p in gov.items()])

    torpido_aglari = []
    for t in TUPLER:
        govde = sferoid_kapali(t, (1.0, 0.0, 0.0), TORPIDO_BOY / 2, TORPIDO_CAP / 2, 32, 18)
        kanat = Parca()
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            d = (0.0, math.cos(a), math.sin(a))
            e = capraz((1.0, 0.0, 0.0), d)
            orta = ekle((t[0] - 0.088, t[1], t[2]), olc(d, 0.022))
            kanat.birlestir(pahli_kutu(orta, (0.010, 0.0085, 0.0008), 0.0, ((1.0, 0.0, 0.0), d, e)))
        torpido_aglari.append((govde, kanat))

    # Yüzerlik ağları (A7-K25): gövde kutusu A7 uygunluk örneği ve A5 hacmiyle aynı;
    # tüp ağları torpido sferoidinin çokyüzlüsü, hacmi analitik değere ölçeklenmiş.
    buoy_govde = kutu_kapali((-0.20, -0.24, -0.01 - 0.071350 / 2), (0.20, 0.24, -0.01 + 0.071350 / 2))
    hedef_hacim = 4.0 / 3.0 * math.pi * (TORPIDO_BOY / 2) * (TORPIDO_CAP / 2) ** 2
    buoy_tup = []
    for t in TUPLER:
        p = sferoid_kapali(t, (1.0, 0.0, 0.0), TORPIDO_BOY / 2, TORPIDO_CAP / 2, 24, 12)
        V, _ = hacim_merkez(p)
        s = math.sqrt(hedef_hacim / V)
        p.p = [(q[0], t[1] + (q[1] - t[1]) * s, t[2] + (q[2] - t[2]) * s) for q in p.p]
        buoy_tup.append(p)
    col = kutu_kapali((-0.235, -0.290, -0.240), (0.235, 0.290, 0.145))

    dugum = {}
    dugum["REF"] = glb.dugum("REF")
    dugum["govde"] = glb.dugum("govde", govde_ag)
    for k, (g, kn) in enumerate(torpido_aglari, 1):
        dugum["torpido_%d" % k] = glb.dugum("torpido_%d" % k, glb.ag("torpido_%d" % k, [(mat["torpido"], g),
                                                                                     (mat["kanat"], kn)]))
    dugum["BUOY_govde"] = glb.dugum("BUOY_govde", glb.ag("BUOY_govde", [(mat["kopuk"], buoy_govde)]))
    for k, p in enumerate(buoy_tup, 1):
        dugum["BUOY_tup_%d" % k] = glb.dugum("BUOY_tup_%d" % k, glb.ag("BUOY_tup_%d" % k, [(mat["kopuk"], p)]))
    dugum["COL_govde"] = glb.dugum("COL_govde", glb.ag("COL_govde", [(mat["cerceve"], col)]))
    dugum["MNT_kamera0"] = glb.dugum("MNT_kamera0", None, KAMERA)
    dugum["MNT_ping360_0"] = glb.dugum("MNT_ping360_0", None, PING360)
    dugum["MNT_dvl0"] = glb.dugum("MNT_dvl0", None, DVL)
    for k, t in enumerate(TUPLER, 1):
        dugum["TUBE_%d" % k] = glb.dugum("TUBE_%d" % k, None, t)
    # Işıklar: glTF spot ışığı düğümün −Z'sine bakar; +Z_gltf (gövde ileri) için Y etrafında 180°.
    isik = glb.spot_isik("on_isik", (1.0, 0.95, 0.85), 2.5, 10.0, 0.30, 0.62)
    for k, p in enumerate(isik_konumlari):
        glb.dugum("isik_%s" % ("sancak" if p[1] > 0 else "iskele"), None, p, (0.0, 1.0, 0.0, 0.0), isik)

    gorsel = [("govde", "her_zaman"), ("torpido_1", "tup_1_dolu"), ("torpido_2", "tup_2_dolu")]
    kutu_parcalari = list(gov.values()) + [x for tg in torpido_aglari for x in tg] + [col]
    yuzerlik = []
    for ad, kosul, p in (("BUOY_govde", "her_zaman", buoy_govde), ("BUOY_tup_1", "tup_1_dolu", buoy_tup[0]),
                         ("BUOY_tup_2", "tup_2_dolu", buoy_tup[1])):
        assert manifold_mu(p), ad
        V, c = hacim_merkez(p)
        assert V > 0, ad
        yuzerlik.append({"dugum": dugum[ad], "ad": ad, "kosul": kosul, "hacim_m3": round(V, 9),
                         "hacim_merkezi_govde_m": [round(x, 9) + 0.0 for x in c], "en_uzun_kenar_m": round(en_uzun_kenar(p), 9)})
    assert manifold_mu(col) and disbukey_mu(col)
    girdi = {
        "kimlik": "ida0", "tur": "arac", "dosya": "ida0.glb",
        "referans": {"dugum": dugum["REF"], "ad": "REF"},
        "gorsel": [{"dugum": dugum[a], "ad": a, "kosul": k} for a, k in gorsel],
        "yuzerlik": yuzerlik,
        "carpisma": [{"dugum": dugum["COL_govde"], "ad": "COL_govde", "ayristirma": "yok"}],
        "baglanti_noktalari": [
            {"dugum": dugum["MNT_kamera0"], "ad": "MNT_kamera0", "tur": "montaj", "konum_govde_m": list(KAMERA),
             "q_govde": [1.0, 0.0, 0.0, 0.0]},
            {"dugum": dugum["MNT_ping360_0"], "ad": "MNT_ping360_0", "tur": "montaj", "konum_govde_m": list(PING360),
             "q_govde": [1.0, 0.0, 0.0, 0.0]},
            {"dugum": dugum["MNT_dvl0"], "ad": "MNT_dvl0", "tur": "montaj", "konum_govde_m": list(DVL),
             "q_govde": [1.0, 0.0, 0.0, 0.0]},
            {"dugum": dugum["TUBE_1"], "ad": "TUBE_1", "tur": "tup", "konum_govde_m": list(TUPLER[0]),
             "q_govde": [1.0, 0.0, 0.0, 0.0]},
            {"dugum": dugum["TUBE_2"], "ad": "TUBE_2", "tur": "tup", "konum_govde_m": list(TUPLER[1]),
             "q_govde": [1.0, 0.0, 0.0, 0.0]},
        ],
    }
    return girdi, kutu_parcalari, {i: "govde_plastik" for i in range(len(glb.malzemeler))}


# --------------------------------------------------------------------------------------
# Varlık: sabit hedef (bağlı şamandıra)
# --------------------------------------------------------------------------------------

HEDEF_HALAT = 5.75  # şamandıranın altından batırma bloğuna (senaryo: hedef 4 m, taban 10 m)


def hedef(glb):
    mat = {
        "turuncu": glb.malzeme("hedef_turuncu", (0.90, 0.20, 0.015, 1.0), 0.15, 0.4),
        "beyaz": glb.malzeme("hedef_beyaz", (0.80, 0.80, 0.78, 1.0), 0.15, 0.4),
        "celik": glb.malzeme("hedef_celik", (0.45, 0.45, 0.47, 1.0), 0.9, 0.4),
        "halat": glb.malzeme("hedef_halat", (0.35, 0.30, 0.12, 1.0), 0.0, 0.9),
        "beton": glb.malzeme("hedef_beton", (0.30, 0.30, 0.28, 1.0), 0.0, 0.95),
    }
    R = 0.25
    g = {k: Parca() for k in mat}
    bantlar = [(-0.25, -0.12, "turuncu"), (-0.12, 0.12, "beyaz"), (0.12, 0.25, "turuncu")]
    for x0, x1, m in bantlar:
        g[m].birlestir(silindir((x0, 0.0, 0.0), (x1, 0.0, 0.0), R, 64, False, False))
    g["turuncu"].birlestir(kubbe((0.25, 0.0, 0.0), (1.0, 0.0, 0.0), R, 64, 16))
    g["turuncu"].birlestir(kubbe((-0.25, 0.0, 0.0), (-1.0, 0.0, 0.0), R, 64, 16))
    for z, s in ((-R, -1), (R, 1)):  # kaldırma ve bağlama mapaları
        g["celik"].birlestir(boru((-0.035, 0.0, z + s * 0.035), (0.035, 0.0, z + s * 0.035), 0.030, 0.018, 24))
        g["celik"].birlestir(kutu_min_max((-0.01, -0.01, min(z, z + s * 0.01)), (0.01, 0.01, max(z, z + s * 0.01))))
    g["halat"].birlestir(silindir((0.0, 0.0, R + 0.06), (0.0, 0.0, R + HEDEF_HALAT), 0.008, 12))
    g["beton"].birlestir(kutu_min_max((-0.2, -0.2, R + HEDEF_HALAT), (0.2, 0.2, R + HEDEF_HALAT + 0.25), 0.02))
    col = kutu_kapali((-0.5, -0.25, -0.25), (0.5, 0.25, 0.25))
    assert manifold_mu(col) and disbukey_mu(col)
    ref = glb.dugum("REF")
    gv = glb.dugum("hedef", glb.ag("hedef", [(mat[k], p) for k, p in g.items()]))
    cd = glb.dugum("COL_hedef", glb.ag("COL_hedef", [(mat["turuncu"], col)]))
    girdi = {
        "kimlik": "hedef_samandira", "tur": "hedef", "dosya": "hedef_samandira.glb",
        "referans": {"dugum": ref, "ad": "REF"},
        "gorsel": [{"dugum": gv, "ad": "hedef", "kosul": "her_zaman"}],
        "carpisma": [{"dugum": cd, "ad": "COL_hedef", "ayristirma": "yok"}],
    }
    return girdi, list(g.values()) + [col], {i: "hedef_boyali_metal" for i in range(len(glb.malzemeler))}


# --------------------------------------------------------------------------------------
# Varlık: deniz tabanı (statik)
# --------------------------------------------------------------------------------------

TABAN_YARI = 80.0
TABAN_HUCRE = 1.6
HEDEF_TABAN_NE = (14.0, 9.0)  # sahte senaryodaki hedefin kuzey/doğu konumu (düz zemin)


def taban_yukseklik(x, y):
    """Gövde z (aşağı +) yönünde sapma: yumuşak tepecikler; hedefin altında düz."""
    z = 0.0
    for (kx, ky, a, f) in ((0.21, 0.13, 0.35, 0.4), (-0.11, 0.37, 0.18, 1.7), (0.52, -0.29, 0.07, 2.9),
                           (0.05, 0.09, 0.55, 5.1)):
        z += a * math.sin(kx * x + ky * y + f)
    d = math.hypot(x - HEDEF_TABAN_NE[0], y - HEDEF_TABAN_NE[1])
    duz = min(1.0, max(0.0, (d - 2.0) / 5.0))
    duz = duz * duz * (3 - 2 * duz)
    return z * duz


def kum_dokusu(n=512):
    """Döşenebilir kum dokusu: periyodik değer gürültüsü + dalga izleri + benekler (sRGB)."""
    rnd = random.Random(11)

    def izgara(boyut):
        return [[rnd.random() for _ in range(boyut)] for _ in range(boyut)]

    katmanlar = [(izgara(8), 8, 0.45), (izgara(16), 16, 0.25), (izgara(32), 32, 0.18), (izgara(64), 64, 0.12)]

    def deger(iz, g, u, v):
        x, y = u * g, v * g
        x0, y0 = int(math.floor(x)) % g, int(math.floor(y)) % g
        x1, y1 = (x0 + 1) % g, (y0 + 1) % g
        fx, fy = x - math.floor(x), y - math.floor(y)
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a = iz[y0][x0] * (1 - fx) + iz[y0][x1] * fx
        b = iz[y1][x0] * (1 - fx) + iz[y1][x1] * fx
        return a * (1 - fy) + b * fy

    pk = bytearray()
    for j in range(n):
        v = j / n
        for i in range(n):
            u = i / n
            g = sum(deger(iz, gg, u, v) * w for iz, gg, w in katmanlar)
            # dalga izleri (döşenebilir: tamsayı dalga sayıları)
            ripple = 0.5 + 0.5 * math.sin(2 * math.pi * (6 * u + 3 * v) + 3.0 * deger(katmanlar[1][0], 16, u, v))
            b = 0.78 + 0.30 * (g - 0.5) + 0.10 * (ripple - 0.5)
            if deger(katmanlar[3][0], 64, u * 2 % 1.0, v * 2 % 1.0) > 0.86:
                b *= 0.72
            r, gr, bl = 186 * b, 168 * b, 128 * b
            pk += bytes((max(0, min(255, int(r))), max(0, min(255, int(gr))), max(0, min(255, int(bl)))))
    return png_yaz(n, n, bytes(pk))


def deniz_tabani(glb):
    doku = glb.doku_png(kum_dokusu())
    m_kum = glb.malzeme("kum", (1.0, 1.0, 1.0, 1.0), 0.0, 0.95, doku=doku)
    m_kaya = glb.malzeme("kaya", (0.16, 0.14, 0.11, 1.0), 0.0, 0.9)
    m_kaya2 = glb.malzeme("kaya_koyu", (0.07, 0.075, 0.06, 1.0), 0.0, 0.85)
    n = int(2 * TABAN_YARI / TABAN_HUCRE)
    kum = Parca()
    for j in range(n + 1):
        for i in range(n + 1):
            x = -TABAN_YARI + i * TABAN_HUCRE
            y = -TABAN_YARI + j * TABAN_HUCRE
            e = 0.05
            dzdx = (taban_yukseklik(x + e, y) - taban_yukseklik(x - e, y)) / (2 * e)
            dzdy = (taban_yukseklik(x, y + e) - taban_yukseklik(x, y - e)) / (2 * e)
            nrm = birim((dzdx, dzdy, -1.0))  # yukarı (−z) bakan normal
            kum.kose((x, y, taban_yukseklik(x, y)), nrm, (x / 4.0, y / 4.0))
    for j in range(n):
        for i in range(n):
            a = j * (n + 1) + i
            b, c, d = a + 1, a + n + 1, a + n + 2
            # yukarıdan (−z) bakınca saat yönünün tersi: gövde çerçevesinde (x, y) sağ el, z aşağı
            kum.ucgen(a, c, b)
            kum.ucgen(b, c, d)
    # Üçgen yönü denetimi: ilk üçgenin normali yukarı (−z) bakmalı.
    p = kum.p
    nn = capraz(cikar(p[kum.i[1]], p[kum.i[0]]), cikar(p[kum.i[2]], p[kum.i[0]]))
    assert nn[2] < 0, "taban üçgenleri ters"
    rnd = random.Random(3)
    kaya, kaya2 = Parca(), Parca()
    for k in range(34):
        while True:
            x, y = rnd.uniform(-35, 35), rnd.uniform(-35, 35)
            if math.hypot(x - HEDEF_TABAN_NE[0], y - HEDEF_TABAN_NE[1]) > 3.0 and math.hypot(x, y) > 4.0:
                break
        r = rnd.uniform(0.18, 0.9) if k % 5 else rnd.uniform(0.9, 1.6)
        olcek = (rnd.uniform(0.8, 1.4), rnd.uniform(0.8, 1.4), rnd.uniform(0.45, 0.75))
        hedefp = kaya if k % 3 else kaya2
        hedefp.birlestir(ikosfer((x, y, taban_yukseklik(x, y) + 0.25 * r), r, 0.22, rnd, 1, olcek))
    col = kutu_kapali((-TABAN_YARI, -TABAN_YARI, 0.0), (TABAN_YARI, TABAN_YARI, 2.0))
    assert manifold_mu(col) and disbukey_mu(col)
    ref = glb.dugum("REF")
    gv = glb.dugum("taban", glb.ag("taban", [(m_kum, kum)]))
    kv = glb.dugum("kayalar", glb.ag("kayalar", [(m_kaya, kaya), (m_kaya2, kaya2)]))
    cd = glb.dugum("COL_taban", glb.ag("COL_taban", [(m_kum, col)]))
    girdi = {
        "kimlik": "deniz_tabani", "tur": "statik", "dosya": "deniz_tabani.glb",
        "referans": {"dugum": ref, "ad": "REF"},
        "gorsel": [{"dugum": gv, "ad": "taban", "kosul": "her_zaman"},
                   {"dugum": kv, "ad": "kayalar", "kosul": "her_zaman"}],
        "carpisma": [{"dugum": cd, "ad": "COL_taban", "ayristirma": "yok"}],
    }
    return girdi, [kum, kaya, kaya2, col], {m_kum: "taban_kum", m_kaya: "kaya", m_kaya2: "kaya"}


# --------------------------------------------------------------------------------------
# Manifest
# --------------------------------------------------------------------------------------

MALZEMELER = [
    {"kimlik": "govde_plastik", "aciklama": "İDA-0 çerçeve, köpük, tüpler ve iticiler",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.01, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.5,
                 "puruzluluk_gamma_p": 0.5, "kaynak": "tahmin",
                 "dayanak": "750 kHz'te yansıtıcılık bilinmiyor (05 2.4.1); A7 uygunluk örneğiyle aynı yer tutucu"}},
    {"kimlik": "hedef_boyali_metal", "aciklama": "Şamandıra hedef, maplar, halat, batırma bloğu",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.05, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.9,
                 "puruzluluk_gamma_p": 0.3, "kaynak": "tahmin", "dayanak": "yer tutucu; kalibrasyon A5'te"}},
    {"kimlik": "taban_kum", "aciklama": "Kumlu deniz tabanı",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.02, "gelis_yasasi": "cos1", "ayna_yansima_rs": 0.4,
                 "puruzluluk_gamma_p": 0.8, "kaynak": "tahmin", "dayanak": "yer tutucu; kalibrasyon A5'te"}},
    {"kimlik": "kaya", "aciklama": "Taban kayaları",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.04, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.6,
                 "puruzluluk_gamma_p": 0.6, "kaynak": "tahmin", "dayanak": "yer tutucu; kalibrasyon A5'te"}},
]


def sinir_kutusu(parcalar):
    mn = [math.inf] * 3
    mx = [-math.inf] * 3
    for p in parcalar:
        for q in p.p:
            # glTF'e yazılan float32 değeri gövdeye geri çevrilerek (A7-K23 tüketici hesabıyla aynı)
            g = struct.unpack("<3f", struct.pack("<3f", *govde_gltf(q)))
            b = (g[2], -g[0], -g[1])
            for j in range(3):
                mn[j] = min(mn[j], b[j])
                mx[j] = max(mx[j], b[j])
    return {"en_kucuk": [round(x, 6) for x in mn], "en_buyuk": [round(x, 6) for x in mx]}


def uret():
    varliklar = []
    dosyalar = {}
    for kurucu in (ida0, hedef, deniz_tabani):
        glb = Glb()
        girdi, kutu, atama = kurucu(glb)
        veri = glb.yaz()
        dosyalar[girdi["dosya"]] = veri
        tam = {
            "kimlik": girdi["kimlik"], "tur": girdi["tur"], "dosya": girdi["dosya"],
            "sha256": hashlib.sha256(veri).hexdigest(), "bayt": len(veri),
            "referans": girdi["referans"], "sinir_kutusu_govde_m": sinir_kutusu(kutu),
            "malzeme_atamasi": [{"gltf_malzeme": i, "malzeme": m} for i, m in sorted(atama.items())],
            "gorsel": girdi["gorsel"],
        }
        for anahtar in ("yuzerlik", "carpisma", "baglanti_noktalari"):
            if anahtar in girdi:
                tam[anahtar] = girdi[anahtar]
        varliklar.append(tam)
    manifest = {
        "sema_surumu": "1.0",
        "kume_kimligi": "acik_su",
        "kume_surumu": 1,
        "aciklama": ("W3-viz görselleştirici geliştirme kümesi: İDA-0 (BlueROV2 Heavy sınıfı açık çerçeve, iki "
                     "torpido tüpü, sonar başı), bağlı şamandıra hedef ve kumlu deniz tabanı. Geometri "
                     "viz/arac/model-uret/uret.py ile basit şekillerden üretilir; elle çizilmez. Yüzerlik "
                     "gövde kutusu A7 uygunluk örneğindeki 04 yer tutucusuyla aynıdır. Akustik değerler tahmindir. "
                     "Varlık sırası sahte A2 kaynağının varlık indeksleridir (0 araç, 1 hedef, 2 taban)."),
        "birimler": {"uzunluk": "m", "aci": "rad", "kutle": "kg", "zaman": "s"},
        "eksenler": {"gltf": "sag_el_y_yukari_z_on", "gltf_govde": [[0, 0, 1], [-1, 0, 0], [0, -1, 0]],
                     "gltf_godot_yerel": [[-1, 0, 0], [0, 1, 0], [0, 0, -1]]},
        "uretim": {"disa_aktarici": {"arac": "betik", "surum": "1",
                                     "ayarlar": {"betik": "viz/arac/model-uret/uret.py"}}},
        "malzemeler": MALZEMELER,
        "varliklar": varliklar,
    }
    metin = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    dosyalar["manifest.json"] = metin.encode("utf-8")
    return dosyalar


# --------------------------------------------------------------------------------------
# Görselleştirici sınama kümeleri (viz/test/varlik/<küme>/): A7-K20 (sıfır olmayan REF, ayrı
# kökler) ve A7-K53 olumsuz durumları (sol #6, #7). Her küme kendi içinde tutarlıdır (özet ve
# boyut doğru); yalnız hedeflenen kural bozuktur.
# --------------------------------------------------------------------------------------

T_REF = (1.0, 0.0, 0.0)  # glTF koordinatında REF ötelemesi


def _deneme_kume(kume, bozukluk=None):
    glb = Glb()
    m0 = glb.malzeme("deneme_a", (0.2, 0.4, 0.8, 1.0), 0.0, 0.5)
    m1 = glb.malzeme("deneme_b", (0.8, 0.4, 0.2, 1.0), 0.3, 0.6)
    kutu = kutu_min_max((-0.1, -0.05, -0.02), (0.1, 0.05, 0.02))
    col = kutu_kapali((-0.12, -0.06, -0.03), (0.12, 0.06, 0.03))
    ref = glb.dugum("REF")
    gv = glb.dugum("kutu", glb.ag("kutu", [(m0, kutu)]))
    cd = glb.dugum("COL_kutu", glb.ag("COL_kutu", [(m1 if bozukluk == "malzeme" else m0, col)]))
    for d in (ref, gv, cd):  # ayrı kökler, hepsi REF ile aynı yerde (gövde orijininde)
        glb.dugumler[d]["translation"] = list(T_REF)
    if bozukluk == "olcek":
        glb.dugumler[gv]["scale"] = [0.01, 0.01, 0.01]
    if bozukluk == "rol":
        glb.dugum("fazla", glb.ag("fazla", [(m0, kutu_min_max((-0.01,) * 3, (0.01,) * 3))]))
    degistir = None
    if bozukluk == "uri":
        def degistir(j):
            j["buffers"][0]["uri"] = "dis.bin"
    veri = glb.yaz(degistir)
    atama = [{"gltf_malzeme": 0, "malzeme": "deneme_malzeme"}]
    if bozukluk != "malzeme":
        atama.append({"gltf_malzeme": 1, "malzeme": "deneme_malzeme"})
    girdi = {
        "kimlik": "deneme", "tur": "hedef", "dosya": "deneme.glb",
        "sha256": hashlib.sha256(veri).hexdigest(), "bayt": len(veri),
        "referans": {"dugum": ref, "ad": "REF"},
        "sinir_kutusu_govde_m": sinir_kutusu([kutu, col]),
        "malzeme_atamasi": atama,
        "gorsel": [{"dugum": gv, "ad": "kutu", "kosul": "her_zaman"}],
        "carpisma": [{"dugum": cd, "ad": "COL_kutu", "ayristirma": "yok"}],
    }
    manifest = {
        "sema_surumu": "1.0", "kume_kimligi": kume, "kume_surumu": 1,
        "aciklama": "Görselleştirici sınama kümesi (viz/arac/model-uret/uret.py --deneme): " + (bozukluk or "geçerli"),
        "birimler": {"uzunluk": "m", "aci": "rad", "kutle": "kg", "zaman": "s"},
        "eksenler": {"gltf": "sag_el_y_yukari_z_on", "gltf_govde": [[0, 0, 1], [-1, 0, 0], [0, -1, 0]],
                     "gltf_godot_yerel": [[-1, 0, 0], [0, 1, 0], [0, 0, -1]]},
        "uretim": {"disa_aktarici": {"arac": "betik", "surum": "1"}},
        "malzemeler": [{"kimlik": "deneme_malzeme", "optik": {"model": "gltf_metalik_puruzluluk"},
                        "akustik": {"geri_sacilim_mu": 0.01, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.5,
                                    "puruzluluk_gamma_p": 0.5, "kaynak": "tahmin"}}],
        "varliklar": [girdi],
    }
    if bozukluk == "sema":
        manifest["fazla_alan"] = 1
    return {"deneme.glb": veri,
            "manifest.json": (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")}


def deneme_kumeleri():
    return {"deneme_ref": _deneme_kume("deneme_ref"),
            "bozuk_olcek": _deneme_kume("bozuk_olcek", "olcek"),
            "bozuk_uri": _deneme_kume("bozuk_uri", "uri"),
            "bozuk_sema": _deneme_kume("bozuk_sema", "sema"),
            "bozuk_rol": _deneme_kume("bozuk_rol", "rol"),
            "bozuk_malzeme": _deneme_kume("bozuk_malzeme", "malzeme")}


def _yaz_ya_da_denetle(kumeler, denetle):
    hata = 0
    for dizin, dosyalar in kumeler.items():
        for ad, veri in sorted(dosyalar.items()):
            yol = os.path.join(dizin, ad)
            if denetle:
                mevcut = open(yol, "rb").read() if os.path.exists(yol) else None
                if mevcut != veri:
                    print("FARKLI: %s" % yol)
                    hata = 1
                continue
            os.makedirs(dizin, exist_ok=True)
            with open(yol, "wb") as f:
                f.write(veri)
            print("%-48s %9d B  %s" % (os.path.relpath(yol), len(veri), hashlib.sha256(veri).hexdigest()))
    return hata


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    kok = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
    ap.add_argument("--cikti", default=os.path.join(kok, "assets", "acik_su"))
    ap.add_argument("--deneme-cikti", default=os.path.join(kok, "viz", "test", "varlik"))
    ap.add_argument("--denetle", action="store_true")
    a = ap.parse_args()
    kumeler = {a.cikti: uret()}
    for ad, dosyalar in deneme_kumeleri().items():
        kumeler[os.path.join(a.deneme_cikti, ad)] = dosyalar
    hata = _yaz_ya_da_denetle(kumeler, a.denetle)
    if a.denetle:
        print("denetim: %s" % ("farklı" if hata else "aynı"))
    return hata


if __name__ == "__main__":
    sys.exit(main())
