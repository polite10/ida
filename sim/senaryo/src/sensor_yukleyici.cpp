// Sensör yükleyici (dilim 2): çözümlenmiş senaryo → ida::sensor::SensorKumesiAyari.
#include "ida/senaryo/sensor_yukleyici.hpp"

#include <map>
#include <string_view>

namespace ida::senaryo {

namespace {

class Ayar {
public:
    Ayar(const Deger* a, std::string yol, std::string& hata) : a_(a), yol_(std::move(yol)), hata_(hata) {}

    void sayi(const char* k, double& o) const
    {
        if (const Deger* d = bul(k); d != nullptr) {
            if (d->sayisal_mi()) o = d->cift();
            else tip(k);
        }
    }
    void tam(const char* k, std::int64_t& o) const
    {
        if (const Deger* d = bul(k); d != nullptr) {
            if (d->tur == Tur::tamsayi) o = d->tam;
            else tip(k);
        }
    }
    void tam(const char* k, int& o) const
    {
        std::int64_t v = o;
        tam(k, v);
        o = static_cast<int>(v);
    }
    void mantik(const char* k, bool& o) const
    {
        if (const Deger* d = bul(k); d != nullptr) {
            if (d->mantiksal_mi()) o = d->mantik;
            else tip(k);
        }
    }
    template <std::size_t N>
    void vek(const char* k, std::array<double, N>& o) const
    {
        if (const Deger* d = bul(k); d != nullptr) {
            if (!d->dizi_mi() || d->dizi.size() != N) {
                tip(k);
                return;
            }
            for (std::size_t i = 0; i < N; ++i) o[i] = d->dizi[i].cift();
        }
    }
    void mat3(const char* k, fizik::Mat3& o) const
    {
        if (const Deger* d = bul(k); d != nullptr) {
            if (!d->dizi_mi() || d->dizi.size() != 3) {
                tip(k);
                return;
            }
            for (std::size_t i = 0; i < 3; ++i) {
                const Deger& s = d->dizi[i];
                if (!s.dizi_mi() || s.dizi.size() != 3) {
                    tip(k);
                    return;
                }
                for (std::size_t j = 0; j < 3; ++j) o[i][j] = s.dizi[j].cift();
            }
        }
    }
    const Deger* bul(const char* k) const { return a_ != nullptr ? a_->bul(k) : nullptr; }

private:
    const Deger* a_;
    std::string yol_;
    std::string& hata_;
    void tip(const char* k) const
    {
        if (hata_.empty()) hata_ = "beklenmeyen tip: " + yol_ + "." + k;
    }
};

sensor::SensorAyarGovdesi govde(sensor::SensorTuru tur, const Ayar& a)
{
    using namespace sensor;
    switch (tur) {
    case SensorTuru::imu: {
        ImuAyar x;
        a.sayi("jiroskop_yogunluk", x.jiroskop_yogunluk);
        a.vek("ivme_yogunluk", x.ivme_yogunluk);
        a.sayi("jiroskop_acilis_bias_siniri", x.jiroskop_acilis_bias_siniri);
        a.sayi("ivme_acilis_bias_siniri", x.ivme_acilis_bias_siniri);
        a.sayi("jiroskop_sicaklik_kats_siniri", x.jiroskop_sicaklik_kats_siniri);
        a.sayi("ivme_sicaklik_kats_siniri", x.ivme_sicaklik_kats_siniri);
        a.sayi("jiroskop_olcek_siniri", x.jiroskop_olcek_siniri);
        a.sayi("jiroskop_eksenler_arasi_siniri", x.jiroskop_eksenler_arasi_siniri);
        a.sayi("ivme_olcek_siniri", x.ivme_olcek_siniri);
        a.sayi("ivme_eksenler_arasi_siniri", x.ivme_eksenler_arasi_siniri);
        a.sayi("jiroskop_dogrusalsizlik", x.jiroskop_dogrusalsizlik);
        a.sayi("ivme_dogrusalsizlik", x.ivme_dogrusalsizlik);
        a.tam("jiroskop_gm_tau_ns", x.jiroskop_gm_tau_ns);
        a.tam("ivme_gm_tau_ns", x.ivme_gm_tau_ns);
        a.sayi("jiroskop_gm_sigma", x.jiroskop_gm_sigma);
        a.sayi("ivme_gm_sigma", x.ivme_gm_sigma);
        a.sayi("jiroskop_aralik", x.jiroskop_aralik);
        a.sayi("ivme_aralik", x.ivme_aralik);
        a.tam("bit", x.bit);
        a.mantik("suzgec_etkin", x.suzgec_etkin);
        a.tam("suzgec_tau_ns", x.suzgec_tau_ns);
        a.mantik("dunya_donusu_etkin", x.dunya_donusu_etkin);
        return x;
    }
    case SensorTuru::manyetometre: {
        ManyetometreAyar x;
        a.sayi("gurultu_sigma", x.gurultu_sigma);
        a.mat3("yumusak_demir", x.yumusak_demir);
        a.vek("sert_demir", x.sert_demir);
        a.sayi("sicaklik_kats", x.sicaklik_kats);
        a.sayi("itici_girisim_orani", x.itici_girisim_orani);
        a.tam("teslim_gecikmesi_ns", x.teslim_gecikmesi_ns);
        return x;
    }
    case SensorTuru::basinc: {
        BasincAyar x;
        a.sayi("gurultu_sigma", x.gurultu_sigma);
        a.sayi("mutlak_dogruluk_siniri", x.mutlak_dogruluk_siniri);
        a.sayi("sicaklik_kats", x.sicaklik_kats);
        a.sayi("kuantalama", x.kuantalama);
        a.tam("donusum_penceresi_ns", x.donusum_penceresi_ns);
        a.tam("okuma_suresi_ns", x.okuma_suresi_ns);
        a.sayi("dinamik_basinc_kats", x.dinamik_basinc_kats);
        a.sayi("sicaklik_ofset_siniri", x.sicaklik_ofset_siniri);
        return x;
    }
    case SensorTuru::gnss: {
        GnssAyar x;
        a.sayi("cep", x.cep);
        a.tam("gm_tau_ns", x.gm_tau_ns);
        a.sayi("gm_varyans_payi", x.gm_varyans_payi);
        a.sayi("dusey_carpan", x.dusey_carpan);
        a.sayi("hiz_sigma", x.hiz_sigma);
        a.tam("teslim_gecikmesi_ns", x.teslim_gecikmesi_ns);
        a.tam("uydu_sayisi", x.uydu_sayisi);
        a.tam("n_sv_ref", x.n_sv_ref);
        a.sayi("w50", x.w50);
        a.sayi("w_gecis", x.w_gecis);
        a.tam("t_kisa_ns", x.t_kisa_ns);
        a.tam("t_efemeris_ns", x.t_efemeris_ns);
        a.tam("hizli_medyan_ns", x.hizli_medyan_ns);
        a.tam("sicak_medyan_ns", x.sicak_medyan_ns);
        a.tam("soguk_medyan_ns", x.soguk_medyan_ns);
        a.tam("t_edinim_ns", x.t_edinim_ns);
        a.sayi("yakinsama_baslangic_hatasi", x.yakinsama_baslangic_hatasi);
        a.tam("yakinsama_tau_ns", x.yakinsama_tau_ns);
        a.sayi("rapor_carpani", x.rapor_carpani);
        return x;
    }
    case SensorTuru::dvl: {
        DvlAyar x;
        a.sayi("isin_acisi", x.isin_acisi);
        a.sayi("yari_guc_genisligi", x.yari_guc_genisligi);
        a.sayi("azimut_ofseti", x.azimut_ofseti);
        a.tam("alt_isin_sayisi", x.alt_isin_sayisi);
        a.sayi("gelis_acisi_siniri", x.gelis_acisi_siniri);
        a.sayi("sigma0", x.sigma0);
        a.sayi("k_r", x.k_r);
        a.sayi("k_u", x.k_u);
        a.sayi("uzun_donem_olcek_siniri", x.uzun_donem_olcek_siniri);
        a.sayi("kappa", x.kappa);
        if (const Deger* c = a.bul("c_ayar"); c != nullptr && c->sayisal_mi()) {
            x.c_ayar = c->cift();  // yoksa KE: ortamın ses hızı (A5-K101)
        }
        a.tam("islem_suresi_ns", x.islem_suresi_ns);
        a.mantik("uc_isinla_cozum", x.uc_isinla_cozum);
        a.mantik("su_takibi", x.su_takibi);
        if (const Deger* b = a.bul("baglanti"); b != nullptr && b->dizgi_mi()) {
            x.baglanti = b->dizgi == "tcp" ? DvlBaglanti::tcp : DvlBaglanti::seri;
        }
        a.tam("seri_baud", x.seri_baud);
        a.tam("mod_histerezis_ping", x.mod_histerezis_ping);
        a.tam("kilit_arama_ping", x.kilit_arama_ping);
        a.tam("yakin_mod_deneme_ns", x.yakin_mod_deneme_ns);
        return x;
    }
    case SensorTuru::batarya_olcer: {
        BataryaOlcerAyar x;
        a.sayi("gerilim_bolucu", x.gerilim_bolucu);
        a.sayi("akim_ofseti", x.akim_ofseti);
        a.sayi("akim_olcegi", x.akim_olcegi);
        a.sayi("analog_gurultu_sigma", x.analog_gurultu_sigma);
        a.tam("adc_bit", x.adc_bit);
        a.sayi("adc_referans", x.adc_referans);
        a.sayi("sicaklik_kats", x.sicaklik_kats);
        a.tam("teslim_gecikmesi_ns", x.teslim_gecikmesi_ns);
        return x;
    }
    case SensorTuru::sizinti: {
        SizintiAyar x;
        a.tam("prob_sayisi", x.prob_sayisi);
        a.tam("algilama_gecikmesi_ns", x.algilama_gecikmesi_ns);
        a.sayi("sahte_pozitif_orani", x.sahte_pozitif_orani);
        return x;
    }
    case SensorTuru::firlatici_geri_okuma: {
        FirlaticiGeriOkumaAyar x;
        a.tam("anahtar_gecikmesi_ns", x.anahtar_gecikmesi_ns);
        a.tam("teslim_gecikmesi_ns", x.teslim_gecikmesi_ns);
        return x;
    }
    case SensorTuru::ping1d:
    case SensorTuru::ping360:
    case SensorTuru::kamera:
        break;
    }
    return KapsamDisiAyar{};
}

}  // namespace

std::optional<sensor::SensorKumesiAyari> sensor_kur(const DogrulanmisSenaryo& dogrulanmis, std::string* hata_cikti)
{
    using namespace sensor;
    std::string hata;
    std::string fizik_hatasi;
    const Deger& c = dogrulanmis.agac();
    const std::optional<FizikKurulumu> f = fizik_kur(dogrulanmis, &fizik_hatasi);
    if (!f) {
        if (hata_cikti != nullptr) *hata_cikti = fizik_hatasi;
        return std::nullopt;
    }
    static const std::map<std::string_view, SensorTuru> kTurler = {
        {"imu", SensorTuru::imu},
        {"manyetometre", SensorTuru::manyetometre},
        {"basinc", SensorTuru::basinc},
        {"gnss", SensorTuru::gnss},
        {"dvl", SensorTuru::dvl},
        {"ping1d", SensorTuru::ping1d},
        {"ping360", SensorTuru::ping360},
        {"kamera", SensorTuru::kamera},
        {"batarya_olcer", SensorTuru::batarya_olcer},
        {"sizinti", SensorTuru::sizinti},
        {"firlatici_geri_okuma", SensorTuru::firlatici_geri_okuma},
    };
    SensorKumesiAyari k;
    if (const Deger* sl = c.yol_bul("arac.sensorler"); sl != nullptr && sl->tablo_mu()) {
        for (const auto& [ad, sn] : sl->tablo) {
            if (deneysel_mi(ad) || !sn.tablo_mu()) {
                continue;
            }
            const Deger* tur = sn.bul("tur");
            const auto it = tur != nullptr && tur->dizgi_mi() ? kTurler.find(tur->dizgi) : kTurler.end();
            if (it == kTurler.end()) {
                hata = "bilinmeyen sensör türü: " + ad;
                break;
            }
            SensorAyari s;
            s.ad = ad;
            s.tur = it->second;
            const Ayar ust(&sn, "arac.sensorler." + ad, hata);
            std::int64_t v = 0;
            ust.tam("kimlik", v);
            s.kimlik = static_cast<std::uint16_t>(v);
            ust.tam("inst", v);
            s.inst = static_cast<std::uint8_t>(v);
            if (const Deger* p = sn.bul("profil"); p != nullptr && p->dizgi_mi()) s.profil = p->dizgi;
            ust.mantik("etkin", s.etkin);
            if (const Deger* h = sn.bul("hiz_hz"); h != nullptr && h->sayisal_mi()) s.hiz_hz = h->cift();
            ust.sayi("saat_kaymasi_ppm", s.saat_kaymasi_ppm);
            const Ayar montaj(sn.bul("montaj"), "arac.sensorler." + ad + ".montaj", hata);
            montaj.vek("konum", s.montaj.konum);
            montaj.vek("yonelim", s.montaj.yonelim);
            s.ayar = govde(s.tur, Ayar(sn.bul("ayar"), "arac.sensorler." + ad + ".ayar", hata));
            k.sensorler.push_back(std::move(s));
        }
    }
    const Ayar ortam(c.bul("ortam"), "ortam", hata);
    ortam.sayi("yercekimi", k.ortam.yercekimi);
    ortam.sayi("atmosfer_basinci", k.ortam.atmosfer_basinci);
    ortam.sayi("derinlik", k.ortam.derinlik);
    const Ayar su(c.yol_bul("ortam.su"), "ortam.su", hata);
    su.sayi("yogunluk", k.ortam.su.yogunluk);
    su.sayi("sicaklik_c", k.ortam.su.sicaklik_c);
    if (const Deger* sh = c.yol_bul("ortam.su.ses_hizi"); sh != nullptr && sh->sayisal_mi()) {
        k.ortam.su.ses_hizi = sh->cift();
    }
    if (const Deger* o = c.yol_bul("ortam.orijin"); o != nullptr) {
        OrtamOrijin x;
        const Ayar oa(o, "ortam.orijin", hata);
        oa.sayi("enlem", x.enlem);
        oa.sayi("boylam", x.boylam);
        oa.sayi("yukseklik", x.yukseklik);
        k.ortam.orijin = x;
    }
    if (c.yol_bul("ortam.manyetik_alan") != nullptr) {
        Vek3 b{};
        ortam.vek("manyetik_alan", b);
        k.ortam.manyetik_alan = b;
    }
    const Ayar akinti(c.yol_bul("ortam.akinti"), "ortam.akinti", hata);
    akinti.vek("hiz_ned", k.ortam.akinti.hiz_ned);
    k.guc = guc_parametreleri(f->arac);
    if (const Deger* t = c.yol_bul("rastgelelik.ana_tohum"); t != nullptr && t->tur == Tur::tamsayi) {
        k.ana_tohum = static_cast<std::uint64_t>(t->tam);
    }
    if (!hata.empty()) {
        if (hata_cikti != nullptr) *hata_cikti = hata;
        return std::nullopt;
    }
    return k;
}

}  // namespace ida::senaryo
