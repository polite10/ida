"""MP6a çözümlemesi: koşu dizinlerinden durum x yol tablosu üretir.

Kullanım: python3 -I analiz.py <kok_dizin> [koşu_adı ...]
  <kok_dizin> altında her koşu bir dizindir (ör. a_gorunur, b_gizli, b_gorunur_1280_esz).
  İçerik referansı aynı yolun "<yol>_gorunur" koşusudur (aynı kare kimliği k).
  Sonuç: <kok_dizin>/ozet_tablo.json ve stdout'a Markdown tablolar.
"""
import csv
import json
import os
import re
import sys

import numpy as np

GOLGE_ESIK = 0.75


def yuzdelik(a, q):
    if len(a) == 0:
        return None
    return float(np.percentile(np.asarray(a, dtype=np.float64), q))


def oku_kare(yol):
    with open(os.path.join(yol, "kare.csv"), newline="") as f:
        return list(csv.DictReader(f))


def oku_durum(yol):
    with open(os.path.join(yol, "durum.csv"), newline="") as f:
        return list(csv.DictReader(f))


def pencere_dogrula(yol, durum):
    """Dış gözlem: CGWindowList ve NSRunningApplication (pencere.js)."""
    kos = open(os.path.join(yol, "kos.log")).read()
    m = re.search(r"olcum_pid (\d+)", kos)
    pid = int(m.group(1)) if m else None
    m2 = re.search(r"ortu_pid (\d+)", kos)
    ortu = int(m2.group(1)) if m2 else None
    p = os.path.join(yol, "pencere.jsonl")
    if not os.path.exists(p):
        return {"ornek": 0}
    satirlar = [json.loads(s) for s in open(p) if s.strip()]
    # İlk örnek durum geçişiyle aynı anda alınır (animasyon, örtü açılışı); ayrı sayılır.
    sonraki = satirlar[1:]
    ekranda = 0
    gizli = 0
    ortulu = 0
    for d in sonraki:
        olc = [w for w in d["onscreen_all"] if w["pid"] == pid]
        if olc:
            ekranda += 1
        if d.get("hidden"):
            gizli += 1
        if ortu is not None and olc:
            o = [w for w in d["onscreen_all"] if w["pid"] == ortu]
            for w in olc:
                b = w["b"]
                for c in o:
                    cb = c["b"]
                    if (c["order"] < w["order"] and cb["X"] <= b["X"] and cb["Y"] <= b["Y"]
                            and cb["X"] + cb["Width"] >= b["X"] + b["Width"]
                            and cb["Y"] + cb["Height"] >= b["Y"] + b["Height"]):
                        ortulu += 1
                        break
    return {"ornek": len(sonraki), "olcum_penceresi_ekranda": ekranda, "uygulama_gizli": gizli,
            "tamamen_ortulu": ortulu}


def ps_ozet(yol):
    p = os.path.join(yol, "ps.log")
    if not os.path.exists(p):
        return {}
    pri = []
    cpu = []
    for s in open(p):
        parca = s.split()
        if len(parca) >= 5:
            pri.append(int(parca[2]))
            cpu.append(float(parca[4].replace(",", ".")))
    return {"pri": sorted(set(pri)), "cpu_yuzde_ort": float(np.mean(cpu)) if cpu else None}


def ham_kare(yol, k):
    for uz, bpp in (("rgb", 3), ("rgba", 4)):
        p = os.path.join(yol, "kare_%07d.%s" % (k, uz))
        if os.path.exists(p):
            return np.fromfile(p, dtype=np.uint8), bpp
    return None, None


def kos_coz(yol, ref_yol):
    bilgi = json.load(open(os.path.join(yol, "bilgi.json")))
    ozet = json.load(open(os.path.join(yol, "ozet.json")))
    satir = oku_kare(yol)
    durum_s = [d for d in oku_durum(yol) if d["faz"] == "olcum"]
    w, h = bilgi["cozunurluk"]
    sayac = ozet["sayac"]
    istek = sayac["istek"]
    uretilen = [r for r in satir if r["sonuc"] == "uretildi"]
    dusen = istek - len(uretilen)
    nedenler = {k: v for k, v in sayac.items() if k not in ("istek", "uretildi") and v}
    ks = sorted(int(r["k"]) for r in uretilen)
    bosluk = 0
    if ks:
        bosluk = int(max(np.diff([-1] + ks + [istek]))) - 1
    gec = [int(r["gecikme_us"]) for r in uretilen]
    geri = [int(r["geri_okuma_us"]) for r in uretilen]
    gimg = [int(r["get_image_us"]) for r in uretilen if int(r["get_image_us"]) >= 0]
    ikinci = [int(r["ikinci_us"]) for r in uretilen if r.get("ikinci_us") not in (None, "", "-1")]
    gec_sched = [int(r["t_req_us"]) - int(r["t_sched_us"]) for r in uretilen]
    poz = [np.hypot(float(r["cx"]) - float(r["gx"]), float(r["cy"]) - float(r["gy"]))
           for r in uretilen if float(r["cx"]) >= 0]
    kurede_yok = sum(1 for r in uretilen if float(r["cx"]) < 0)
    golge = [float(r["golge_orani"]) for r in uretilen]
    sonuc = {
        "kos": os.path.basename(yol), "yol": bilgi["yol"], "durum": bilgi["durum"], "geri": bilgi["geri"],
        "cozunurluk": "%dx%d" % (w, h), "sure_s": bilgi["sure_s"],
        "durum_dogrulandi_ic": ozet["durum_dogrulandi"], "durum_dogrulama_ic": ozet["durum_dogrulama"],
        "istek": istek, "uretilen": len(uretilen), "dusen": dusen,
        "dusen_yuzde": 100.0 * dusen / istek if istek else None, "dusme_nedenleri": nedenler,
        "en_buyuk_kimlik_boslugu": bosluk,
        "gecikme_ms": {"p50": yuzdelik(gec, 50) and yuzdelik(gec, 50) / 1000, "p99": yuzdelik(gec, 99) and yuzdelik(gec, 99) / 1000,
                       "maks": max(gec) / 1000 if gec else None},
        "geri_okuma_ms": {"p50": yuzdelik(geri, 50) and yuzdelik(geri, 50) / 1000, "p99": yuzdelik(geri, 99) and yuzdelik(geri, 99) / 1000,
                          "maks": max(geri) / 1000 if geri else None},
        "get_image_ms": {"p50": yuzdelik(gimg, 50) and yuzdelik(gimg, 50) / 1000, "p99": yuzdelik(gimg, 99) and yuzdelik(gimg, 99) / 1000,
                         "maks": max(gimg) / 1000 if gimg else None},
        "ikinci_okuma_ms": {"p50": yuzdelik(ikinci, 50) and yuzdelik(ikinci, 50) / 1000, "p99": yuzdelik(ikinci, 99) and yuzdelik(ikinci, 99) / 1000,
                            "maks": max(ikinci) / 1000 if ikinci else None},
        "istek_gecikmesi_planla_ms": {"p50": yuzdelik(gec_sched, 50) and yuzdelik(gec_sched, 50) / 1000,
                                      "p99": yuzdelik(gec_sched, 99) and yuzdelik(gec_sched, 99) / 1000,
                                      "maks": max(gec_sched) / 1000 if gec_sched else None},
        "poz_hata_px": {"ort": float(np.mean(poz)) if poz else None, "maks": float(max(poz)) if poz else None,
                        "kure_bulunamadi": kurede_yok},
        "golge_orani": {"maks": max(golge) if golge else None, "min": min(golge) if golge else None,
                        "golge_var_yuzde": 100.0 * sum(1 for g in golge if g <= GOLGE_ESIK) / len(golge) if golge else None},
        "frames_drawn_olcumde": sum(int(d["d_drawn"]) for d in durum_s),
        "process_hz": {"min": min(int(d["d_process"]) for d in durum_s) if durum_s else None,
                       "medyan": float(np.median([int(d["d_process"]) for d in durum_s])) if durum_s else None},
        "pencere_kipi": sorted(set(int(d["pencere_kipi"]) for d in durum_s)),
        "pencere_dis": pencere_dogrula(yol, bilgi["durum"]),
        "ps": ps_ozet(yol),
    }
    # Durumun koşu boyunca tutulup tutulmadığı (iç gözlem, saniyelik): görünürde frames_drawn
    # ilerlemeli, görünmez durumlarda durmalı. Her yuva, planlı zamanının düştüğü saniyeye atanır.
    tum_durum = oku_durum(yol)
    sinirlar = []
    onceki = None
    for d in tum_durum:
        t = int(d["t_us"])
        if d["faz"] == "olcum_basladi":
            onceki = t
            continue
        if onceki is None:
            continue
        cizdi = int(d["d_drawn"]) > 0
        tutuldu = cizdi if bilgi["durum"] == "gorunur" else not cizdi
        sinirlar.append((onceki, t, tutuldu))
        onceki = t
    tutulan_sn = sum(1 for s in sinirlar if s[2])
    sonuc["durum_tutuldu"] = {"saniye": tutulan_sn, "toplam_saniye": len(sinirlar)}
    if sinirlar and bilgi["durum"] != "basliksiz":
        bas = np.array([s[0] for s in sinirlar])
        bit = np.array([s[1] for s in sinirlar])
        tut = np.array([s[2] for s in sinirlar])
        ist_t = 0
        ur_t = 0
        for r in satir:
            ts = int(r["t_sched_us"])
            i = int(np.searchsorted(bit, ts, side="left"))
            if i < len(bit) and bas[i] < ts <= bit[i] and tut[i]:
                ist_t += 1
                if r["sonuc"] == "uretildi":
                    ur_t += 1
        sonuc["durum_tutulurken"] = {"istek": ist_t, "uretilen": ur_t,
                                     "dusen_yuzde": 100.0 * (ist_t - ur_t) / ist_t if ist_t else None}
    # İçerik: referans koşuyla aynı k için özet eşitliği ve örnek karelerde ortalama mutlak fark.
    if ref_yol and os.path.isdir(ref_yol):
        ref = {int(r["k"]): r for r in oku_kare(ref_yol) if r["sonuc"] == "uretildi"}
        esit = 0
        karsilastirilan = 0
        poz_ref = []
        for r in uretilen:
            k = int(r["k"])
            if k in ref:
                karsilastirilan += 1
                if ref[k]["md5"] == r["md5"]:
                    esit += 1
                if float(r["cx"]) >= 0 and float(ref[k]["cx"]) >= 0:
                    poz_ref.append(np.hypot(float(r["cx"]) - float(ref[k]["cx"]), float(r["cy"]) - float(ref[k]["cy"])))
        mad = []
        for r in uretilen:
            k = int(r["k"])
            a, bpa = ham_kare(yol, k)
            if a is None:
                continue
            b, bpb = ham_kare(ref_yol, k)
            if b is None or bpa != bpb:
                continue
            a3 = a.reshape(h, w, bpa)[:, :, :3].astype(np.int16)
            b3 = b.reshape(h, w, bpb)[:, :, :3].astype(np.int16)
            mad.append(float(np.mean(np.abs(a3 - b3))))
        sonuc["referans"] = os.path.basename(ref_yol)
        sonuc["md5_esit"] = {"esit": esit, "karsilastirilan": karsilastirilan}
        sonuc["mad_255"] = {"ornek": len(mad), "ort": float(np.mean(mad)) if mad else None,
                            "maks": float(max(mad)) if mad else None}
        sonuc["poz_fark_ref_px_maks"] = float(max(poz_ref)) if poz_ref else None
    return sonuc


def f(x, n=2):
    if x is None:
        return "-"
    return ("%." + str(n) + "f") % x


def main():
    kok = sys.argv[1]
    adlar = sys.argv[2:] or sorted(d for d in os.listdir(kok) if os.path.isfile(os.path.join(kok, d, "ozet.json")))
    tum = []
    for ad in adlar:
        yol = os.path.join(kok, ad)
        bilgi = json.load(open(os.path.join(yol, "bilgi.json")))
        ref = os.path.join(kok, "%s_gorunur" % bilgi["yol"])
        if os.path.abspath(ref) == os.path.abspath(yol):
            diger = "b" if bilgi["yol"] == "a" else "a"
            ref = os.path.join(kok, "%s_gorunur" % diger)
        tum.append(kos_coz(yol, ref))
    with open(os.path.join(kok, "ozet_tablo.json"), "w") as fo:
        json.dump(tum, fo, indent=2, ensure_ascii=False)
    print("| koşu | istek | üretilen | düşen % | nedenler | kimlik boşluğu | gecikme p50/p99/maks ms | geri okuma p50/p99/maks ms | md5 eşit | MAD ort/maks (/255) | poz hata ort/maks px | gölge var % | frames_drawn | process Hz min/medyan |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for s in tum:
        md5 = "%d/%d" % (s["md5_esit"]["esit"], s["md5_esit"]["karsilastirilan"]) if "md5_esit" in s else "-"
        mad = "%s/%s (n=%d)" % (f(s["mad_255"]["ort"], 3), f(s["mad_255"]["maks"], 3), s["mad_255"]["ornek"]) if "mad_255" in s else "-"
        print("| %s | %d | %d | %s | %s | %d | %s/%s/%s | %s/%s/%s | %s | %s | %s/%s | %s | %d | %s/%s |" % (
            s["kos"], s["istek"], s["uretilen"], f(s["dusen_yuzde"]), json.dumps(s["dusme_nedenleri"], ensure_ascii=False),
            s["en_buyuk_kimlik_boslugu"],
            f(s["gecikme_ms"]["p50"]), f(s["gecikme_ms"]["p99"]), f(s["gecikme_ms"]["maks"]),
            f(s["geri_okuma_ms"]["p50"]), f(s["geri_okuma_ms"]["p99"]), f(s["geri_okuma_ms"]["maks"]),
            md5, mad, f(s["poz_hata_px"]["ort"], 3), f(s["poz_hata_px"]["maks"], 3), f(s["golge_orani"]["golge_var_yuzde"], 1),
            s["frames_drawn_olcumde"], s["process_hz"]["min"], s["process_hz"]["medyan"]))


if __name__ == "__main__":
    main()
