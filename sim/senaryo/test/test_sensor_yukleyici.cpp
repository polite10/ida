// Dilim 2: sensör yükleyici ve koddaki İDA-0 sensör varsayılanlarıyla karşılaştırma
// (madde 11). Yalnız sim/sensor depoda varken derlenir.
//
//   ida_senaryo_sensor_test <sensor_yukleyici|sensor_karsilastirma> <depo kökü>
#include "ida/senaryo/sensor_yukleyici.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "karsilastirma.hpp"
#include "test_destek.hpp"

#include <cstring>
#include <variant>

namespace ida::senaryo::test {

namespace {

std::optional<sensor::SensorKumesiAyari> ornekten(const TestBaglami& tb, bool ses_hizi_ver = false)
{
    BellekKaynagi kaynak = temel_kaynak(tb);
    if (ses_hizi_ver) {
        kaynak.dosyalar[kSenaryo] =
            *degistir(kaynak.dosyalar[kSenaryo], "sicaklik_c = 20.0", "sicaklik_c = 20.0\nses_hizi = 1482.3");
    }
    const CozumSonucu s = senaryo_coz(kaynak, kSenaryo);
    if (!s.cozumlenmis) {
        std::printf("%s", bulgu_ozeti(s.rapor).c_str());
        return std::nullopt;
    }
    std::string hata;
    auto k = sensor_kur(*s.cozumlenmis, &hata);
    if (!k) {
        std::printf("  sensor_kur: %s\n", hata.c_str());
    }
    return k;
}

void ayar_karsilastir(Karsilastirma& c, const std::string& p, const sensor::SensorAyarGovdesi& a,
                      const sensor::SensorAyarGovdesi& b)
{
    using namespace sensor;
    c.tam(p + ".(govde turu)", a.index(), b.index());
    if (a.index() != b.index()) {
        return;
    }
    if (const auto* x = std::get_if<ImuAyar>(&a)) {
        const auto& y = std::get<ImuAyar>(b);
        c.sayi(p + ".jiroskop_yogunluk", x->jiroskop_yogunluk, y.jiroskop_yogunluk);
        c.dizi(p + ".ivme_yogunluk", x->ivme_yogunluk, y.ivme_yogunluk);
        c.sayi(p + ".jiroskop_acilis_bias_siniri", x->jiroskop_acilis_bias_siniri, y.jiroskop_acilis_bias_siniri);
        c.sayi(p + ".ivme_acilis_bias_siniri", x->ivme_acilis_bias_siniri, y.ivme_acilis_bias_siniri);
        c.sayi(p + ".jiroskop_sicaklik_kats_siniri", x->jiroskop_sicaklik_kats_siniri, y.jiroskop_sicaklik_kats_siniri);
        c.sayi(p + ".ivme_sicaklik_kats_siniri", x->ivme_sicaklik_kats_siniri, y.ivme_sicaklik_kats_siniri);
        c.sayi(p + ".jiroskop_olcek_siniri", x->jiroskop_olcek_siniri, y.jiroskop_olcek_siniri);
        c.sayi(p + ".jiroskop_eksenler_arasi_siniri", x->jiroskop_eksenler_arasi_siniri, y.jiroskop_eksenler_arasi_siniri);
        c.sayi(p + ".ivme_olcek_siniri", x->ivme_olcek_siniri, y.ivme_olcek_siniri);
        c.sayi(p + ".ivme_eksenler_arasi_siniri", x->ivme_eksenler_arasi_siniri, y.ivme_eksenler_arasi_siniri);
        c.sayi(p + ".jiroskop_dogrusalsizlik", x->jiroskop_dogrusalsizlik, y.jiroskop_dogrusalsizlik);
        c.sayi(p + ".ivme_dogrusalsizlik", x->ivme_dogrusalsizlik, y.ivme_dogrusalsizlik);
        c.tam(p + ".jiroskop_gm_tau_ns", x->jiroskop_gm_tau_ns, y.jiroskop_gm_tau_ns);
        c.tam(p + ".ivme_gm_tau_ns", x->ivme_gm_tau_ns, y.ivme_gm_tau_ns);
        c.sayi(p + ".jiroskop_gm_sigma", x->jiroskop_gm_sigma, y.jiroskop_gm_sigma);
        c.sayi(p + ".ivme_gm_sigma", x->ivme_gm_sigma, y.ivme_gm_sigma);
        c.sayi(p + ".jiroskop_aralik", x->jiroskop_aralik, y.jiroskop_aralik);
        c.sayi(p + ".ivme_aralik", x->ivme_aralik, y.ivme_aralik);
        c.tam(p + ".bit", x->bit, y.bit);
        c.mantik(p + ".suzgec_etkin", x->suzgec_etkin, y.suzgec_etkin);
        c.tam(p + ".suzgec_tau_ns", x->suzgec_tau_ns, y.suzgec_tau_ns);
        c.mantik(p + ".dunya_donusu_etkin", x->dunya_donusu_etkin, y.dunya_donusu_etkin);
    } else if (const auto* m = std::get_if<ManyetometreAyar>(&a)) {
        const auto& y = std::get<ManyetometreAyar>(b);
        c.sayi(p + ".gurultu_sigma", m->gurultu_sigma, y.gurultu_sigma);
        c.mat3(p + ".yumusak_demir", m->yumusak_demir, y.yumusak_demir);
        c.dizi(p + ".sert_demir", m->sert_demir, y.sert_demir);
        c.sayi(p + ".sicaklik_kats", m->sicaklik_kats, y.sicaklik_kats);
        c.sayi(p + ".itici_girisim_orani", m->itici_girisim_orani, y.itici_girisim_orani);
        c.tam(p + ".teslim_gecikmesi_ns", m->teslim_gecikmesi_ns, y.teslim_gecikmesi_ns);
    } else if (const auto* s = std::get_if<BasincAyar>(&a)) {
        const auto& y = std::get<BasincAyar>(b);
        c.sayi(p + ".gurultu_sigma", s->gurultu_sigma, y.gurultu_sigma);
        c.sayi(p + ".mutlak_dogruluk_siniri", s->mutlak_dogruluk_siniri, y.mutlak_dogruluk_siniri);
        c.sayi(p + ".sicaklik_kats", s->sicaklik_kats, y.sicaklik_kats);
        c.sayi(p + ".kuantalama", s->kuantalama, y.kuantalama);
        c.tam(p + ".donusum_penceresi_ns", s->donusum_penceresi_ns, y.donusum_penceresi_ns);
        c.tam(p + ".okuma_suresi_ns", s->okuma_suresi_ns, y.okuma_suresi_ns);
        c.sayi(p + ".dinamik_basinc_kats", s->dinamik_basinc_kats, y.dinamik_basinc_kats);
        c.sayi(p + ".sicaklik_ofset_siniri", s->sicaklik_ofset_siniri, y.sicaklik_ofset_siniri);
    } else if (const auto* g = std::get_if<GnssAyar>(&a)) {
        const auto& y = std::get<GnssAyar>(b);
        c.sayi(p + ".cep", g->cep, y.cep);
        c.tam(p + ".gm_tau_ns", g->gm_tau_ns, y.gm_tau_ns);
        c.sayi(p + ".gm_varyans_payi", g->gm_varyans_payi, y.gm_varyans_payi);
        c.sayi(p + ".dusey_carpan", g->dusey_carpan, y.dusey_carpan);
        c.sayi(p + ".hiz_sigma", g->hiz_sigma, y.hiz_sigma);
        c.tam(p + ".teslim_gecikmesi_ns", g->teslim_gecikmesi_ns, y.teslim_gecikmesi_ns);
        c.tam(p + ".uydu_sayisi", g->uydu_sayisi, y.uydu_sayisi);
        c.tam(p + ".n_sv_ref", g->n_sv_ref, y.n_sv_ref);
        c.sayi(p + ".w50", g->w50, y.w50);
        c.sayi(p + ".w_gecis", g->w_gecis, y.w_gecis);
        c.tam(p + ".t_kisa_ns", g->t_kisa_ns, y.t_kisa_ns);
        c.tam(p + ".t_efemeris_ns", g->t_efemeris_ns, y.t_efemeris_ns);
        c.tam(p + ".hizli_medyan_ns", g->hizli_medyan_ns, y.hizli_medyan_ns);
        c.tam(p + ".sicak_medyan_ns", g->sicak_medyan_ns, y.sicak_medyan_ns);
        c.tam(p + ".soguk_medyan_ns", g->soguk_medyan_ns, y.soguk_medyan_ns);
        c.tam(p + ".t_edinim_ns", g->t_edinim_ns, y.t_edinim_ns);
        c.sayi(p + ".yakinsama_baslangic_hatasi", g->yakinsama_baslangic_hatasi, y.yakinsama_baslangic_hatasi);
        c.tam(p + ".yakinsama_tau_ns", g->yakinsama_tau_ns, y.yakinsama_tau_ns);
        c.sayi(p + ".rapor_carpani", g->rapor_carpani, y.rapor_carpani);
    } else if (const auto* v = std::get_if<DvlAyar>(&a)) {
        const auto& y = std::get<DvlAyar>(b);
        c.sayi(p + ".isin_acisi", v->isin_acisi, y.isin_acisi);
        c.sayi(p + ".yari_guc_genisligi", v->yari_guc_genisligi, y.yari_guc_genisligi);
        c.sayi(p + ".azimut_ofseti", v->azimut_ofseti, y.azimut_ofseti);
        c.tam(p + ".alt_isin_sayisi", v->alt_isin_sayisi, y.alt_isin_sayisi);
        c.sayi(p + ".gelis_acisi_siniri", v->gelis_acisi_siniri, y.gelis_acisi_siniri);
        c.sayi(p + ".sigma0", v->sigma0, y.sigma0);
        c.sayi(p + ".k_r", v->k_r, y.k_r);
        c.sayi(p + ".k_u", v->k_u, y.k_u);
        c.sayi(p + ".uzun_donem_olcek_siniri", v->uzun_donem_olcek_siniri, y.uzun_donem_olcek_siniri);
        c.sayi(p + ".kappa", v->kappa, y.kappa);
        c.secimli(p + ".c_ayar", v->c_ayar, y.c_ayar);
        c.tam(p + ".islem_suresi_ns", v->islem_suresi_ns, y.islem_suresi_ns);
        c.mantik(p + ".uc_isinla_cozum", v->uc_isinla_cozum, y.uc_isinla_cozum);
        c.mantik(p + ".su_takibi", v->su_takibi, y.su_takibi);
        c.tam(p + ".baglanti", static_cast<int>(v->baglanti), static_cast<int>(y.baglanti));
        c.tam(p + ".seri_baud", v->seri_baud, y.seri_baud);
        c.tam(p + ".mod_histerezis_ping", v->mod_histerezis_ping, y.mod_histerezis_ping);
        c.tam(p + ".kilit_arama_ping", v->kilit_arama_ping, y.kilit_arama_ping);
        c.tam(p + ".yakin_mod_deneme_ns", v->yakin_mod_deneme_ns, y.yakin_mod_deneme_ns);
    } else if (const auto* bo = std::get_if<BataryaOlcerAyar>(&a)) {
        const auto& y = std::get<BataryaOlcerAyar>(b);
        c.sayi(p + ".gerilim_bolucu", bo->gerilim_bolucu, y.gerilim_bolucu);
        c.sayi(p + ".akim_ofseti", bo->akim_ofseti, y.akim_ofseti);
        c.sayi(p + ".akim_olcegi", bo->akim_olcegi, y.akim_olcegi);
        c.sayi(p + ".analog_gurultu_sigma", bo->analog_gurultu_sigma, y.analog_gurultu_sigma);
        c.tam(p + ".adc_bit", bo->adc_bit, y.adc_bit);
        c.sayi(p + ".adc_referans", bo->adc_referans, y.adc_referans);
        c.sayi(p + ".sicaklik_kats", bo->sicaklik_kats, y.sicaklik_kats);
        c.tam(p + ".teslim_gecikmesi_ns", bo->teslim_gecikmesi_ns, y.teslim_gecikmesi_ns);
    } else if (const auto* z = std::get_if<SizintiAyar>(&a)) {
        const auto& y = std::get<SizintiAyar>(b);
        c.tam(p + ".prob_sayisi", z->prob_sayisi, y.prob_sayisi);
        c.tam(p + ".algilama_gecikmesi_ns", z->algilama_gecikmesi_ns, y.algilama_gecikmesi_ns);
        c.sayi(p + ".sahte_pozitif_orani", z->sahte_pozitif_orani, y.sahte_pozitif_orani);
    } else if (const auto* f = std::get_if<FirlaticiGeriOkumaAyar>(&a)) {
        const auto& y = std::get<FirlaticiGeriOkumaAyar>(b);
        c.tam(p + ".anahtar_gecikmesi_ns", f->anahtar_gecikmesi_ns, y.anahtar_gecikmesi_ns);
        c.tam(p + ".teslim_gecikmesi_ns", f->teslim_gecikmesi_ns, y.teslim_gecikmesi_ns);
    }
}

}  // namespace

int test_sensor_yukleyici(const TestBaglami& tb)
{
    Denetci d;
    const auto k = ornekten(tb);
    DOGRULA(d, k.has_value(), "örnekten sensör kümesi kurulamadı");
    if (!k) {
        return d.hata;
    }
    DOGRULA(d, k->sensorler.size() == 11, "11 sensör bekleniyordu");
    DOGRULA(d, !k->ortam.su.ses_hizi.has_value(), "A5'te verilmemiş ses hızı çözümlenmiş biçimde yoktur (formülden)");
    // Örnek senaryoda ses hızı verilmez (A5 §3.9: "formülden"); W4-K1 gereği sensör kümesi ses
    // hızını formülden hesaplar (W4-sonar), `ornek-havuz` + etkin DVL birleşimi kurulur.
    {
        sensor::SensorKumesi kume;
        const sensor::KurulumSonucu r = kume.kur(*k);
        DOGRULA(d, r.tamam, "W4-K1: ses hızı verilmeden DVL'li küme formülle kurulmalı: " + r.hata);
    }
    // Ses hızı senaryoda verilince sensör kümesi bu ayarla kurulur.
    const auto v = ornekten(tb, true);
    DOGRULA(d, v.has_value() && v->ortam.su.ses_hizi == 1482.3, "verilmiş ses hızı taşınmalı");
    if (v) {
        sensor::SensorKumesi kume;
        const sensor::KurulumSonucu r = kume.kur(*v);
        DOGRULA(d, r.tamam, "SensorKumesi::kur başarısız: " + r.hata);
    }
    return d.hata;
}

int test_sensor_karsilastirma(const TestBaglami& tb)
{
    Denetci d;
    const auto k = ornekten(tb);
    DOGRULA(d, k.has_value(), "örnek kurulamadı");
    if (!k) {
        return d.hata;
    }
    const sensor::SensorKumesiAyari kod = sensor::ida0_sensor_kumesi();
    Karsilastirma c;
    c.tam("sensor sayısı", k->sensorler.size(), kod.sensorler.size());
    for (const auto& s : k->sensorler) {
        const sensor::SensorAyari* y = nullptr;
        for (const auto& x : kod.sensorler) {
            if (x.ad == s.ad) y = &x;
        }
        const std::string p = "arac.sensorler." + s.ad;
        if (y == nullptr) {
            c.farklar.push_back(p + ": kodda yok");
            continue;
        }
        c.tam(p + ".tur", static_cast<int>(s.tur), static_cast<int>(y->tur));
        c.tam(p + ".kimlik", s.kimlik, y->kimlik);
        c.tam(p + ".inst", s.inst, y->inst);
        c.dizgi(p + ".profil", s.profil, y->profil);
        c.mantik(p + ".etkin", s.etkin, y->etkin);
        c.secimli(p + ".hiz_hz", s.hiz_hz, y->hiz_hz);
        if (s.hiz_hz && y->hiz_hz) c.sayi(p + ".hiz_hz", *s.hiz_hz, *y->hiz_hz);
        c.sayi(p + ".saat_kaymasi_ppm", s.saat_kaymasi_ppm, y->saat_kaymasi_ppm);
        c.dizi(p + ".montaj.konum", s.montaj.konum, y->montaj.konum);
        c.dizi(p + ".montaj.yonelim", s.montaj.yonelim, y->montaj.yonelim);
        ayar_karsilastir(c, p + ".ayar", s.ayar, y->ayar);
    }
    c.sayi("ortam.yercekimi", k->ortam.yercekimi, kod.ortam.yercekimi);
    c.sayi("ortam.atmosfer_basinci", k->ortam.atmosfer_basinci, kod.ortam.atmosfer_basinci);
    c.sayi("ortam.derinlik", k->ortam.derinlik, kod.ortam.derinlik);
    c.sayi("ortam.su.yogunluk", k->ortam.su.yogunluk, kod.ortam.su.yogunluk);
    c.sayi("ortam.su.sicaklik_c", k->ortam.su.sicaklik_c, kod.ortam.su.sicaklik_c);
    c.secimli("ortam.su.ses_hizi", k->ortam.su.ses_hizi, kod.ortam.su.ses_hizi);
    c.secimli("ortam.orijin", k->ortam.orijin, kod.ortam.orijin);
    if (k->ortam.orijin && kod.ortam.orijin) {
        c.sayi("ortam.orijin.enlem", k->ortam.orijin->enlem, kod.ortam.orijin->enlem);
        c.sayi("ortam.orijin.boylam", k->ortam.orijin->boylam, kod.ortam.orijin->boylam);
        c.sayi("ortam.orijin.yukseklik", k->ortam.orijin->yukseklik, kod.ortam.orijin->yukseklik);
    }
    c.secimli("ortam.manyetik_alan", k->ortam.manyetik_alan, kod.ortam.manyetik_alan);
    if (k->ortam.manyetik_alan && kod.ortam.manyetik_alan) {
        c.dizi("ortam.manyetik_alan", *k->ortam.manyetik_alan, *kod.ortam.manyetik_alan);
    }
    c.dizi("ortam.akinti.hiz_ned", k->ortam.akinti.hiz_ned, kod.ortam.akinti.hiz_ned);
    c.sayi("guc.v_sabit", k->guc.v_sabit, kod.guc.v_sabit);
    c.sayi("guc.aviyonik_guc", k->guc.aviyonik_guc, kod.guc.aviyonik_guc);
    c.dizi("guc.gerilim_noktalari", k->guc.gerilim_noktalari, kod.guc.gerilim_noktalari);
    c.dizi("guc.azami_itki_ileri", k->guc.azami_itki_ileri, kod.guc.azami_itki_ileri);
    c.dizi("guc.azami_itki_geri", k->guc.azami_itki_geri, kod.guc.azami_itki_geri);
    c.dizi("guc.tam_gaz_akimi", k->guc.tam_gaz_akimi, kod.guc.tam_gaz_akimi);
    c.sayi("guc.akim_ussu", k->guc.akim_ussu, kod.guc.akim_ussu);
    for (std::size_t i = 0; i < k->guc.itici_konumu.size(); ++i) {
        c.dizi("guc.itici_konumu[" + std::to_string(i) + "]", k->guc.itici_konumu[i], kod.guc.itici_konumu[i]);
        c.dizi("guc.itici_yonu[" + std::to_string(i) + "]", k->guc.itici_yonu[i], kod.guc.itici_yonu[i]);
    }
    c.tam("ana_tohum", k->ana_tohum, kod.ana_tohum);
    c.tam("adim_ns", k->adim_ns, kod.adim_ns);
    c.tam("kontrol_periyodu_ns", k->kontrol_periyodu_ns, kod.kontrol_periyodu_ns);
    c.yazdir("sensör kümesi (ida0.toml + ornek-havuz.toml ↔ sensor::ida0_sensor_kumesi())");
    std::printf("  (farklar raporlanır, eşitlenmez; karar mimarındır)\n");
    return d.hata;
}

}  // namespace ida::senaryo::test

int main(int argc, char** argv)
{
    using namespace ida::senaryo::test;
    if (argc < 3) {
        std::printf("kullanım: %s <test> <depo kökü>\n", argv[0]);
        return 2;
    }
    TestBaglami b;
    b.kok = argv[2];
    int r = 2;
    if (std::strcmp(argv[1], "sensor_yukleyici") == 0) {
        r = test_sensor_yukleyici(b);
    } else if (std::strcmp(argv[1], "sensor_karsilastirma") == 0) {
        r = test_sensor_karsilastirma(b);
    } else {
        std::printf("bilinmeyen test: %s\n", argv[1]);
        return 2;
    }
    std::printf("[%s] %s\n", argv[1], r == 0 ? "GEÇTİ" : "KALDI");
    return r == 0 ? 0 : 1;
}
