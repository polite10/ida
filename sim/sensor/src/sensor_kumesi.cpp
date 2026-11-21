// SensorKumesi: kurulum doğrulaması, adım işleme, arıza zinciri, teslim kuyruğu.
#include "ida/sensor/sensor_kumesi.hpp"

#include "model.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <limits>
#include <string>

namespace ida::sensor {

namespace {

using ic::TemelKayit;

constexpr std::size_t kKuyrukSiniri = 64;   // sensör başına (05 §3 [T])
constexpr std::int64_t kSonsuz = std::numeric_limits<std::int64_t>::max();
constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- WGS-84 (A5 orijin)
constexpr double kWgsA = 6378137.0;
constexpr double kWgsF = 1.0 / 298.257223563;
constexpr double kWgsE2 = kWgsF * (2.0 - kWgsF);

Vek3 lla_ecef(double enlem, double boylam, double h)
{
    const double se = std::sin(enlem), ce = std::cos(enlem);
    const double n = kWgsA / std::sqrt(1.0 - kWgsE2 * se * se);
    return {(n + h) * ce * std::cos(boylam), (n + h) * ce * std::sin(boylam),
            (n * (1.0 - kWgsE2) + h) * se};
}

// Orijine göre NED konumundan WGS-84 enlem, boylam (rad) ve elipsoit yüksekliği (m).
Vek3 ned_lla(const OrtamOrijin& o, const Vek3& ned)
{
    const double se = std::sin(o.enlem), ce = std::cos(o.enlem);
    const double sb = std::sin(o.boylam), cb = std::cos(o.boylam);
    const Vek3 e0 = lla_ecef(o.enlem, o.boylam, o.yukseklik);
    // R_ecef←ned sütunları: N, E, D.
    const Vek3 x = {e0[0] - se * cb * ned[0] - sb * ned[1] - ce * cb * ned[2],
                    e0[1] - se * sb * ned[0] + cb * ned[1] - ce * sb * ned[2],
                    e0[2] + ce * ned[0] - se * ned[2]};
    const double p = std::sqrt(x[0] * x[0] + x[1] * x[1]);
    const double boylam = std::atan2(x[1], x[0]);
    double enlem = std::atan2(x[2], p * (1.0 - kWgsE2));
    double h = 0.0;
    for (int i = 0; i < 8; ++i) {
        const double s = std::sin(enlem);
        const double n = kWgsA / std::sqrt(1.0 - kWgsE2 * s * s);
        h = p / std::cos(enlem) - n;
        enlem = std::atan2(x[2], p * (1.0 - kWgsE2 * n / (n + h)));
    }
    return {enlem, boylam, h};
}

std::int32_t e7(double rad)
{
    const double d = rad * 180.0 / kPi * 1e7;
    return static_cast<std::int32_t>(std::llround(std::clamp(d, -1.8e9, 1.8e9)));
}

// ---------------------------------------------------------------- kurulum denetimi
bool ad_gecerli(const std::string& ad)
{
    if (ad.empty() || ad.size() > 32 || ad[0] < 'a' || ad[0] > 'z') {
        return false;
    }
    if (ad.size() >= 2 && ad[0] == 'x' && ad[1] == '_') {
        return false;
    }
    return std::all_of(ad.begin(), ad.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

bool govde_uyumlu(const SensorAyari& a)
{
    switch (a.tur) {
    case SensorTuru::imu: return std::holds_alternative<ImuAyar>(a.ayar);
    case SensorTuru::manyetometre: return std::holds_alternative<ManyetometreAyar>(a.ayar);
    case SensorTuru::basinc: return std::holds_alternative<BasincAyar>(a.ayar);
    case SensorTuru::gnss: return std::holds_alternative<GnssAyar>(a.ayar);
    case SensorTuru::dvl: return std::holds_alternative<DvlAyar>(a.ayar);
    case SensorTuru::batarya_olcer: return std::holds_alternative<BataryaOlcerAyar>(a.ayar);
    case SensorTuru::sizinti: return std::holds_alternative<SizintiAyar>(a.ayar);
    case SensorTuru::firlatici_geri_okuma:
        return std::holds_alternative<FirlaticiGeriOkumaAyar>(a.ayar);
    case SensorTuru::ping1d:
    case SensorTuru::ping360:
    case SensorTuru::kamera: return std::holds_alternative<KapsamDisiAyar>(a.ayar);
    }
    return false;
}

bool kapsam_disi(SensorTuru t)
{
    return t == SensorTuru::ping1d || t == SensorTuru::ping360 || t == SensorTuru::kamera;
}

// Süre alanlarının üst sınırı (ns, ≈ 104 gün): sim zamanı (≤ 2⁶², A4-K08) ile toplamları int64'te
// taşmaz. A5 üst sınır vermez; bu, kurulumun temsil denetimidir (W4-sensor sol #5).
constexpr std::int64_t kAzamiSure = std::int64_t{1} << 53;
constexpr std::int64_t kAzamiZaman = std::int64_t{1} << 62;   // IDA_TIME_MAX_NS (A4-K08)

// Süre denetimi: `sifir_olabilir` ise [0, kAzamiSure], değilse (0, kAzamiSure] (A5-K48).
bool sure_ok(std::int64_t v, bool sifir_olabilir)
{
    return (sifir_olabilir ? v >= 0 : v > 0) && v <= kAzamiSure;
}

bool negatif_degil(double v) { return std::isfinite(v) && v >= 0.0; }

// Boş dizgi: geçerli.
std::string sensor_denetle(const SensorAyari& a, const SensorKumesiAyari& k)
{
    const std::string on = "arac.sensorler." + a.ad + ": ";
    if (!govde_uyumlu(a)) {
        return on + "tur ile ayar tablosu uyuşmuyor";
    }
    if (a.inst > 3) {
        return on + "inst 0–3 olmalı (A4-K30, A1 §3.4)";
    }
    const double qn = std::sqrt(a.montaj.yonelim[0] * a.montaj.yonelim[0] +
                                a.montaj.yonelim[1] * a.montaj.yonelim[1] +
                                a.montaj.yonelim[2] * a.montaj.yonelim[2] +
                                a.montaj.yonelim[3] * a.montaj.yonelim[3]);
    if (!(std::abs(qn - 1.0) <= 1e-6) || !ic::sonlu(a.montaj.konum)) {
        return on + "montaj.yonelim birim kuaterniyon değil ya da konum sonlu değil (A5-K32)";
    }
    const bool hizsiz = a.tur == SensorTuru::imu || a.tur == SensorTuru::dvl;
    if (hizsiz && a.hiz_hz) {
        return on + "hiz_hz bu türde verilemez (A5-K41)";
    }
    if (a.tur == SensorTuru::imu && a.saat_kaymasi_ppm != 0.0) {
        return on + "imu için saat_kaymasi_ppm verilemez (A5-K41)";
    }
    if (a.hiz_hz && !(*a.hiz_hz > 0.0 && *a.hiz_hz <= 800.0)) {
        return on + "hiz_hz (0, 800] dışında (A5-K43)";
    }
    if (!std::isfinite(a.saat_kaymasi_ppm) || std::abs(a.saat_kaymasi_ppm) >= 1e5) {
        return on + "saat_kaymasi_ppm geçersiz";
    }
    if (kapsam_disi(a.tur)) {
        return {};
    }
    switch (a.tur) {
    case SensorTuru::imu: {
        const auto& p = std::get<ImuAyar>(a.ayar);
        if (!sure_ok(p.jiroskop_gm_tau_ns, false) || !sure_ok(p.ivme_gm_tau_ns, false)) {
            return on + "gm_tau_ns > 0 olmalı (A5-K48)";
        }
        if (!(p.jiroskop_aralik > 0.0) || !(p.ivme_aralik > 0.0) || p.bit < 1 || p.bit > 31) {
            return on + "aralik ya da bit geçersiz";
        }
        if (p.suzgec_etkin && !sure_ok(p.suzgec_tau_ns, false)) {
            return on + "suzgec_etkin iken suzgec_tau_ns > 0 zorunlu (A5-K46)";
        }
        if (p.dunya_donusu_etkin && !k.ortam.orijin) {
            return on + "dunya_donusu_etkin iken ortam.orijin zorunlu (A5-K45)";
        }
        break;
    }
    case SensorTuru::manyetometre: {
        const auto& p = std::get<ManyetometreAyar>(a.ayar);
        if (!k.ortam.manyetik_alan) {
            return on + "etkin manyetometre için ortam.manyetik_alan zorunlu (A5-K44)";
        }
        if (!sure_ok(p.teslim_gecikmesi_ns, true) || !negatif_degil(p.gurultu_sigma)) {
            return on + "teslim_gecikmesi_ns ≥ 0 ve gurultu_sigma ≥ 0 olmalı (A5-K48)";
        }
        break;
    }
    case SensorTuru::basinc: {
        const auto& p = std::get<BasincAyar>(a.ayar);
        if (!sure_ok(p.donusum_penceresi_ns, false) || !sure_ok(p.okuma_suresi_ns, true) ||
            !negatif_degil(p.gurultu_sigma)) {
            return on + "donusum_penceresi_ns > 0, okuma_suresi_ns ≥ 0 olmalı (A5-K48)";
        }
        if (p.donusum_penceresi_ns > static_cast<std::int64_t>(UINT32_MAX)) {
            return on + "donusum_penceresi_ns window_ns'e (u32) sığmıyor (A4 §3.2)";
        }
        const double f = a.hiz_hz.value_or(25.0);
        if (f > 1e9 / static_cast<double>(2 * p.donusum_penceresi_ns + p.okuma_suresi_ns)) {
            return on + "hiz_hz > 1 / (2 · donusum_penceresi + okuma_suresi) (A5-K42)";
        }
        break;
    }
    case SensorTuru::gnss: {
        const auto& p = std::get<GnssAyar>(a.ayar);
        if (!k.ortam.orijin) {
            return on + "etkin gnss için ortam.orijin zorunlu (A5-K45)";
        }
        if (p.uydu_sayisi < 1 || p.uydu_sayisi > 64 || p.n_sv_ref < 1 || p.gm_tau_ns <= 0 ||
            !(p.w_gecis > 0.0) || p.gm_varyans_payi < 0.0 || p.gm_varyans_payi > 1.0) {
            return on + "gnss ayarı aralık dışında";
        }
        if (!sure_ok(p.teslim_gecikmesi_ns, true) || !sure_ok(p.gm_tau_ns, false) ||
            !sure_ok(p.t_kisa_ns, true) || !sure_ok(p.t_efemeris_ns, true) ||
            !sure_ok(p.hizli_medyan_ns, false) || !sure_ok(p.sicak_medyan_ns, false) ||
            !sure_ok(p.soguk_medyan_ns, false) || !sure_ok(p.t_edinim_ns, true) ||
            !negatif_degil(p.cep) || !negatif_degil(p.hiz_sigma) || !(p.w50 > 0.0) ||
            !negatif_degil(p.yakinsama_baslangic_hatasi) || !negatif_degil(p.rapor_carpani)) {
            return on + "gnss süre ya da hata parametresi aralık dışında (A5-K48)";
        }
        if (p.yakinsama_baslangic_hatasi > 0.0 && !sure_ok(p.yakinsama_tau_ns, false)) {
            return on + "yakinsama_baslangic_hatasi > 0 iken yakinsama_tau_ns zorunlu (A5-K40)";
        }
        break;
    }
    case SensorTuru::dvl: {
        const auto& p = std::get<DvlAyar>(a.ayar);
        if (p.su_takibi) {
            return on + "su_takibi bu dilimde desteklenmiyor";
        }
        if (!k.ortam.su.ses_hizi || !(*k.ortam.su.ses_hizi > 0.0)) {
            return on + "DVL için çözümlenmiş ortam.su.ses_hizi gerekli (formül sonar işinde)";
        }
        if (!(p.isin_acisi > 0.0 && p.isin_acisi < 0.5 * kPi) || p.seri_baud <= 0 ||
            p.mod_histerezis_ping < 1 || p.kilit_arama_ping < 1 ||
            !sure_ok(p.yakin_mod_deneme_ns, false) || !sure_ok(p.islem_suresi_ns, true)) {
            return on + "dvl ayarı aralık dışında";
        }
        break;
    }
    case SensorTuru::batarya_olcer: {
        const auto& p = std::get<BataryaOlcerAyar>(a.ayar);
        if (!(p.gerilim_bolucu > 0.0) || !(p.akim_olcegi > 0.0) || p.adc_bit < 1 ||
            p.adc_bit > 24 || !(p.adc_referans > 0.0)) {
            return on + "batarya_olcer ayarı aralık dışında";
        }
        if (!sure_ok(p.teslim_gecikmesi_ns, true) || !negatif_degil(p.analog_gurultu_sigma)) {
            return on + "teslim_gecikmesi_ns ≥ 0 olmalı (A5-K48)";
        }
        if (!(k.guc.v_sabit > 0.0)) {
            return on + "güç girdisi: v_sabit > 0 olmalı";
        }
        const double pencere = 1e9 / a.hiz_hz.value_or(10.0);
        if (pencere > 4.0e9) {
            return on + "örnek aralığı window_ns'e (u32) sığmıyor";
        }
        break;
    }
    case SensorTuru::sizinti: {
        const auto& p = std::get<SizintiAyar>(a.ayar);
        if (p.prob_sayisi < 1 || p.prob_sayisi > 8) {
            return on + "prob_sayisi 1–8 olmalı (ida_leak_t.mask 8 bit)";
        }
        if (!sure_ok(p.algilama_gecikmesi_ns, true) ||
            !(p.sahte_pozitif_orani >= 0.0 && p.sahte_pozitif_orani <= 1.0)) {
            return on + "algilama_gecikmesi_ns ≥ 0, sahte_pozitif_orani [0, 1] olmalı";
        }
        break;
    }
    case SensorTuru::firlatici_geri_okuma: {
        // EMNİYET: negatif gecikme gevşetmeyi aynı sınırda algılatır ya da geçmişe teslim damgası
        // üretir; reddedilir (W4-sensor sol #5).
        const auto& p = std::get<FirlaticiGeriOkumaAyar>(a.ayar);
        if (!sure_ok(p.anahtar_gecikmesi_ns, true) || !sure_ok(p.teslim_gecikmesi_ns, true)) {
            return on + "anahtar_gecikmesi_ns ve teslim_gecikmesi_ns ≥ 0 olmalı (A5-K48)";
        }
        break;
    }
    default:
        break;
    }
    return {};
}

// Arıza türünün sensör türüne uygunluğu (A5-K65, Tablo 3.10-A).
bool sayisal_kanalli(SensorTuru t)
{
    return t != SensorTuru::sizinti && t != SensorTuru::firlatici_geri_okuma;
}

}  // namespace

// ================================================================ iç yapı
struct SensorKumesi::Ic {
    struct SensorDurumu {
        std::unique_ptr<ic::Model> model;
        bool son_var = false;
        TemelKayit son_cikti{};      // arıza zincirinin son çıktısı (kayıptan önce)
        bool temel_var = false;
        TemelKayit son_temel{};      // son sonlu temel kayıt (NaN reddi için)
        std::int64_t son_teslim = std::numeric_limits<std::int64_t>::min();
    };
    struct ArizaKaydi {
        ArizaTutamaci tutamac = 0;
        SensorArizasi t{};
        std::size_t sensor = 0;
        std::int64_t t_b = 0;
        std::int64_t t_bitis = kSonsuz;
        Akis akis;
        Akis gecis;
        std::vector<std::int64_t> ge_anlar;  // T_0, T_1, … (D.5)
        bool donma_bakildi = false;
        bool donma_var = false;
        TemelKayit donma{};
    };

    SensorKumesiAyari ayar{};
    bool kuruldu = false;
    std::vector<SensorDurumu> sensorler;
    ic::Halka halka;
    DisDurum dis{};
    bool ilk = true;
    std::uint64_t son_adim = 0;
    std::int64_t t_son = -1;
    double ic_sicaklik = 0.0;
    std::vector<Olcum> kuyruk;       // (t_teslim, sensor_id, inst, seq) sıralı
    Sayaclar say{};
    std::vector<ArizaKaydi> arizalar;
    ArizaTutamaci sonraki_tutamac = 1;
    std::vector<TemelKayit> uretilen;

    static bool once(const OlcumBasligi& a, const OlcumBasligi& b)
    {
        if (a.t_teslim_ns != b.t_teslim_ns) return a.t_teslim_ns < b.t_teslim_ns;
        if (a.sensor_id != b.sensor_id) return a.sensor_id < b.sensor_id;
        if (a.inst != b.inst) return a.inst < b.inst;
        return a.seq < b.seq;
    }

    bool ge_kotu(ArizaKaydi& f, std::int64_t t)
    {
        if (f.ge_anlar.empty()) {
            f.ge_anlar.push_back(f.t_b);
        }
        while (f.ge_anlar.back() <= t) {
            const std::size_t i = f.ge_anlar.size() - 1;
            const double m = static_cast<double>((i % 2 == 0) ? f.t.zaman_deseni.t_iyi_ns
                                                              : f.t.zaman_deseni.t_kotu_ns);
            const double u = duzgun_donusum(f.gecis.sozcukler(i, 0)[0]);
            const double sure = std::ceil(-std::log(u) * m);
            const std::int64_t d = std::max<std::int64_t>(1, static_cast<std::int64_t>(sure));
            f.ge_anlar.push_back(f.ge_anlar.back() + d);
        }
        // t'yi içeren kalış: T_i ≤ t < T_{i+1}
        const auto it = std::upper_bound(f.ge_anlar.begin(), f.ge_anlar.end(), t);
        const std::size_t i = static_cast<std::size_t>(it - f.ge_anlar.begin()) - 1;
        return (i % 2) == 1;
    }

    bool desen_etkin(ArizaKaydi& f, const TemelKayit& r, std::int64_t t_temel)
    {
        switch (f.t.zaman_deseni.tur) {
        case ZamanDeseniTuru::surekli: return true;
        case ZamanDeseniTuru::bernoulli:
            return f.akis.duzgun(r.hdr.seq, c2_kodla(0xFF, 0, 0)) < f.t.zaman_deseni.p;
        case ZamanDeseniTuru::gilbert_elliott: return ge_kotu(f, t_temel);
        }
        return false;
    }

    // Arıza zinciri (05 §2.7, A5 Tablo 3.10-A bileşim sırası). false: kayıt teslim edilmez.
    // Pencereler, zaman desenleri ve sürüklenme kaydın değişmez temel ölçüm zamanıyla
    // değerlendirilir; donmuş damga yalnız çıktı başlığına yazılır (W4-sensor sol #3).
    bool ariza_zinciri(std::size_t si, TemelKayit& r)
    {
        SensorDurumu& s = sensorler[si];
        const std::int64_t t_temel = r.hdr.t_meas_ns;
        bool teslim = true;
        bool sira_boz = false;
        auto uygula = [&](ArizaTuru tur, auto&& islem) {
            for (ArizaKaydi& f : arizalar) {
                if (f.sensor != si || f.t.tur != tur) continue;
                if (!(f.t_b <= t_temel && t_temel < f.t_bitis)) continue;
                if (tur == ArizaTuru::donma || tur == ArizaTuru::donma_zaman_damgasi) {
                    if (!f.donma_bakildi) {
                        f.donma_bakildi = true;
                        f.donma_var = s.son_var;
                        if (s.son_var) {
                            f.donma = s.son_cikti;
                        } else {
                            ++say.donma_onceki_cikti_yok;
                        }
                    }
                }
                if (!desen_etkin(f, r, t_temel)) continue;
                islem(f);
                r.hdr.ariza_maskesi |= 1u << static_cast<unsigned>(tur);
            }
        };
        const std::size_t n = r.n_kanal;
        const double tm = static_cast<double>(t_temel);
        uygula(ArizaTuru::ofset, [&](ArizaKaydi& f) {
            for (std::size_t i = 0; i < n; ++i) r.k[i] += f.t.siddet.ofset;
        });
        uygula(ArizaTuru::olcek, [&](ArizaKaydi& f) {
            for (std::size_t i = 0; i < n; ++i) r.k[i] *= f.t.siddet.olcek;
        });
        uygula(ArizaTuru::suruklenme_dogrusal, [&](ArizaKaydi& f) {
            const double dt = (tm - static_cast<double>(f.t_b)) * 1e-9;
            for (std::size_t i = 0; i < n; ++i) r.k[i] += f.t.siddet.oran * dt;
        });
        uygula(ArizaTuru::suruklenme_ustel, [&](ArizaKaydi& f) {
            const double e = 1.0 - std::exp(-(tm - static_cast<double>(f.t_b)) /
                                            static_cast<double>(f.t.siddet.tau_ns));
            for (std::size_t i = 0; i < n; ++i) r.k[i] += f.t.siddet.son_deger * e;
        });
        uygula(ArizaTuru::gurultu_artisi, [&](ArizaKaydi& f) {
            const double c = std::sqrt(f.t.siddet.k * f.t.siddet.k - 1.0);
            for (std::size_t i = 0; i < n; ++i) {
                r.k[i] += r.sigma_nom[i] * c *
                          f.akis.normal(r.hdr.seq, c2_kodla(static_cast<std::uint32_t>(i), 0, 0));
            }
        });
        uygula(ArizaTuru::sicrama, [&](ArizaKaydi& f) {
            for (std::size_t i = 0; i < n; ++i) {
                const auto w = f.akis.sozcukler(r.hdr.seq, c2_kodla(static_cast<std::uint32_t>(i), 1, 0));
                if (duzgun_donusum(w[0]) < f.t.siddet.p) {
                    r.k[i] += f.t.siddet.genlik * (w[1] < 0x80000000u ? 1.0 : -1.0);
                }
            }
        });
        auto dondur = [&](ArizaKaydi& f, bool damga) {
            if (!f.donma_var) {
                teslim = false;   // dondurulacak önceki çıktı yok: kayıt üretilmez
                return;
            }
            const OlcumBasligi h = r.hdr;
            r = f.donma;
            r.hdr.seq = h.seq;
            r.hdr.t_hazir_ns = h.t_hazir_ns;
            r.hdr.t_teslim_ns = h.t_teslim_ns;
            r.hdr.ariza_maskesi = h.ariza_maskesi;
            if (!damga) {
                r.hdr.t_meas_ns = h.t_meas_ns;
                r.hdr.window_ns = h.window_ns;
                r.hdr.t_tx_ns = h.t_tx_ns;
            }
        };
        uygula(ArizaTuru::donma, [&](ArizaKaydi& f) { dondur(f, false); });
        uygula(ArizaTuru::donma_zaman_damgasi, [&](ArizaKaydi& f) { dondur(f, true); });
        uygula(ArizaTuru::yanlis_gecerlilik, [&](ArizaKaydi&) {
            r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags ^ kBayrakGecerli);
        });
        uygula(ArizaTuru::kendini_bildiren, [&](ArizaKaydi&) {
            r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags | kBayrakSensorArizasi);
        });
        if (teslim) {
            s.son_var = true;
            s.son_cikti = r;
        }
        uygula(ArizaTuru::kayip, [&](ArizaKaydi&) {
            if (teslim) ++say.kayip;
            teslim = false;
        });
        uygula(ArizaTuru::gec_teslim, [&](ArizaKaydi& f) {
            const std::int64_t j_s = f.t.siddet.titresim_ns;
            std::int64_t j = 0;
            if (j_s > 0) {
                const double u = duzgun_donusum(f.akis.sozcukler(r.hdr.seq, c2_kodla(0, 2, 0))[0]);
                j = static_cast<std::int64_t>(std::floor(u * static_cast<double>(2 * j_s + 1))) - j_s;
            }
            r.hdr.t_teslim_ns += f.t.siddet.gecikme_ns + j;
            sira_boz = sira_boz || f.t.sira_boz;
        });
        if (teslim) {
            if (!sira_boz) {
                r.hdr.t_teslim_ns = std::max(r.hdr.t_teslim_ns, s.son_teslim);
            }
            s.son_teslim = r.hdr.t_teslim_ns;
        }
        return teslim;
    }

    // GNSS anten konumu (kanal 0–2, orijine göre NED) → enlem, boylam (rad) ve ortalama deniz
    // seviyesine göre yükseklik `h − N` (m; A4 §3.3, W4-K7). Tel alanlarına temsil edilemezse false.
    bool gnss_konumu(const TemelKayit& r, Vek3& lla) const
    {
        if (!ayar.ortam.orijin) {
            return false;
        }
        lla = ned_lla(*ayar.ortam.orijin, {r.k[0], r.k[1], r.k[2]});
        lla[2] -= ayar.ortam.jeoit_yuksekligi;
        return std::isfinite(lla[0]) && std::isfinite(lla[1]) && std::isfinite(lla[2]) &&
               std::abs(lla[0]) <= 0.5 * kPi && std::abs(lla[1]) <= kPi &&
               std::abs(lla[2] * 1000.0) <= 2147483647.0;
    }

    // Arıza zinciri sonrası, binary32'ye daraltmadan önce: bütün yük alanları sonlu ve tel
    // tipinin aralığında mı (W4-sensor sol #4; A1 §3.4 "bütün float alanlar sonlu").
    bool temsil_edilebilir(const TemelKayit& r) const
    {
        auto f32 = [](double x) { return std::isfinite(x) && std::abs(x) <= static_cast<double>(FLT_MAX); };
        for (std::size_t i = 0; i < r.n_kanal; ++i) {
            if (!f32(r.k[i])) return false;
        }
        for (double x : r.dvl_cov) {
            if (!f32(x)) return false;
        }
        if (!f32(r.dvl_fom) || !f32(r.eph) || !f32(r.epv)) {
            return false;
        }
        if (r.hdr.tur == SensorTuru::gnss && !r.gnss_cozum_yok) {
            Vek3 lla{};
            return gnss_konumu(r, lla);
        }
        return true;
    }

    Olcum sonlandir(const TemelKayit& r) const
    {
        Olcum o;
        o.hdr = r.hdr;
        auto f = [&r](std::size_t i) { return static_cast<float>(r.k[i]); };
        switch (r.hdr.tur) {
        case SensorTuru::imu:
            o.yuk = ImuOlcumu{{f(0), f(1), f(2)}, {f(3), f(4), f(5)}, f(6)};
            break;
        case SensorTuru::manyetometre:
            o.yuk = MagOlcumu{{f(0), f(1), f(2)}};
            break;
        case SensorTuru::basinc:
            o.yuk = BaroOlcumu{f(0), f(1)};
            break;
        case SensorTuru::gnss: {
            GnssOlcumu g;
            g.fix_type = r.fix_type;
            g.n_sv = r.n_sv;
            Vek3 lla{};
            if (!r.gnss_cozum_yok && gnss_konumu(r, lla)) {
                g.lat_e7 = e7(lla[0]);
                g.lon_e7 = e7(lla[1]);
                g.alt_mm = static_cast<std::int32_t>(std::llround(lla[2] * 1000.0));
                g.vel_ned = {f(3), f(4), f(5)};
                g.eph = static_cast<float>(r.eph);
                g.epv = static_cast<float>(r.epv);
            }
            o.yuk = g;
            break;
        }
        case SensorTuru::dvl: {
            DvlOlcumu d;
            d.vel = {f(0), f(1), f(2)};
            for (std::size_t i = 0; i < 6; ++i) d.cov[i] = static_cast<float>(r.dvl_cov[i]);
            d.fom = static_cast<float>(r.dvl_fom);
            d.beam_dist = {f(3), f(4), f(5), f(6)};
            d.altitude = f(7);
            d.dvl_flags = r.dvl_flags;
            d.beam_valid = r.beam_valid;
            o.yuk = d;
            break;
        }
        case SensorTuru::batarya_olcer:
            o.yuk = BataryaOlcumu{f(0), f(1), f(2)};
            break;
        case SensorTuru::sizinti:
            o.yuk = SizintiOlcumu{r.mask};
            break;
        case SensorTuru::firlatici_geri_okuma:
            o.yuk = EmniyetOlcumu{r.estop, r.tube, r.safety_rev};
            break;
        case SensorTuru::ping1d:
        case SensorTuru::ping360:
        case SensorTuru::kamera:
            break;
        }
        return o;
    }

    void kuyruga(const Olcum& o)
    {
        ++say.uretilen;
        std::size_t ayni = 0;
        for (const Olcum& q : kuyruk) {
            ayni += (q.hdr.sensor_id == o.hdr.sensor_id) ? 1u : 0u;
        }
        if (ayni >= kKuyrukSiniri) {
            const auto it = std::find_if(kuyruk.begin(), kuyruk.end(), [&](const Olcum& q) {
                return q.hdr.sensor_id == o.hdr.sensor_id;
            });
            kuyruk.erase(it);
            ++say.kuyruk_tasma;
        }
        const auto yer = std::upper_bound(kuyruk.begin(), kuyruk.end(), o,
                                          [](const Olcum& a, const Olcum& b) {
                                              return once(a.hdr, b.hdr);
                                          });
        kuyruk.insert(yer, o);
    }
};

// ================================================================ arayüz
SensorKumesi::SensorKumesi() : ic_(std::make_unique<Ic>()) {}
SensorKumesi::~SensorKumesi() = default;
SensorKumesi::SensorKumesi(SensorKumesi&&) noexcept = default;
SensorKumesi& SensorKumesi::operator=(SensorKumesi&&) noexcept = default;

KurulumSonucu SensorKumesi::kur(const SensorKumesiAyari& ayar)
{
    ic_ = std::make_unique<Ic>();
    KurulumSonucu s;
    if (ayar.adim_ns <= 0 || ayar.kontrol_periyodu_ns <= 0 ||
        ayar.kontrol_periyodu_ns % ayar.adim_ns != 0) {
        s.hata = "kontrol periyodu fizik adımının tam katı olmalı (iskelet §4)";
        return s;
    }
    if (ayar.kontrol_periyodu_ns > static_cast<std::int64_t>(UINT32_MAX) || ayar.adim_ns > kAzamiSure) {
        s.hata = "kontrol periyodu IMU window_ns'e (u32) sığmıyor (A4 §3.2)";
        return s;
    }
    if (!std::isfinite(ayar.ortam.jeoit_yuksekligi) || std::abs(ayar.ortam.jeoit_yuksekligi) > 200.0) {
        s.hata = "ortam.jeoit_yuksekligi sonlu ve |N| ≤ 200 m olmalı";
        return s;
    }
    if (ayar.sensorler.size() > 64) {
        s.hata = "arac.sensorler: en çok 64 sensör (A5 §3.6)";
        return s;
    }
    if (ayar.ic_sicaklik.etkin &&
        (!(ayar.ic_sicaklik.isil_kapasite > 0.0) || !(ayar.ic_sicaklik.isil_iletkenlik > 0.0))) {
        s.hata = "iç sıcaklık modeli: ısıl kapasite ve iletkenlik > 0 olmalı";
        return s;
    }
    for (std::size_t i = 0; i < ayar.sensorler.size(); ++i) {
        const SensorAyari& a = ayar.sensorler[i];
        if (!ad_gecerli(a.ad)) {
            s.hata = "arac.sensorler: geçersiz sensör adı '" + a.ad + "' (A5 §3.6)";
            return s;
        }
        for (std::size_t j = 0; j < i; ++j) {
            const SensorAyari& b = ayar.sensorler[j];
            if (b.ad == a.ad || b.kimlik == a.kimlik || (b.tur == a.tur && b.inst == a.inst)) {
                s.hata = "arac.sensorler." + a.ad + ": ad, kimlik ya da (tur, inst) tekil değil (A5 §3.6)";
                return s;
            }
        }
        if (!a.etkin) {
            continue;   // A5-K49: kayıt üretmez
        }
        const std::string h = sensor_denetle(a, ayar);
        if (!h.empty()) {
            s.hata = h;
            return s;
        }
        if (kapsam_disi(a.tur)) {
            s.uyarilar.push_back("arac.sensorler." + a.ad +
                                 ": sonar ve kamera bu kümenin kapsamı dışında; kayıt üretilmez");
        }
    }
    ic_->ayar = ayar;
    std::size_t halka = 4;
    for (const SensorAyari& a : ayar.sensorler) {
        if (!a.etkin || kapsam_disi(a.tur)) {
            continue;
        }
        SensorAyari n = a;
        n.montaj.yonelim = ic::normalle(a.montaj.yonelim);
        std::string hata;
        Ic::SensorDurumu d;
        d.model = ic::model_olustur(n, ayar, hata);
        if (!d.model) {
            s.hata = "arac.sensorler." + a.ad + ": " + hata;
            ic_ = std::make_unique<Ic>();
            return s;
        }
        halka = std::max(halka, d.model->halka_ihtiyaci(ayar.adim_ns));
        ic_->sensorler.push_back(std::move(d));
    }
    ic_->halka.kur(halka);
    ic_->kuruldu = true;
    s.tamam = true;
    return s;
}

void SensorKumesi::dis_durum_ver(const DisDurum& d) { ic_->dis = d; }

AdimSonucu SensorKumesi::adim(const fizik::DurumAnlik& d, std::uint64_t adim_no)
{
    Ic& c = *ic_;
    if (!c.kuruldu) {
        return AdimSonucu::kurulmadi;
    }
    if (d.adim_no != adim_no ||
        adim_no > static_cast<std::uint64_t>(kAzamiZaman / c.ayar.adim_ns) ||
        d.t_ns != static_cast<std::int64_t>(adim_no) * c.ayar.adim_ns) {
        return AdimSonucu::tutarsiz;
    }
    if (!c.ilk && adim_no != c.son_adim + 1) {
        return AdimSonucu::sira_disi;
    }
    // Gövde içi sıcaklık (05 §2.3): üstel ayrık güncelleme.
    const IcSicaklikModeli& ts = c.ayar.ic_sicaklik;
    if (c.ilk || !ts.etkin) {
        c.ic_sicaklik = c.ayar.ortam.su.sicaklik_c;
    } else {
        const double pe = ts.guc.value_or(c.ayar.guc.aviyonik_guc);
        const double t_son = c.ayar.ortam.su.sicaklik_c + pe / ts.isil_iletkenlik;
        const double h = static_cast<double>(c.ayar.adim_ns) * 1e-9;
        c.ic_sicaklik =
            t_son + (c.ic_sicaklik - t_son) * std::exp(-h * ts.isil_iletkenlik / ts.isil_kapasite);
    }
    c.halka.ekle(d, c.ic_sicaklik);

    ic::Baglam b;
    b.adim_no = adim_no;
    b.t_ns = d.t_ns;
    b.adim_ns = c.ayar.adim_ns;
    b.kontrol_periyodu_ns = c.ayar.kontrol_periyodu_ns;
    b.halka = &c.halka;
    b.ortam = &c.ayar.ortam;
    b.guc = &c.ayar.guc;
    b.dis = &c.dis;
    b.ic_sicaklik = c.ic_sicaklik;
    if (c.ilk) {
        for (auto& s : c.sensorler) {
            s.model->baslat(b);
        }
    }
    c.ilk = false;
    c.son_adim = adim_no;
    c.t_son = d.t_ns;

    for (std::size_t si = 0; si < c.sensorler.size(); ++si) {
        Ic::SensorDurumu& s = c.sensorler[si];
        c.uretilen.clear();
        s.model->adim(b, c.uretilen);
        for (TemelKayit& r : c.uretilen) {
            // 05 §4: sonlu olmayan model çıktısı karta gitmez.
            bool sonlu = true;
            for (std::size_t i = 0; i < r.n_kanal; ++i) {
                sonlu = sonlu && std::isfinite(r.k[i]);
            }
            if (!sonlu) {
                ++c.say.nan_reddi;
                for (std::size_t i = 0; i < r.n_kanal; ++i) {
                    r.k[i] = s.temel_var ? s.son_temel.k[i] : 0.0;
                }
                r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags & (0xFFFFu ^ kBayrakGecerli));
            } else {
                s.temel_var = true;
                s.son_temel = r;
            }
            // W4-K9: teslim, kaydın üretildiği fizik sınırından önce olamaz.
            r.hdr.t_teslim_ns = std::max(r.hdr.t_teslim_ns, r.hdr.t_hazir_ns);
            if (c.ariza_zinciri(si, r)) {
                // W4-sensor sol #4: arıza zinciri sonrası temsil edilemeyen değer karta gitmez.
                // 05 §4'ün sonlu olmayan çıktı kuralıyla aynı: VALID = 0, değer son geçerli temel
                // değer (yoksa 0); kayıt teslim edilir, seq boşluğu açılmaz.
                if (!c.temsil_edilebilir(r)) {
                    ++c.say.aralik_reddi;
                    const TemelKayit* t = s.temel_var ? &s.son_temel : nullptr;
                    for (std::size_t i = 0; i < r.n_kanal; ++i) r.k[i] = t ? t->k[i] : 0.0;
                    r.dvl_cov = t ? t->dvl_cov : std::array<double, 6>{};
                    r.dvl_fom = t ? t->dvl_fom : 0.0;
                    r.eph = t ? t->eph : 0.0;
                    r.epv = t ? t->epv : 0.0;
                    r.gnss_cozum_yok = t ? t->gnss_cozum_yok : true;
                    if (!c.temsil_edilebilir(r)) {
                        r.k.fill(0.0);
                        r.dvl_cov.fill(0.0);
                        r.dvl_fom = r.eph = r.epv = 0.0;
                        r.gnss_cozum_yok = true;
                    }
                    r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags & (0xFFFFu ^ kBayrakGecerli));
                }
                c.kuyruga(c.sonlandir(r));
            }
        }
    }
    return AdimSonucu::tamam;
}

std::size_t SensorKumesi::bekleyen() const { return ic_->kuyruk.size(); }

bool SensorKumesi::siradaki(std::int64_t t_kadar_ns, Olcum& cikti)
{
    auto& q = ic_->kuyruk;
    if (q.empty() || q.front().hdr.t_teslim_ns > t_kadar_ns) {
        return false;
    }
    cikti = q.front();
    q.erase(q.begin());
    return true;
}

void SensorKumesi::bosalt(std::int64_t t_kadar_ns, std::vector<Olcum>& cikti)
{
    Olcum o;
    while (siradaki(t_kadar_ns, o)) {
        cikti.push_back(o);
    }
}

ArmSonucu SensorKumesi::arm(const SensorArizasi& f)
{
    Ic& c = *ic_;
    ArmSonucu s;
    if (!c.kuruldu) {
        s.hata = "küme kurulmadı";
        return s;
    }
    std::size_t si = c.sensorler.size();
    for (std::size_t i = 0; i < c.sensorler.size(); ++i) {
        if (c.sensorler[i].model->ayar().ad == f.hedef) {
            si = i;
        }
    }
    if (si == c.sensorler.size()) {
        s.hata = "hedef etkin bir sensör değil (A5-K65)";
        return s;
    }
    const SensorTuru tur = c.sensorler[si].model->ayar().tur;
    const ArizaSiddeti& z = f.siddet;
    switch (f.tur) {
    case ArizaTuru::motor_takilmasi:
    case ArizaTuru::dvl_isin_kaybi:
    case ArizaTuru::guc_cevrimi:
        s.hata = "durumlu arıza bu dilimde desteklenmiyor (yer tutucu)";
        return s;
    case ArizaTuru::ofset:
    case ArizaTuru::olcek:
    case ArizaTuru::suruklenme_dogrusal:
    case ArizaTuru::suruklenme_ustel:
    case ArizaTuru::gurultu_artisi:
    case ArizaTuru::sicrama:
        if (!sayisal_kanalli(tur)) {
            s.hata = "sayısal arıza kanalsız sensörde uygulanamaz (A5-K65; 05 §2.7)";
            return s;
        }
        break;
    default:
        break;
    }
    bool gecerli = std::isfinite(z.ofset) && std::isfinite(z.olcek) && std::isfinite(z.oran) &&
                   std::isfinite(z.son_deger) && std::isfinite(z.genlik);
    if (f.tur == ArizaTuru::suruklenme_ustel) gecerli = gecerli && sure_ok(z.tau_ns, false);
    if (f.tur == ArizaTuru::gurultu_artisi) gecerli = gecerli && z.k >= 1.0 && std::isfinite(z.k);
    if (f.tur == ArizaTuru::sicrama) gecerli = gecerli && z.genlik >= 0.0 && z.p >= 0.0 && z.p <= 1.0;
    if (f.tur == ArizaTuru::gec_teslim) {
        // Toplamlar (t_teslim + gecikme + titreşim) int64'te taşmasın: süreler kAzamiSure ile sınırlı.
        gecerli = gecerli && sure_ok(z.gecikme_ns, true) && z.titresim_ns >= 0 &&
                  z.titresim_ns <= z.gecikme_ns;
    }
    const ZamanDeseni& zd = f.zaman_deseni;
    if (zd.tur == ZamanDeseniTuru::bernoulli) gecerli = gecerli && zd.p >= 0.0 && zd.p <= 1.0;
    if (zd.tur == ZamanDeseniTuru::gilbert_elliott) {
        gecerli = gecerli && sure_ok(zd.t_iyi_ns, false) && sure_ok(zd.t_kotu_ns, false);
    }
    if (!gecerli || f.sure_ns < 0 || f.sure_ns > kAzamiZaman / 2 || f.zaman_ns < 0 ||
        f.zaman_ns > kAzamiZaman) {
        s.hata = "arıza parametresi geçersiz (A5-K48, K66, K67, K68; 05 §2.7)";
        return s;
    }
    if (f.kanal_maskesi != 0xFFFFFFFFu) {
        s.hata = "kanal_maskesi bu sürümde yalnız 4294967295 (A5-K69)";
        return s;
    }
    if (f.kimlik.empty()) {
        s.hata = "arıza kimliği boş (A5-K60)";
        return s;
    }
    for (const auto& a : c.arizalar) {
        if (a.t.kimlik == f.kimlik) {
            s.hata = "arıza kimliği tekil değil (A5-K60)";
            return s;
        }
    }
    Ic::ArizaKaydi k;
    k.tutamac = c.sonraki_tutamac++;
    k.t = f;
    k.sensor = si;
    const std::int64_t h = c.ayar.adim_ns;
    const std::int64_t sonraki_sinir = c.ilk ? 0 : c.t_son + h;
    k.t_b = std::max(ic::ust_sinir(f.zaman_ns, h), sonraki_sinir);
    k.t_bitis = f.sure_ns > 0 ? ic::ust_sinir(k.t_b + f.sure_ns, h) : kSonsuz;
    k.akis = Akis(c.ayar.ana_tohum, "ariza/" + f.kimlik);
    k.gecis = Akis(c.ayar.ana_tohum, "ariza/" + f.kimlik + "/gecis");
    c.arizalar.push_back(std::move(k));
    s.tamam = true;
    s.tutamac = c.arizalar.back().tutamac;
    return s;
}

bool SensorKumesi::disarm(ArizaTutamaci t)
{
    Ic& c = *ic_;
    for (auto& a : c.arizalar) {
        if (a.tutamac == t && a.t_bitis > c.t_son) {
            const std::int64_t sonraki_sinir = c.ilk ? 0 : c.t_son + c.ayar.adim_ns;
            a.t_bitis = std::min(a.t_bitis, sonraki_sinir);
            return true;
        }
    }
    return false;
}

std::vector<EtkinAriza> SensorKumesi::active() const
{
    std::vector<EtkinAriza> v;
    for (const auto& a : ic_->arizalar) {
        if (a.t_bitis > ic_->t_son) {
            v.push_back(EtkinAriza{a.tutamac, a.t, a.t_b, a.t_bitis});
        }
    }
    return v;
}

bool SensorKumesi::kuruldu() const { return ic_->kuruldu; }
std::int64_t SensorKumesi::zaman_ns() const { return ic_->t_son; }
Sayaclar SensorKumesi::sayaclar() const { return ic_->say; }

}  // namespace ida::sensor
