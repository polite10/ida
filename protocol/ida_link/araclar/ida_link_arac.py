"""ida_link derleme zamanı üreteçleri ve sözleşme denetimleri (W2-protokol §1, §3).

Yalnız Python standart kitaplığı kullanılır; `python3 -I` ile çağrılır. Her alt komut
başarıda 0, denetim başarısızlığında 1, kullanım hatasında 2 ile çıkar; bulgu stderr'e
yazılır.

Alt komutlar:
  tablo          ida.xml + A1 Tablo 3.3 -> mesaj tablosu (C, özel) ve dialect başlığı (C, açık)
  dialect-denetle  ida.xml'den dialect_hash ve satır sayılarını hesaplar, beklenen değer
                 dosyasıyla karşılaştırır (A1 §3.2)
  altin-cikar    A1 Ek B'deki 38 altın çerçeveyi JSON veri dosyasına çıkarır
  altin-denetle  Veri dosyasının Ek B'den yeniden çıkarılanla aynı olduğunu denetler
  altin-c        altin-denetle + veri dosyasından C test verisi (.c/.h) üretir
  ayna           protocol/ayna.json'dan A1-K03 eşitlik başlığını üretir; tamlık ve sayı denetimi
  ayna-boz       port.h'nin, ayna eşlemesindeki ilk #define değeri değiştirilmiş kopyasını yazar
                 (yalnız derleme denetiminin deneme hedefi için)
  deneme-ek-b    Ek B'nin bir baytı değiştirilmiş sözleşme kopyası (altın denetiminin denemesi)
  deneme-xml     Bir alan adı değiştirilmiş ida.xml kopyası (dialect_hash denetiminin denemesi)
  deneme-bos-altin     Ek B çerçeveleri silinmiş sözleşme + boş JSON (boş küme denemesi)
  deneme-tablo-yinele  Tablo 3.3'ün ilk satırı yinelenmiş sözleşme (yinelenen kimlik denemesi)

Denetim hataları stderr'de makinece aranabilir bir etiketle başlar: [DIALECT_FARKI],
[ALTIN_FARKI], [ALTIN_SAYISI], [TABLO_YINELENEN], [AYNA_DENETIMI].

Sözleşme metni (Markdown) yalnız okunur; ayrıştırma desenleri sözleşmenin bugünkü
biçimine bağlıdır ve biçim değişirse denetim açık bir hatayla durur (sessizce geçmez).
"""

import argparse
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

# ---------------------------------------------------------------------------------------
# Ortak
# ---------------------------------------------------------------------------------------

TIP_BOYU = {
    "uint64_t": 8, "int64_t": 8, "double": 8,
    "uint32_t": 4, "int32_t": 4, "float": 4,
    "uint16_t": 2, "int16_t": 2,
    "uint8_t": 1, "int8_t": 1, "char": 1,
}


class DenetimHatasi(Exception):
    """Sözleşme ya da veri tutarsızlığı (çıkış kodu 1)."""


def oku(yol):
    with open(yol, "r", encoding="utf-8") as f:
        return f.read()


def yaz_degisirse(yol, metin):
    """Dosyayı yalnız içerik değiştiyse yazar (gereksiz yeniden derlemeyi önler)."""
    if os.path.isfile(yol) and oku(yol) == metin:
        return
    os.makedirs(os.path.dirname(os.path.abspath(yol)), exist_ok=True)
    with open(yol, "w", encoding="utf-8", newline="\n") as f:
        f.write(metin)


def x25_biriktir(veri, crc=0xFFFF):
    """CRC-16/MCRF4XX (MAVLink X.25), A1 §3.2."""
    for b in veri:
        tmp = b ^ (crc & 0xFF)
        tmp = (tmp ^ (tmp << 4)) & 0xFF
        crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return crc


# ---------------------------------------------------------------------------------------
# ida.xml
# ---------------------------------------------------------------------------------------

class Alan:
    def __init__(self, tip, ad, dizi):
        self.tip = tip      # dizi işaretsiz temel tip
        self.ad = ad
        self.dizi = dizi    # dizi değilse 0

    @property
    def boy(self):
        return TIP_BOYU[self.tip] * (self.dizi if self.dizi else 1)


class Mesaj:
    def __init__(self, kimlik, ad, alanlar, aciklama):
        self.kimlik = kimlik
        self.ad = ad
        self.alanlar = alanlar          # XML sırası
        self.aciklama = aciklama

    @property
    def tel_sirasi(self):
        # Boya göre büyükten küçüğe; aynı boyda XML sırası korunur (sorted kararlıdır).
        return sorted(self.alanlar, key=lambda a: -TIP_BOYU[a.tip])

    @property
    def yuk_azami(self):
        return sum(a.boy for a in self.alanlar)

    @property
    def crc_extra(self):
        crc = x25_biriktir((self.ad + " ").encode("ascii"))
        for a in self.tel_sirasi:
            crc = x25_biriktir((a.tip + " ").encode("ascii"), crc)
            crc = x25_biriktir((a.ad + " ").encode("ascii"), crc)
            if a.dizi:
                crc = x25_biriktir(bytes([a.dizi]), crc)
        return (crc & 0xFF) ^ (crc >> 8)


def xml_oku(yol):
    kok = ET.parse(yol).getroot()
    if kok.find("include") is not None:
        raise DenetimHatasi("ida.xml başka dialect içeriyor; A1 §3.2 içermez")
    mesajlar = []
    for m in kok.iter("message"):
        alanlar = []
        for c in m:
            if c.tag == "extensions":
                raise DenetimHatasi(f"{m.get('name')}: <extensions/> A1'de yok (A1 §3.2)")
            if c.tag != "field":
                continue
            tip = c.get("type")
            mt = re.fullmatch(r"([a-z0-9_]+)(?:\[(\d+)\])?", tip or "")
            if not mt or mt.group(1) not in TIP_BOYU:
                raise DenetimHatasi(f"{m.get('name')}.{c.get('name')}: tanınmayan tip {tip}")
            alanlar.append(Alan(mt.group(1), c.get("name"), int(mt.group(2) or 0)))
        acik = m.find("description")
        mesajlar.append(Mesaj(int(m.get("id")), m.get("name"), alanlar,
                              (acik.text or "") if acik is not None else ""))
    mesajlar.sort(key=lambda x: x.kimlik)
    for onceki, sonraki in zip(mesajlar, mesajlar[1:]):
        if onceki.kimlik == sonraki.kimlik:
            raise DenetimHatasi(f"[TABLO_YINELENEN] ida.xml'de kimlik {onceki.kimlik} birden çok "
                                f"mesajda ({onceki.ad}, {sonraki.ad})")
    numaralandirmalar = {}
    for e in kok.iter("enum"):
        girdiler = []
        for g in e.iter("entry"):
            deger = g.get("value")
            if deger is None:
                raise DenetimHatasi(f"{e.get('name')}.{g.get('name')}: değersiz girdi")
            girdiler.append((g.get("name"), int(deger, 0)))
        numaralandirmalar[e.get("name")] = girdiler
    return mesajlar, numaralandirmalar


def kanonik_metin(mesajlar, numaralandirmalar):
    """A1 §3.2 dialect_hash kanonik metni ve satır sayıları."""
    satirlar = []
    n_alan = 0
    for m in mesajlar:
        satirlar.append(f"M {m.kimlik} {m.ad} {m.crc_extra} {m.yuk_azami}")
        for a in m.tel_sirasi:
            satirlar.append(f"F {a.tip} {a.ad} {a.dizi}")
            n_alan += 1
    n_girdi = 0
    for ad in sorted(numaralandirmalar, key=lambda s: s.encode("ascii")):
        for g, d in sorted(numaralandirmalar[ad], key=lambda t: t[1]):
            satirlar.append(f"E {ad} {g} {d}")
            n_girdi += 1
    metin = "".join(s + "\n" for s in satirlar)
    return metin, len(mesajlar), n_alan, n_girdi


def dialect_hash(metin):
    return int.from_bytes(hashlib.sha256(metin.encode("ascii")).digest()[:8], "little")


# ---------------------------------------------------------------------------------------
# A1 sözleşme metni
# ---------------------------------------------------------------------------------------

YON_KODU = {"sim → kart": "IDA_LINK_YON_SIM_KART",
            "kart → sim": "IDA_LINK_YON_KART_SIM",
            "iki yön": "IDA_LINK_YON_IKI"}
XML_YON = {"Sim -> kart.": "sim → kart", "Kart -> sim.": "kart → sim", "Iki yon.": "iki yön"}


def bolum(metin, baslik, sonraki_desen):
    """`baslik` satırından sonraki ilk `sonraki_desen` satırına kadar olan metin."""
    i = metin.find("\n" + baslik + "\n")
    if i < 0:
        raise DenetimHatasi(f"sözleşmede bölüm bulunamadı: {baslik}")
    govde = metin[i + len(baslik) + 2:]
    m = re.search(sonraki_desen, govde, re.M)
    return govde[:m.start()] if m else govde


def tablo_3_3(sozlesme):
    govde = bolum(sozlesme, "### 3.3 Mesaj listesi", r"^### ")
    satir_deseni = re.compile(
        r"^\| (\d+) \| `(IDA_[A-Z0-9_]+)` \| (sim → kart|kart → sim|iki yön) \|[^|]*\|"
        r" (\d+) / (\d+) \|", re.M)
    sonuc = {}
    for m in satir_deseni.finditer(govde):
        kimlik = int(m.group(1))
        if kimlik in sonuc:
            raise DenetimHatasi(f"[TABLO_YINELENEN] A1 Tablo 3.3'te kimlik {kimlik} birden çok "
                                f"satırda ({sonuc[kimlik][0]}, {m.group(2)})")
        sonuc[kimlik] = (m.group(2), m.group(3), int(m.group(4)), int(m.group(5)))
    if not sonuc:
        raise DenetimHatasi("A1 Tablo 3.3 satırları ayrıştırılamadı")
    return sonuc


def ek_b(sozlesme):
    """A1 Ek B'deki altın çerçeveler (sözleşmedeki sırayla)."""
    govde = bolum(sozlesme, "## Ek B. Altın bayt dizileri", r"^## ")
    bolunmus = re.split(r"^### ", govde, flags=re.M)
    parcalar_oncesi, parcalar = bolunmus[0], bolunmus[1:]
    baslik_deseni = re.compile(
        r"- Başlık: seq = (\d+), sysid = (\d+), compid = (\d+); CRC_EXTRA = (\d+); "
        r"yük azami (\d+) B, çerçeve azami (\d+) B; bu dizide yük (\d+) B, çerçeve (\d+) B\.")
    cerceveler = []
    for p in parcalar:
        ilk = p.split("\n", 1)[0].strip()
        mb = re.match(r"(\d+) (IDA_[A-Z0-9_]+)( — Ek vaka.*)?$", ilk)
        if not mb:
            raise DenetimHatasi(f"Ek B başlığı ayrıştırılamadı: {ilk!r}")
        mh = baslik_deseni.search(p)
        if not mh:
            raise DenetimHatasi(f"Ek B '{ilk}': başlık satırı ayrıştırılamadı")
        bloklar = re.findall(r"^```\n(.*?)^```", p, flags=re.M | re.S)
        if len(bloklar) != 1:
            raise DenetimHatasi(f"Ek B '{ilk}': tam bir bayt bloğu beklenir, {len(bloklar)} var")
        ham = bloklar[0].split()
        if not all(re.fullmatch(r"[0-9a-f]{2}", h) for h in ham):
            raise DenetimHatasi(f"Ek B '{ilk}': bayt bloğunda onaltılık olmayan öğe")
        (seq, sysid, compid, crc_extra, yuk_azami, cerceve_azami,
         yuk, cerceve) = (int(x) for x in mh.groups())
        kayit = {
            "msgid": int(mb.group(1)), "ad": mb.group(2), "ek_vaka": bool(mb.group(3)),
            "seq": seq, "sysid": sysid, "compid": compid, "crc_extra": crc_extra,
            "yuk_azami": yuk_azami, "cerceve_azami": cerceve_azami,
            "yuk": yuk, "cerceve": cerceve, "bayt": " ".join(ham),
        }
        # Ek B'nin kendi iç tutarlılığı (başlık satırı ile bayt dizisi).
        b = bytes(int(h, 16) for h in ham)
        if len(b) != cerceve or cerceve != yuk + 12 or cerceve_azami != yuk_azami + 12:
            raise DenetimHatasi(f"Ek B '{ilk}': boy tutarsız ({len(b)} B)")
        if (b[0] != 0xFD or b[1] != yuk or b[4] != seq or b[5] != sysid or b[6] != compid
                or int.from_bytes(b[7:10], "little") != kayit["msgid"]):
            raise DenetimHatasi(f"Ek B '{ilk}': başlık baytları başlık satırıyla tutarsız")
        if any(k["msgid"] == kayit["msgid"] and k["ek_vaka"] == kayit["ek_vaka"]
               for k in cerceveler):
            raise DenetimHatasi(f"[ALTIN_SAYISI] Ek B '{ilk}' yinelenmiş")
        cerceveler.append(kayit)
    # Boş küme kabul edilmez; sayı Ek B'nin giriş paragrafındaki "toplam N çerçeve"ye bağlıdır.
    if not cerceveler:
        raise DenetimHatasi("[ALTIN_SAYISI] Ek B'de hiç altın çerçeve yok")
    mt = re.search(r"toplam (\d+) çerçeve", parcalar_oncesi)
    if not mt:
        raise DenetimHatasi("[ALTIN_SAYISI] Ek B girişinde 'toplam N çerçeve' bulunamadı")
    if int(mt.group(1)) != len(cerceveler):
        raise DenetimHatasi(f"[ALTIN_SAYISI] Ek B {mt.group(1)} çerçeve bildiriyor, "
                            f"{len(cerceveler)} çerçeve ayrıştırıldı")
    return cerceveler


def k03_numaralandirmalari(sozlesme):
    m = re.search(r"^- \*\*A1-K03\.\*\* (.*)$", sozlesme, re.M)
    if not m:
        raise DenetimHatasi("A1-K03 kuralı bulunamadı")
    # Kuralın ilk parantezindeki liste (numaralandırma adları ters tırnak içinde).
    liste = m.group(1).split(")", 1)[0]
    adlar = re.findall(r"`(IDAW_[A-Z0-9_]+)`", liste)
    if not adlar:
        raise DenetimHatasi("A1-K03 numaralandırma listesi ayrıştırılamadı")
    return adlar


def k03_esitlik_sayisi(sozlesme):
    """A1 §7.1'in K03 satırındaki (7.3'e atıflı) eşitlik sayısı."""
    m = re.search(r"^\| K03 \|.*?(\d+) eşitlik", sozlesme, re.M)
    if not m:
        raise DenetimHatasi("A1 §7.1 K03 satırında eşitlik sayısı bulunamadı")
    return int(m.group(1))


# ---------------------------------------------------------------------------------------
# Alt komutlar
# ---------------------------------------------------------------------------------------

def k_tablo(a):
    mesajlar, numaralandirmalar = xml_oku(a.xml)
    t33 = tablo_3_3(oku(a.sozlesme))
    xml_kimlikler = {m.kimlik for m in mesajlar}
    if set(t33) != xml_kimlikler:
        raise DenetimHatasi(
            f"Tablo 3.3 ile ida.xml mesaj kümeleri farklı: yalnız tabloda "
            f"{sorted(set(t33) - xml_kimlikler)}, yalnız XML'de {sorted(xml_kimlikler - set(t33))}")
    satirlar = []
    for m in mesajlar:
        ad, yon, yuk, cerceve = t33[m.kimlik]
        if ad != m.ad:
            raise DenetimHatasi(f"{m.kimlik}: Tablo 3.3 adı {ad}, XML adı {m.ad}")
        if yuk != m.yuk_azami or cerceve != m.yuk_azami + 12:
            raise DenetimHatasi(f"{m.ad}: Tablo 3.3 boyu {yuk}/{cerceve}, XML'den {m.yuk_azami}")
        xml_yon = next((v for k, v in XML_YON.items() if m.aciklama.startswith(k)), None)
        if xml_yon != yon:
            raise DenetimHatasi(f"{m.ad}: Tablo 3.3 yönü '{yon}', XML açıklaması '{xml_yon}'")
        if not 1 <= m.yuk_azami <= 255:
            raise DenetimHatasi(f"{m.ad}: yük boyu {m.yuk_azami} MAVLink 2 sınırı dışında")
        satirlar.append((m, YON_KODU[yon]))

    metin, n_m, n_f, n_e = kanonik_metin(mesajlar, numaralandirmalar)
    h = dialect_hash(metin)
    en_buyuk = max(m.yuk_azami for m in mesajlar)

    t = ["/* Üretilmiş dosya (protocol/ida_link/araclar/ida_link_arac.py tablo); değiştirmeyin.",
         " * Kaynak: protocol/ida.xml (alanlar, boylar) ve A1 Tablo 3.3 (yön).",
         " * Her satır: kimlik, CRC_EXTRA, en büyük yük, üretilmiş yapı tipi, yön. */",
         "#ifndef IDA_LINK_TABLO_URETILEN_H", "#define IDA_LINK_TABLO_URETILEN_H", "",
         "#define IDA_LINK_TABLO_SATIRLARI(X) \\"]
    for m, yon in satirlar:
        u = "MAVLINK_MSG_ID_" + m.ad
        t.append(f"    X({u}, {u}_CRC, {u}_LEN, mavlink_{m.ad.lower()}_t, {yon}) \\")
    t.append("")
    t.append(f"#define IDA_LINK_TABLO_SAYISI {len(satirlar)}u")
    t.append("")
    t.append("/* Üretecin XML'den bağımsız hesabı ile pymavlink'in ürettiği değerler aynı olmalı. */")
    t.append("#define IDA_LINK_TABLO_CAPRAZ_DENETIM(D) \\")
    for m, _ in satirlar:
        u = "MAVLINK_MSG_ID_" + m.ad
        t.append(f"    D({u} == {m.kimlik} && {u}_CRC == {m.crc_extra} && {u}_LEN == {m.yuk_azami}"
                 f" && {u}_MIN_LEN == {u}_LEN, {m.ad}) \\")
    t.append("")
    t.append("#endif")
    yaz_degisirse(a.cikti_tablo, "\n".join(t) + "\n")

    d = ["/* Üretilmiş dosya (protocol/ida_link/araclar/ida_link_arac.py tablo); değiştirmeyin.",
         " *",
         " * dialect_hash: protocol/ida.xml'den A1 §3.2'deki kanonik metinle derleme sırasında",
         " * hesaplanır (SHA-256'nın ilk 8 baytı, küçük uçlu). Beklenen değer",
         " * protocol/ida_link/veri/dialect_beklenen.json'dadır ve ayrı bir test karşılaştırır. */",
         "#ifndef IDA_LINK_DIALECT_H", "#define IDA_LINK_DIALECT_H", "",
         "#include <stdint.h>", "",
         f"#define IDA_LINK_DIALECT_HASH UINT64_C(0x{h:016X})",
         f"#define IDA_LINK_DIALECT_MESAJ_SATIRI {n_m}u",
         f"#define IDA_LINK_DIALECT_ALAN_SATIRI {n_f}u",
         f"#define IDA_LINK_DIALECT_GIRDI_SATIRI {n_e}u",
         "/* Dialect'in en büyük yükü (B); alıcı tamponu MAVLink 2 sınırı (255) ile kurulur. */",
         f"#define IDA_LINK_DIALECT_YUK_AZAMI {en_buyuk}u", "",
         "#endif"]
    yaz_degisirse(a.cikti_dialect, "\n".join(d) + "\n")
    return 0


def k_dialect_denetle(a):
    mesajlar, numaralandirmalar = xml_oku(a.xml)
    metin, n_m, n_f, n_e = kanonik_metin(mesajlar, numaralandirmalar)
    h = dialect_hash(metin)
    with open(a.beklenen, "r", encoding="utf-8") as f:
        b = json.load(f)
    hesap = {"dialect_hash": f"0x{h:016X}", "mesaj_satiri": n_m, "alan_satiri": n_f,
             "girdi_satiri": n_e, "toplam_satir": n_m + n_f + n_e}
    # Betiğin XML'deki her alanı ve girdiyi metne aldığının ayrı sayımı (A1 §7.3).
    kok = ET.parse(a.xml).getroot()
    xml_alan = sum(1 for m in kok.iter("message") for _ in m.iter("field"))
    xml_girdi = sum(1 for _ in kok.iter("entry"))
    hatalar = []
    if (n_f, n_e) != (xml_alan, xml_girdi):
        hatalar.append(f"kanonik metin XML'in {xml_alan} alanını ve {xml_girdi} girdisini içermiyor")
    for k, v in hesap.items():
        bv = b.get(k)
        if isinstance(v, str):
            esit = isinstance(bv, str) and bv.upper() == v.upper()
        else:
            esit = bv == v
        if not esit:
            hatalar.append(f"{k}: hesaplanan {v}, beklenen {bv}")
    if a.baslik:
        # Derlenen sabitin (üretilmiş ida_link/dialect.h) da aynı değer olduğu.
        mb = re.search(r"#define IDA_LINK_DIALECT_HASH UINT64_C\(0x([0-9A-Fa-f]{16})\)",
                       oku(a.baslik))
        if not mb or int(mb.group(1), 16) != h:
            hatalar.append(f"derlenen başlık {a.baslik} hesaplanan değeri taşımıyor")
    print(f"dialect_hash = {hesap['dialect_hash']}; satır: {n_m} M + {n_f} F + {n_e} E = "
          f"{n_m + n_f + n_e}")
    if hatalar:
        raise DenetimHatasi("[DIALECT_FARKI] dialect_hash denetimi başarısız: " + "; ".join(hatalar))
    return 0


def k_altin_cikar(a):
    c = ek_b(oku(a.sozlesme))
    veri = {
        "kaynak": "docs/sozlesmeler/A1-kart-baglantisi.md, Ek B",
        "uretici": "protocol/ida_link/araclar/ida_link_arac.py altin-cikar",
        "aciklama": "Elle değiştirilmez; Ek B değişirse yeniden çıkarılır ve denetim bunu her "
                    "derlemede karşılaştırır.",
        "cerceveler": c,
    }
    yaz_degisirse(a.cikti, json.dumps(veri, ensure_ascii=False, indent=2) + "\n")
    print(f"{len(c)} çerçeve yazıldı: {a.cikti}")
    return 0


def altin_karsilastir(sozlesme_yolu, json_yolu):
    c = ek_b(oku(sozlesme_yolu))
    with open(json_yolu, "r", encoding="utf-8") as f:
        veri = json.load(f)
    kayitli = veri.get("cerceveler")
    if not isinstance(kayitli, list) or not kayitli:
        raise DenetimHatasi(f"[ALTIN_SAYISI] altın veri dosyası {json_yolu}: çerçeve listesi boş "
                            f"ya da yok")
    if kayitli != c:
        ayrinti = ""
        if isinstance(kayitli, list):
            for i, (x, y) in enumerate(zip(kayitli, c)):
                if x != y:
                    ayrinti = f" (ilk fark {i}. çerçevede: {y.get('msgid')} {y.get('ad')})"
                    break
            else:
                ayrinti = f" (çerçeve sayısı: veri {len(kayitli)}, Ek B {len(c)})"
        raise DenetimHatasi(
            f"[ALTIN_FARKI] altın veri dosyası {json_yolu} sözleşmenin Ek B'siyle aynı "
            f"değil{ayrinti}")
    return c


def k_altin_denetle(a):
    c = altin_karsilastir(a.sozlesme, a.json)
    print(f"altın veri dosyası Ek B ile aynı: {len(c)} çerçeve")
    return 0


def k_altin_c(a):
    c = altin_karsilastir(a.sozlesme, a.json)
    h = ["/* Üretilmiş dosya (ida_link_arac.py altin-c); değiştirmeyin.",
         " * Kaynak: protocol/ida_link/veri/altin.json (A1 Ek B'den çıkarılmış ve her derlemede",
         " * Ek B ile karşılaştırılmış). Yalnız testler ve ölçüm programı kullanır. */",
         "#ifndef IDA_LINK_ALTIN_VERI_H", "#define IDA_LINK_ALTIN_VERI_H", "",
         "#include <stddef.h>", "#include <stdint.h>", "",
         "typedef struct {",
         "    uint32_t msgid;",
         "    const char *ad;",
         "    uint8_t ek_vaka;      /* 1: A1-K51 ek vakası (sıfır Ping360 parçası) */",
         "    uint8_t seq, sysid, compid, crc_extra;",
         "    uint16_t yuk_azami, cerceve_azami, yuk, cerceve;",
         "    const uint8_t *bayt;  /* cerceve bayt */",
         "} ida_link_altin_t;", "",
         f"#define IDA_LINK_ALTIN_SAYISI {len(c)}u", "",
         "extern const ida_link_altin_t ida_link_altin[IDA_LINK_ALTIN_SAYISI];", "",
         "#endif"]
    s = ["/* Üretilmiş dosya (ida_link_arac.py altin-c); değiştirmeyin. */",
         '#include "ida_link_altin_veri.h"', ""]
    for i, k in enumerate(c):
        b = [f"0x{x}u" for x in k["bayt"].split()]
        s.append(f"static const uint8_t altin_{i}[{len(b)}] = {{")
        for j in range(0, len(b), 12):
            s.append("    " + ", ".join(b[j:j + 12]) + ",")
        s.append("};")
    s.append("")
    s.append("const ida_link_altin_t ida_link_altin[IDA_LINK_ALTIN_SAYISI] = {")
    for i, k in enumerate(c):
        s.append(f"    {{{k['msgid']}u, \"{k['ad']}\", {int(k['ek_vaka'])}u, {k['seq']}u, "
                 f"{k['sysid']}u, {k['compid']}u, {k['crc_extra']}u, {k['yuk_azami']}u, "
                 f"{k['cerceve_azami']}u, {k['yuk']}u, {k['cerceve']}u, altin_{i}}},")
    s.append("};")
    yaz_degisirse(a.cikti_h, "\n".join(h) + "\n")
    yaz_degisirse(a.cikti_c, "\n".join(s) + "\n")
    return 0


def ayna_oku_ve_denetle(xml_yolu, ayna_yolu, sozlesme_yolu, port_yolu):
    _, numaralandirmalar = xml_oku(xml_yolu)
    with open(ayna_yolu, "r", encoding="utf-8") as f:
        ayna = json.load(f)
    sozlesme = oku(sozlesme_yolu)
    port = oku(port_yolu)
    hatalar = []

    k03 = k03_numaralandirmalari(sozlesme)
    if ayna.get("numaralandirmalar") != k03:
        hatalar.append(f"ayna.json numaralandırma listesi A1-K03'teki listeyle aynı değil "
                       f"(K03: {k03})")
    gruplar = ayna.get("gruplar")
    if not isinstance(gruplar, list):
        raise DenetimHatasi("ayna.json: 'gruplar' listesi yok")
    if [g.get("numaralandirma") for g in gruplar] != ayna.get("numaralandirmalar"):
        hatalar.append("ayna.json: grupların sırası ya da kümesi 'numaralandirmalar' ile aynı değil")

    ciftler = []
    gorulen = {}
    for g in gruplar:
        ad = g.get("numaralandirma")
        if ad not in numaralandirmalar:
            hatalar.append(f"{ad}: ida.xml'de yok")
            continue
        xml_girdi = dict(numaralandirmalar[ad])
        beklenen = {n for n, d in numaralandirmalar[ad]
                    if ad != "IDAW_REASON" or 0 <= d <= 15}
        for c in g.get("girdiler", []):
            w, i = c.get("idaw"), c.get("ida")
            if not (isinstance(w, str) and isinstance(i, str)
                    and re.fullmatch(r"IDAW_[A-Z0-9_]+", w) and re.fullmatch(r"IDA_[A-Z0-9_]+", i)):
                hatalar.append(f"{ad}: geçersiz çift {c!r}")
                continue
            gorulen[w] = gorulen.get(w, 0) + 1
            if w not in xml_girdi:
                hatalar.append(f"{ad}: {w} XML'de bu numaralandırmanın girdisi değil")
            elif w not in beklenen:
                hatalar.append(f"{ad}: {w} = {xml_girdi[w]} eşleme kapsamı dışında "
                               f"(IDAW_REASON yalnız 0–15)")
            if not re.search(r"(?<![A-Za-z0-9_])" + re.escape(i) + r"(?![A-Za-z0-9_])", port):
                hatalar.append(f"{ad}: {i} port.h'te yok")
            ciftler.append((w, i))
        for n in sorted(beklenen):
            if gorulen.get(n, 0) == 0:
                hatalar.append(f"{ad}: XML girdisi {n} eşlemede yok")
    for w, n in gorulen.items():
        if n != 1:
            hatalar.append(f"{w} eşlemede {n} kez geçiyor (tam bir kez olmalı)")

    beklenen_sayi = k03_esitlik_sayisi(sozlesme)
    if len(ciftler) != beklenen_sayi:
        hatalar.append(f"eşitlik sayısı {len(ciftler)}, sözleşmedeki sayı {beklenen_sayi}")
    if hatalar:
        raise DenetimHatasi("[AYNA_DENETIMI] ayna eşleme denetimi başarısız:\n  "
                            + "\n  ".join(hatalar))
    return ciftler, len(ayna.get("numaralandirmalar"))


def k_ayna(a):
    ciftler, n_num = ayna_oku_ve_denetle(a.xml, a.ayna, a.sozlesme, a.port)
    s = ["/* Üretilmiş dosya (ida_link_arac.py ayna); değiştirmeyin.",
         " * Kaynak: protocol/ayna.json (A1-K03). Bu başlık, üretilmiş MAVLink başlığı ve",
         " * core/include/ida/port.h ile aynı çeviri biriminde içerilir; her satır bir",
         " * IDAW_ = IDA_ eşitliğini derleme zamanında denetler (C17 ve C++20). */",
         "#ifndef IDA_LINK_AYNA_ESITLIK_H", "#define IDA_LINK_AYNA_ESITLIK_H", "",
         "#ifdef __cplusplus",
         "#define IDA_LINK_AYNA_ESIT(w, a) \\",
         "    static_assert((long long)(w) == (long long)(a), \"ayna_esitsizligi: \" #w \" != \" #a)",
         "#else",
         "#define IDA_LINK_AYNA_ESIT(w, a) \\",
         "    _Static_assert((long long)(w) == (long long)(a), \"ayna_esitsizligi: \" #w \" != \" #a)",
         "#endif", "",
         f"#define IDA_LINK_AYNA_NUMARALANDIRMA_SAYISI {n_num}",
         f"#define IDA_LINK_AYNA_ESITLIK_SAYISI {len(ciftler)}", ""]
    for w, i in ciftler:
        s.append(f"IDA_LINK_AYNA_ESIT({w}, {i});")
    s += ["", "#endif"]
    yaz_degisirse(a.cikti, "\n".join(s) + "\n")
    print(f"ayna: {n_num} numaralandırma, {len(ciftler)} eşitlik")
    return 0


def k_ayna_boz(a):
    with open(a.ayna, "r", encoding="utf-8") as f:
        ayna = json.load(f)
    port = oku(a.port)
    for g in ayna["gruplar"]:
        for c in g["girdiler"]:
            desen = re.compile(r"^(#define[ \t]+" + re.escape(c["ida"]) + r"[ \t]+)(\d+)u",
                               re.M)
            m = desen.search(port)
            if m:
                yeni = desen.sub(lambda x: x.group(1) + str(int(x.group(2)) + 101) + "u",
                                 port, count=1)
                yaz_degisirse(a.cikti, yeni)
                print(f"bozulan değer: {c['ida']} ({m.group(2)} -> {int(m.group(2)) + 101})")
                return 0
    raise DenetimHatasi("ayna eşlemesinde #define ile tanımlı port.h adı bulunamadı")


def k_deneme_ek_b(a):
    """Ek B'nin ilk bayt bloğunda bir yük baytını değiştirilmiş sözleşme kopyası (deneme)."""
    metin = oku(a.sozlesme)
    i = metin.find("\n## Ek B. Altın bayt dizileri\n")
    j = metin.find("\n```\nfd ", i) if i >= 0 else -1
    if j < 0:
        raise DenetimHatasi("Ek B bayt bloğu bulunamadı")
    # Blok "fd" ile başlar; 12. bayt (dizin 11) yüktedir: bloğun başından 11 * 3 karakter.
    k = j + len("\n```\n") + 11 * 3
    eski = metin[k:k + 2]
    yeni = f"{(int(eski, 16) ^ 0x01):02x}"
    yaz_degisirse(a.cikti, metin[:k] + yeni + metin[k + 2:])
    print(f"Ek B'nin ilk dizisinde 12. bayt {eski} -> {yeni}")
    return 0


def k_deneme_xml(a):
    """ida.xml'de ilk alanın adı değiştirilmiş kopya (deneme)."""
    metin = oku(a.xml)
    m = re.search(r'(<field [^>]*name=")([a-z0-9_]+)(")', metin)
    if not m:
        raise DenetimHatasi("ida.xml'de alan bulunamadı")
    yaz_degisirse(a.cikti, metin[:m.start(2)] + m.group(2) + "_x" + metin[m.end(2):])
    print(f"ilk alan adı {m.group(2)} -> {m.group(2)}_x")
    return 0


def k_deneme_bos_altin(a):
    """Ek B'nin çerçeve bölümleri silinmiş sözleşme kopyası ve boş çerçeve listeli JSON (deneme).
    Giriş paragrafı korunur; denetim boş kümeyi reddetmelidir."""
    metin = oku(a.sozlesme)
    bas = metin.find("\n## Ek B. Altın bayt dizileri\n")
    if bas < 0:
        raise DenetimHatasi("Ek B bulunamadı")
    ilk = metin.find("\n### ", bas)
    son = metin.find("\n## ", bas + 1)
    if ilk < 0 or son < 0 or ilk > son:
        raise DenetimHatasi("Ek B bölümleri bulunamadı")
    yaz_degisirse(a.cikti_md, metin[:ilk] + "\n" + metin[son:])
    yaz_degisirse(a.cikti_json, json.dumps({"cerceveler": []}) + "\n")
    print("Ek B çerçeveleri silindi; JSON listesi boş")
    return 0


def k_deneme_tablo_yinele(a):
    """A1 Tablo 3.3'ün ilk satırı yinelenmiş sözleşme kopyası (deneme)."""
    metin = oku(a.sozlesme)
    bas = metin.find("\n### 3.3 Mesaj listesi\n")
    if bas < 0:
        raise DenetimHatasi("Tablo 3.3 bulunamadı")
    m = re.compile(r"^\| \d+ \| `IDA_[A-Z0-9_]+` \|.*$", re.M).search(metin, bas)
    if not m:
        raise DenetimHatasi("Tablo 3.3 satırı bulunamadı")
    yaz_degisirse(a.cikti, metin[:m.end()] + "\n" + m.group(0) + metin[m.end():])
    print("Tablo 3.3'ün ilk satırı yinelendi")
    return 0


def main(argv):
    for akis in (sys.stdout, sys.stderr):
        # -I altında PYTHONIOENCODING okunmaz; Windows kod sayfasında Türkçe metin kırılmasın.
        if hasattr(akis, "reconfigure"):
            akis.reconfigure(encoding="utf-8", errors="replace")
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    alt = p.add_subparsers(dest="komut", required=True)

    k = alt.add_parser("tablo")
    k.add_argument("--xml", required=True)
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--cikti-tablo", required=True)
    k.add_argument("--cikti-dialect", required=True)
    k.set_defaults(f=k_tablo)

    k = alt.add_parser("dialect-denetle")
    k.add_argument("--xml", required=True)
    k.add_argument("--beklenen", required=True)
    k.add_argument("--baslik", help="üretilmiş ida_link/dialect.h (isteğe bağlı)")
    k.set_defaults(f=k_dialect_denetle)

    k = alt.add_parser("deneme-ek-b")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_deneme_ek_b)

    k = alt.add_parser("deneme-bos-altin")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--cikti-md", required=True)
    k.add_argument("--cikti-json", required=True)
    k.set_defaults(f=k_deneme_bos_altin)

    k = alt.add_parser("deneme-tablo-yinele")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_deneme_tablo_yinele)

    k = alt.add_parser("deneme-xml")
    k.add_argument("--xml", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_deneme_xml)

    k = alt.add_parser("altin-cikar")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_altin_cikar)

    k = alt.add_parser("altin-denetle")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--json", required=True)
    k.set_defaults(f=k_altin_denetle)

    k = alt.add_parser("altin-c")
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--json", required=True)
    k.add_argument("--cikti-c", required=True)
    k.add_argument("--cikti-h", required=True)
    k.set_defaults(f=k_altin_c)

    k = alt.add_parser("ayna")
    k.add_argument("--xml", required=True)
    k.add_argument("--ayna", required=True)
    k.add_argument("--sozlesme", required=True)
    k.add_argument("--port", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_ayna)

    k = alt.add_parser("ayna-boz")
    k.add_argument("--ayna", required=True)
    k.add_argument("--port", required=True)
    k.add_argument("--cikti", required=True)
    k.set_defaults(f=k_ayna_boz)

    a = p.parse_args(argv[1:])
    try:
        return a.f(a)
    except DenetimHatasi as e:
        print(f"ida_link_arac {a.komut}: {e}", file=sys.stderr)
        return 1
    except (OSError, ValueError, ET.ParseError) as e:
        print(f"ida_link_arac {a.komut}: okunamadı: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
