#!/usr/bin/env python3
"""firmware/araclar/test_olcum_oku.py — ölçüm bloğu okuyucusunun tutarlılık denetimi (sol bulgu 7).

Kart gerekmez: okuma, sahte bir okuyucuyla taklit edilir. Yarı güncellenmiş blok (sıra numarası
tek) ve okuma sırasında güncellenen blok (sonraki sıra farklı) reddedilmeli, tutarlı okuma
kabul edilmeli. CTest: firmware.araclar.olcum_oku.
"""
import os
import sys
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import olcum_oku  # noqa: E402


class TutarlilikTesti(unittest.TestCase):
    def setUp(self):
        self.adlar = olcum_oku.duzen()
        self.sira = self.adlar.index("yayin_sira")

    def blok(self, sira, deger):
        s = [deger] * len(self.adlar)
        s[self.sira] = sira
        return s

    def test_yari_guncellenmis_blok_reddedilir(self):
        # Sıra tek: güncelleme sürerken okundu; sonraki denemede tutarlı.
        okumalar = iter([(self.blok(7, 1), 7), (self.blok(8, 2), 8)])
        s = olcum_oku.tutarli_oku(lambda: next(okumalar), self.sira)
        self.assertEqual(s[0], 2)

    def test_okuma_sirasinda_guncellenen_blok_reddedilir(self):
        # Blok okunurken yeni yayın başladı/bitti: sonraki sıra farklı.
        okumalar = iter([(self.blok(8, 1), 10), (self.blok(10, 3), 10)])
        s = olcum_oku.tutarli_oku(lambda: next(okumalar), self.sira)
        self.assertEqual(s[0], 3)

    def test_hic_tutarli_okuma_yoksa_hata(self):
        okumalar = iter([(self.blok(9, 1), 9)] * 5)
        with self.assertRaises(SystemExit):
            olcum_oku.tutarli_oku(lambda: next(okumalar), self.sira, deneme=5)

    def test_sira_sozcugu_duzende(self):
        self.assertIn("yayin_sira", self.adlar)
        self.assertTrue(olcum_oku.tutarli_mi(4, 4))
        self.assertFalse(olcum_oku.tutarli_mi(5, 5))
        self.assertFalse(olcum_oku.tutarli_mi(4, 6))


class DuzenTesti(unittest.TestCase):
    """W4-z2 sol #3: PARAM_SAVE ayrıntısı yalnız fw-measure imajında, bloğun sonunda."""

    def test_surum_duzeninde_olcum_sozcukleri_yok(self):
        adlar = olcum_oku.duzen()
        self.assertNotIn("ps_sayi", adlar)
        self.assertNotIn("ol_fark_sayi", adlar)
        self.assertEqual(adlar[-1], "rx_durdu")

    def test_olcum_duzeni_surum_duzeninin_uzantisi(self):
        surum = olcum_oku.duzen()
        olcum = olcum_oku.duzen(True)
        self.assertEqual(olcum[:len(surum)], surum)
        self.assertEqual(olcum[len(surum):], [
            "ps_sayi", "ps_son_tur_cevrim", "ps_son_step_cevrim", "ps_son_skart_us", "ps_son_dongu_cevrim",
            "ps_son_kaydet_cevrim", "ol_fark_sayi", "ol_fark_en_kotu_cevrim", "ol_fark_toplam_cevrim_lo",
            "ol_fark_toplam_cevrim_hi"])

    def test_derleme_turu_uyusmazsa_hata(self):
        olcum_oku.derleme_denetle({"derleme_olcum": 1}, True)
        olcum_oku.derleme_denetle({"derleme_olcum": 0}, False)
        with self.assertRaises(SystemExit):
            olcum_oku.derleme_denetle({"derleme_olcum": 0}, True)
        with self.assertRaises(SystemExit):
            olcum_oku.derleme_denetle({"derleme_olcum": 1}, False)


if __name__ == "__main__":
    # Çıktı konsol kodlamasından bağımsız (Windows'ta cp1252 Türkçe karakteri yazamaz).
    for _akis in (sys.stdout, sys.stderr):
        try:
            _akis.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    unittest.main()
