#!/usr/bin/env python3
"""W0-MP5a ölçüm prototipi (atılabilir): ham seri kaydın denetimi ve özeti. Yalnız standart kitaplık.

  python3 -I ozet.py denetle <kayit.txt> --kimlik K [--beklenen beklenen.json]
      Kaydı doğrular; herhangi bir denetim tutmazsa çıkış 1 ve özet basılmaz. Denetimler: kimliği K olan
      açılıştan sonraki her satırın CRC-16'sı (kartın satır CRC'si) ve sıra sayacının kesintisizliği; tek
      açılış; B/B2 (160 MHz, saat ve UART kurulumu başarılı, ICACHE açık); veri kimliği beklenen.json ile aynı;
      her V satırında üç besleme biçiminin doğrulaması geçti; Y satırı; her S satırının H satırları E'deki
      sayıyla; dağılımın adet toplamı = n, en küçük/en büyük/toplam S satırıyla aynı; serilerde ozet_hata,
      hata, bayt_hata, fnv_hata = 0 ve sayac_ok = 1; kodlama serilerinin beklenen FNV'si beklenen.json'la aynı;
      T satırı hata = 0 ve seri sayısı.
  python3 -I ozet.py tablo <kayit.txt> [<kayit2.txt>] --kimlik K [--kimlik2 K2]
      Raporun tablolarını (Markdown) basar; iki kayıt verilirse ikincisiyle farkı da basar.

Yüzdelik: sıralı örneklerde ⌈q·n⌉'inci örnek (en yakın sıra; tam sayı aritmetiği). µs = çevrim / 160.
"Net" değerler, aynı önbellek kipindeki boş ölçüm bölgesinin (iki ardışık CYCCNT okuması) ortancası
çıkarılmış değerlerdir.
"""
import argparse
import json
import math
import sys

HZ = 160_000_000
CYC_US = HZ / 1_000_000


def crc16(b: bytes) -> int:
    crc = 0xFFFF
    for x in b:
        t = (x ^ (crc & 0xFF)) & 0xFF
        t = (t ^ (t << 4)) & 0xFF
        crc = ((crc >> 8) ^ (t << 8) ^ (t << 3) ^ (t >> 4)) & 0xFFFF
    return crc


class Hata(Exception):
    pass


def oku(yol, kimlik):
    """Kimliği verilen açılıştan itibaren satırları doğrular; (satırlar) döndürür."""
    # Açılıştan önceki baytlar (sıfırlama anındaki hat gürültüsü) ASCII olmayabilir; kimlikli açılıştan
    # sonraki her satır kesin ASCII olmalıdır (aşağıda).
    with open(yol, "rb") as f:
        ham = [s.rstrip(b"\n").decode("latin-1") for s in f]
    # kimlikli B satırını bul
    bas = None
    for i, s in enumerate(ham):
        if s.startswith("B ") and f" kimlik={kimlik} " in s:
            if bas is not None:
                raise Hata("ikinci açılış (aynı kimlikle iki B satırı)")
            bas = i
    if bas is None:
        raise Hata(f"kimlik {kimlik} ile açılış satırı yok")
    satirlar = []
    beklenen_sira = None
    for no, s in enumerate(ham[bas:], start=bas + 1):
        if s == "" or s.startswith(" *"):
            continue  # boş satır (açılışın ilk satırı) ya da CRC'li boş satır
        if any(ord(ch) > 127 for ch in s):
            raise Hata(f"satır {no}: ASCII olmayan bayt")
        if " *" not in s:
            raise Hata(f"satır {no}: CRC yok: {s[:60]}")
        govde, crc = s.rsplit(" *", 1)
        if len(crc) != 4 or int(crc, 16) != crc16(govde.encode("ascii")):
            raise Hata(f"satır {no}: CRC tutmuyor")
        p = govde.split(" ")
        tip, sira = p[0], int(p[1])
        if beklenen_sira is not None and sira != beklenen_sira:
            raise Hata(f"satır {no}: sıra {sira}, beklenen {beklenen_sira}")
        beklenen_sira = sira + 1
        kv, ciplak = {}, []
        for t in p[2:]:
            if "=" in t:
                k, v = t.split("=", 1)
                if k in kv:
                    raise Hata(f"satır {no}: yinelenen alan {k}")
                kv[k] = v
            else:
                ciplak.append(t)
        if tip == "B" and satirlar:
            raise Hata(f"satır {no}: ikinci açılış")
        if tip == "X":
            raise Hata(f"satır {no}: kart hata bildirdi: {govde}")
        satirlar.append((tip, sira, kv, ciplak))
    return satirlar


def u(kv, k):
    v = kv[k]
    if not v.isdigit():
        raise Hata(f"{k}={v} sayı değil")
    return int(v)


def seriler(satirlar):
    """S satırlarını, dağılımlarıyla birlikte döndürür (doğrulayarak)."""
    out = []
    i = 0
    while i < len(satirlar):
        tip, _, kv, _ = satirlar[i]
        if tip != "S":
            i += 1
            continue
        sid = u(kv, "id")
        n = u(kv, "n")
        dag = {}
        j = i + 1
        h = 0
        while j < len(satirlar) and satirlar[j][0] == "H":
            _, _, hkv, hc = satirlar[j]
            if u(hkv, "id") != sid:
                raise Hata(f"seri {sid}: H satırı başka seriye ait")
            for t in hc:
                v, c = t.split(":")
                v, c = int(v), int(c)
                if v in dag:
                    raise Hata(f"seri {sid}: değer iki kez")
                dag[v] = c
            h += 1
            j += 1
        if j >= len(satirlar) or satirlar[j][0] != "E":
            raise Hata(f"seri {sid}: E satırı yok")
        ekv = satirlar[j][2]
        if u(ekv, "id") != sid or u(ekv, "h") != h or u(ekv, "farkli") != len(dag):
            raise Hata(f"seri {sid}: E satırı H satırlarıyla tutarsız")
        if sum(dag.values()) != n:
            raise Hata(f"seri {sid}: dağılım toplamı {sum(dag.values())} != n {n}")
        if min(dag) != u(kv, "min") or max(dag) != u(kv, "max"):
            raise Hata(f"seri {sid}: en küçük/en büyük dağılımla tutarsız")
        if sum(v * c for v, c in dag.items()) != u(kv, "top"):
            raise Hata(f"seri {sid}: toplam dağılımla tutarsız")
        out.append({"kv": kv, "dag": dag, "id": sid})
        i = j + 1
    return out


def yuzdelik(dag, q):
    n = sum(dag.values())
    sira = max(1, math.ceil(q * n))  # q·n tam sayıysa ceil aynıdır
    say = 0
    for v in sorted(dag):
        say += dag[v]
        if say >= sira:
            return v
    raise AssertionError


def istatistik(sr):
    d = sr["dag"]
    return {
        "min": min(d),
        "med": yuzdelik(d, 0.5),
        "p999": yuzdelik(d, 0.999),
        "max": max(d),
        "ort": sum(v * c for v, c in d.items()) / sum(d.values()),
    }


def denetle(satirlar, beklenen):
    tipler = [s[0] for s in satirlar]
    if tipler[0] != "B" or tipler[1] != "B2":
        raise Hata("açılış B, B2 ile başlamıyor")
    b, b2 = satirlar[0][2], satirlar[1][2]
    if u(b2, "hesap_hz") != HZ or u(b2, "saat_adim") != 0 or u(b2, "uart_hata") != 0:
        raise Hata(f"B2: saat/UART kurulumu beklenen değil: {b2}")
    if (int(b2["icache_cr"], 16) & 1) != 1:
        raise Hata("B2: ICACHE kapalı")
    if beklenen is not None and b["veri_kimlik"].lower() != "0x" + beklenen["veri_kimlik"].lower():
        raise Hata(f"veri kimliği {b['veri_kimlik']} != beklenen.json {beklenen['veri_kimlik']}")
    v_say = 0
    for tip, _, kv, _ in satirlar:
        if tip == "V":
            v_say += 1
            for bs in ("tek", "p64", "bayt"):
                if kv.get(bs) != "1":
                    raise Hata(f"doğrulama geçmedi: {kv}")
    if v_say != u(b, "ayr_n"):
        raise Hata(f"V satırı {v_say}, beklenen {b['ayr_n']}")
    ys = [s for s in satirlar if s[0] == "Y"]
    if len(ys) != 1:
        raise Hata("Y satırı yok ya da birden çok")
    for k, v in ys[0][2].items():
        if not v.isdigit() or int(v) >= 2**32 - 1:
            raise Hata(f"yığın ölçümü geçersiz: {k}={v}")
    sr = seriler(satirlar)
    bek_kod = {}
    if beklenen is not None:
        bek_kod = {(k["ad"], k["varyant"]): k for k in beklenen["kod"]}
    for x in sr:
        kv = x["kv"]
        for alan in ("ozet_hata", "hata", "bayt_hata", "fnv_hata"):
            if alan in kv and u(kv, alan) != 0:
                raise Hata(f"seri {x['id']}: {alan}={kv[alan]}")
        if kv["tur"] == "ayr" and kv.get("sayac_ok") != "1":
            raise Hata(f"seri {x['id']}: sayaçlar beklenen değil")
        if kv["tur"] == "kod" and bek_kod:
            k = bek_kod[(kv["akis"], kv["varyant"])]
            if kv["fnv_beklenen"].lower() != "0x" + k["fnv"].lower() or u(kv, "bayt") != k["bayt"]:
                raise Hata(f"seri {x['id']}: kodlama referansı beklenen.json'la aynı değil")
    t = [s for s in satirlar if s[0] == "T"]
    if len(t) != 1 or tipler[-1] != "T":
        raise Hata("T satırı yok ya da son satır değil")
    if u(t[0][2], "hata") != 0 or u(t[0][2], "seri") != len(sr):
        raise Hata(f"T: {t[0][2]}, seri {len(sr)}")
    return b, b2, ys[0][2], sr


def anahtar(kv):
    return (kv["tur"], kv.get("akis", "-"), kv.get("varyant", "-"), kv.get("besleme", "-"), kv.get("islem", "-"),
            kv["onbellek"])


def us(c):
    return c / CYC_US


def ms(c):
    return c / HZ * 1000


def dogrusal(noktalar):
    """En küçük kareler: y = a + b·x. (a, b, R², en büyük |artık|)."""
    n = len(noktalar)
    sx = sum(x for x, _ in noktalar)
    sy = sum(y for _, y in noktalar)
    sxx = sum(x * x for x, _ in noktalar)
    sxy = sum(x * y for x, y in noktalar)
    b = (n * sxy - sx * sy) / (n * sxx - sx * sx)
    a = (sy - b * sx) / n
    ort = sy / n
    ss_t = sum((y - ort) ** 2 for _, y in noktalar)
    ss_r = sum((y - a - b * x) ** 2 for x, y in noktalar)
    return a, b, 1 - ss_r / ss_t, max(abs(y - a - b * x) for x, y in noktalar)


def tablo(sr, b, b2, y, sr2=None):
    idx = {anahtar(x["kv"]): x for x in sr}
    kipler = [ob for ob in ("sicak", "soguk", "kapali") if ("bos", "-", "-", "-", "-", ob) in idx]
    bos = {ob: istatistik(idx[("bos", "-", "-", "-", "-", ob)]) for ob in kipler}
    p = print
    p(f"Kayıt: derleme {b['derleme']}, kimlik {b['kimlik']}, veri kimliği {b['veri_kimlik']}, "
      f"yineleme {b['yineleme']}, dialect_hash {b['dialect_hi']}{b['dialect_lo'][2:]}")
    p(f"Kart: idcode {b2['idcode']}, {u(b2, 'hesap_hz')} Hz, flash_acr {b2['flash_acr']}, icache_cr {b2['icache_cr']}")
    p(f"Yapılar: sizeof(alici) {b['sizeof_alici']} B, sizeof(gonderici) {b['sizeof_gonderici']} B, "
      f"sizeof(cerceve) {b['sizeof_cerceve']} B")
    p("Yığın (main'in SP'sinden; ölçüm işlevi + kitaplık, B): " + ", ".join(f"{k} {v}" for k, v in y.items()))
    p()
    p("### Boş ölçüm bölgesi (çevrim)\n")
    p("| Önbellek | Tür | en küçük | ortanca | p99,9 | en büyük |\n|---|---|---:|---:|---:|---:|")
    for ob in kipler:
        for tur in ("bos", "bos_cagri"):
            s = istatistik(idx[(tur, "-", "-", "-", "-", ob)])
            p(f"| {ob} | {tur} | {s['min']} | {s['med']} | {s['p999']} | {s['max']} |")
    p()
    p("### Bütün seriler (çevrim; net = boş bölge ortancası çıkarılmış; µs net)\n")
    p("| Tür | Akış | Varyant | Besleme | İşlem | Önbellek | Bayt | Çerçeve | en küçük | ortanca | p99,9 | en büyük "
      "| net en büyük µs | net ortanca çevrim/B | ısınma |")
    p("|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for x in sr:
        kv = x["kv"]
        if kv["tur"] in ("bos", "bos_cagri") or kv.get("akis", "").startswith("FIT-"):
            continue
        s = istatistik(x)
        o = bos[kv["onbellek"]]["med"]
        bay = u(kv, "bayt")
        p(f"| {kv['tur']} | {kv['akis']} | {kv['varyant']} | {kv.get('besleme', '-')} | {kv['islem']} | "
          f"{kv['onbellek']} | {bay} | {kv['cerceve']} | {s['min']} | {s['med']} | {s['p999']} | {s['max']} | "
          f"{us(s['max'] - o):.2f} | {(s['med'] - o) / bay:.2f} | {kv.get('isinma', '-')} |")
    p()

    def en_kotu(tur, akis, besleme, islem, obler=("sicak", "soguk"), varyantlar=("altin", "ham", "sifir")):
        en = None
        for ob in obler:
            for v in varyantlar:
                x = idx.get((tur, akis, v, besleme, islem, ob))
                if x is None:
                    continue
                s = istatistik(x)
                net = s["max"] - bos[ob]["med"]
                if en is None or net > en[0]:
                    en = (net, ob, v, s, u(x["kv"], "bayt"))
        return en

    p("### Eşikler (en kötü = ürün yapılandırmasında, ICACHE açık: sıcak ve soğuk kiplerin en büyüğü; "
      "üç varyantın en kötüsü; net)\n")
    p("| # | Büyüklük | Ölçülen en kötü | Nerede | Eşik | Sonuç |\n|---|---|---:|---|---:|---|")
    for islem in ("al", "alcoz"):
        for bs in ("tek", "p64", "bayt"):
            if islem == "alcoz" and bs != "tek":
                continue
            e = en_kotu("ayr", "E1", bs, islem)
            p(f"| E1 | E1 akışı ayrıştırma ({islem}, {bs}) | {ms(e[0]):.4f} ms ({e[0]} çevrim) | {e[1]}, {e[2]}, "
              f"{e[4]} B | ≤ 0,20 ms | {'geçti' if ms(e[0]) <= 0.20 else 'GEÇMEDİ'} |")
    k = en_kotu("kod", "KS", "-", "kodla")
    for islem in ("al", "alcoz"):
        for bs in ("tek", "p64", "bayt"):
            if islem == "alcoz" and bs != "tek":
                continue
            e = en_kotu("ayr", "PEN", bs, islem)
            top = e[0] + k[0]
            p(f"| E2 | PEN ayrıştırma ({islem}, {bs}) + KS kodlama | {ms(top):.4f} ms ({e[0]} + {k[0]} çevrim) | "
              f"PEN: {e[1]}, {e[2]}, {e[4]} B; KS: {k[1]}, {k[2]}, {k[4]} B | ≤ 0,50 ms | "
              f"{'geçti' if ms(top) <= 0.50 else 'GEÇMEDİ'} |")
    for bs in ("tek", "p64", "bayt"):
        e = en_kotu("ayr", "BUY", bs, "al", varyantlar=("ham",))
        p(f"| E3 | 258 B çerçeve ayrıştırma (al, {bs}) | {us(e[0]):.2f} µs ({e[0]} çevrim) | {e[1]} | raporlanır | — |")
    e = en_kotu("ayr", "BUY", "tek", "alcoz", varyantlar=("ham",))
    p(f"| E3 | 258 B çerçeve ayrıştırma + çözme (alcoz, tek) | {us(e[0]):.2f} µs ({e[0]} çevrim) | {e[1]} | raporlanır | — |")
    e = en_kotu("kod", "BUYK", "-", "kodla", varyantlar=("ham",))
    p(f"| E3 | 258 B çerçeve kodlama | {us(e[0]):.2f} µs ({e[0]} çevrim) | {e[1]} | raporlanır | — |")
    p()
    if "kapali" in kipler:
        p("### ICACHE kapalı (ürün yapılandırması değil; bilgi için, net en büyük)\n")
        p("| Seri | en kötü (ms) | varyant |\n|---|---:|---|")
        for akis, bs, islem, tur in (("E1", "tek", "al", "ayr"), ("E1", "bayt", "al", "ayr"),
                                     ("PEN", "tek", "al", "ayr"), ("PEN", "bayt", "al", "ayr"),
                                     ("KS", "-", "kodla", "kod")):
            e = en_kotu(tur, akis, bs, islem, obler=("kapali",))
            p(f"| {akis} {islem} {bs} | {ms(e[0]):.4f} | {e[2]} |")
        p()
    # doğrusal uyum
    p("### Doğrusal uyum: tek çerçeve, kırpılmamış, dialect'in 37 mesajı (sıcak, net ortanca; çevrim = a + b·bayt)\n")
    p("| İşlem | a (çerçeve başına, çevrim) | b (bayt başına, çevrim) | R² | en büyük artık (çevrim) | a (µs) |")
    p("|---|---:|---:|---:|---:|---:|")
    o = bos["sicak"]["med"]
    for tur, islem in (("ayr", "al"), ("kod", "kodla")):
        nok = []
        for x in sr:
            kv = x["kv"]
            if kv["tur"] == tur and kv.get("akis", "").startswith("FIT-") and kv["onbellek"] == "sicak":
                nok.append((u(kv, "bayt"), istatistik(x)["med"] - o))
        a, bb, r2, art = dogrusal(nok)
        p(f"| {'ayrıştırma (al, tek)' if tur == 'ayr' else 'kodlama'} ({len(nok)} nokta) | {a:.1f} | {bb:.3f} | "
          f"{r2:.5f} | {art:.1f} | {us(a):.3f} |")
    p()
    p("### Ana akışlarda bayt başına çevrim (sıcak, net ortanca / bayt)\n")
    p("| Akış | Varyant | Bayt | Çerçeve | tek | p64 | bayt | alcoz (tek) |\n|---|---|---:|---:|---:|---:|---:|---:|")
    for akis in ("E1", "PEN", "BUY"):
        for v in ("altin", "ham", "sifir"):
            xs = {bs: idx.get(("ayr", akis, v, bs, "al", "sicak")) for bs in ("tek", "p64", "bayt")}
            xc = idx.get(("ayr", akis, v, "tek", "alcoz", "sicak"))
            if xs["tek"] is None:
                continue
            bay = u(xs["tek"]["kv"], "bayt")
            hucre = [f"{(istatistik(xs[bs])['med'] - o) / bay:.2f}" for bs in ("tek", "p64", "bayt")]
            p(f"| {akis} | {v} | {bay} | {xs['tek']['kv']['cerceve']} | " + " | ".join(hucre) +
              f" | {(istatistik(xc)['med'] - o) / bay:.2f} |")
    for v in ("altin", "ham", "sifir"):
        x = idx.get(("kod", "KS", v, "-", "kodla", "sicak"))
        if x is not None:
            bay = u(x["kv"], "bayt")
            p(f"| KS (kodlama) | {v} | {bay} | {x['kv']['cerceve']} | {(istatistik(x)['med'] - o) / bay:.2f} | | | |")
    x = idx.get(("kod", "BUYK", "ham", "-", "kodla", "sicak"))
    p(f"| BUYK (kodlama) | ham | 258 | 1 | {(istatistik(x)['med'] - o) / 258:.2f} | | | |")
    if sr2 is not None:
        idx2 = {anahtar(x["kv"]): x for x in sr2}
        p()
        p("### İki bağımsız koşunun farkı (koşu 2 − koşu 1; çevrim)\n")
        en_med = en_max = (0, ("fark", "yok"), 0)
        ayni_dagilim = sum(1 for kk, x in idx.items() if kk in idx2 and idx2[kk]["dag"] == x["dag"])
        satir = []
        for kk, x in idx.items():
            x2 = idx2.get(kk)
            if x2 is None:
                raise Hata(f"ikinci koşuda seri yok: {kk}")
            s1, s2 = istatistik(x), istatistik(x2)
            dmed = s2["med"] - s1["med"]
            dmax = s2["max"] - s1["max"]
            if abs(dmed) > abs(en_med[0]):
                en_med = (dmed, kk, s1["med"])
            if abs(dmax) > abs(en_max[0]):
                en_max = (dmax, kk, s1["max"])
            if kk[1] in ("E1", "PEN", "KS", "BUY", "BUYK") and kk[5] in ("sicak", "soguk"):
                satir.append((kk, s1, s2))
        if len(idx2) != len(idx):
            raise Hata("koşuların seri sayıları farklı")
        p("| Seri | ortanca 1 | ortanca 2 | en büyük 1 | en büyük 2 |\n|---|---:|---:|---:|---:|")
        for kk, s1, s2 in satir:
            if kk[2] != "ham" or kk[4] == "alcoz":
                continue
            p(f"| {' '.join(kk[:2] + kk[3:])} | {s1['med']} | {s2['med']} | {s1['max']} | {s2['max']} |")
        p()
        p(f"Bütün serilerde ({len(idx)}) en büyük ortanca farkı: {en_med[0]} çevrim ({' '.join(en_med[1])}; "
          f"koşu 1 ortancası {en_med[2]}); en büyük 'en büyük' farkı: {en_max[0]} çevrim ({' '.join(en_max[1])}; "
          f"koşu 1 en büyüğü {en_max[2]}). Dağılımı (değer:adet) iki koşuda birebir aynı olan seri: "
          f"{ayni_dagilim} / {len(idx)}.")


def deneme(kosular):
    """kosular: [(etiket, b, sr)]; ilk koşu tabandır. Ana serilerin en kötüsü, ortancası ve doğrusal uyum."""
    p = print

    def olcu(sr):
        idx = {anahtar(x["kv"]): x for x in sr}
        bos = {ob: istatistik(idx[("bos", "-", "-", "-", "-", ob)])["med"] for ob in ("sicak", "soguk")}
        m = {}
        for tur, akis, bs, islem in (("ayr", "E1", "tek", "al"), ("ayr", "E1", "p64", "al"), ("ayr", "E1", "bayt", "al"),
                                     ("ayr", "E1", "tek", "alcoz"), ("ayr", "PEN", "tek", "al"),
                                     ("ayr", "PEN", "bayt", "al"), ("ayr", "PEN", "tek", "alcoz"),
                                     ("kod", "KS", "-", "kodla"), ("ayr", "BUY", "tek", "al"),
                                     ("kod", "BUYK", "-", "kodla")):
            en, med = 0, None
            for ob in ("sicak", "soguk"):
                for v in ("altin", "ham", "sifir"):
                    x = idx.get((tur, akis, v, bs, islem, ob))
                    if x is None:
                        continue
                    s = istatistik(x)
                    en = max(en, s["max"] - bos[ob])
                    if ob == "sicak" and v == "ham":
                        med = s["med"] - bos[ob]
            m[(akis, bs, islem)] = (en, med)
        for tur, islem in (("ayr", "al"), ("kod", "kodla")):
            nok = [(u(x["kv"], "bayt"), istatistik(x)["med"] - bos["sicak"]) for x in sr
                   if x["kv"]["tur"] == tur and x["kv"].get("akis", "").startswith("FIT-")
                   and x["kv"]["onbellek"] == "sicak"]
            m[("FIT", tur)] = dogrusal(nok)
        return m

    ms_ = [(et, olcu(sr)) for et, _, sr in kosular]
    p("| Seri (en kötü net, ICACHE açık; ortanca: sıcak, ham) | " + " | ".join(et for et, _ in ms_) + " |")
    p("|---|" + "---:|" * len(ms_))
    for k in ms_[0][1]:
        if k[0] == "FIT":
            continue
        taban = ms_[0][1][k][0]
        hucre = []
        for _, m in ms_:
            en, med = m[k]
            hucre.append(f"{ms(en):.4f} ms ({(en - taban) / taban * 100:+.1f} %); ortanca {med}")
        p(f"| {' '.join(k)} | " + " | ".join(hucre) + " |")
    for tur in ("ayr", "kod"):
        hucre = []
        for _, m in ms_:
            a, bb, r2, _ = m[("FIT", tur)]
            hucre.append(f"a = {a:.0f}, b = {bb:.2f} (R² {r2:.4f})")
        p(f"| doğrusal uyum {'ayrıştırma' if tur == 'ayr' else 'kodlama'} (çevrim) | " + " | ".join(hucre) + " |")
    p()
    for et, m in ms_:
        e1 = max(m[("E1", bs, "al")][0] for bs in ("tek", "p64", "bayt"))
        e1t = m[("E1", "tek", "al")][0]
        e2t = m[("PEN", "tek", "al")][0] + m[("KS", "-", "kodla")][0]
        p(f"- {et}: E1 (tek) {ms(e1t):.4f} ms {'geçti' if ms(e1t) <= 0.2 else 'GEÇMEDİ'}; E1 (en kötü besleme) "
          f"{ms(e1):.4f} ms; E2 (tek) {ms(e2t):.4f} ms {'geçti' if ms(e2t) <= 0.5 else 'GEÇMEDİ'}")


def adimlar(kosular):
    """İyileştirme turu: koşu başına tek satır (net en kötü, ICACHE açık sıcak/soğuk, üç desen)."""
    p = print
    p("| Çeşit | E1 tek (ms) | E1 p64 | E1 bayt | E1 tek+çözme | PEN tek | KS kodlama | E2 tek (ms) "
      "| Ayr. a + b·B (çevrim) | Kod. a + b·B (çevrim) |")
    p("|---|---:|---:|---:|---:|---:|---:|---:|---|---|")
    for et, _, sr in kosular:
        idx = {anahtar(x["kv"]): x for x in sr}
        bos = {ob: istatistik(idx[("bos", "-", "-", "-", "-", ob)])["med"] for ob in ("sicak", "soguk")}

        def en(tur, akis, bs, islem):
            e = 0
            for ob in ("sicak", "soguk"):
                for v in ("altin", "ham", "sifir"):
                    x = idx.get((tur, akis, v, bs, islem, ob))
                    if x is not None:
                        e = max(e, istatistik(x)["max"] - bos[ob])
            return e

        fit = {}
        for tur in ("ayr", "kod"):
            nok = [(u(x["kv"], "bayt"), istatistik(x)["med"] - bos["sicak"]) for x in sr
                   if x["kv"]["tur"] == tur and x["kv"].get("akis", "").startswith("FIT-")
                   and x["kv"]["onbellek"] == "sicak"]
            fit[tur] = dogrusal(nok)
        e1 = en("ayr", "E1", "tek", "al")
        pen = en("ayr", "PEN", "tek", "al")
        ks = en("kod", "KS", "-", "kodla")
        p(f"| {et} | {ms(e1):.4f}{'' if ms(e1) <= 0.2 else ' ✗'} | {ms(en('ayr', 'E1', 'p64', 'al')):.4f} | "
          f"{ms(en('ayr', 'E1', 'bayt', 'al')):.4f} | {ms(en('ayr', 'E1', 'tek', 'alcoz')):.4f} | {ms(pen):.4f} | "
          f"{ms(ks):.4f} | {ms(pen + ks):.4f}{'' if ms(pen + ks) <= 0.5 else ' ✗'} | "
          f"{fit['ayr'][0]:.0f} + {fit['ayr'][1]:.2f} | {fit['kod'][0]:.0f} + {fit['kod'][1]:.2f} |")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("komut", choices=("denetle", "tablo", "deneme", "adimlar"))
    ap.add_argument("kayit", nargs="+")
    ap.add_argument("--kimlik", required=True, help="kayıtların kimlikleri, virgülle (deneme) ya da tek")
    ap.add_argument("--kimlik2")
    ap.add_argument("--beklenen")
    a = ap.parse_args()
    try:
        bek = json.load(open(a.beklenen, encoding="utf-8")) if a.beklenen else None
        if a.komut in ("deneme", "adimlar"):
            kimlikler = a.kimlik.split(",")
            if len(kimlikler) != len(a.kayit):
                raise Hata("kayıt ve kimlik sayıları farklı")
            kosular = []
            for yol, k in zip(a.kayit, kimlikler):
                b, _, _, sr = denetle(oku(yol, k), bek)
                kosular.append((b["derleme"], b, sr))
            (deneme if a.komut == "deneme" else adimlar)(kosular)
            return 0
        b, b2, y, sr = denetle(oku(a.kayit[0], a.kimlik), bek)
        if a.komut == "denetle":
            print(f"DENETİM GEÇTİ: {len(sr)} seri, kimlik {a.kimlik}")
            return 0
        sr2 = None
        if len(a.kayit) > 1:
            _, _, _, sr2 = denetle(oku(a.kayit[1], a.kimlik2 or a.kimlik), bek)
        tablo(sr, b, b2, y, sr2)
        return 0
    except Hata as e:
        print(f"DENETİM GEÇMEDİ: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
