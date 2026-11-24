#!/usr/bin/env python3
"""Sonarlı havuz senaryolarının A7 kümesi `havuz`: araç, torpido, hedef ve kapalı havuz (W4-sonar-sahne).

Çıktı: assets/havuz/{manifest.json, ida0.glb, torpido_ida0.glb, hedef_ornek.glb, havuz_ornek.glb}.
`acik_su` kümesinin emsaliyle aynı düzen: kaynak .glb dosyaları ve manifest depoda durur, üretim
derlemenin parçası değildir (A7 §3.1, K47); `--denetle` üretilenin depodakiyle bayt bayt aynı
olduğunu sınar (CTest `sim.senaryo.havuz_kumesi_uretici`). Yalnız Python standart kitaplığı ve
aynı dizindeki uret.py kullanılır (Glb yazıcısı, kutu ağları, İDA-0 aracı).

Kullanım:  python3 -I viz/arac/model-uret/havuz.py [--cikti assets/havuz] [--denetle]

Varlıklar (kimlikler A7 uygunluk örneğindekiyle aynı; A5 senaryoları yalnız `varliklar` tablosunu
değiştirerek bu kümeyi gösterir):
  ida0          acik_su kümesindeki araç tanımının aynısı (uret.ida0; dosya baytları da aynı).
  torpido_ida0  A5 araç dosyası (scenarios/ida0.toml, arac.torpido.varlik) bu kimliği ister (A5-K19);
                uygunluk örneğinin sınır kutusundaki (0,20 × 0,04 × 0,04 m) yalın kutu.
  hedef_ornek   uygunluk örneğinin sınır kutusundaki (1,0 × 0,5 × 0,5 m) kapalı kutu; yalnız akustik
                yansıtıcı cisimdir (isabet bölgesi yok).
  havuz_ornek   25 m × 12,5 m iç ölçülü kapalı havuz: 0,2 m kalın taban (üst yüzü REF'in 3,0 m
                altında; taban_derinligi_m = 3,0, A7-K56) ve dört duvar (0,2 m kalın, su yüzeyinin
                0,5 m üstünden tabanın altına); uygunluk örneğindeki havuz_ornek'in sınır kutusu ve
                düğüm tablosu. REF havuz ortasında, su yüzeyinde. Duvarlar dünyada (birim statik
                pozla) x = ±12,5 m (kuzey/güney) ve y = ±6,25 m (doğu/batı) iç yüzlerindedir.

Kutular trigonometri kullanmadan yazılır (köşeler 1/20 m'nin katları); İDA-0 uret.py'deki gibi
math.sin/cos ile üretilir.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import sys

_URET = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uret.py")
_spec = importlib.util.spec_from_file_location("ida_model_uret", _URET)
uret = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(uret)

URETICI = "İDA viz/arac/model-uret/havuz.py"

# Havuz (gövde çerçevesi: x ileri/kuzey, y sancak/doğu, z aşağı; REF havuz ortasında, su yüzeyinde).
YARI_UZUNLUK = 12.5     # m, iç ölçü 25 m
YARI_GENISLIK = 6.25    # m, iç ölçü 12,5 m
TABAN = 3.0             # m, taban üst yüzü (taban_derinligi_m)
KALINLIK = 0.2          # m, duvar ve taban
DUVAR_UST = -0.5        # m, duvarın üst kenarı (su yüzeyinin 0,5 m üstü)


def _uretici(j):
    j["asset"]["generator"] = URETICI


def _rol(dugum, ad, **ek):
    r = {"dugum": dugum, "ad": ad}
    r.update(ek)
    return r


def havuz(glb):
    m = glb.malzeme("havuz_beton", (0.55, 0.60, 0.62, 1.0), 0.0, 0.85)
    L, W, T, t, u = YARI_UZUNLUK, YARI_GENISLIK, TABAN, KALINLIK, DUVAR_UST
    kutular = [
        ("COL_taban", (-L, -W, T), (L, W, T + t)),
        ("COL_duvar_kuzey", (L, -W - t, u), (L + t, W + t, T + t)),
        ("COL_duvar_guney", (-L - t, -W - t, u), (-L, W + t, T + t)),
        ("COL_duvar_dogu", (-L, W, u), (L, W + t, T + t)),
        ("COL_duvar_bati", (-L, -W - t, u), (L, -W, T + t)),
    ]
    gorsel = uret.Parca()
    col = []
    for ad, mn, mx in kutular:
        gorsel.birlestir(uret.kutu_min_max(mn, mx))
        p = uret.kutu_kapali(mn, mx)
        assert uret.manifold_mu(p) and uret.disbukey_mu(p), ad
        col.append((ad, p))
    ref = glb.dugum("REF")
    gv = glb.dugum("havuz", glb.ag("havuz", [(m, gorsel)]))
    carpisma = []
    for ad, p in col:
        d = glb.dugum(ad, glb.ag(ad, [(m, p)]))
        carpisma.append(_rol(d, ad, ayristirma="yok"))
    girdi = {
        "kimlik": "havuz_ornek", "tur": "statik", "dosya": "havuz_ornek.glb", "taban_derinligi_m": TABAN,
        "referans": _rol(ref, "REF"),
        "gorsel": [_rol(gv, "havuz", kosul="her_zaman")],
        "carpisma": carpisma,
    }
    return girdi, [gorsel] + [p for _, p in col], {m: "havuz_beton"}, _uretici


def hedef(glb):
    m = glb.malzeme("hedef_metal", (0.85, 0.30, 0.05, 1.0), 0.6, 0.45)
    mn, mx = (-0.5, -0.25, -0.25), (0.5, 0.25, 0.25)
    gorsel = uret.kutu_min_max(mn, mx)
    col = uret.kutu_kapali(mn, mx)
    assert uret.manifold_mu(col) and uret.disbukey_mu(col)
    ref = glb.dugum("REF")
    gv = glb.dugum("hedef", glb.ag("hedef", [(m, gorsel)]))
    cd = glb.dugum("COL_hedef", glb.ag("COL_hedef", [(m, col)]))
    girdi = {
        "kimlik": "hedef_ornek", "tur": "hedef", "dosya": "hedef_ornek.glb",
        "referans": _rol(ref, "REF"),
        "gorsel": [_rol(gv, "hedef", kosul="her_zaman")],
        "carpisma": [_rol(cd, "COL_hedef", ayristirma="yok")],
    }
    return girdi, [gorsel, col], {m: "hedef_boyali_metal"}, _uretici


def torpido(glb):
    m = glb.malzeme("torpido_govde", (0.85, 0.42, 0.02, 1.0), 0.1, 0.35)
    mn, mx = (-0.10, -0.02, -0.02), (0.10, 0.02, 0.02)
    gorsel = uret.kutu_min_max(mn, mx)
    col = uret.kutu_kapali(mn, mx)
    assert uret.manifold_mu(col) and uret.disbukey_mu(col)
    ref = glb.dugum("REF")
    gv = glb.dugum("torpido", glb.ag("torpido", [(m, gorsel)]))
    cd = glb.dugum("COL_torpido", glb.ag("COL_torpido", [(m, col)]))
    girdi = {
        "kimlik": "torpido_ida0", "tur": "torpido", "dosya": "torpido_ida0.glb",
        "referans": _rol(ref, "REF"),
        "gorsel": [_rol(gv, "torpido", kosul="her_zaman")],
        "carpisma": [_rol(cd, "COL_torpido", ayristirma="yok")],
    }
    return girdi, [gorsel, col], {m: "govde_plastik"}, _uretici


def ida0(glb):
    girdi, kutu, atama = uret.ida0(glb)
    return girdi, kutu, atama, None   # acik_su kümesindeki dosyayla bayt bayt aynı (üretici adı dahil)


MALZEMELER = [
    {"kimlik": "govde_plastik", "aciklama": "İDA-0 çerçeve, köpük, tüpler, iticiler ve torpido",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.01, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.5,
                 "puruzluluk_gamma_p": 0.5, "kaynak": "tahmin",
                 "dayanak": "750 kHz'te yansıtıcılık bilinmiyor (05 2.4.1); A7 uygunluk örneğiyle aynı yer tutucu"}},
    {"kimlik": "hedef_boyali_metal", "aciklama": "Hedef kutusu",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.05, "gelis_yasasi": "lambert_cos2", "ayna_yansima_rs": 0.9,
                 "puruzluluk_gamma_p": 0.3, "kaynak": "tahmin",
                 "dayanak": "A7 uygunluk örneğiyle aynı yer tutucu; kalibrasyon A5'te"}},
    {"kimlik": "havuz_beton", "aciklama": "Havuz tabanı ve duvarları",
     "optik": {"model": "gltf_metalik_puruzluluk"},
     "akustik": {"geri_sacilim_mu": 0.02, "gelis_yasasi": "cos1", "ayna_yansima_rs": 0.6,
                 "puruzluluk_gamma_p": 0.7, "kaynak": "tahmin",
                 "dayanak": "A7 uygunluk örneğiyle aynı yer tutucu; Ping360 havuz kayıtlarıyla uydurulacak (05 7)"}},
]


def uret_kume():
    varliklar = []
    dosyalar = {}
    for kurucu in (ida0, torpido, hedef, havuz):
        glb = uret.Glb()
        girdi, kutu, atama, degistir = kurucu(glb)
        veri = glb.yaz(degistir)
        dosyalar[girdi["dosya"]] = veri
        tam = {
            "kimlik": girdi["kimlik"], "tur": girdi["tur"], "dosya": girdi["dosya"],
            "sha256": hashlib.sha256(veri).hexdigest(), "bayt": len(veri),
        }
        if "taban_derinligi_m" in girdi:
            tam["taban_derinligi_m"] = girdi["taban_derinligi_m"]
        tam.update({
            "referans": girdi["referans"], "sinir_kutusu_govde_m": uret.sinir_kutusu(kutu),
            "malzeme_atamasi": [{"gltf_malzeme": i, "malzeme": m} for i, m in sorted(atama.items())],
            "gorsel": girdi["gorsel"],
        })
        for anahtar in ("yuzerlik", "carpisma", "baglanti_noktalari"):
            if anahtar in girdi:
                tam[anahtar] = girdi[anahtar]
        varliklar.append(tam)
    manifest = {
        "sema_surumu": "1.0",
        "kume_kimligi": "havuz",
        "kume_surumu": 1,
        "aciklama": ("Sonarlı havuz senaryolarının kümesi (W4-sonar-sahne): İDA-0 (acik_su kümesindeki araç), "
                     "yalın kutu torpido, kutu hedef (yalnız akustik yansıtıcı) ve 25 m × 12,5 m, 3 m derin kapalı "
                     "havuz (taban ve dört duvar ayrı çarpışma ağları). Kimlikler ve sınır kutuları A7 uygunluk "
                     "örneğindekiyle aynıdır. Geometri viz/arac/model-uret/havuz.py ile üretilir; elle çizilmez. "
                     "Akustik değerler tahmindir."),
        "birimler": {"uzunluk": "m", "aci": "rad", "kutle": "kg", "zaman": "s"},
        "eksenler": {"gltf": "sag_el_y_yukari_z_on", "gltf_govde": [[0, 0, 1], [-1, 0, 0], [0, -1, 0]],
                     "gltf_godot_yerel": [[-1, 0, 0], [0, 1, 0], [0, 0, -1]]},
        "uretim": {"disa_aktarici": {"arac": "betik", "surum": "1",
                                     "ayarlar": {"betik": "viz/arac/model-uret/havuz.py"}}},
        "malzemeler": MALZEMELER,
        "varliklar": varliklar,
    }
    metin = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    dosyalar["manifest.json"] = metin.encode("utf-8")
    return dosyalar


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    kok = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
    ap.add_argument("--cikti", default=os.path.join(kok, "assets", "havuz"))
    ap.add_argument("--denetle", action="store_true")
    a = ap.parse_args()
    dosyalar = uret_kume()
    hata = uret._yaz_ya_da_denetle({a.cikti: dosyalar}, a.denetle)
    if a.denetle:
        # A7-K01: manifestte adı geçmeyen .glb dizinde bulunmaz.
        if os.path.isdir(a.cikti):
            for ad in sorted(os.listdir(a.cikti)):
                if ad.endswith(".glb") and ad not in dosyalar:
                    print("FAZLA: %s" % os.path.join(a.cikti, ad))
                    hata = 1
        print("denetim: %s" % ("farklı" if hata else "aynı"))
    return hata


if __name__ == "__main__":
    sys.exit(main())
