"""belge-denetimi.py'nin kendi testi: her denetim bilerek bozulmuş girdiyi reddeder.

Kullanım:
    python3 -I tools/denetim/test/belge_denetimi_test.py [-v]

Her durum geçici bir git deposunda kurulur (main dalında temiz taban, ardından bir iş dalı
ve bozma). Kişisel yol ve Rust/ardıl kart desenleri bu dosyada parçalardan birleştirilir;
dosyanın kendisi denetime takılmaz. Yalnız standart kitaplık ve git kullanılır.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

BURASI = os.path.dirname(os.path.abspath(__file__))
DENETIM = os.path.normpath(os.path.join(BURASI, "..", "belge-denetimi.py"))

# Desenler parçalardan kurulur (bu dosya da depoda denetlenir).
EV = "/" + "Users" + "/" + "ornekkisi" + "/proje/derleme"
WIN_EV = "C:" + "\\" + "Users" + "\\" + "ornekkisi" + "\\proje"
CARGO = "Car" + "go.toml"
ARDIL = "-mcpu=" + "cortex-m" + "7"
ARDIL_AILE = "STM32" + "H743"


def calistir(argumanlar, kok):
    return subprocess.run(argumanlar, cwd=kok, capture_output=True, text=True, check=False)


class BelgeDenetimiTesti(unittest.TestCase):

    def setUp(self):
        self.kok = tempfile.mkdtemp(prefix="ida-belge-denetimi-")
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "test")
        self.git("config", "commit.gpgsign", "false")
        self.yaz("docs/research/00-duzeltmeler-ve-kararlar.md", "# 00\n")
        self.yaz("docs/research/01-ornek.md", "# Ham rapor\n\nmetin\n")
        self.yaz("docs/research/dogrulama/01-codex.md", "# Doğrulama\n")
        self.yaz("docs/DURUM.md", "# Durum\n\n[rapor](research/01-ornek.md)\n")
        self.yaz("README.md", "# Kök\n\n[durum](docs/DURUM.md#su-an) [dış](https://example.org)\n")
        self.yaz("core/src/a.c", "int a(void) { return 0; }\n")
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "taban")
        self.git("switch", "-q", "-c", "is/deneme")

    def tearDown(self):
        shutil.rmtree(self.kok, ignore_errors=True)

    def git(self, *argumanlar):
        sonuc = calistir(["git", *argumanlar], self.kok)
        self.assertEqual(sonuc.returncode, 0, sonuc.stderr)

    def yaz(self, yol, metin):
        tam = os.path.join(self.kok, yol)
        os.makedirs(os.path.dirname(tam), exist_ok=True)
        with open(tam, "w", encoding="utf-8", newline="\n") as f:
            f.write(metin)

    def denetle(self, *ek):
        return calistir([sys.executable, "-I", DENETIM, "--kok", self.kok, "--taban", "main",
                         *ek], self.kok)

    def bulgu_bekle(self, denetim, parca):
        sonuc = self.denetle()
        self.assertEqual(sonuc.returncode, 1, sonuc.stdout + sonuc.stderr)
        satirlar = [s for s in sonuc.stdout.splitlines() if s.startswith(denetim + ": ")]
        self.assertTrue(any(parca in s for s in satirlar),
                        "beklenen bulgu yok: %s / %s\n%s" % (denetim, parca, sonuc.stdout))
        return sonuc

    # ---- karşı örnekler: temiz girdi geçer -------------------------------------------

    def test_temiz_depo_gecer(self):
        sonuc = self.denetle()
        self.assertEqual(sonuc.returncode, 0, sonuc.stdout + sonuc.stderr)
        self.assertEqual(sonuc.stdout, "")

    def test_yeni_ham_rapor_ve_00_degisikligi_serbest(self):
        self.yaz("docs/research/02-yeni.md", "# Yeni rapor\n")
        self.yaz("docs/research/dogrulama/02-codex.md", "# Yeni doğrulama\n")
        self.yaz("docs/research/00-duzeltmeler-ve-kararlar.md", "# 00\n\nyeni düzeltme\n")
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "ekleme")
        self.assertEqual(self.denetle().returncode, 0)

    def test_kod_blogundaki_baglanti_denetlenmez(self):
        self.yaz("docs/kod.md", "```\n[yok](olmayan.md)\n```\n\n`[yok](olmayan2.md)`\n")
        self.assertEqual(self.denetle().returncode, 0)

    # ---- bilerek bozulmuş girdiler reddedilir ----------------------------------------

    def test_degistirilmis_ham_rapor_reddedilir(self):
        self.yaz("docs/research/01-ornek.md", "# Ham rapor\n\ndeğiştirilmiş metin\n")
        self.bulgu_bekle("ham-rapor", "docs/research/01-ornek.md")

    def test_islenmis_ham_rapor_degisikligi_reddedilir(self):
        self.yaz("docs/research/01-ornek.md", "# Ham rapor\n\nişlenmiş değişiklik\n")
        self.git("commit", "-q", "-am", "bozma")
        self.bulgu_bekle("ham-rapor", "değiştirilmiş")

    def test_silinmis_dogrulama_reddedilir(self):
        self.git("rm", "-q", "docs/research/dogrulama/01-codex.md")
        self.git("commit", "-q", "-m", "silme")
        self.bulgu_bekle("ham-rapor", "silinmiş")

    def test_yeniden_adlandirilmis_ham_rapor_reddedilir(self):
        self.git("mv", "docs/research/01-ornek.md", "docs/research/01-yeni-ad.md")
        self.git("commit", "-q", "-m", "ad")
        self.bulgu_bekle("ham-rapor", "docs/research/01-ornek.md")

    def test_kisisel_yol_reddedilir(self):
        self.yaz("tools/betik.sh", "#!/bin/sh\ncd " + EV + "\n")
        self.bulgu_bekle("kisisel-yol", "tools/betik.sh:2")

    def test_windows_kisisel_yol_reddedilir(self):
        self.yaz("docs/not.md", "Yol: " + WIN_EV + "\n")
        self.bulgu_bekle("kisisel-yol", "Windows")

    def test_kirik_goreli_baglanti_reddedilir(self):
        self.yaz("docs/bozuk.md", "[yok](olmayan/dosya.md)\n")
        self.bulgu_bekle("baglanti", "docs/bozuk.md:1")

    def test_degistirilemez_belgedeki_kirik_baglanti_uyaridir(self):
        self.yaz("docs/research/dogrulama/03-codex.md", "[yok](olmayan.md)\n")
        sonuc = self.denetle()
        self.assertEqual(sonuc.returncode, 0, sonuc.stdout + sonuc.stderr)
        self.assertIn("uyarı: baglanti", sonuc.stderr)

    def test_inceleme_kaydindaki_kirik_baglanti_uyaridir(self):
        self.yaz("docs/sozlesmeler/inceleme/A1-sol.md",
                 "[bölüm](docs/sozlesmeler/A1.md) ve [ek](olmayan.md)\n")
        sonuc = self.denetle()
        self.assertEqual(sonuc.returncode, 0, sonuc.stdout + sonuc.stderr)
        self.assertEqual(sonuc.stdout, "")
        self.assertIn("uyarı: baglanti: docs/sozlesmeler/inceleme/A1-sol.md:1", sonuc.stderr)

    def test_inceleme_kaydinda_kisisel_yol_reddedilir(self):
        self.yaz("docs/mimari/inceleme/01-sol.md", "Çalışma dizini: " + EV + "\n")
        self.bulgu_bekle("kisisel-yol", "docs/mimari/inceleme/01-sol.md:1")

    def test_rust_dosyasi_reddedilir(self):
        self.yaz("sim/" + CARGO, "[package]\n")
        self.bulgu_bekle("rust-ardil", "Rust dosyası")

    def test_ardil_kart_bayragi_reddedilir(self):
        self.yaz("cmake/baska.cmake", "set(CPU " + ARDIL + ")\n")
        self.bulgu_bekle("rust-ardil", "cmake/baska.cmake:1")

    def test_ardil_kart_ailesi_reddedilir(self):
        self.yaz("firmware/x.c", "/* " + ARDIL_AILE + " */\n")
        self.bulgu_bekle("rust-ardil", "firmware/x.c:1")

    def test_hedef_kart_ve_st_araclari_gecer(self):
        self.yaz("firmware/y.c", "/* STM32U5A9J-DK, STM32CubeIDE, STM32_Programmer_CLI, "
                                 "cortex-m33 */\n")
        self.assertEqual(self.denetle().returncode, 0)

    def test_belgede_ardil_kart_anilabilir(self):
        self.yaz("docs/karsilastirma.md", ARDIL_AILE + " karşılaştırması\n")
        self.assertEqual(self.denetle().returncode, 0)

    def test_bilinmeyen_denetim_kullanim_hatasidir(self):
        self.assertEqual(self.denetle("--yalniz", "yok").returncode, 2)


if __name__ == "__main__":
    unittest.main()
