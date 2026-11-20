#!/usr/bin/env python3
"""A1 kabul testleri için sözleşmeden veri çıkarır (bağımsız kabul testleri; W2-protokol).

Bu betik uygulayıcının çıkarımını ve veri dosyasını kullanmaz. Girdiler yalnız sözleşmedir:
  - docs/sozlesmeler/A1-kart-baglantisi.md: Tablo 3.3 (yön, en büyük yük/çerçeve boyu) ve
    Ek B (38 altın çerçeve: başlık satırı, "Alanlar" satırı, onaltılık baytlar);
  - protocol/ida.xml: alan tipleri ve adları (tel sırası ve CRC_EXTRA, A1 §3.2'deki tanımla
    burada ayrıca hesaplanır; C testleri Ek B'deki değerle karşılaştırır).

Çıktı: bir C kaynağı ve başlığı (derleme dizinine). Karşılaştırmaların çoğu C testlerinde
yapılır; betik yalnız ayrıştırır, XML'den türetilen değerleri hesaplar ve ayrıştırma
düzeyindeki uyumsuzlukları (Ek B "Alanlar" satırı ile XML alan kümesi, tip aralığı, float
gösterilebilirliği) bir dizi olarak C'ye yazar; bir C testi bu dizinin boş olduğunu denetler.

Yalnız standart kitaplık. `python3 -I ek_b_cikar.py --sozlesme ... --xml ... --cikti-c ...
--cikti-h ...`
"""

import argparse
import re
import struct
import sys
import xml.etree.ElementTree as ET

TIP_BOY = {
    "uint8_t": 1, "int8_t": 1, "char": 1,
    "uint16_t": 2, "int16_t": 2,
    "uint32_t": 4, "int32_t": 4, "float": 4,
    "uint64_t": 8, "int64_t": 8, "double": 8,
}


def hata(mesaj):
    sys.stderr.write("[KABUL_EK_B] " + mesaj + "\n")
    sys.exit(2)


# ---- CRC-16/MCRF4XX (A1 §3.2) ------------------------------------------------------------
def crc_bayt(crc, b):
    tmp = (b ^ (crc & 0xFF)) & 0xFF
    tmp = (tmp ^ (tmp << 4)) & 0xFF
    return ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF


def crc(veri, baslangic=0xFFFF):
    c = baslangic
    for b in veri:
        c = crc_bayt(c, b)
    return c


# ---- XML ------------------------------------------------------------------------------------
def xml_oku(yol):
    kok = ET.parse(yol).getroot()
    mesajlar = {}
    for m in kok.iter("message"):
        kimlik = int(m.get("id"))
        ad = m.get("name")
        alanlar = []
        for cocuk in m:
            if cocuk.tag == "extensions":
                hata("ida.xml'de <extensions/> var; A1 §3.2 uzantı alanı olmadığını söyler")
            if cocuk.tag != "field":
                continue
            tip = cocuk.get("type")
            mt = re.fullmatch(r"([a-z0-9_]+)(?:\[(\d+)\])?", tip)
            if not mt or mt.group(1) not in TIP_BOY:
                hata("tanınmayan alan tipi: %s.%s %s" % (ad, cocuk.get("name"), tip))
            alanlar.append({
                "ad": cocuk.get("name"),
                "tip": mt.group(1),
                "dizi": int(mt.group(2)) if mt.group(2) else 0,
            })
        # Tel sırası: tip boyuna göre büyükten küçüğe, aynı boyda XML sırası (kararlı sıralama).
        tel = sorted(alanlar, key=lambda a: -TIP_BOY[a["tip"]])
        ofset = 0
        for a in tel:
            a["ofset"] = ofset
            ofset += TIP_BOY[a["tip"]] * max(1, a["dizi"])
        # CRC_EXTRA: ad + boşluk; her alan için tip + boşluk, ad + boşluk, dizide dizi boyu baytı.
        c = crc((ad + " ").encode("ascii"))
        for a in tel:
            c = crc((a["tip"] + " ").encode("ascii"), c)
            c = crc((a["ad"] + " ").encode("ascii"), c)
            if a["dizi"]:
                c = crc_bayt(c, a["dizi"])
        mesajlar[kimlik] = {
            "ad": ad,
            "tel": tel,
            "yuk_azami": ofset,
            "crc_extra": ((c & 0xFF) ^ (c >> 8)) & 0xFF,
        }
    return mesajlar


# ---- Sözleşme (Markdown) -------------------------------------------------------------------
def bolum(satirlar, bas_desen, son_desen):
    bas = None
    for i, s in enumerate(satirlar):
        if bas is None and re.match(bas_desen, s):
            bas = i + 1
        elif bas is not None and re.match(son_desen, s):
            return satirlar[bas:i]
    if bas is None:
        hata("bölüm bulunamadı: " + bas_desen)
    return satirlar[bas:]


YON = {"sim → kart": 1, "kart → sim": 2, "iki yön": 3}


def tablo_3_3(satirlar):
    govde = bolum(satirlar, r"^### 3\.3 Mesaj listesi", r"^### ")
    sonuc = {}
    for s in govde:
        m = re.match(r"^\| (\d{5}) \| `(IDA_\w+)` \| ([^|]+?) \| [^|]* \| (\d+) / (\d+) \|", s)
        if not m:
            continue
        yon = m.group(3).strip()
        if yon not in YON:
            hata("Tablo 3.3: tanınmayan yön: " + s)
        sonuc[int(m.group(1))] = {
            "ad": m.group(2), "yon": YON[yon],
            "yuk_azami": int(m.group(4)), "cerceve_azami": int(m.group(5)),
        }
    if len(sonuc) != 37:
        hata("Tablo 3.3'te 37 satır beklenirken %d bulundu" % len(sonuc))
    return sonuc


BASLIK_DESEN = re.compile(
    r"^- Başlık: seq = (\d+), sysid = (\d+), compid = (\d+); CRC_EXTRA = (\d+); "
    r"yük azami (\d+) B, çerçeve azami (\d+) B; bu dizide yük (\d+) B, çerçeve (\d+) B\.")


def deger_ayir(metin):
    """'a = 1; b = [1, 2, … (6 öğe)]' -> [(ad, deger)]; deger int/float ya da (liste, sayi)."""
    sonuc = []
    for parca in metin.split("; "):
        m = re.fullmatch(r"(\w+) = (.+)", parca.strip())
        if not m:
            hata("Alanlar satırı ayrıştırılamadı: " + parca)
        ad, d = m.group(1), m.group(2).strip()
        if d.startswith("["):
            m2 = re.fullmatch(r"\[(.*?)(?:, … \((\d+) öğe\))?\]", d)
            if not m2:
                hata("dizi değeri ayrıştırılamadı: " + d)
            ogeler = [sayi(x.strip()) for x in m2.group(1).split(",")]
            toplam = int(m2.group(2)) if m2.group(2) else len(ogeler)
            sonuc.append((ad, (ogeler, toplam)))
        else:
            sonuc.append((ad, sayi(d)))
    return sonuc


def sayi(metin):
    if re.fullmatch(r"-?\d+", metin):
        return int(metin)
    if re.fullmatch(r"-?\d+\.\d+(e-?\d+)?", metin):
        return float(metin)
    hata("sayı değil: " + metin)
    return None


def ek_b(satirlar):
    govde = bolum(satirlar, r"^## Ek B\. ", r"^## Ek C\. ")
    cerceveler = []
    i = 0
    while i < len(govde):
        m = re.match(r"^### (\d+) (IDA_\w+)(.*)$", govde[i])
        if not m:
            i += 1
            continue
        c = {"msgid": int(m.group(1)), "ad": m.group(2), "baslik": govde[i][4:].strip()}
        j = i + 1
        while j < len(govde) and not govde[j].startswith("### "):
            s = govde[j]
            mb = BASLIK_DESEN.match(s)
            if mb:
                (c["seq"], c["sysid"], c["compid"], c["crc_extra"], c["yuk_azami"],
                 c["cerceve_azami"], c["yuk_bu"], c["cerceve_bu"]) = map(int, mb.groups())
            elif s.startswith("- Alanlar: "):
                c["alanlar"] = deger_ayir(s[len("- Alanlar: "):])
            elif s.strip() == "```":
                k = j + 1
                bayt = []
                while govde[k].strip() != "```":
                    for t in govde[k].split():
                        if not re.fullmatch(r"[0-9a-f]{2}", t):
                            hata("onaltılık bayt değil: %r (%s)" % (t, c["baslik"]))
                        bayt.append(int(t, 16))
                    k += 1
                c["bayt"] = bayt
                j = k
            j += 1
        for anahtar in ("seq", "alanlar", "bayt"):
            if anahtar not in c:
                hata("Ek B bölümünde eksik: %s (%s)" % (anahtar, c["baslik"]))
        cerceveler.append(c)
        i = j
    if len(cerceveler) != 38:
        hata("Ek B'de 38 çerçeve beklenirken %d bulundu" % len(cerceveler))
    return cerceveler


# ---- C üretimi -----------------------------------------------------------------------------
def c_dize(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def ham_bit(tip, deger):
    """Alan değerinin küçük uçlu ham bitleri (uint64), ya da aralık dışıysa None."""
    boy = TIP_BOY[tip]
    if tip == "float":
        if not isinstance(deger, (int, float)):
            return None
        b = struct.pack("<f", float(deger))
        if struct.unpack("<f", b)[0] != float(deger):
            return None
        return int.from_bytes(b, "little")
    if not isinstance(deger, int):
        return None
    if tip.startswith("u"):
        if deger < 0 or deger >= (1 << (8 * boy)):
            return None
        return deger
    if deger < -(1 << (8 * boy - 1)) or deger >= (1 << (8 * boy - 1)):
        return None
    return deger & ((1 << (8 * boy)) - 1)


def c_deger(tip, deger):
    if tip == "float":
        return repr(float(deger)) + "f"
    if tip.startswith("u"):
        return "UINT64_C(%d)" % deger
    return "INT64_C(%d)" % deger


def uret(xml, t33, cerceveler, cikti_c, cikti_h):
    uyumsuz = []
    hs = []
    hs.append("/* Üretildi: tests/kabul/protokol/ek_b_cikar.py (elle düzenlenmez). */")
    hs.append("#ifndef KABUL_EK_B_H\n#define KABUL_EK_B_H\n")
    hs.append("#include <stddef.h>\n#include <stdint.h>\n")
    hs.append('#include "mavlink/ida/mavlink.h"\n')
    hs.append("#define KABUL_MESAJ_SAYISI %du" % len(t33))
    hs.append("#define KABUL_ALTIN_SAYISI %du\n" % len(cerceveler))
    hs.append("""/* Yön (A1 Tablo 3.3): bit maskesi. */
#define KABUL_YON_SIM_KART 1u
#define KABUL_YON_KART_SIM 2u

typedef struct {
    uint32_t    msgid;
    const char *ad;
    uint8_t     yon;            /* Tablo 3.3 "Yön" */
    uint8_t     yuk_azami;      /* Tablo 3.3 yük */
    uint16_t    cerceve_azami;  /* Tablo 3.3 çerçeve */
    uint8_t     crc_extra;      /* Ek B başlık satırı */
    uint8_t     crc_extra_xml;  /* ida.xml'den A1 §3.2 tanımıyla (bu betik) */
    uint8_t     yuk_azami_xml;  /* ida.xml alan boylarının toplamı */
    uint8_t     yuk_azami_ek_b; /* Ek B başlık satırı */
    uint16_t    yapi_boyu;      /* sizeof(mavlink_ida_<ad>_t) */
} kabul_mesaj_t;

/* Ek B "Alanlar" satırındaki bir değerin teldeki yeri (bu betiğin tel sırası hesabıyla). */
typedef struct {
    const char *ad;
    uint16_t    ofset;   /* yük içinde */
    uint8_t     boy;     /* bayt */
    uint64_t    ham;     /* küçük uçlu ham bitler */
} kabul_alan_t;

typedef struct {
    const char    *baslik;        /* Ek B alt başlığı */
    uint32_t       msgid;
    uint8_t        seq, sysid, compid, crc_extra;
    uint8_t        yuk_azami;
    uint16_t       cerceve_azami;
    uint8_t        yuk_bu;
    uint16_t       cerceve_bu;
    const uint8_t *bayt;
    uint16_t       n;
    const kabul_alan_t *alanlar;
    uint16_t       alan_sayisi;
    /* Çözülmüş tipli yapının (mavlink_ida_<ad>_t) adlı alanlarını Ek B değerleriyle
       karşılaştırır (Unity iddialarıyla). */
    void (*yapi_denetle)(const void *yapi);  /* yapi: en az sizeof(o yapı) bayt; memcpy ile okunur */
} kabul_altin_t;

/* Gönderici ve çözücüye verilen mesaj nesnesi: üretilmiş 37 yapının birleşimi. Kitaplık
   void * üzerinden kendi yapı tipiyle okur/yazar; nesne o tipte bir üyeyi gerçekten içerir
   (yalnız bir bayt tamponu verilmez; tür erişimi kuralı). b[] bayt düzeyinde doldurma ve
   karşılaştırma içindir (unsigned char erişimi her nesnede geçerlidir). */
typedef union {
@@BIRLESIM@@
    uint8_t b[512];
} kabul_yapi_t;

extern const kabul_mesaj_t kabul_mesajlar[KABUL_MESAJ_SAYISI];
extern const kabul_altin_t kabul_altin[KABUL_ALTIN_SAYISI];
/* Ayrıştırma düzeyindeki uyumsuzluklar; NULL ile biter. */
extern const char *const kabul_ek_b_uyumsuzluk[];
""")
    hs.append("#endif")

    cs = []
    cs.append("/* Üretildi: tests/kabul/protokol/ek_b_cikar.py (elle düzenlenmez). */")
    cs.append('#include "kabul_ek_b.h"\n#include "kabul_ortak.h"\n\n#include <string.h>\n')
    cs.append('#include "mavlink/ida/mavlink.h"\n')

    ekb_crc = {}
    ekb_yuk = {}
    for c in cerceveler:
        ekb_crc.setdefault(c["msgid"], c["crc_extra"])
        ekb_yuk.setdefault(c["msgid"], c["yuk_azami"])

    cs.append("const kabul_mesaj_t kabul_mesajlar[KABUL_MESAJ_SAYISI] = {")
    for kimlik in sorted(t33):
        t = t33[kimlik]
        if kimlik not in xml:
            hata("Tablo 3.3'teki %d ida.xml'de yok" % kimlik)
        x = xml[kimlik]
        if x["ad"] != t["ad"]:
            uyumsuz.append("%d: Tablo 3.3 adı %s, XML adı %s" % (kimlik, t["ad"], x["ad"]))
        if kimlik not in ekb_crc:
            hata("Ek B'de %d yok" % kimlik)
        cs.append("    {%du, %s, %du, %du, %du, %du, %du, %du, %du, (uint16_t)sizeof(mavlink_%s_t)}," % (
            kimlik, c_dize(t["ad"]), t["yon"], t["yuk_azami"], t["cerceve_azami"],
            ekb_crc[kimlik], x["crc_extra"], x["yuk_azami"], ekb_yuk[kimlik], t["ad"].lower()))
    cs.append("};\n")
    if len(xml) != len(t33):
        uyumsuz.append("XML'de %d mesaj, Tablo 3.3'te %d" % (len(xml), len(t33)))

    alan_sayilari = []
    for i, c in enumerate(cerceveler):
        x = xml.get(c["msgid"])
        if x is None:
            hata("Ek B çerçevesinin kimliği XML'de yok: %s" % c["baslik"])
        cs.append("static const uint8_t altin_bayt_%d[%d] = {" % (i, len(c["bayt"])))
        for k in range(0, len(c["bayt"]), 16):
            cs.append("    " + ", ".join("0x%02x" % b for b in c["bayt"][k:k + 16]) + ",")
        cs.append("};")

        alan_tipi = {a["ad"]: a for a in x["tel"]}
        verilen = [ad for ad, _ in c["alanlar"]]
        if sorted(verilen) != sorted(alan_tipi):
            uyumsuz.append("%s: Alanlar satırındaki adlar XML alanlarıyla aynı değil (%s / %s)" % (
                c["baslik"], ",".join(verilen), ",".join(a["ad"] for a in x["tel"])))
        ham = []
        denet = []
        yapi = "mavlink_%s_t" % x["ad"].lower()
        for ad, d in c["alanlar"]:
            a = alan_tipi.get(ad)
            if a is None:
                continue
            boy = TIP_BOY[a["tip"]]
            if a["dizi"]:
                if not isinstance(d, tuple):
                    uyumsuz.append("%s: %s dizi alanı ama değer dizi değil" % (c["baslik"], ad))
                    continue
                ogeler, toplam = d
                if toplam != a["dizi"]:
                    uyumsuz.append("%s: %s öğe sayısı %d, XML %d" % (c["baslik"], ad, toplam, a["dizi"]))
                cifti = [("%s[%d]" % (ad, k), v, a["ofset"] + k * boy, "%s[%d]" % (ad, k))
                         for k, v in enumerate(ogeler)]
            else:
                if isinstance(d, tuple):
                    uyumsuz.append("%s: %s tekil alan ama değer dizi" % (c["baslik"], ad))
                    continue
                cifti = [(ad, d, a["ofset"], ad)]
            for gorunen, v, ofset, uye in cifti:
                hb = ham_bit(a["tip"], v)
                if hb is None:
                    uyumsuz.append("%s: %s = %r tipine (%s) sığmıyor" % (c["baslik"], gorunen, v, a["tip"]))
                    continue
                ham.append('    {%s, %du, %du, UINT64_C(0x%x)},' % (c_dize(gorunen), ofset, boy, hb))
                if a["tip"] == "float":
                    denet.append("    kabul_esit_f32(%s, %s, m.%s);" % (c_dize(gorunen), c_deger("float", v), uye))
                elif a["tip"].startswith("u"):
                    denet.append("    kabul_esit_u64(%s, %s, (uint64_t)m.%s);" % (c_dize(gorunen), c_deger(a["tip"], v), uye))
                else:
                    denet.append("    kabul_esit_i64(%s, %s, (int64_t)m.%s);" % (c_dize(gorunen), c_deger(a["tip"], v), uye))
        alan_sayilari.append(len(ham))
        cs.append("static const kabul_alan_t altin_alan_%d[%d] = {" % (i, max(1, len(ham))))
        cs.extend(ham if ham else ['    {"", 0u, 0u, UINT64_C(0)},'])
        cs.append("};")
        cs.append("static void altin_yapi_%d(const void *p)\n{" % i)
        cs.append("    %s m;" % yapi)
        cs.append("    memcpy(&m, p, sizeof m);")
        cs.extend(denet)
        cs.append("}\n")

    cs.append("const kabul_altin_t kabul_altin[KABUL_ALTIN_SAYISI] = {")
    for i, c in enumerate(cerceveler):
        cs.append("    {%s, %du, %du, %du, %du, %du, %du, %du, %du, %du, altin_bayt_%d, %du, altin_alan_%d, %du, altin_yapi_%d}," % (
            c_dize(c["baslik"]), c["msgid"], c["seq"], c["sysid"], c["compid"], c["crc_extra"],
            c["yuk_azami"], c["cerceve_azami"], c["yuk_bu"], c["cerceve_bu"], i, len(c["bayt"]),
            i, alan_sayilari[i], i))
    cs.append("};\n")

    cs.append("const char *const kabul_ek_b_uyumsuzluk[] = {")
    for u in uyumsuz:
        cs.append("    %s," % c_dize(u))
    cs.append("    NULL\n};")

    birlesim = "\n".join("    mavlink_%s_t m%d;" % (t33[k]["ad"].lower(), k) for k in sorted(t33))
    with open(cikti_h, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(hs).replace("@@BIRLESIM@@", birlesim) + "\n")
    with open(cikti_c, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cs) + "\n")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sozlesme", required=True)
    p.add_argument("--xml", required=True)
    p.add_argument("--cikti-c", required=True)
    p.add_argument("--cikti-h", required=True)
    a = p.parse_args()
    with open(a.sozlesme, encoding="utf-8") as f:
        satirlar = f.read().split("\n")
    xml = xml_oku(a.xml)
    t33 = tablo_3_3(satirlar)
    cerceveler = ek_b(satirlar)
    uret(xml, t33, cerceveler, a.cikti_c, a.cikti_h)


if __name__ == "__main__":
    main()
