#!/usr/bin/env python3
"""W0-MP4a: PC araçlarının hedefli sınamaları (atılabilir prototip).

Kullanım (depo kökünden): python3 -I tools/olcum/mp4/test/analiz_test.py
Önkoşul: tools/olcum/mp4/derleme/pc/{mp4_seri,mp4_cagri} (derle.sh ya da cmake --build).

Sınananlar:
  - mp4_seri analiz: eksiksiz yapay kampanya geçerli; eksik faz, eksik T, CRC, sıra boşluğu, ikinci açılış,
    X satırı, yanlış kimlik, n ≠ faz_adim, histogram toplamı, E satır sayısı, en-az-adım/süre, min/max
    tutarsızlığı geçersiz; histogram taşması içeren kampanya KAPSAM DIŞI (geçersiz); yüzdelik değerleri elle
    hesaplananla aynı.
  - sol ilk incelemesinin (docs/olcumler/inceleme/MP4-sol.md) olumsuz örnekleri ve ek sınır durumları:
    boş/yanlış faz maskesi, beklenen kampanya uyuşmazlığı, B2 eksik/bozuk, hz=0, S/G/Q şeması, kesin sayı
    ayrıştırması (eksi, taşma, çöp), gecikme histogramı tutarlılığı, sayaç sınırları, süre alt sınırı, T satırı;
    geçersiz kayıtta sayısal özet basılmaması; yüzdelik üst sınırının gerçek sıralı örnekten küçük olmaması.
  - mp4_seri kayit: T satırıyla biter (0), T gelmezse azami sürede 4 döner (sözde uçbirimle).
  - mp4_cagri: erişilen/erişilmeyen çift duyarlıklı çağrı, koşullu kuyruk dallanması, .f64 komutu,
    dolaylı çağrı (3), eksik kök (2).
"""
import os
import pty
import random
import subprocess
import sys
import tempfile
import threading
import time

KOK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERI = os.path.join(KOK, "derleme", "pc", "mp4_seri")
CAGRI = os.path.join(KOK, "derleme", "pc", "mp4_cagri")

basarisiz = 0
gecen = 0


def crc16(s: str) -> int:
    crc = 0xFFFF
    for b in s.encode():
        t = (b ^ (crc & 0xFF)) & 0xFF
        t = (t ^ (t << 4)) & 0xFF
        crc = ((crc >> 8) ^ (t << 8) ^ (t << 3) ^ (t >> 4)) & 0xFFFF
    return crc


def imzala(govde: str) -> str:
    return f"{govde} *{crc16(govde):04X}"


N = 1000          # faz_adim
HZ = 400
PERIYOT = 160_000_000 // HZ
LAT_KABA_N = (PERIYOT + 15) // 16
B2_GOVDE = ("rcc_csr=0x0 idcode=0x30006481 saat_adim=0 hesap_hz=160000000 hal_hz=160000000 cfgr2=0x00000000 "
            "cfgr3=0x00000000 flash_acr=0x00000104 icache_cr=0x00000005 pwr_svmsr=0x00038002 fpscr=0x00000000 "
            "cpacr=0x00F00000 fpccr=0xC0000018 usart_cr1=0x2000000D uart_hata=0")


def faz_satirlari(faz, n=N, ince=None, tasan=0, mx_ek=5):
    """Bir fazın özet ve histogram gövdeleri (sırasız; sıra numarası sonra eklenir)."""
    if ince is None:
        ince = {6000: n - 10, 6001: 9, 8000: 1}
    mn = min(ince) * 16 + 3
    mx = (max(ince) * 16 + mx_ek) if tasan == 0 else 4000000  # 25 ms > 20 ms aralık
    top = 0
    for k, c in ince.items():
        if k == min(ince):
            top += (k * 16 + 3) * c
        elif k == max(ince) and tasan == 0:
            top += (k * 16 + mx_ek) + (k * 16) * (c - 1)
        else:
            top += (k * 16) * c
    top += 4000000 * tasan
    lat_ince = {58: n - 1}
    lat_kaba = {300: 1}
    g = []
    g.append(("F", f"faz={faz} derleme=SINAMA n={n} min={mn} max={mx} max_adim=17 top={top} tasan={tasan} "
                   f"asim=0 asim_dwt=0 atlanan=0 duvar_cyc={(n - 1) * PERIYOT + 10} tik_aralik={n} saglama=0x12345678"))
    g.append(("G", f"faz={faz} lat_min=58 lat_max={300 * 16 + 2} lat_top={58 * (n - 1) + 4802} lat_tasan=0 "
                   f"lat_temiz_n={n} lat_temiz_max=4802 isr_min=20 isr_max=31"))
    g.append(("Q", f"faz={faz} " + " ".join(f"p{p}={1000 * n}/{2000}" for p in range(10))))
    g.append(("S", f"faz={faz} kov=1 skaler=2 sifirlama=3 kapi_gecti=4 kapi_red=5 gec=0 ldl_hata=0 yeniden_hiza=0 "
                   f"crc_hata=0 doyma=0 tasma=0 yigin_kayit=3000 yigin_kayit_tasti=0 yigin_tepe=3500 d_atlanan=0"))
    hs = [("H", f"faz={faz} " + " ".join(f"{k}:{c}" for k, c in sorted(ince.items())))]
    ls = [("L", f"faz={faz} " + " ".join(f"{k}:{c}" for k, c in sorted(lat_ince.items())))]
    ms = [("M", f"faz={faz} " + " ".join(f"{k}:{c}" for k, c in sorted(lat_kaba.items())))]
    g += hs + ls + ms
    g.append(("E", f"faz={faz} h={len(hs)} l={len(ls)} m={len(ms)}"))
    return g


def maske(fazlar):
    return sum(1 << "ABC".index(f) for f in fazlar)


def kampanya(fazlar="ABC", kimlik="K1", n=N, faz_ozel=None, t_var=True):
    govdeler = [("B", f"derleme=SINAMA kimlik={kimlik} hz={HZ} periyot_cyc={PERIYOT} fazlar={maske(fazlar)} "
                      f"faz_adim={n} gecis={HZ} ince_kutu_cyc=16 ince_kutu_n=200000 lat_ince_n=4096 lat_kaba_cyc=16 "
                      f"lat_kaba_n={LAT_KABA_N}"),
                ("B2", B2_GOVDE),
                ("K", "damga_100_cyc=1107"),
                ("K", f"tim2_periyot_cyc={PERIYOT} beklenen={PERIYOT} sizeof_yt=35560 yigin_tepe_acilis=1616")]
    for f in fazlar:
        govdeler.append(("D", f"faz={f} durum=2 n=10 max=1 son=1 asim=0 atlanan=0 lat_max=1 d_atlanan=0"))
        govdeler += (faz_ozel or {}).get(f, faz_satirlari(f, n))
    if t_var:
        govdeler.append(("T", f"adim={2 * HZ + len(fazlar) * (HZ + n)} fazlar={maske(fazlar)} d_atlanan=0"))
    return [imzala(f"{t} {i} {g}") for i, (t, g) in enumerate(govdeler)]


def csv_yaz(satirlar, on=("D 77 eski imaj kırıntısı *0000", "")):
    fd, yol = tempfile.mkstemp(suffix=".csv")
    with os.fdopen(fd, "w") as f:
        f.write("mono_ns,satir\n")
        for i, s in enumerate(list(on) + [imzala("")] + satirlar):
            f.write(f"{i},{s}\n")
    return yol


BEKLENEN = ("--beklenen-hz", str(HZ), "--beklenen-fazlar", "7", "--beklenen-faz-adim", str(N))


YIGIN_AZAMI = "1586416"   # -O2 imajının _estack − (_ebss + 256) değeri (kos.sh ELF'ten hesaplar)


def analiz(satirlar, *ek, on=("D 77 eski imaj kırıntısı *0000", ""), beklenen=BEKLENEN, yigin=YIGIN_AZAMI):
    """Varsayılan beklenti: 400 Hz, A+B+C, faz başına N adım (zorunlu kampanya); yığın üst sınırı -O2 imajınınki."""
    yol = csv_yaz(satirlar, on)
    yg = ("--yigin-azami", yigin) if yigin is not None else ()
    try:
        r = subprocess.run([SERI, "analiz", yol, "--kimlik", "K1", *beklenen, *yg, *ek], capture_output=True, text=True)
    finally:
        os.unlink(yol)
    return r.returncode, r.stdout + r.stderr


def bekle(ad, kosul, cikti=""):
    global basarisiz, gecen
    if kosul:
        gecen += 1
        print(f"  geçti: {ad}")
    else:
        basarisiz += 1
        print(f"  BAŞARISIZ: {ad}\n{cikti}")


def yeniden_imzala(satirlar):
    """Gövdeyi koruyup sıra numaralarını 0'dan yeniden verir ve imzalar (boşluk bırakmadan silme için)."""
    out = []
    for i, s in enumerate(satirlar):
        govde = s[: s.rfind(" *")]
        parca = govde.split(" ", 2)
        out.append(imzala(" ".join([parca[0], str(i)] + parca[2:])))
    return out


def degistir(satirlar, fn):
    """Her gövdeye fn uygular (None dönerse satır silinir), sonra sıraları yeniler ve imzalar."""
    out = []
    for s in satirlar:
        b = fn(s[: s.rfind(" *")])
        if b is not None:
            out.append(imzala(b))
    return yeniden_imzala(out)


def tip_degistir(tip, eski, yeni, faz=None):
    def f(b):
        if b.startswith(tip + " ") and (faz is None or f" faz={faz} " in b + " "):
            return b.replace(eski, yeni, 1)
        return b
    return f


def sayisal_ozet_yok(o):
    return "adım süresi µs" not in o and "tetik gecikmesi" not in o and "p99" not in o


def sinama_analiz():
    print("mp4_seri analiz — temel")
    r = subprocess.run([SERI, "imza", "F 3 faz=A n=1"], capture_output=True, text=True)
    bekle("imza kipi CRC'si Python'la aynı", r.stdout.strip() == imzala("F 3 faz=A n=1"), r.stdout)

    rc, o = analiz(kampanya())
    bekle("eksiksiz kampanya geçerli (0)", rc == 0 and "sonuç: GEÇERLİ" in o, o)
    # Kesin gösterim: p50 → kutu 6000 → kapsayıcı üst kenar 6000·16+15 = 96015 çevrim = 600.09375 µs
    bekle("p50 kutunun kapsayıcı üst kenarı, kesin", "p50 ≤600.09375 " in o, o)
    bekle("p99,9 doğru kutu (sıra 999 → kutu 6001 → 96031 çevrim)", "p99,9 ≤600.19375 " in o, o)
    bekle("p99,99 en büyük değerle kırpılır (128005 çevrim)", "p99,99 ≤800.03125 " in o, o)
    # sol #5: 58 çevrim = 0.3625 µs; üst sınır aşağı yuvarlanmamalı
    bekle("gecikme p99,9 = 58 çevrim = 0.36250 µs (aşağı yuvarlanmaz)", "p99,9 ≤0.36250 " in o and "≤0.362 " not in o, o)
    bekle("süre alt sınırı ve son adım dahil aralık yazılır",
          "süre alt sınırı (ilk adım başı → son adım başı) 2.497500 s" in o
          and "son adım dahil süre [2.498100, 2.498301] s" in o, o)

    s = kampanya()
    rc, o = analiz(s[: s.index(next(x for x in s if x.startswith("F ") and "faz=C" in x)) - 1])
    bekle("C fazı ve T eksik → geçersiz", rc == 1 and "faz C" in o and "T) satırı yok" in o, o)
    rc, o = analiz(kampanya(t_var=False))
    bekle("T satırı eksik → geçersiz", rc == 1 and "T) satırı yok" in o, o)

    s = kampanya()
    i = next(k for k, x in enumerate(s) if x.startswith("Q "))
    s[i] = s[i].replace("p3=", "p3=9", 1)
    rc, o = analiz(s)
    bekle("CRC bozuk → geçersiz", rc == 1 and "CRC" in o, o)

    s = kampanya()
    del s[next(k for k, x in enumerate(s) if x.startswith("D "))]
    rc, o = analiz(s)
    bekle("satır kaybı (sıra boşluğu) → geçersiz", rc == 1 and "sıra boşluğu" in o, o)

    s = kampanya()
    s.insert(10, imzala("B 10 derleme=SINAMA kimlik=K1 hz=400"))
    rc, o = analiz(yeniden_imzala(s))
    bekle("ikinci açılış → geçersiz", rc == 1 and "yeniden açılış" in o, o)

    s = kampanya()
    s.insert(12, imzala("X 12 neden=SAAT_HATASI"))
    rc, o = analiz(yeniden_imzala(s))
    bekle("X satırı → geçersiz", rc == 1 and "kart hata satırı" in o, o)

    rc, o = analiz(kampanya(kimlik="BASKA"))
    bekle("kimlik uyuşmazlığı → geçersiz", rc == 1 and "kimliği K1" in o, o)

    ozel = {"B": [(t, g.replace(f"n={N} ", f"n={N - 1} ", 1) if t == "F" else g) for t, g in faz_satirlari("B")]}
    rc, o = analiz(kampanya(faz_ozel=ozel))
    bekle("n ≠ faz_adim → geçersiz", rc == 1 and "≠ faz_adim" in o, o)

    ozel = {"A": faz_satirlari("A", ince={6000: N - 10, 6001: 8, 8000: 1})}
    ozel["A"] = [(t, g.replace(f"n={N - 1} ", f"n={N} ") if t == "F" else g) for t, g in ozel["A"]]
    rc, o = analiz(kampanya(faz_ozel=ozel))
    bekle("histogram toplamı ≠ n → geçersiz", rc == 1 and "histogramı toplamı" in o, o)

    s = kampanya()
    del s[next(k for k, x in enumerate(s) if x.startswith("L "))]
    rc, o = analiz(yeniden_imzala(s))
    bekle("histogram satırı eksik (sıra kesintisiz) → geçersiz", rc == 1 and "E satırıyla uyuşmuyor" in o, o)

    rc, o = analiz(degistir(kampanya(), tip_degistir("F", "min=96003", "min=96100", "A")))
    bekle("en küçük değer histogramla tutarsız → geçersiz", rc == 1 and "en küçük değer" in o, o)

    rc, o = analiz(kampanya(), "--en-az-adim", "240000")
    bekle("en-az-adım sağlanmadı → geçersiz", rc == 1 and "< 240000" in o, o)
    rc, o = analiz(kampanya(), "--en-az-sn", "600")
    bekle("en-az-süre (alt sınırla) sağlanmadı → geçersiz", rc == 1 and "kayıt süresi alt sınırı" in o, o)

    ozel = {"B": faz_satirlari("B", ince={6000: N - 5}, tasan=5)}
    rc, o = analiz(kampanya(faz_ozel=ozel))
    bekle("kısmi taşma (5 örnek 20 ms üstü) → KAPSAM DIŞI, geçersiz (önceki 'geçerli, belirlenemedi' testi kaldırıldı)",
          rc == 1 and "KAPSAM DIŞI" in o and sayisal_ozet_yok(o), o)

    b_bek = ("--beklenen-hz", "400", "--beklenen-fazlar", "2", "--beklenen-faz-adim", str(N))
    rc, o = analiz(kampanya(fazlar="B"), beklenen=b_bek)
    bekle("tek fazlı kampanya (yalnız B), açıkça beklenince geçerli", rc == 0, o)
    s = kampanya(fazlar="B")
    rc, o = analiz(s[:-1] + [imzala(f"F {len(s) - 1} faz=A n=1")] +
                   [imzala(f"T {len(s)} adim=1 fazlar=2 d_atlanan=0")], beklenen=b_bek)
    bekle("beklenmeyen faz satırı → geçersiz", rc == 1 and "beklenmeyen faz" in o, o)


def sinama_sol():
    """sol ilk incelemesinin bağımsız olumsuz örnekleri (MP4-sol.md) ve ek sınır durumları."""
    print("mp4_seri analiz — sol olumsuz örnekleri ve sınır durumları")
    base = kampanya()
    vakalar = {
        # sol #1
        "boş faz maskesi (fazlar=0), --en-az ile": (kampanya(fazlar=""), ("--en-az-adim", "240000", "--en-az-sn", "600"), "faz maskesi"),
        "B2 yok": (degistir(base, lambda b: None if b.startswith("B2 ") else b), (), "B2"),
        "B2 saat/UART hatalı (saat_adim=7 hesap_hz=80000000 uart_hata=1)": (
            degistir(base, tip_degistir("B2", "saat_adim=0 hesap_hz=160000000", "saat_adim=7 hesap_hz=80000000")),
            (), "saat"),
        "yalnız uart_hata=1": (degistir(base, tip_degistir("B2", "uart_hata=0", "uart_hata=1")), (), "UART"),
        "hz=0": (degistir(base, tip_degistir("B", "hz=400 ", "hz=0 ")), (), "hz"),
        "iki B2": (yeniden_imzala(base[:2] + [imzala("B2 2 " + B2_GOVDE)] + base[2:]), (), "B2"),
        "TIM2 periyodu DWT ile uyuşmuyor": (degistir(base, tip_degistir("K", f"tim2_periyot_cyc={PERIYOT}", "tim2_periyot_cyc=399000")), (), "TIM2"),
        "T toplam adım yanlış": (degistir(base, tip_degistir("T", "adim=", "adim=1")), (), "toplam adım"),
        # sol #2
        "S ölçüm alanları silinmiş": (degistir(base, lambda b: " ".join(b.split()[:3]) if b.startswith("S ") else b), (), "alan yok"),
        "Q p0=nan/nan": (degistir(base, tip_degistir("Q", "p0=1000000/2000", "p0=nan/nan")), (), "parça alanı bozuk"),
        "G lat_max=0 (histogramla çelişki)": (degistir(base, tip_degistir("G", "lat_max=4802", "lat_max=0")), (), "lat_"),
        "G lat_min > lat_max": (degistir(base, tip_degistir("G", "lat_min=58", "lat_min=5000")), (), "lat_min > lat_max"),
        "G lat_top histogramla çelişiyor": (degistir(base, tip_degistir("G", f"lat_top={58 * (N - 1) + 4802}", "lat_top=58000")), (), "lat_top"),
        "G temiz en büyük > lat_max": (degistir(base, tip_degistir("G", "lat_temiz_max=4802", "lat_temiz_max=9999")), (), "temiz"),
        "G alan eksik (isr_max)": (degistir(base, tip_degistir("G", " isr_max=31", "")), (), "isr_max"),
        "eksi sayı (F n=-1000)": (degistir(base, tip_degistir("F", f"n={N} ", f"n=-{N} ")), (), "sayı değil"),
        "taşan sayı (F top=99999999999999999999999)": (degistir(base, tip_degistir("F", "top=", "top=99999999999999999999999 x=", "A")), (), "taşıyor"),
        "Q sayı sonrası çöp (p0=1000000x/2000)": (degistir(base, tip_degistir("Q", "p0=1000000/2000", "p0=1000000x/2000")), (), "parça alanı bozuk"),
        "Q parça toplamı > adım toplamı": (degistir(base, lambda b: b if not b.startswith("Q ") else
                                          " ".join(w if not w.startswith("p") else w.split("=")[0] + "=9700000/12000" for w in b.split())), (), "adım süresi toplamından büyük"),
        "S yığın ölçümü taştı": (degistir(base, tip_degistir("S", "yigin_kayit_tasti=0", "yigin_kayit_tasti=1")), (), "yığın ölçüm bölgesi taştı"),
        "S crc_hata ≠ 0": (degistir(base, tip_degistir("S", "crc_hata=0", "crc_hata=3")), (), "CRC hatası 0 değil"),
        "F aşım > n": (degistir(base, tip_degistir("F", "asim=0 ", f"asim={N + 1} ")), (), "aşım"),
        "F duvar_cyc tetik aralığıyla uyuşmuyor": (degistir(base, tip_degistir("F", f"duvar_cyc={(N - 1) * PERIYOT + 10}", "duvar_cyc=1")), (), "duvar_cyc"),
        "F tanımsız alan": (degistir(base, tip_degistir("F", "asim=0 ", "asim=0 fazladan=1 ")), (), "tanımsız alan"),
        "yinelenen alan": (degistir(base, tip_degistir("F", "asim=0 ", "asim=0 asim=0 ")), (), "biçim"),
        "F max_adim ≥ n": (degistir(base, tip_degistir("F", "max_adim=17", f"max_adim={N}")), (), "max_adim"),
        "H yinelenen kutu": (degistir(base, tip_degistir("H", "6000:", "6001:5 6000:", "A")), (), "histogram"),
        "H sıfır sayılı kutu": (degistir(base, tip_degistir("H", "6000:", "7000:0 6000:", "A")), (), "histogram sözcüğü bozuk"),
        "M kaba kutu ince aralıkta": (degistir(base, tip_degistir("M", "300:1", "100:1")), (), "kaba gecikme kutusu ince aralıkta"),
        # beklenen kampanya
        "beklenen faz_adim uyuşmuyor": (base, (), "faz_adim"),
    }
    for ad, (satir, ek, ipucu) in vakalar.items():
        bek = BEKLENEN if ad != "beklenen faz_adim uyuşmuyor" else \
            ("--beklenen-hz", "400", "--beklenen-fazlar", "7", "--beklenen-faz-adim", "241000")
        rc, o = analiz(satir, *ek, beklenen=bek)
        bekle(f"{ad} → geçersiz, sayısal özet yok", rc == 1 and ipucu in o and sayisal_ozet_yok(o), f"rc={rc}\n{o}")

    rc, o = analiz(kampanya(fazlar="B"))
    bekle("zorunlu A+B+C beklenirken imajın bildirdiği yalnız-B maskesi kampanyayı daraltamaz",
          rc == 1 and "faz maskesi 2 ≠ beklenen 7" in o and sayisal_ozet_yok(o), o)
    for eksik in ("--beklenen-hz", "--beklenen-fazlar", "--beklenen-faz-adim"):
        bek = list(BEKLENEN)
        i = bek.index(eksik)
        del bek[i:i + 2]
        rc, o = analiz(kampanya(), beklenen=tuple(bek))
        bekle(f"{eksik} verilmezse kullanım hatası (2)", rc == 2, o)


def sinama_yuzdelik_ozellik():
    """Rastgele histogramlar: basılan üst sınır ≥ gerçek sıralı örnek ve ≤ onun kutusunun üst kenarı."""
    print("mp4_seri analiz — yüzdelik özelliği (rastgele)")
    rnd = random.Random(20261118)
    hata = 0
    for deneme in range(25):
        n = rnd.randint(1, 3000)
        ornek = sorted(rnd.choice([rnd.randint(95000, 96000), rnd.randint(200000, 214000)]) for _ in range(n))
        ince = {}
        for v in ornek:
            ince[v // 16] = ince.get(v // 16, 0) + 1
        mn, mx, top = ornek[0], ornek[-1], sum(ornek)
        sat = faz_satirlari("A", n=n)
        sat = [(t, g) for t, g in sat if t not in ("F", "H", "E")]
        sat.insert(0, ("F", f"faz=A derleme=SINAMA n={n} min={mn} max={mx} max_adim=0 top={top} tasan=0 asim=0 "
                            f"asim_dwt=0 atlanan=0 duvar_cyc={(n - 1) * PERIYOT + 10} tik_aralik={n} saglama=0x1"))
        hs = []
        kutular = sorted(ince.items())
        for i in range(0, len(kutular), 16):
            hs.append(("H", "faz=A " + " ".join(f"{k}:{c}" for k, c in kutular[i:i + 16])))
        # Q toplamları n'e göre
        sat = [(t, "faz=A " + " ".join(f"p{p}=0/0" for p in range(10)) if t == "Q" else g) for t, g in sat]
        sat = sat[:4] + hs + sat[4:] + [("E", f"faz=A h={len(hs)} l=1 m=1")]
        satir = kampanya(fazlar="A", n=n, faz_ozel={"A": sat})
        rc, o = analiz(satir, beklenen=("--beklenen-hz", "400", "--beklenen-fazlar", "1", "--beklenen-faz-adim", str(n)))
        if rc != 0:
            hata += 1
            print(o)
            break
        satir_o = next(x for x in o.splitlines() if x.startswith("adım süresi"))
        for etiket, pay, payda in (("p50", 1, 2), ("p99", 99, 100), ("p99,9", 999, 1000), ("p99,99", 9999, 10000)):
            sira = -(-n * pay // payda)
            gercek = ornek[sira - 1]
            ust_kenar = min((gercek // 16) * 16 + 15, mx)
            deger = satir_o.split(f"{etiket} ≤")[1].split(" ")[0]
            tam, kesir = deger.split(".")
            cyc = int(tam) * 160 + int(kesir) // 625
            if not (gercek <= cyc <= ust_kenar) or int(kesir) % 625:
                hata += 1
                print(f"deneme {deneme} {etiket}: gerçek {gercek}, basılan {deger} ({cyc}), üst kenar {ust_kenar}")
    bekle("25 rastgele histogramda yüzdelik üst sınırı gerçek sıralı örnekten küçük değil, kutusunu aşmıyor", hata == 0)


def sinama_kayit():
    print("mp4_seri kayit (sözde uçbirim)")
    for t_gonder, beklenen_rc in ((True, 0), (False, 4)):
        ana, yan = pty.openpty()
        ad = os.ttyname(yan)
        d = tempfile.mkdtemp()
        cikti = os.path.join(d, "k.csv")
        p = subprocess.Popen([SERI, "kayit", ad, "3", cikti], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

        def yaz():
            time.sleep(0.5)
            for s in kampanya()[:5]:
                os.write(ana, (s + "\r\n").encode())
            if t_gonder:
                os.write(ana, (imzala("T 5 adim=1 fazlar=7 d_atlanan=0") + "\r\n").encode())

        th = threading.Thread(target=yaz)
        th.start()
        t0 = time.time()
        out, _ = p.communicate(timeout=10)
        th.join()
        sure = time.time() - t0
        os.close(ana)
        os.close(yan)
        satir = open(cikti).read().count("\n")
        ad_ = "T gelince erken biter (0)" if t_gonder else "T gelmezse azami sürede EKSİK (4)"
        bekle(ad_, p.returncode == beklenen_rc and (sure < 2.5 if t_gonder else sure >= 2.9) and satir >= 6,
              f"rc={p.returncode} süre={sure:.1f} satır={satir}\n{out}")


def sinama_cagri():
    print("mp4_cagri")

    def od(govde):
        return "\nmp4.elf:     file format elf32-littlearm\n\nDisassembly of section .text:\n\n" + govde

    kok = ("08000100 <yt_adim>:\n"
           " 8000100:\tb580      \tpush\t{r7, lr}\n"
           " 8000102:\tf000 f801 \tbl\t8000108 <ara>\n"
           " 8000106:\tbd80      \tpop\t{r7, pc}\n\n")
    ara_temiz = ("08000108 <ara>:\n"
                 " 8000108:\tee20 0a01 \tvmul.f32\ts0, s0, s2\n"
                 " 800010c:\t4770      \tbx\tlr\n\n")
    erisilmez = ("08000200 <baska>:\n"
                 " 8000200:\tf000 f900 \tbl\t8000400 <__aeabi_dmul>\n\n"
                 "08000400 <__aeabi_dmul>:\n"
                 " 8000400:\t4770      \tbx\tlr\n\n")
    vakalar = [
        ("temiz; erişilmeyen __aeabi_dmul sayılmaz → 0", kok + ara_temiz + erisilmez, 0),
        ("erişilen __aeabi_dmul → 1", kok + ("08000108 <ara>:\n"
                                           " 8000108:\tf000 f97a \tbl\t8000400 <__aeabi_dmul>\n"
                                           " 800010c:\t4770      \tbx\tlr\n\n") + erisilmez, 1),
        ("koşullu kuyruk dallanması bne.w → __aeabi_dadd → 1", kok + ("08000108 <ara>:\n"
                                                                    " 8000108:\tf47f aefa \tbne.w\t8000500 <__aeabi_dadd>\n"
                                                                    " 800010c:\t4770      \tbx\tlr\n\n"
                                                                    "08000500 <__aeabi_dadd>:\n"
                                                                    " 8000500:\t4770      \tbx\tlr\n\n"), 1),
        ("alt çizgili çift duyarlıklı libm (__kernel_rem_pio2) → 1", kok + ("08000108 <ara>:\n"
                                                                          " 8000108:\tf000 f97a \tbl\t8000600 <__kernel_rem_pio2>\n\n"
                                                                          "08000600 <__kernel_rem_pio2>:\n"
                                                                          " 8000600:\t4770      \tbx\tlr\n\n"), 1),
        (".f64 komutu → 1", kok + ("08000108 <ara>:\n"
                                  " 8000108:\teeb7 0bc0 \tvcvt.f64.f32\td0, s0\n"
                                  " 800010c:\t4770      \tbx\tlr\n\n"), 1),
        ("dolaylı çağrı blx r3 → 3 (kanıtlanamadı)", kok + ("08000108 <ara>:\n"
                                                           " 8000108:\t4798      \tblx\tr3\n"
                                                           " 800010a:\t4770      \tbx\tlr\n\n"), 3),
    ]
    for ad, metin, beklenen in vakalar:
        fd, yol = tempfile.mkstemp(suffix=".txt")
        with os.fdopen(fd, "w") as f:
            f.write(od(metin))
        r = subprocess.run([CAGRI, yol, "yt_adim"], capture_output=True, text=True)
        bekle(ad, r.returncode == beklenen, f"rc={r.returncode}\n{r.stdout}{r.stderr}")
        r2 = subprocess.run([CAGRI, yol, "yok_boyle"], capture_output=True, text=True)
        os.unlink(yol)
    bekle("eksik kök sembol → 2", r2.returncode == 2, r2.stdout + r2.stderr)


def uzun_adim_zamani(n, adim_cyc):
    """Periyottan uzun adımlar için tutarlı duvar_cyc / tik_aralik / atlanan (adımlar arka arkaya koşar)."""
    duvar = (n - 1) * adim_cyc + 10
    tik_aralik = duvar // PERIYOT + 1
    return f"atlanan={tik_aralik - n} duvar_cyc={duvar} tik_aralik={tik_aralik}"


def faz_tasmis(faz, n=N, mn=3300000, mx=4000000, top=None):
    """Bütün örnekleri 20 ms aralığının dışında (taşmış) bir faz; H satırı yok."""
    if top is None:
        top = mn + mx + 3500000 * (n - 2)
    sat = [(t, g) for t, g in faz_satirlari(faz, n) if t not in ("F", "H", "E")]
    sat.insert(0, ("F", f"faz={faz} derleme=SINAMA n={n} min={mn} max={mx} max_adim=17 top={top} tasan={n} "
                        f"asim={n} asim_dwt={n} {uzun_adim_zamani(n, mx)} saglama=0x1"))
    # her adım periyottan uzun → aşım n; G/Q tutarlı kalsın
    sat.append(("E", f"faz={faz} h=0 l=1 m=1"))
    return sat


def sinama_ikinci():
    """sol ikinci incelemesi (MP4-sol-2.md) Y1–Y3 hedefli testleri."""
    print("mp4_seri analiz — sol ikinci incelemesi Y1–Y3")
    base = kampanya()
    # ---- Y1 ----
    rc, o = analiz(degistir(base, lambda b: None if (b.startswith("H ") and " faz=A " in b + " ") else
                            (b.replace(" h=1 ", " h=0 ") if (b.startswith("E ") and " faz=A " in b + " ") else b)))
    bekle("Y1: eksik H tablosu (taşma yok) → geçersiz", rc == 1 and "histogram" in o and sayisal_ozet_yok(o), o)
    # KAPSAM DARALTMASI (2026-11-18, docs/gorevler/W0-MP4a-kapsam-daraltma.md): taşmalı kampanya desteklenmez.
    # Önceki "geçerli-taşmalı" başarı testleri kaldırıldı; aynı girdiler artık KAPSAM DIŞI olarak reddedilmeli.
    def kapsam_disi(ad, satir, beklenen=BEKLENEN, ek=()):
        rc, o = analiz(satir, *ek, beklenen=beklenen)
        bekle(f"KAPSAM: {ad} → geçersiz (KAPSAM DIŞI), sayısal özet yok", rc == 1 and "KAPSAM DIŞI" in o and sayisal_ozet_yok(o), o)

    kapsam_disi("bütün örnekler taşmış, kendi içinde tutarlı (tasan=n)", kampanya(faz_ozel={"A": faz_tasmis("A")}))
    kapsam_disi("Y1 sol karşı örneği (tasan=n, min=96003, top=96035119)",
                kampanya(faz_ozel={"A": faz_tasmis("A", mn=96003, top=96035119)}))
    kapsam_disi("tasan=n, toplam n·3.200.000'den küçük", kampanya(faz_ozel={"A": faz_tasmis("A", top=3200000 * N - 1)}))
    ust = 200000 * 16  # 20 ms aralık üstü (çevrim)
    sinir = faz_satirlari("A", ince={199999: N - 1}, tasan=1)
    sinir = [(t, g.replace("max=4000000", f"max={ust}").replace(
        f"top={(199999 * 16 + 3) * (N - 1) + 4000000}", f"top={(199999 * 16 + 3) * (N - 1) + ust}")
        .replace("asim=0 asim_dwt=0 ", f"asim={N} asim_dwt={N} ")
        .replace(f"atlanan=0 duvar_cyc={(N - 1) * PERIYOT + 10} tik_aralik={N}", uzun_adim_zamani(N, ust)))
        if t == "F" else (t, g) for t, g in sinir]
    kapsam_disi("tek taşan örnek tam aralık üstünde (tasan=1, max=3.200.000)", kampanya(faz_ozel={"A": sinir}))
    kapsam_disi("tek taşan örnek, max aralığın son çevriminde (tasan=1)",
                kampanya(faz_ozel={"A": [(t, g.replace(f"max={ust}", f"max={ust - 1}")) if t == "F" else (t, g) for t, g in sinir]}))
    # Y5 karşı örnekleri (MP4-sol-3.md): n = tasan = 241000; iki olanaksız uç ve iki sınır toplamı — hepsi reddedilir
    for top in (795300000000, 964000000000, 795300700000, 963999300000):
        kapsam_disi(f"Y5 tasan=n=241000, top={top} (sol'un beklentileriyle)",
                    kampanya(n=241000, faz_ozel={"A": faz_tasmis("A", n=241000, top=top)}),
                    beklenen=("--beklenen-hz", "400", "--beklenen-fazlar", "7", "--beklenen-faz-adim", "241000"),
                    ek=("--en-az-adim", "240000", "--en-az-sn", "600"))
    # gecikme histogramı taşması
    kapsam_disi("gecikme histogramı taşması (lat_tasan=1)",
                degistir(base, lambda b: (b.replace("lat_tasan=0", "lat_tasan=1") if b.startswith("G ") and " faz=B " in b + " "
                                          else (b.replace(f"58:{N - 1}", f"58:{N - 2}") if b.startswith("L ") and " faz=B " in b + " " else b))))
    # aralığın içindeki sınır: en büyük değer son kutuda, taşma yok → desteklenir
    ic = faz_satirlari("A", ince={199999: N})
    ic = [(t, g.replace("asim=0 asim_dwt=0 ", f"asim={N} asim_dwt={N} ")
           .replace(f"atlanan=0 duvar_cyc={(N - 1) * PERIYOT + 10} tik_aralik={N}", uzun_adim_zamani(N, 199999 * 16 + 5)))
          if t == "F" else (t, g) for t, g in ic]
    rc, o = analiz(kampanya(faz_ozel={"A": ic}))
    bekle("KAPSAM: taşmasız, en büyük değer 20 ms aralığının son kutusunda → geçerli", rc == 0 and "20ms_ustu 0" in o, o)
    rc, o = analiz(kampanya(faz_ozel={"A": [(t, g.replace(f"max={199999 * 16 + 5}", f"max={ust - 1}")) if t == "F" else (t, g) for t, g in ic]}))
    bekle("KAPSAM: taşmasız, en büyük = 3.199.999 çevrim (aralığın son çevrimi) → geçerli", rc == 0 and "maks 19999.99375" in o, o)
    rc, o = analiz(kampanya(faz_ozel={"A": [(t, g.replace(f"max={199999 * 16 + 5}", f"max={ust}")) if t == "F" else (t, g) for t, g in ic]}))
    bekle("KAPSAM: tasan=0 ama en büyük = 3.200.000 çevrim (aralık dışı) → geçersiz", rc == 1 and "KAPSAM DIŞI" in o, o)
    rc, o = analiz(base)
    bekle("Y1: taşmasız olağan kampanya geçerli kalır", rc == 0 and "sonuç: GEÇERLİ" in o, o)
    # ---- Y2 ----
    for ad, eski, yeni, ipucu in (
            ("yigin_tepe=4294967295", "yigin_tepe=3500", "yigin_tepe=4294967295", "boyanabilen bölgesinden büyük"),
            ("yigin_tepe=1, yigin_kayit=3000", "yigin_tepe=3500", "yigin_tepe=1", "kapsama aykırı"),
            ("yigin_tepe = yigin_kayit", "yigin_tepe=3500", "yigin_tepe=3000", "kapsama aykırı"),
            ("yigin_tepe = sınır + 1", "yigin_tepe=3500", f"yigin_tepe={int(YIGIN_AZAMI) + 1}", "boyanabilen bölgesinden büyük"),
            ("yigin_kayit = 33281", "yigin_kayit=3000", "yigin_kayit=33281", "yigin_kayit aralık dışı")):
        rc, o = analiz(degistir(base, tip_degistir("S", eski, yeni, "B")))
        bekle(f"Y2: {ad} → geçersiz", rc == 1 and ipucu in o and sayisal_ozet_yok(o), o)
    rc, o = analiz(degistir(base, tip_degistir("S", "yigin_tepe=3500", f"yigin_tepe={YIGIN_AZAMI}", "B")))
    bekle("Y2: yigin_tepe tam sınırda → geçerli", rc == 0, o)
    rc, o = analiz(base, yigin=None)
    bekle("Y2: --yigin-azami verilmezse kullanım hatası (2)", rc == 2, o)
    rc, o = analiz(base, yigin="3400")
    bekle("Y2: sınır kayıttan değil imajdan gelir; daha dar imaj sınırı (3400 B) kaydı reddeder",
          rc == 1 and "boyanabilen" in o, o)
    # ---- Y3 ----
    rc, o = analiz(degistir(base, tip_degistir("G", "lat_temiz_max=4802", "lat_temiz_max=59", "A")))
    bekle("Y3: bütün küme temiz (lat_temiz_n=n) ama temiz maks < genel maks → geçersiz",
          rc == 1 and "genel en büyüğe eşit değil" in o and sayisal_ozet_yok(o), o)
    rc, o = analiz(degistir(base, tip_degistir("G", f"lat_temiz_n={N} lat_temiz_max=4802", f"lat_temiz_n={N - 1} lat_temiz_max=58", "A")))
    bekle("Y3: alt küme temiz (n−1), temiz maks < genel maks → geçerli", rc == 0, o)
    rc, o = analiz(degistir(base, tip_degistir("G", f"lat_temiz_n={N} lat_temiz_max=4802", "lat_temiz_n=0 lat_temiz_max=0", "A")))
    bekle("Y3: temiz örnek yok (0, 0) → geçerli", rc == 0, o)
    rc, o = analiz(degistir(base, tip_degistir("G", f"lat_temiz_n={N} lat_temiz_max=4802", "lat_temiz_n=0 lat_temiz_max=58", "A")))
    bekle("Y3: temiz örnek yokken temiz maks ≠ 0 → geçersiz", rc == 1 and "temiz adım yokken" in o, o)


def sinama_disa_aktarim():
    """Y4: histogram dosyaları yalnız bütün denetimler (T dahil) geçince yazılır; var olan dosya ezilmez."""
    print("mp4_seri analiz — Y4 dışa aktarım (dosya sistemi)")
    base = kampanya()

    def kos(satir, on_dosya=None):
        d = tempfile.mkdtemp()
        onek = os.path.join(d, "h")
        if on_dosya:
            with open(os.path.join(d, on_dosya), "w") as f:
                f.write("kullanıcı verisi\n")
        rc, o = analiz(satir, "--hist", onek)
        dosyalar = sorted(os.listdir(d))
        icerik = open(os.path.join(d, on_dosya)).read() if on_dosya else None
        for x in dosyalar:
            os.unlink(os.path.join(d, x))
        os.rmdir(d)
        return rc, o, dosyalar, icerik

    rc, o, ds, _ = kos(base)
    bekle("Y4: geçerli kampanya → 9 histogram dosyası", rc == 0 and len(ds) == 9 and "h-A-10us.csv" in ds, f"{rc} {ds}\n{o}")
    rc, o, ds, _ = kos(kampanya(t_var=False))
    bekle("Y4: eksik T → geçersiz, hiç dosya yok", rc == 1 and ds == [], f"{rc} {ds}\n{o}")
    rc, o, ds, _ = kos(degistir(base, tip_degistir("T", "adim=", "adim=1")))
    bekle("Y4: yanlış T → geçersiz, hiç dosya yok", rc == 1 and ds == [], f"{rc} {ds}\n{o}")
    rc, o, ds, _ = kos(degistir(base, tip_degistir("F", "asim=0 ", f"asim={N + 1} ", "C")))
    bekle("Y4: yalnız C fazında hata (A, B geçerli) → hiç dosya yok", rc == 1 and ds == [], f"{rc} {ds}\n{o}")
    rc, o, ds, _ = kos(kampanya(faz_ozel={"B": faz_satirlari("B", ince={6000: N - 5}, tasan=5)}))
    bekle("KAPSAM: taşmalı kampanya → geçersiz, hiç histogram dosyası yok", rc == 1 and ds == [], f"{rc} {ds}\n{o}")
    rc, o, ds, ic = kos(base, on_dosya="h-B-ince.csv")
    bekle("Y4: hedef dosya zaten var → ezilmez, başka dosya da yazılmaz, çıkış 3",
          rc == 3 and ds == ["h-B-ince.csv"] and ic == "kullanıcı verisi\n" and "ezilmedi" in o and "p50" not in o,
          f"{rc} {ds} {ic!r}\n{o}")


def sinama_gercek_kayit():
    """Gerçek O2 uzun kaydının geçici kopyası üzerinde sol'un Y2/Y3 bozma örnekleri (kayıt yoksa atlanır)."""
    print("mp4_seri analiz — gerçek O2 kaydı üzerinde bozma örnekleri")
    yol = os.path.join(os.environ.get("IDA_CACHE") or os.path.expanduser("~/.cache/ida"), "mp4", "kos", "o2-20261117-190043.csv")
    if not os.path.exists(yol):
        print(f"  ATLANDI: {yol} yok (sayılmadı)")
        return
    satirlar = open(yol, encoding="utf-8", errors="surrogateescape").read().split("\n")
    bek = ("--kimlik", "20261117-185727-o2-a11153bb", "--beklenen-hz", "400", "--beklenen-fazlar", "7",
           "--beklenen-faz-adim", "241000", "--en-az-adim", "240000", "--en-az-sn", "600", "--yigin-azami", "1586416")

    def calistir(fn):
        out = []
        for x in satirlar:
            v = x.find(",")
            if v < 0 or x.startswith("mono_ns"):
                out.append(x)
                continue
            g = x[v + 1:]
            if " *" in g:
                govde = g[: g.rfind(" *")]
                yeni = fn(govde)
                if yeni != govde:
                    g = imzala(yeni)
            out.append(x[:v + 1] + g)
        fd, t = tempfile.mkstemp(suffix=".csv")
        with os.fdopen(fd, "w", encoding="utf-8", errors="surrogateescape") as f:
            f.write("\n".join(out))
        r = subprocess.run([SERI, "analiz", t, *bek], capture_output=True, text=True)
        os.unlink(t)
        return r.returncode, r.stdout + r.stderr

    rc, o = calistir(lambda g: g)
    bekle("gerçek O2 kaydı (değiştirilmemiş kopya) geçerli", rc == 0 and "yigin_tepe 3452 B" in o, o[-400:])
    rc, o = calistir(lambda g: g.replace("lat_temiz_max=86", "lat_temiz_max=59") if g.startswith("G ") and " faz=A " in g else g)
    bekle("gerçek O2: A lat_temiz_max 86→59 → geçersiz", rc == 1 and "genel en büyüğe eşit değil" in o and sayisal_ozet_yok(o), o[-400:])
    rc, o = calistir(lambda g: g.replace("yigin_tepe=3392", "yigin_tepe=4294967295") if g.startswith("S ") and " faz=A " in g else g)
    bekle("gerçek O2: A yigin_tepe=4294967295 → geçersiz", rc == 1 and "boyanabilen" in o and sayisal_ozet_yok(o), o[-400:])


if __name__ == "__main__":
    for x in (SERI, CAGRI):
        if not os.access(x, os.X_OK):
            print(f"yok: {x} (önce derle.sh)")
            sys.exit(2)
    sinama_analiz()
    sinama_sol()
    sinama_ikinci()
    sinama_disa_aktarim()
    sinama_gercek_kayit()
    sinama_yuzdelik_ozellik()
    sinama_kayit()
    sinama_cagri()
    print(f"denetim: {gecen} geçti, {basarisiz} başarısız")
    print("sonuç:", "GEÇTİ" if basarisiz == 0 else f"{basarisiz} BAŞARISIZ")
    sys.exit(1 if basarisiz else 0)
