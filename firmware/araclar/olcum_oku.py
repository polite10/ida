#!/usr/bin/env python3
"""firmware/araclar/olcum_oku.py — kartın ölçüm bloğunu SWD ile okur (W3-kart dilim 1).

Tutarlılık: blok sıra kilidiyle yayımlanır (yayin_sira güncellemede tek, bitince çift); okuyucu
aynı oturumda bloğu ve sonra sıra numarasını okur, eşit ve çift değilse yeniden dener.

Ölçüm bloğu (firmware/app/olcum.h) A1'de taşınmayan sayaçları tutar: adım (tick) süresi
histogramı, ayrıştırma süresi, ızgara kaçırması, sürücü hata sayaçları, yığın, sıfırlama ve hata
kaydı. Blok aynı ST-LINK kablosu üzerinden STM32_Programmer_CLI ile işlemci durdurulmadan
(mode=HOTPLUG) okunur; adres ELF'teki `ida_fw_olcum` simgesinden, sözcük düzeni olcum.h'deki
X-makrosundan alınır (tek kaynak).

Kullanım:
  python3 -I firmware/araclar/olcum_oku.py --onayar fw-release [--json cikti.json]
  python3 -I firmware/araclar/olcum_oku.py --onayar fw-measure --sinama iwdg|hata

--sinama yalnız fw-measure imajında vardır: `ida_fw_sinama_komutu`'na komut yazar (IWDG: ana
döngü durur; HATA: tanımsız komut ile UsageFault).
"""
import argparse
import json
import os
import re
import subprocess
import sys

KOK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ARAC = os.path.join(KOK, "firmware", "araclar", "arac.sh")
OLCUM_H = os.path.join(KOK, "firmware", "app", "olcum.h")
KUTU_UST_US = [10, 20, 30, 50, 75, 100, 150, 200, 300, 500, 750, 1000, 2000, 3500, 5000, None]
SINAMA = {"iwdg": 0x49574447, "hata": 0x48415441, "kayit_iwdg": 0x4B495744, "tx_dur": 0x54584452, "ecc": 0x45433259,
          "temizle": 0}


def duzen():
    metin = open(OLCUM_H, encoding="utf-8").read()
    m = re.search(r"#define IDA_FW_OLCUM_ALANLARI\(X, XK\)(.*?)\n/\* clang-format on", metin, re.S)
    if not m:
        raise SystemExit("olcum.h: X-makrosu bulunamadı")
    adlar = []
    for tur, ad in re.findall(r"\b(XK|X)\((\w+)\)", m.group(1)):
        if tur == "XK":
            adlar += ["%s[%d]" % (ad, i) for i in range(16)]
        else:
            adlar.append(ad)
    return adlar


def arac(*arg):
    r = subprocess.run(["sh", ARAC] + list(arg), capture_output=True, text=True)
    return r.returncode, re.sub(r"\x1b\[[0-9;]*m", "", r.stdout + r.stderr)


def simge(elf, ad):
    rc, cikti = arac("arm-none-eabi-nm", elf)
    if rc != 0:
        raise SystemExit("nm başarısız: " + cikti[-300:])
    for satir in cikti.splitlines():
        p = satir.split()
        if len(p) == 3 and p[2] == ad:
            return int(p[0], 16)
    return None


def _satirlar(cikti):
    """CLI çıktısındaki okuma bölümleri: her bölüm {adres: sözcük}."""
    bolumler = []
    for parca in cikti.split("Reading 32-bit memory content")[1:]:
        degerler = {}
        for satir in parca.splitlines():
            m = re.match(r"\s*0x([0-9A-Fa-f]{8})\s*:\s*((?:[0-9A-Fa-f]{8}\s*)+)$", satir)
            if m:
                a = int(m.group(1), 16)
                for i, s in enumerate(m.group(2).split()):
                    degerler[a + 4 * i] = int(s, 16)
        bolumler.append(degerler)
    return bolumler


def ham_oku(adres, sozcuk, sira_indis):
    """Tek programlayıcı oturumunda önce blok, sonra yayın sıra numarası yeniden okunur.
    Dönüş: (sözcükler, sonraki sıra numarası)."""
    sira_adres = adres + 4 * sira_indis
    rc, cikti = arac("STM32_Programmer_CLI", "-c", "port=SWD", "mode=HOTPLUG",
                     "-r32", "0x%08X" % adres, "0x%X" % (4 * sozcuk),
                     "-r32", "0x%08X" % sira_adres, "0x4")
    if rc != 0:
        raise SystemExit("okuma başarısız (çıkış %d):\n%s" % (rc, cikti[-800:]))
    bolumler = _satirlar(cikti)
    if len(bolumler) != 2:
        raise SystemExit("okuma çıktısı tanınmadı (%d bölüm)" % len(bolumler))
    sozcukler = []
    for i in range(sozcuk):
        if adres + 4 * i not in bolumler[0]:
            raise SystemExit("okuma eksik: 0x%08X" % (adres + 4 * i))
        sozcukler.append(bolumler[0][adres + 4 * i])
    if sira_adres not in bolumler[1]:
        raise SystemExit("sıra numarası okunamadı")
    return sozcukler, bolumler[1][sira_adres]


def tutarli_mi(sira_blokta, sira_sonra):
    """Sıra kilidi: blok güncellenirken sıra tek; blok okunurken güncelleme başladıysa ya da
    bittiyse sonraki okuma farklıdır."""
    return sira_blokta % 2 == 0 and sira_blokta == sira_sonra


def tutarli_oku(okuyucu, sira_indis, deneme=5):
    """okuyucu() -> (sözcükler, sonraki sıra). Tutarlı görüntü gelene kadar en çok `deneme` kez
    okur; gelmezse hata verir (yarı güncellenmiş blok hiçbir zaman döndürülmez)."""
    for _ in range(deneme):
        sozcukler, sonra = okuyucu()
        if tutarli_mi(sozcukler[sira_indis], sonra):
            return sozcukler
    raise SystemExit("ölçüm bloğunun tutarlı görüntüsü %d denemede alınamadı" % deneme)


def coz(adlar, s):
    d = {}
    for ad, v in zip(adlar, s):
        if ad.endswith("_i"):
            v = v - (1 << 32) if v & 0x80000000 else v
        d[ad] = v
    birlesik = {}
    for ad in list(d):
        if ad.endswith("_lo") and ad[:-3] + "_hi" in d:
            birlesik[ad[:-3]] = d[ad] | (d[ad[:-3] + "_hi"] << 32)
    d.update(birlesik)
    return d


def _hist(d, onek):
    kutu = [d["%s_kutu[%d]" % (onek, i)] for i in range(16)]
    h = {}
    alt = 0
    for i, n in enumerate(kutu):
        ust = KUTU_UST_US[i]
        if n:
            h["[%d, %s) us" % (alt, ust if ust is not None else "inf")] = n
        alt = ust if ust is not None else alt
    return h


def _sure(d, onek, sayi_ad=None):
    us = lambda c: c / 160.0  # noqa: E731  (DWT çevrimi -> µs, 160 MHz)
    n = d[sayi_ad or onek + "_sayisi"]
    g = {"sayi": n, "en_kotu_us": us(d[onek + "_en_kotu_cevrim"])}
    if onek + "_en_iyi_cevrim" in d:
        g["en_iyi_us"] = us(d[onek + "_en_iyi_cevrim"])
    if onek + "_toplam_cevrim" in d and n:
        g["ortalama_us"] = us(d[onek + "_toplam_cevrim"]) / n
    if onek + "_kutu[0]" in d:
        g["histogram"] = _hist(d, onek)
    return g


def ozet(d):
    us = lambda c: c / 160.0  # noqa: E731
    o = {}
    o["magic_ok"] = d["magic"] == 0x4F414449
    o["surum"] = d["surum"]
    o["calisma_s"] = d["zaman_us"] / 1e6
    o["sysclk_hz"] = d["sysclk_hz"]
    o["baud_brr"] = d["baud_brr"]
    o["capraz"] = {"ornek": d["capraz_ornek"], "sapma_en_kucuk_cevrim": d["capraz_sapma_en_kucuk_i"],
                   "sapma_en_buyuk_cevrim": d["capraz_sapma_en_buyuk_i"]}
    o["step"] = _sure(d, "step")
    o["step"]["butce_asimi"] = d["step_butce_asimi"]
    o["tick"] = _sure(d, "tick")
    o["tick"]["butce_asimi"] = d["tick_butce_asimi"]
    o["skart"] = _sure(d, "skart")
    o["cagri"] = {"rc_hata": d["cagri_rc_hata"], "son_rc": d["cagri_son_rc"],
                  "tur_en_kotu_us": us(d["cagri_tur_en_kotu_cevrim"]),
                  # W4-K16: çağrı turu ≤ 1,9 ms; PARAM_SAVE'i işleyen çağrı ayrı.
                  "turu_sinir_us": d.get("cagri_turu_sinir_us"), "turu_sayi": d.get("cagri_turu_sayi"),
                  "turu_asim": d.get("cagri_turu_asim"),
                  "turu_en_kotu_us": d.get("cagri_turu_en_kotu_us"),
                  "turu_asim_param_save": d.get("cagri_turu_asim_ps"),
                  "turu_param_save_en_kotu_us": d.get("cagri_turu_ps_en_kotu_us")}
    # W4-K16 (b): PARAM_SAVE'i işleyen son çağrı ve olağan step çağrılarında "çağrı turu − step".
    o["param_save_cagrisi"] = {
        "sayi": d.get("ps_sayi"), "son_tur_us": us(d.get("ps_son_tur_cevrim", 0)),
        "son_step_us": us(d.get("ps_son_step_cevrim", 0)), "son_skart_us": d.get("ps_son_skart_us"),
        "son_dongu_turu_us": us(d.get("ps_son_dongu_cevrim", 0)),
        "son_tur_eksi_step_us": us(d.get("ps_son_tur_cevrim", 0) - d.get("ps_son_step_cevrim", 0))}
    _n = d.get("ol_fark_sayi") or 0
    _top = d.get("ol_fark_toplam_cevrim_lo", 0) + (d.get("ol_fark_toplam_cevrim_hi", 0) << 32)
    o["olagan_tur_eksi_step"] = {
        "sayi": _n, "en_kotu_us": us(d.get("ol_fark_en_kotu_cevrim", 0)),
        "ortalama_us": (us(_top) / _n) if _n else None}
    o["blok"] = _sure(d, "blok")
    o["blok"]["en_kotu_blok_bayt"] = d["blok_en_kotu_bayt"]
    if d.get("blok_bayt"):
        o["blok"]["toplam_bayt"] = d["blok_bayt"]
        o["blok"]["ns_bayt"] = d["blok_toplam_cevrim"] * 6.25 / d["blok_bayt"]
    bb = {}
    for i in range(16):
        n = d["bb_sayi[%d]" % i]
        if n:
            b = d["bb_bayt[%d]" % i]
            c = d["bb_cevrim[%d]" % i]
            bb["[%d, %d) B" % (1 << i, 1 << (i + 1))] = {
                "blok": n, "ort_bayt": b / n, "ort_us": c / 160.0 / n,
                "ns_bayt": c * 6.25 / b if b else None}
    o["blok_boyuna_gore"] = bb
    o["periyot"] = _sure(d, "periyot")
    o["periyot"]["en_kotu_periyot_bayt"] = d["periyot_en_kotu_bayt"]
    o["izgara"] = {"islenen": d["izgara_sayisi"], "kacirma_200hz": d["izgara_kacirma"],
                   "gecikme_en_kotu_us": d["izgara_gecikme_en_kotu_us"]}
    o["dongu"] = {"tur": d["tur_sayisi"], "tur_en_kotu_us": us(d["tur_en_kotu_cevrim"]),
                  "besleme": d["besleme_sayisi"], "besleme_aralik_en_kotu_us":
                  d["besleme_aralik_en_kotu_us"], "besleme_ret": d["besleme_ret"],
                  "besleme_ret_bitleri": d["besleme_ret_bitleri"],
                  "rx_bekleme_en_uzun_us": d["rx_bekleme_en_uzun_us"]}
    o["cekirdek"] = {"durum": d["cekirdek_durum"] & 0xFF, "neden": d["cekirdek_durum"] >> 8,
                     "init_rc": d["init_rc"], "core_size": d["core_size"], "core_ok": d["core_ok"],
                     "bg_sayisi": d["bg_sayisi"], "bg_en_kotu_us": us(d["bg_en_kotu_cevrim"])}
    o["surucu"] = {k: d[k] for k in ("ore", "fe", "ne", "pe", "dma_rx_hata", "dma_tx_hata",
                                     "rx_kararsiz", "rx_belirsiz", "rx_gecersiz", "rx_geri",
                                     "rx_olay_birlesme", "rx_tasma", "rx_tasma_bayt",
                                     "rx_halka_azami", "uart_bekle_hata", "tx_bayt", "tx_ret",
                                     "tx_doluluk_azami", "rx_yeniden", "rx_durdu")}
    o["gonderim"] = {k[4:]: d[k] for k in d if k.startswith("gon_")}
    o["kapanis_neden"] = d.get("kapanis_neden")
    o["kapanis"] = {k: d.get(k) for k in ("kapanis_neden", "kapanis_evre", "kapanis_oturum_sonu_teslim",
                                          "son_armed", "son_mode", "son_fc_state", "son_fc_inhibit",
                                          "son_link_event")}
    o["link"] = {"durum": d["link_durum"], "sess": d["link_sess"]}
    o["sayac"] = {k[4:]: d[k] for k in d if k.startswith("say_")}
    o["kalici"] = {k[4:]: d[k] for k in d if k.startswith("kal_")}
    o["kalici"]["acilis_us"] = us(d["kal_acilis_cevrim"])
    o["kalici"]["kayit_en_kotu_tur_us"] = us(d["kal_kayit_en_kotu_tur_cevrim"])
    o["flash"] = {k[3:]: d[k] for k in d if k.startswith("fl_")}
    o["flash"]["silme_en_kotu_us"] = us(d["fl_silme_en_kotu_cevrim"])
    o["flash"]["yazma_en_kotu_us"] = us(d["fl_yazma_en_kotu_cevrim"])
    o["yigin"] = {"boyu": d["yigin_boyu"], "en_yuksek": d["yigin_en_yuksek"]}
    o["sifirlama"] = {"reset_cause": d["reset_cause"], "reset_flags": "0x%08X" % d["reset_flags"],
                      "ardisik_ariza": d["ardisik_ariza"]}
    o["ariza_kaydi"] = {"onceki_ariza": d["onceki_ariza"], "ariza_sayisi": d["ariza_sayisi"],
                        "cfsr": "0x%08X" % d["ariza_cfsr"], "pc": "0x%08X" % d["ariza_pc"],
                        "istisna": d["ariza_istisna"]}
    o["nvic_yeniden"] = d["nvic_yeniden"]
    o["derleme_olcum"] = d["derleme_olcum"]
    o["fw_git"] = "0x%08X" % d["fw_git"]
    return o


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--onayar", default="fw-release")
    p.add_argument("--elf", default=None)
    p.add_argument("--json", default=None)
    p.add_argument("--ham", action="store_true", help="bütün sözcükleri yaz")
    p.add_argument("--sinama", choices=sorted(SINAMA))
    a = p.parse_args(argv)
    elf = a.elf or os.path.join(KOK, "build", a.onayar, "firmware", "ida_fw.elf")
    if a.sinama:
        adres = simge(elf, "ida_fw_sinama_komutu")
        if adres is None:
            raise SystemExit("ida_fw_sinama_komutu yok (yalnız fw-measure imajında)")
        rc, cikti = arac("STM32_Programmer_CLI", "-c", "port=SWD", "mode=HOTPLUG",
                         "-w32", "0x%08X" % adres, "0x%08X" % SINAMA[a.sinama])
        print(cikti[-400:])
        return 0 if rc == 0 else 1
    adlar = duzen()
    adres = simge(elf, "ida_fw_olcum")
    if adres is None:
        raise SystemExit("ida_fw_olcum simgesi yok: " + elf)
    sira_indis = adlar.index("yayin_sira")
    s = tutarli_oku(lambda: ham_oku(adres, len(adlar), sira_indis), sira_indis)
    d = coz(adlar, s)
    if d["sozcuk_sayisi"] != len(adlar):
        raise SystemExit("blok boyu uyuşmuyor: kart %d, olcum.h %d (imaj ve kaynak aynı mı?)"
                         % (d["sozcuk_sayisi"], len(adlar)))
    sonuc = d if a.ham else ozet(d)
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"ham": d, "ozet": ozet(d)}, f, indent=2, ensure_ascii=False)
    print(json.dumps(sonuc, indent=2, ensure_ascii=False))
    return 0


def ozet_oku(onayar="fw-release", elf=None):
    """Başka araçlar için (duman.py --olcum): tutarlı ölçüm bloğunu okur, ham sözlük döner."""
    elf = elf or os.path.join(KOK, "build", onayar, "firmware", "ida_fw.elf")
    adlar = duzen()
    adres = simge(elf, "ida_fw_olcum")
    if adres is None:
        raise SystemExit("ida_fw_olcum simgesi yok: " + elf)
    sira_indis = adlar.index("yayin_sira")
    return coz(adlar, tutarli_oku(lambda: ham_oku(adres, len(adlar), sira_indis), sira_indis))


if __name__ == "__main__":
    # Çıktı konsol kodlamasından bağımsız (Windows'ta cp1252 Türkçe karakteri yazamaz).
    for _akis in (sys.stdout, sys.stderr):
        try:
            _akis.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    sys.exit(main(sys.argv[1:]))
