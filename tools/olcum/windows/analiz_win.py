"""MP8a: Windows ölçüm paketinin çıktı klasörünü çözümler.

tools/olcum/mp6/analiz.py'nin koşu çözümlemesini (kos_coz) olduğu gibi kullanır; o dosya
değiştirilmez. Bu uyarlama Windows'a özgü parçaları ve hüküm kurallarını ekler:
  - deney koşullarının hükümden önce doğrulanması (bilgi.json, ozet.json; görev belgesinin
    dondurduğu değerler): deneme ya da test parametreleriyle alınmış kayıt kabul ölçümü sayılmaz,
  - sürecin temiz bitişi (kos.log 'surec_temiz'), ölçümün yazılmasından ayrı,
  - dış gözlem (pencere_win.jsonl): ölçüm aralığıyla eşleştirme, hata satırları, zaman boşlukları,
    örtülü durumda pencere geometrisi (dikdörtgen kapsaması + Z sırası + DPI bağlamı),
  - geçersiz görünür referansın çözümlemeyi durdurmaması,
  - donanım kaydının anlamsal denetimi (H1), çizim yöntemi bandı (H5).

Kullanım: python3 -I tools/olcum/windows/analiz_win.py <IDA-MP8-... klasörü>
Sonuç: <klasör>/mp8_ozet.json ve stdout'a Markdown.
"""
import json
import os
import re
import sys
import traceback

BURADA = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True  # tools/olcum/mp6/ altına __pycache__ yazılmasın (o dizine dokunulmaz)
sys.path.insert(0, os.path.join(os.path.dirname(BURADA), "mp6"))
import analiz  # noqa: E402  tools/olcum/mp6/analiz.py (değiştirilmez)

# Görev belgesinin dondurduğu deney koşulları (W0-MP8a).
KOSUL_DURUM = {"yol": "b", "sure_s": 120.0, "cozunurluk": [640, 480], "hz": 30.0, "geri": "esz", "istek": 3600}
KOSUL_OKUMA = {"yol": "b", "sure_s": 60.0, "cozunurluk": [640, 480], "hz": 30.0, "geri": "esz", "cift": True,
               "istek": 1800, "durum": "gorunur"}
SURE_TOLERANS_S = 1.0       # ölçülen ölçüm süresi ile dondurulan süre farkı
MAD_ESIK = 2.0              # H3: ortalama mutlak fark <= 2/255 (analiz.py 0..255 ölçeğinde verir)
DUSME_ESIK_YUZDE = 1.0      # H2: düşen kare < %1
AZAMI_GOZLEM_BOSLUGU_S = 15.0   # betik 10 s'de bir gözler; daha büyük boşluk kapsamı yetersiz kılar
ZAMAN_TOLERANS_S = 1.0      # 'hazir' dosyasının görülme gecikmesi (betik 0,5 s'de bir yoklar)

BANT_RE = re.compile(r"(?P<api>[^\n:]*?(?:Vulkan|D3D12|OpenGL API|Metal)[^\n]*?) - "
                     r"(?P<yontem>Forward\+|Forward Mobile|Compatibility) - Using Device[^\n]*")
UYARI_RE = re.compile(r"switching to[^\n]*|Unable to initialize[^\n]*|seem not to support[^\n]*")


def oku_metin(yol):
    if not os.path.exists(yol):
        return ""
    with open(yol, "rb") as f:
        ham = f.read()
    for kod in ("utf-8-sig", "utf-16", "cp1254", "latin-1"):
        try:
            return ham.decode(kod)
        except UnicodeDecodeError:
            continue
    return ""


def oku_json(yol):
    if not os.path.exists(yol):
        return None
    try:
        return json.loads(oku_metin(yol))
    except ValueError:
        return None


def bant(kos_dizini, onek=""):
    """Godot günlüklerinden çizim yöntemi bandı ve geri düşme uyarıları."""
    metin = ""
    for ad in ("godot_log.txt", "godot_stdout.txt", "godot_stderr.txt"):
        metin += oku_metin(os.path.join(kos_dizini, onek + ad)) + "\n"
    return {"bant": sorted({m.group(0).strip() for m in BANT_RE.finditer(metin)}),
            "yontem": sorted({m.group("yontem") for m in BANT_RE.finditer(metin)}),
            "geri_dusme_uyarilari": sorted({m.group(0).strip() for m in UYARI_RE.finditer(metin)})}


# ---------------------------------------------------------------------------
# Deney koşulları ve süreç bitişi
# ---------------------------------------------------------------------------

def kosul_denetle(kos_dizini, beklenen, durum=None):
    """Hükümden önce: bilgi.json ve ozet.json dondurulan koşulları sağlıyor mu. Uyumsuzluk listesi döner."""
    bilgi = oku_json(os.path.join(kos_dizini, "bilgi.json"))
    ozet = oku_json(os.path.join(kos_dizini, "ozet.json"))
    if bilgi is None or ozet is None:
        return ["bilgi.json ya da ozet.json okunamadi"]
    sorun = []
    for k in ("yol", "geri"):
        if bilgi.get(k) != beklenen[k]:
            sorun.append("%s=%r (beklenen %r)" % (k, bilgi.get(k), beklenen[k]))
    if list(bilgi.get("cozunurluk") or []) != beklenen["cozunurluk"]:
        sorun.append("cozunurluk=%r (beklenen %r)" % (bilgi.get("cozunurluk"), beklenen["cozunurluk"]))
    for k in ("sure_s", "hz"):
        try:
            if abs(float(bilgi.get(k)) - beklenen[k]) > 1e-9:
                sorun.append("%s=%r (beklenen %r)" % (k, bilgi.get(k), beklenen[k]))
        except (TypeError, ValueError):
            sorun.append("%s okunamadi" % k)
    if "cift" in beklenen and bool(bilgi.get("cift")) != beklenen["cift"]:
        sorun.append("cift=%r (beklenen %r)" % (bilgi.get("cift"), beklenen["cift"]))
    bek_durum = beklenen.get("durum", durum)
    if bek_durum is not None and bilgi.get("durum") != bek_durum:
        sorun.append("durum=%r (beklenen %r)" % (bilgi.get("durum"), bek_durum))
    istek = (ozet.get("sayac") or {}).get("istek")
    if istek != beklenen["istek"]:
        sorun.append("istek=%r (beklenen %r)" % (istek, beklenen["istek"]))
    try:
        olculen = (ozet["olcum_bitis_us"] - ozet["olcum_baslangic_us"]) / 1e6
        if abs(olculen - beklenen["sure_s"]) > SURE_TOLERANS_S:
            sorun.append("olculen sure %.3f s (beklenen %.0f s)" % (olculen, beklenen["sure_s"]))
    except (KeyError, TypeError):
        sorun.append("olcum zaman damgalari okunamadi")
    return sorun


def surec_bitisi(kos_dizini):
    """kos.log 'surec_temiz true|false <neden>'. Satır yoksa None (bilinmiyor)."""
    temiz = None
    neden = ""
    for s in oku_metin(os.path.join(kos_dizini, "kos.log")).splitlines():
        if s.startswith("surec_temiz "):
            parca = s.split(" ", 2)
            temiz = parca[1] == "true"
            neden = parca[2] if len(parca) > 2 else ""
    return {"temiz": temiz, "neden": neden}


# ---------------------------------------------------------------------------
# Dış gözlem
# ---------------------------------------------------------------------------

def ornek_degerlendir(w, durum):
    """Tek dış gözlem örneği: 'tutuldu' | 'tutulmadi' | 'dogrulanamadi' ve nedeni."""
    ana = w.get("ana")
    if not ana:
        return "tutulmadi", "olcum penceresi bulunamadi"
    simge = bool(ana.get("simge"))
    gorunur = bool(ana.get("gorunur"))
    nt = w.get("nokta_toplam", 0)
    if durum == "gorunur":
        if gorunur and not simge and nt > 0 and w.get("nokta_olcum", 0) == nt:
            return "tutuldu", ""
        return "tutulmadi", "gorunur=%s simge=%s nokta_olcum=%s/%s" % (gorunur, simge, w.get("nokta_olcum"), nt)
    if durum == "simge":
        return ("tutuldu", "") if simge else ("tutulmadi", "IsIconic false")
    if durum == "ortulu":
        g = w.get("geometri")
        if not g or not g.get("dogrulanabilir") or not w.get("dpi_baglami"):
            return "dogrulanamadi", "geometri dogrulanamadi (DPI baglami ya da dis cerceve yok)"
        ortu = w.get("ortu_ana") or {}
        if not (gorunur and not simge):
            return "tutulmadi", "olcum penceresi gorunur degil ya da simge"
        if not (ortu.get("gorunur") and not ortu.get("simge")):
            return "tutulmadi", "ortu penceresi gorunur degil"
        if not g.get("kapsiyor"):
            return "tutulmadi", "ortu dikdortgeni olcum penceresini kapsamiyor"
        if not g.get("ortu_ustte"):
            return "tutulmadi", "ortu Z sirasinda ustte degil"
        return "tutuldu", ""
    return "dogrulanamadi", "bilinmeyen durum"


def pencere_win(kos_dizini, durum):
    """Dış gözlem özeti ve hükmü: 'tutuldu' | 'tutulmadi' | 'dogrulanmadi'.

    Örnek zamanı (gecen_s) 'hazir' dosyasının görüldüğü andan ölçülür; ölçüm aralığı
    hazir dosyasındaki Godot saatiyle ve ozet.json'daki ölçüm başlangıç/bitişiyle aynı eksene taşınır.
    """
    yol = os.path.join(kos_dizini, "pencere_win.jsonl")
    if not os.path.exists(yol):
        return {"hukum": "dogrulanmadi", "neden": "pencere_win.jsonl yok (dis gozlem yapilamadi)", "ornek": 0}
    ozet = oku_json(os.path.join(kos_dizini, "ozet.json"))
    try:
        hazir_us = int(oku_metin(os.path.join(kos_dizini, "hazir")).strip())
        bas = (ozet["olcum_baslangic_us"] - hazir_us) / 1e6
        bit = (ozet["olcum_bitis_us"] - hazir_us) / 1e6
    except (ValueError, TypeError, KeyError):
        return {"hukum": "dogrulanmadi", "neden": "olcum araligi zaman eksenine tasinamadi (hazir/ozet)", "ornek": 0}
    bozuk = 0
    hatali_icinde = 0
    ornekler = []
    for s in oku_metin(yol).splitlines():
        if not s.strip():
            continue
        try:
            d = json.loads(s)
        except ValueError:
            bozuk += 1
            continue
        t = d.get("gecen_s")
        if t is None:
            bozuk += 1
            continue
        if not (bas - ZAMAN_TOLERANS_S <= t <= bit + ZAMAN_TOLERANS_S):
            continue
        if "hata" in d or "w32" not in d:
            hatali_icinde += 1
            ornekler.append((t, "hata", "gozlem basarisiz"))
            continue
        sonuc, neden = ornek_degerlendir(d["w32"], durum)
        ornekler.append((t, sonuc, neden))
    zamanlar = sorted(o[0] for o in ornekler)
    if zamanlar:
        bosluklar = [zamanlar[0] - bas] + [b - a for a, b in zip(zamanlar, zamanlar[1:])] + [bit - zamanlar[-1]]
        azami_bosluk = max(bosluklar)
    else:
        azami_bosluk = bit - bas
    sayim = {"tutuldu": 0, "tutulmadi": 0, "dogrulanamadi": 0, "hata": 0}
    for o in ornekler:
        sayim[o[1]] += 1
    nedenler = sorted({o[2] for o in ornekler if o[2]})
    if sayim["tutulmadi"] > 0:
        hukum, neden = "tutulmadi", "en az bir ornekte durum tutulmadi"
    elif bozuk or hatali_icinde or sayim["dogrulanamadi"]:
        hukum, neden = "dogrulanmadi", "gozlem hatasi ya da dogrulanamayan ornek"
    elif not ornekler or azami_bosluk > AZAMI_GOZLEM_BOSLUGU_S + ZAMAN_TOLERANS_S:
        hukum, neden = "dogrulanmadi", "kapsam yetersiz (en buyuk bosluk %.1f s)" % azami_bosluk
    else:
        hukum, neden = "tutuldu", ""
    return {"hukum": hukum, "neden": neden, "olcum_araligi_s": [round(bas, 2), round(bit, 2)],
            "ornek": len(ornekler), "sayim": sayim, "bozuk_satir": bozuk,
            "en_buyuk_bosluk_s": round(azami_bosluk, 2), "ornek_nedenleri": nedenler}


def surec_win(kos_dizini):
    yol = os.path.join(kos_dizini, "surec_win.csv")
    if not os.path.exists(yol):
        return {}
    satir = [s.split(",") for s in oku_metin(yol).splitlines()[1:] if s.strip()]
    if len(satir) < 2:
        return {"ornek": len(satir)}
    try:
        dt = float(satir[-1][1]) - float(satir[0][1])
        dcpu = float(satir[-1][3]) - float(satir[0][3])
        yuzde = 100.0 * dcpu / dt if dt > 0 else None
    except (ValueError, IndexError):
        yuzde = None
    return {"ornek": len(satir), "islemci_yuzde_tek_cekirdek": yuzde,
            "oncelik": sorted({s[5] for s in satir if len(s) > 5}),
            "yanit_vermedi": sum(1 for s in satir if len(s) > 6 and s[6].strip().lower() == "false")}


def kare_hizi(kos_dizini, s):
    """Gerçekleşen kare hızı: üretilen kare / ölçüm süresi; operatör penceresinin çizim hızı."""
    durum = [d for d in analiz.oku_durum(kos_dizini) if d["faz"] == "olcum"]
    ozet = oku_json(os.path.join(kos_dizini, "ozet.json"))
    sure = (ozet["olcum_bitis_us"] - ozet["olcum_baslangic_us"]) / 1e6
    cizim = [int(d["d_drawn"]) for d in durum]
    return {"olcum_suresi_s": sure,
            "uretilen_kare_hz": s["uretilen"] / sure if sure > 0 else None,
            "operator_cizim_hz_medyan": float(analiz.np.median(cizim)) if cizim else None,
            "dongu_hz_medyan": s["process_hz"]["medyan"]}


# ---------------------------------------------------------------------------
# Donanım (H1): anlamsal denetim
# ---------------------------------------------------------------------------

def _pozitif(x):
    try:
        return float(x) > 0
    except (TypeError, ValueError):
        return False


def _gecerli_yenileme(x):
    # Win32_VideoController.CurrentRefreshRate: 0 = varsayılan, 0xFFFFFFFF (-1) = "optimal"; gerçek Hz değil.
    try:
        v = int(x)
    except (TypeError, ValueError):
        return False
    return 2 <= v <= 1000


def _dolu(x):
    return isinstance(x, str) and x.strip() != ""


def donanim(kok):
    d = oku_json(os.path.join(kok, "donanim.json"))
    if d is None:
        return {"var": False, "eksik": ["donanim.json yok ya da okunamadi"]}
    eksik = []
    cpu = (d.get("islemci") or [None])[0] or {}
    if not _dolu(cpu.get("ad")):
        eksik.append("islemci modeli")
    if not _pozitif(cpu.get("cekirdek")):
        eksik.append("cekirdek sayisi")
    if not _pozitif(cpu.get("mantiksal_islemci")):
        eksik.append("is parcacigi sayisi")
    if not _pozitif((d.get("bilgisayar") or {}).get("toplam_bellek_bayt")):
        eksik.append("bellek")
    # En az bir ekran kartı kaydı bütün alanları anlamlı biçimde taşımalı.
    kartlar = d.get("ekran_karti") or []
    gecerli_kart = None
    for g in kartlar:
        g = g or {}
        if (_dolu(g.get("ad")) and _dolu(g.get("surucu_surum")) and _pozitif(g.get("cozunurluk_yatay"))
                and _pozitif(g.get("cozunurluk_dikey")) and _gecerli_yenileme(g.get("yenileme_hz"))):
            gecerli_kart = g
            break
    if gecerli_kart is None:
        ilk = (kartlar[0] if kartlar else None) or {}
        if not _dolu(ilk.get("ad")):
            eksik.append("ekran karti adi")
        if not _dolu(ilk.get("surucu_surum")):
            eksik.append("ekran karti surucu surumu")
        if not (_pozitif(ilk.get("cozunurluk_yatay")) and _pozitif(ilk.get("cozunurluk_dikey"))):
            eksik.append("ekran cozunurlugu (iki boyut)")
        if not _gecerli_yenileme(ilk.get("yenileme_hz")):
            eksik.append("yenileme hizi (gercek Hz; 0 ve 4294967295 gecersiz)")
    if not _dolu((d.get("windows_surumu") or {}).get("surum")):
        eksik.append("Windows surumu")
    if (d.get("guc") or {}).get("kaynak") not in ("Online", "Offline"):
        eksik.append("guc durumu (Online/Offline)")
    plan = d.get("guc_plani") or {}
    if not (d.get("guc_plani_cikis_kodu") == 0 and _dolu(plan.get("guid"))):
        eksik.append("guc plani (powercfg cikis kodu 0 ve GUID)")
    return {"var": True, "eksik": eksik, "gecerli_ekran_karti": (gecerli_kart or {}).get("ad"),
            "hatalar": d.get("hatalar", [])}


# ---------------------------------------------------------------------------
# Koşular
# ---------------------------------------------------------------------------

def referans_gecerli(ref):
    """Görünür referans: dosyalar okunur, koşullar dondurulan değerlerde, süreç temiz, kare üretildi."""
    if not os.path.isdir(ref):
        return False, "referans kosusu yok"
    for f in ("kare.csv", "bilgi.json", "ozet.json"):
        if not os.path.isfile(os.path.join(ref, f)):
            return False, "referansta %s yok" % f
    sorun = kosul_denetle(ref, KOSUL_DURUM, "gorunur")
    if sorun:
        return False, "referans kosul disi: " + "; ".join(sorun)
    sb = surec_bitisi(ref)
    if sb["temiz"] is not True:
        # Bilinmeyen ya da eksik süreç bitişi kabul edilmez (sol-2 Y1).
        return False, "referans surec bitisi temiz degil ya da kaydi yok: %s %s" % (sb["temiz"], sb["neden"])
    oz = oku_json(os.path.join(ref, "ozet.json"))
    if not ((oz.get("sayac") or {}).get("uretildi") or 0) > 0:
        return False, "referansta uretilen kare yok"
    return True, ""


def kos_coz_guvenli(yol, ref):
    try:
        s = analiz.kos_coz(yol, ref)
    except Exception as e:  # tek koşunun hatası diğerlerini durdurmaz
        return {"kos": os.path.basename(yol), "cozumleme_hatasi": "%s: %s" % (type(e).__name__, e),
                "iz": traceback.format_exc(limit=3)}
    s["pencere_win"] = pencere_win(yol, s["durum"])
    s["surec_win"] = surec_win(yol)
    s["surec_bitisi"] = surec_bitisi(yol)
    s["cizim"] = bant(yol)
    if os.path.exists(os.path.join(yol, "ortu_godot_log.txt")):
        s["ortu_cizim"] = bant(yol, "ortu_")
    try:
        s["kare_hizi"] = kare_hizi(yol, s)
    except Exception as e:
        s["kare_hizi"] = {"hata": str(e)}
    return s


def h2_hukmu(s, durum):
    kosul = kosul_denetle(os.path.join(s["_kd"], s["kos"]), KOSUL_DURUM, durum)
    sb = s["surec_bitisi"]
    pw = s["pencere_win"]
    dusme_ok = s["dusen_yuzde"] is not None and s["dusen_yuzde"] < DUSME_ESIK_YUZDE
    kimlik_ok = s["en_buyuk_kimlik_boslugu"] == 0
    # İç gözlem: görünürde frames_drawn ilerlemeli, simgede durmalı (bütün kayıt aralıklarında).
    # Windows'ta örtülme olağan çizimi kesmez (07 §2.6); örtülüde iç gözlem ölçüt değildir.
    dt = s["durum_tutuldu"]
    ic = None
    if durum in ("gorunur", "simge"):
        ic = bool(s["durum_dogrulandi_ic"]) and dt["toplam_aralik"] > 0 and dt["aralik"] == dt["toplam_aralik"]
    if kosul:
        h = "kosul_disi"
    elif sb["temiz"] is False:
        h = "duzenek_calismadi"
    elif pw["hukum"] == "tutulmadi" or ic is False:
        h = "gecersiz (durum tutulmadi)"
    elif pw["hukum"] == "dogrulanmadi" or sb["temiz"] is None:
        h = "dogrulanmadi"
    elif dusme_ok and kimlik_ok:
        h = "gecti"
    else:
        h = "gecmedi"
    return {"sonuc": h, "kosul_uyumsuzlugu": kosul, "surec": sb, "istek": s["istek"], "uretilen": s["uretilen"],
            "dusen_yuzde": s["dusen_yuzde"], "dusme_esigi_saglandi": dusme_ok, "kimlik_boslugu": s["en_buyuk_kimlik_boslugu"],
            "kimlik_kesintisiz": kimlik_ok, "dusme_nedenleri": s["dusme_nedenleri"], "dis_gozlem": pw,
            "ic_gozlem": {"tutuldu": ic, "metin": s["durum_dogrulama_ic"], "aralik": "%d/%d" % (dt["aralik"], dt["toplam_aralik"])},
            "frames_drawn_olcumde": s["frames_drawn_olcumde"]}


def olcutler(sonuc, hw):
    o = {"H1": {"sonuc": "gecti" if hw["var"] and not hw["eksik"] else "gecmedi", "eksik": hw["eksik"]}}
    if not sonuc["kampanyalar"]:
        for h in ("H2", "H3", "H4"):
            o[h] = {"sonuc": "degerlendirilemedi", "neden": "kampanya klasoru yok"}
    else:
        k = sonuc["kampanyalar"][0]
        kos = {s["kos"]: s for s in k["kosular"]}
        h2 = {}
        for d in ("gorunur", "simge", "ortulu"):
            s = kos.get("b_" + d)
            if s is None:
                h2[d] = {"sonuc": "degerlendirilemedi", "neden": "kosu yok"}
            elif "cozumleme_hatasi" in s:
                h2[d] = {"sonuc": "degerlendirilemedi", "neden": s["cozumleme_hatasi"]}
            else:
                h2[d] = h2_hukmu(s, d)
        o["H2"] = h2
        # Görünür referans H3 için yalnız şu koşullarda geçerlidir (sol-2 Y1): dosya ve koşul denetimi
        # (referans_gecerli), görünür H2 'gecti' (pencere durumu doğrulandı) ve süreç bitişi açıkça temiz.
        ref_h2 = h2.get("gorunur") or {}
        ref_neden = ""
        if not k["referans_gecerli"]:
            ref_neden = k["referans_nedeni"]
        elif ref_h2.get("sonuc") != "gecti":
            ref_neden = "gorunur H2 '%s' (pencere durumu dogrulanmadi ya da gecmedi)" % ref_h2.get("sonuc")
        elif (ref_h2.get("surec") or {}).get("temiz") is not True:
            ref_neden = "gorunur kosunun surec bitisi acikca temiz degil"
        h3 = {}
        for d in ("simge", "ortulu"):
            s = kos.get("b_" + d)
            if s is None or "cozumleme_hatasi" in s:
                h3[d] = {"sonuc": "degerlendirilemedi", "neden": "kosu yok ya da cozumlenemedi"}
                continue
            if ref_neden:
                h3[d] = {"sonuc": "degerlendirilemedi", "neden": "gorunur referans gecersiz: " + ref_neden}
                continue
            onkosul = h2.get(d, {}).get("sonuc")
            m = s.get("mad_255") or {"ornek": 0, "ort": None, "maks": None}
            if onkosul in ("kosul_disi", "duzenek_calismadi"):
                h = onkosul
            elif onkosul in ("gecersiz (durum tutulmadi)", "dogrulanmadi"):
                h = "dogrulanmadi (durum: %s)" % onkosul
            elif m["ornek"] == 0:
                h = "degerlendirilemedi (karsilastirilan dokum yok)"
            else:
                h = "gecti" if m["maks"] <= MAD_ESIK else "gecmedi"
            h3[d] = {"sonuc": h, "mad_ort": m["ort"], "mad_maks": m["maks"], "ornek": m["ornek"], "md5": s.get("md5_esit"),
                     "konum_hatasi_px": s["poz_hata_px"], "golge_var_yuzde": s["golge_orani"]["golge_var_yuzde"]}
        o["H3"] = h3
        s = kos.get("okuma_640x480_esz")
        if s is None or "cozumleme_hatasi" in s:
            o["H4"] = {"sonuc": "degerlendirilemedi", "neden": "okuma kosusu yok ya da cozumlenemedi"}
        else:
            kosul = kosul_denetle(os.path.join(k["dizin_yolu"], s["kos"]), KOSUL_OKUMA)
            sb = s["surec_bitisi"]
            dolu = (s["uretilen"] > 0 and s["geri_okuma_ms"]["p50"] is not None and s["geri_okuma_ms"]["p99"] is not None)
            if kosul:
                h = "kosul_disi"
            elif sb["temiz"] is False:
                h = "duzenek_calismadi"
            elif not dolu:
                h = "degerlendirilemedi (gecerli ornek yok)"
            elif sb["temiz"] is None:
                h = "raporlandi (surec bitisi kaydi yok; dogrulanmadi)"
            else:
                h = "raporlandi (esik yok)"
            o["H4"] = {"sonuc": h, "kosul_uyumsuzlugu": kosul, "surec": sb, "ilk_okuma_ms": s["geri_okuma_ms"],
                       "yinelenen_okuma_ms": s["ikinci_okuma_ms"], "istekten_hazira_ms": s["gecikme_ms"],
                       "kare_hizi": s.get("kare_hizi"), "dusen_yuzde": s["dusen_yuzde"]}
    yontemler = sorted({y for k in sonuc["kampanyalar"] for s in k["kosular"] if "cizim" in s for y in s["cizim"]["yontem"]})
    o["H5"] = {"sonuc": "kayitli" if yontemler else "kayit yok", "fiili_cizim_yontemi": yontemler,
               "secilen_deneme": sonuc["betik"].get("secilen_cizim"),
               "denemeler": [{"ad": d["ad"], "uretildi": d["uretildi"], "yontem": d["yontem"],
                              "uyari": d["geri_dusme_uyarilari"], "surec": d["surec"]} for d in sonuc["denemeler"]]}
    return o


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    kok = os.path.abspath(sys.argv[1])
    sonuc = {"kok": kok, "donanim": None, "denemeler": [], "kampanyalar": [], "olcutler": {}}
    hw = donanim(kok)
    sonuc["donanim"] = hw
    betik = oku_json(os.path.join(kok, "sonuc.json")) or {}
    sonuc["betik"] = {k: betik.get(k) for k in ("paket", "toplam_s", "kampanya_s", "secilen_cizim", "dis_gozlem", "godot", "hatali_adim")}

    for ad in sorted(os.listdir(kok)):
        yol = os.path.join(kok, ad)
        if ad.startswith("deneme_") and os.path.isdir(yol):
            oz = oku_json(os.path.join(yol, "ozet.json"))
            sonuc["denemeler"].append({"ad": ad, "ozet_var": oz is not None,
                                       "uretildi": (oz.get("sayac") or {}).get("uretildi") if oz else None,
                                       "istek": (oz.get("sayac") or {}).get("istek") if oz else None,
                                       "surec": surec_bitisi(yol), **bant(yol)})

    for kd in sorted(os.path.join(kok, d) for d in os.listdir(kok)
                     if d.startswith("kampanya_") and os.path.isdir(os.path.join(kok, d))):
        ref = os.path.join(kd, "b_gorunur")
        ref_ok, ref_neden = referans_gecerli(ref)
        kosular = []
        for ad in sorted(os.listdir(kd)):
            yol = os.path.join(kd, ad)
            if not os.path.isdir(yol):
                continue
            if not os.path.isfile(os.path.join(yol, "ozet.json")):
                kosular.append({"kos": ad, "cozumleme_hatasi": "ozet.json yok (olcum yazilmadi)",
                                "surec_bitisi": surec_bitisi(yol)})
                continue
            r = ref if (ref_ok and os.path.abspath(yol) != os.path.abspath(ref)) else None
            s = kos_coz_guvenli(yol, r)
            s["_kd"] = kd
            kosular.append(s)
        sonuc["kampanyalar"].append({"dizin": os.path.basename(kd), "dizin_yolu": kd, "referans_gecerli": ref_ok,
                                     "referans_nedeni": ref_neden, "kosular": kosular})

    sonuc["olcutler"] = olcutler(sonuc, hw)
    with open(os.path.join(kok, "mp8_ozet.json"), "w") as f:
        json.dump(sonuc, f, indent=2, ensure_ascii=False, default=str)
    yazdir(sonuc)


def f(x, n=2):
    return "-" if x is None else ("%." + str(n) + "f") % x


def yazdir(sonuc):
    o = sonuc["olcutler"]
    print("## MP8a çözümleme:", sonuc["kok"])
    print()
    print("| Ölçüt | Sonuç | Ayrıntı |")
    print("|---|---|---|")
    print("| H1 | %s | eksik: %s |" % (o["H1"]["sonuc"], ", ".join(o["H1"]["eksik"]) or "yok"))
    for d, v in (o.get("H2") or {}).items():
        if "istek" in v:
            pw = v["dis_gozlem"]
            print("| H2 %s | %s | %s/%s kare, düşen %% %s, kimlik boşluğu %s; dış: %s (%s örnek, en büyük boşluk %s s) %s; iç: %s %s; süreç: %s %s; koşul: %s |" % (
                d, v["sonuc"], v["uretilen"], v["istek"], f(v["dusen_yuzde"]), v["kimlik_boslugu"],
                pw["hukum"], pw.get("ornek"), pw.get("en_buyuk_bosluk_s", "-"), pw.get("neden", ""),
                v["ic_gozlem"]["tutuldu"], v["ic_gozlem"]["aralik"], v["surec"]["temiz"], v["surec"]["neden"],
                "; ".join(v["kosul_uyumsuzlugu"]) or "uygun"))
        else:
            print("| H2 %s | %s | %s |" % (d, v.get("sonuc"), v.get("neden", "")))
    for d, v in (o.get("H3") or {}).items():
        if "mad_maks" in v:
            md5 = v["md5"] or {}
            print("| H3 %s | %s | MAD ort/maks %s/%s (n=%d), MD5 %s/%s |" % (
                d, v["sonuc"], f(v["mad_ort"], 3), f(v["mad_maks"], 3), v["ornek"], md5.get("esit", "-"), md5.get("karsilastirilan", "-")))
        else:
            print("| H3 %s | %s | %s |" % (d, v.get("sonuc"), v.get("neden", "")))
    h4 = o.get("H4", {})
    if "ilk_okuma_ms" in h4:
        kh = h4.get("kare_hizi") or {}
        print("| H4 | %s | ilk okuma p50/p99/maks %s/%s/%s ms; yinelenen %s/%s ms; üretilen %s Hz; operatör çizim %s Hz; koşul: %s |" % (
            h4["sonuc"], f(h4["ilk_okuma_ms"]["p50"]), f(h4["ilk_okuma_ms"]["p99"]), f(h4["ilk_okuma_ms"]["maks"]),
            f(h4["yinelenen_okuma_ms"]["p50"]), f(h4["yinelenen_okuma_ms"]["p99"]),
            f(kh.get("uretilen_kare_hz")), f(kh.get("operator_cizim_hz_medyan"), 1), "; ".join(h4["kosul_uyumsuzlugu"]) or "uygun"))
    else:
        print("| H4 | %s | %s |" % (h4.get("sonuc"), h4.get("neden", "")))
    print("| H5 | %s | %s; denemeler: %s |" % (o["H5"]["sonuc"], ", ".join(o["H5"]["fiili_cizim_yontemi"]) or "-",
                                               "; ".join("%s→%s" % (d["ad"], ",".join(d["yontem"]) or "yok") for d in o["H5"]["denemeler"]) or "-"))
    print()
    for k in sonuc["kampanyalar"]:
        print("### %s (görünür referans: %s%s)" % (k["dizin"], "geçerli" if k["referans_gecerli"] else "geçersiz",
                                                    "" if k["referans_gecerli"] else " — " + k["referans_nedeni"]))
        print()
        print("| koşu | istek | üretilen | düşen % | kimlik boşluğu | gecikme p50/p99 ms | geri okuma p50/p99 ms | dış gözlem | süreç temiz | MAD ort/maks | çizim |")
        print("|---|---|---|---|---|---|---|---|---|---|---|")
        for s in k["kosular"]:
            if "cozumleme_hatasi" in s:
                print("| %s | - | - | - | - | - | - | - | %s | - | %s |" % (s["kos"], (s.get("surec_bitisi") or {}).get("temiz"), s["cozumleme_hatasi"]))
                continue
            mad = "%s/%s" % (f(s["mad_255"]["ort"], 3), f(s["mad_255"]["maks"], 3)) if "mad_255" in s else "-"
            pw = s["pencere_win"]
            print("| %s | %d | %d | %s | %d | %s/%s | %s/%s | %s (%s) | %s | %s | %s |" % (
                s["kos"], s["istek"], s["uretilen"], f(s["dusen_yuzde"]), s["en_buyuk_kimlik_boslugu"],
                f(s["gecikme_ms"]["p50"]), f(s["gecikme_ms"]["p99"]), f(s["geri_okuma_ms"]["p50"]), f(s["geri_okuma_ms"]["p99"]),
                pw["hukum"], pw.get("ornek", 0), s["surec_bitisi"]["temiz"], mad, ", ".join(s["cizim"]["yontem"]) or "-"))
        print()


if __name__ == "__main__":
    main()
