"""Belge ve depo denetimi (W2-depo §4; 09 §2.8 "Belge ve depo denetimi").

Kullanım:
    python3 -I tools/denetim/belge-denetimi.py [--kok DİZİN] [--taban REF]
                                               [--yalniz DENETİM[,DENETİM...]]

Denetimler:
    ham-rapor     docs/research/NN-*.md (00 hariç) ve docs/research/dogrulama/ altındaki
                  dosyalar tabana göre değiştirilmemiş, silinmemiş, yeniden adlandırılmamış.
                  Yeni dosya eklemek serbesttir (AGENTS.md: yeni rapor ve doğrulama).
                  Taban: --taban (varsayılan main) ile HEAD'in ortak atası; karşılaştırma
                  çalışma ağacıyla yapılır (işlenmemiş değişiklikler de görülür).
    kisisel-yol   Metin dosyalarında kişisel makine yolu yok (macOS/Linux ev dizini, Windows
                  kullanıcı dizini, macOS geçici dizini).
    baglanti      Markdown dosyalarındaki göreli bağlantılar var olan bir dosyaya ya da
                  dizine çözülüyor (kod blokları ve satır içi kod hariç; şemalı bağlantılar,
                  yalnız çapa ve / ile başlayan mutlak yollar denetlenmez). Değiştirilemeyen
                  kayıtlardaki kırık bağlantılar bulgu değil, uyarıdır ve çıkış kodunu
                  etkilemez: ham raporlar ve doğrulamalar (AGENTS.md belge kuralları) ile
                  docs/**/inceleme/ altındaki inceleme kayıtları (inceleyenin çıktısı,
                  yazıldığı gibi korunur; mimar kararı 2026-11-20). Öteki denetimler bu
                  dosyalarda da bulgu üretir.
    rust-ardil    Belge dışı dosyalarda Rust araç zinciri ya da bağımlılığı ve ardıl kart /
                  başka mikrodenetleyici hedefi yok (AGENTS.md kesin kararları).

Denetlenen dosyalar: git'in izlediği dosyalar + izlenmeyen ama yok sayılmayan dosyalar.
Çıkış kodu: 0 temiz, 1 bulgu var, 2 kullanım ya da araç hatası.
Yalnız Python standart kitaplığı kullanılır (iskelet §7: yardımcı betik).
"""

import argparse
import os
import re
import subprocess
import sys
import urllib.parse

DENETIMLER = ("ham-rapor", "kisisel-yol", "baglanti", "rust-ardil")

# Bu denetimin kendi dosyaları desen metinlerini içerir; rust-ardil taramasının dışındadır.
KENDI_DOSYALARI = {
    "tools/denetim/belge-denetimi.py",
    "tools/denetim/test/belge_denetimi_test.py",
}

HAM_RAPOR = re.compile(r"^docs/research/(?!00-)[0-9]{2}-[^/]+\.md$")
DOGRULAMA = re.compile(r"^docs/research/dogrulama/")
INCELEME = re.compile(r"^docs/(?:[^/]+/)*inceleme/")


def degistirilemez_kayit(yol):
    """Yazıldığı gibi korunan kayıt: ham rapor, doğrulama ya da inceleme kaydı."""
    return bool(HAM_RAPOR.match(yol) or DOGRULAMA.match(yol) or INCELEME.match(yol))

KISISEL_YOL = [
    ("macOS ev dizini", re.compile(r"/Users/(?!Shared/)[A-Za-z0-9._-]+/")),
    ("Linux ev dizini", re.compile(r"/home/[a-z_][A-Za-z0-9._-]*/")),
    ("Windows kullanıcı dizini",
     re.compile(r"\b[A-Za-z]:[\\/]+Users[\\/]+(?!Public\b)[A-Za-z0-9._-]+", re.IGNORECASE)),
    ("macOS geçici dizini", re.compile(r"/(?:private/)?var/folders/[A-Za-z0-9_+-]{2}/")),
]

RUST_DOSYA = re.compile(r"(^|/)(Cargo\.toml|Cargo\.lock|rust-toolchain(\.toml)?|[^/]+\.rs)$")
RUST_ICERIK = re.compile(
    r"\b(cargo|rustc|rustup|corrosion|crates\.io)\b|rust-toolchain", re.IGNORECASE)
# Hedef yalnız STM32U5A9J-DK kartıdır (MCU STM32U5A9NJ; K8). Yakalananlar: öteki STM32
# aileleri; STM32U5 ailesinin U5A9 dışındaki bütün parçaları ve kartları (ör. STM32U5G9J-DK1,
# STM32U585, NUCLEO-U575ZI-Q, B-U585I-IOT02A); Cortex-M33 dışındaki çekirdekler; başka MCU
# aileleri. Serbest kalanlar: aile başlıkları ve kütüphaneleri (stm32u5xx.h, stm32u5xx_hal),
# ST'nin U5x9J keşif kiti BSP adı (stm32u5x9j_discovery; hedef kartın BSP'si), "x" ile biten
# ürün hattı adları (STM32U59x/5Ax gibi; belirli bir parça seçimi değildir), ortak ST
# araçları (STM32CubeIDE, STM32_Programmer_CLI) ve hedef parçanın adları (STM32U5A9NJ,
# STM32U5A9xx, STM32U5A9J-DK).
ARDIL_ICERIK = re.compile(
    r"\bstm32(?:[a-tv-z][a-z]?[0-9]|u[0-46-9]|u5(?!xx|x9|a9|[0-9a-z]x)[0-9a-z])"
    r"|\bnucleo-[a-z][0-9a-z]*|\bb-u5[0-9]"
    r"|\bcortex-m(?:0|0plus|1|3|4|7|23|35p|52|55|85)\b"
    r"|\b(?:esp32|esp8266|nrf5[0-9]|rp2040|rp2350|imxrt|teensy|atsamd|gd32|ch32v)",
    re.IGNORECASE)

BAGLANTI_SATIR_ICI = re.compile(r"!?\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*(<[^>]*>|[^)\s]+)")
BAGLANTI_TANIM = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*(<[^>]*>|\S+)")
SATIR_ICI_KOD = re.compile(r"(`+)(?:(?!\1).)*?\1")
SEMA = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")


class Hata(Exception):
    pass


def git(kok, *argumanlar):
    try:
        sonuc = subprocess.run(["git", "-C", kok, *argumanlar], capture_output=True,
                               check=False)
    except OSError as e:
        raise Hata("git çalıştırılamadı: %s" % e) from e
    if sonuc.returncode != 0:
        raise Hata("git %s başarısız: %s" % (" ".join(argumanlar),
                                              sonuc.stderr.decode("utf-8", "replace").strip()))
    return sonuc.stdout


def dosyalari_listele(kok):
    izlenen = git(kok, "ls-files", "-z").split(b"\0")
    izlenmeyen = git(kok, "ls-files", "-z", "--others", "--exclude-standard").split(b"\0")
    yollar = set()
    for ham in izlenen + izlenmeyen:
        if not ham:
            continue
        yol = ham.decode("utf-8", "surrogateescape")
        if os.path.isfile(os.path.join(kok, yol)):
            yollar.add(yol)
    return sorted(yollar)


def metin_oku(kok, yol):
    try:
        with open(os.path.join(kok, yol), "rb") as f:
            veri = f.read()
    except OSError:
        return None
    if b"\0" in veri[:8192]:
        return None
    return veri.decode("utf-8", "replace")


def ham_rapor_denetle(kok, taban, bulgular):
    ortak = git(kok, "merge-base", taban, "HEAD").decode().strip()
    cikti = git(kok, "diff", "--name-status", "--no-renames", "-z", ortak, "--")
    parcalar = cikti.split(b"\0")
    i = 0
    while i + 1 < len(parcalar):
        durum = parcalar[i].decode()
        yol = parcalar[i + 1].decode("utf-8", "surrogateescape")
        i += 2
        if not (HAM_RAPOR.match(yol) or DOGRULAMA.match(yol)):
            continue
        if durum.startswith("A"):
            continue
        anlam = {"M": "değiştirilmiş", "D": "silinmiş", "T": "türü değişmiş"}.get(
            durum[:1], "durum " + durum)
        bulgular.append(("ham-rapor", yol, None,
                         "ham rapor ya da doğrulama %s (taban %s); düzeltme "
                         "docs/research/00-duzeltmeler-ve-kararlar.md'ye eklenir" %
                         (anlam, ortak[:12])))


def kisisel_yol_denetle(kok, dosyalar, bulgular):
    for yol in dosyalar:
        metin = metin_oku(kok, yol)
        if metin is None:
            continue
        for no, satir in enumerate(metin.splitlines(), 1):
            for ad, desen in KISISEL_YOL:
                m = desen.search(satir)
                if m:
                    bulgular.append(("kisisel-yol", yol, no, "%s: %s" % (ad, m.group(0))))


def kod_disi_satirlar(metin):
    """Kod blokları dışındaki satırları (satır no, satır içi kodu silinmiş metin) verir."""
    cit = None
    for no, satir in enumerate(metin.splitlines(), 1):
        s = satir.lstrip()
        m = re.match(r"(`{3,}|~{3,})", s)
        if m:
            isaret = m.group(1)
            if cit is None:
                cit = isaret[0] * len(isaret)
                continue
            if isaret[0] == cit[0] and len(isaret) >= len(cit) and not s[len(isaret):].strip():
                cit = None
                continue
        if cit is not None:
            continue
        # Girintili kod blokları ayırt edilmez (liste içi devam satırlarıyla karışır);
        # bu depoda kod örnekleri çitli bloklardadır.
        yield no, SATIR_ICI_KOD.sub("", satir)


def baglanti_denetle(kok, dosyalar, bulgular, uyarilar):
    for yol in dosyalar:
        if not yol.lower().endswith(".md"):
            continue
        metin = metin_oku(kok, yol)
        if metin is None:
            continue
        dizin = os.path.dirname(yol)
        for no, satir in kod_disi_satirlar(metin):
            hedefler = [m.group(1) for m in BAGLANTI_SATIR_ICI.finditer(satir)]
            m = BAGLANTI_TANIM.match(satir)
            if m:
                hedefler.append(m.group(1))
            for hedef in hedefler:
                if hedef.startswith("<") and hedef.endswith(">"):
                    hedef = hedef[1:-1]
                if (not hedef or hedef.startswith("#") or hedef.startswith("/")
                        or SEMA.match(hedef)):
                    continue
                yalin = urllib.parse.unquote(hedef.split("#", 1)[0].split("?", 1)[0])
                if not yalin:
                    continue
                aday = os.path.normpath(os.path.join(kok, dizin, yalin))
                if os.path.exists(aday):
                    continue
                if degistirilemez_kayit(yol):
                    uyarilar.append(("baglanti", yol, no,
                                     "çözülmeyen göreli bağlantı (değiştirilemez kayıt): " + hedef))
                else:
                    bulgular.append(("baglanti", yol, no, "çözülmeyen göreli bağlantı: " + hedef))


def rust_ardil_denetle(kok, dosyalar, bulgular):
    for yol in dosyalar:
        if yol in KENDI_DOSYALARI:
            continue
        if RUST_DOSYA.search(yol):
            bulgular.append(("rust-ardil", yol, None, "Rust dosyası"))
            continue
        if yol.startswith("docs/") or yol.lower().endswith(".md"):
            continue
        metin = metin_oku(kok, yol)
        if metin is None:
            continue
        for no, satir in enumerate(metin.splitlines(), 1):
            m = RUST_ICERIK.search(satir)
            if m:
                bulgular.append(("rust-ardil", yol, no, "Rust araç zinciri ya da bağımlılığı: "
                                 + m.group(0)))
            m = ARDIL_ICERIK.search(satir)
            if m:
                bulgular.append(("rust-ardil", yol, no,
                                 "STM32U5A9J-DK dışında kart ya da mikrodenetleyici: " + m.group(0)))


def main(argv):
    ayristirici = argparse.ArgumentParser(description="İDA belge ve depo denetimi")
    ayristirici.add_argument("--kok", default=os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    ayristirici.add_argument("--taban", default="main")
    ayristirici.add_argument("--yalniz", default=",".join(DENETIMLER))
    secenek = ayristirici.parse_args(argv[1:])

    secilen = [d.strip() for d in secenek.yalniz.split(",") if d.strip()]
    bilinmeyen = [d for d in secilen if d not in DENETIMLER]
    if bilinmeyen:
        print("bilinmeyen denetim: " + ", ".join(bilinmeyen), file=sys.stderr)
        return 2

    kok = os.path.abspath(secenek.kok)
    bulgular = []
    uyarilar = []
    try:
        dosyalar = dosyalari_listele(kok)
        if "ham-rapor" in secilen:
            ham_rapor_denetle(kok, secenek.taban, bulgular)
        if "kisisel-yol" in secilen:
            kisisel_yol_denetle(kok, dosyalar, bulgular)
        if "baglanti" in secilen:
            baglanti_denetle(kok, dosyalar, bulgular, uyarilar)
        if "rust-ardil" in secilen:
            rust_ardil_denetle(kok, dosyalar, bulgular)
    except Hata as e:
        print("belge-denetimi: " + str(e), file=sys.stderr)
        return 2

    for denetim, yol, no, ayrinti in uyarilar:
        konum = yol if no is None else "%s:%d" % (yol, no)
        print("uyarı: %s: %s: %s" % (denetim, konum, ayrinti), file=sys.stderr)
    for denetim, yol, no, ayrinti in bulgular:
        konum = yol if no is None else "%s:%d" % (yol, no)
        print("%s: %s: %s" % (denetim, konum, ayrinti))
    print("belge-denetimi: %d dosya, %d bulgu, %d uyarı (%s)" % (
        len(dosyalar), len(bulgular), len(uyarilar), ", ".join(secilen)), file=sys.stderr)
    return 1 if bulgular else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
