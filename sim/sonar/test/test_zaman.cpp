// Kabul 4: zaman (ölçüm zamanı, ping aralığı, örnek numaraları, ping sırasında hareket).
#include "test_destek.hpp"

namespace ida::sonar::test {

namespace {

std::int64_t ust(std::int64_t t) { return ((t + kAdim - 1) / kAdim) * kAdim; }

// Bütün kayıtlarda nedensellik (05 §2.2; W4-K9) ve kesintisiz seq.
void nedensellik(Denetci& d, const std::vector<SonarOlcumu>& v)
{
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& h = v[i].hdr;
        DOGRULA(d, h.t_tx_ns <= h.t_meas_ns && h.t_meas_ns <= h.t_hazir_ns && h.t_hazir_ns <= h.t_teslim_ns,
                "t_tx ≤ t_meas ≤ t_hazir ≤ t_teslim");
        DOGRULA(d, h.t_hazir_ns % kAdim == 0 && h.t_tx_ns % kAdim == 0, "t_tx ve t_hazir adım sınırında");
        DOGRULA(d, h.window_ns == 0, "window_ns = 0");
        DOGRULA(d, h.seq == static_cast<std::uint32_t>(i), "seq kesintisiz");
    }
}

}  // namespace

int test_zaman_ping360()
{
    Denetci d;
    // KE varsayılanlarında (motor, işlem, iletim 0): ping aralığı T_dinleme'nin üst adım sınırı
    // (7 m: 1200 · 311 · 25 ns = 9,3300 ms → 10 ms). t_meas = gönderim anı (A1-K53).
    {
        auto a = kume_ayari({ping360(p360_yap(311))});
        a.sade_isin_sayisi = 200;
        Kosu k;
        if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 8 * 405)) return 1;
        const auto v = k.sonar(7);
        nedensellik(d, v);
        DOGRULA(d, v.size() >= 402, "en az 402 satır");
        const std::int64_t td = 1200LL * 311 * 25;
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& h = v[i].hdr;
            DOGRULA(d, h.t_meas_ns == h.t_tx_ns, "Ping360 t_meas = t_tx");
            DOGRULA(d, h.t_hazir_ns == ust(h.t_tx_ns + td) && h.t_teslim_ns == h.t_hazir_ns,
                    "teslim fiziksel bitişin üst sınırında (W4-K9)");
            DOGRULA(d, p360(v[i]).angle_grad == static_cast<std::uint16_t>(i % 400), "açı her pingte 1 gradyan; 399 → 0");
            if (i > 0) {
                const std::int64_t ara = h.t_tx_ns - v[i - 1].hdr.t_tx_ns;
                DOGRULA(d, ara == ust(td), "ping aralığı = T_dinleme'nin üst adım sınırı");
            }
        }
    }
    // Kalibre edilmiş süreler: T_ping = T_motor + T_dinleme + T_işlem + T_iletim + T_delay
    // (05 §2.4.5); T_iletim = (24 + n) · 10 / baud; sektör taraması uçlarda yön değiştirir.
    {
        auto s = ping360(p360_yap(311, 1200, 10, 20, 4));
        std::get<Ping360Yapilandirmasi>(s.yapilandirma).bekleme_ns = 5'000'000;
        auto& p = std::get<Ping360Ayar>(s.ayar);
        p.t_m0_ns = 2'000'000;
        p.t_grad_ns = 100'000;
        p.islem_suresi_ns = 3'000'000;
        p.baud = 115200;
        auto a = kume_ayari({s});
        a.sade_isin_sayisi = 200;
        Kosu k;
        if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 1600)) return 1;
        const auto v = k.sonar(7);
        nedensellik(d, v);
        const int beklenen_aci[] = {10, 14, 18, 20, 16, 12, 10, 14, 18, 20};
        const std::int64_t td = 1200LL * 311 * 25;
        const std::int64_t iletim = static_cast<std::int64_t>(std::llround((24.0 + 1200.0) * 10.0 / 115200.0 * 1e9));
        // İdeal gönderim anları birikimsiz: T_{i+1} = T_i + T_ping; gönderim en yakın adım
        // sınırında, önceki kaydın üretiminden önce değil; geciken gönderimde ideal an yeniden
        // demirlenir (sol #6 düzeltmesi).
        auto yakin = [](std::int64_t t) { return ((t + kAdim / 2) / kAdim) * kAdim; };
        std::int64_t ideal = 0;
        for (std::size_t i = 0; i < v.size() && i < 10; ++i) {
            DOGRULA(d, p360(v[i]).angle_grad == beklenen_aci[i], "sektör açısı " + std::to_string(i));
            const std::int64_t bitis = v[i].hdr.t_tx_ns + td + 3'000'000 + iletim;
            DOGRULA(d, v[i].hdr.t_hazir_ns == ust(bitis), "üretim fiziksel bitişin üst sınırında");
            if (v[i].hdr.t_tx_ns != yakin(ideal)) ideal = v[i].hdr.t_tx_ns;
            if (i + 1 < v.size() && i + 1 < 10) {
                const int fark = std::abs(beklenen_aci[i + 1] - beklenen_aci[i]);
                const std::int64_t motor = 2'000'000 + fark * 100'000;
                ideal += td + 3'000'000 + iletim + 5'000'000 + motor;
                DOGRULA(d, v[i + 1].hdr.t_tx_ns == std::max(yakin(ideal), ust(bitis)),
                        "T_ping = T_motor + T_dinleme + T_işlem + T_iletim + T_delay");
            }
        }
    }
    // Yapılandırma değişimi: süren ping tamamlanır, sonraki ping yeni başlangıç açısında ve yeni
    // örnek sayısıyla (05 §2.4.5).
    {
        auto a = kume_ayari({ping360(p360_yap(311))});
        a.sade_isin_sayisi = 200;
        Kosu k;
        k.adim_oncesi = [&d](std::uint64_t n, SonarKumesi& km) {
            if (n == 20) {
                DOGRULA(d, km.ping360_yapilandir("p360", p360_yap(800, 600, 200, 200, 2)), "yapılandırma kabul");
            }
        };
        if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 120)) return 1;
        const auto v = k.sonar(7);
        nedensellik(d, v);
        bool degisti = false;
        for (std::size_t i = 1; i < v.size(); ++i) {
            const auto& o = p360(v[i]);
            const auto& on = p360(v[i - 1]);
            if (o.n_samples == 600 && on.n_samples == 1200) {
                degisti = true;
                DOGRULA(d, o.angle_grad == 200 && o.sample_period == 800, "yeni yapılandırma");
                DOGRULA(d, v[i - 1].hdr.t_tx_ns <= 20 * kAdim, "süren ping eski yapılandırmayla tamamlandı");
                DOGRULA(d, o.data.size() == 600, "örnek sayısı");
            }
        }
        DOGRULA(d, degisti, "yapılandırma uygulandı");
        Ping360Yapilandirmasi kotu = p360_yap(311);
        kotu.n_samples = 1201;
        DOGRULA(d, !k.kume.ping360_yapilandir("p360", kotu), "aralık dışı yapılandırma reddedilir");
    }
    return d.hata;
}

int test_zaman_ping1d()
{
    Denetci d;
    // Ping aralığı max(ping_araligi_ns, T_dinleme + T_işlem) (05 §2.4.6); t_meas yansıma anı.
    {
        Ping1dYapilandirmasi w;
        w.scan_length = 6.0;
        w.otomatik = false;
        auto a = kume_ayari({ping1d(w)});
        Kosu k;
        if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 800)) return 1;
        const auto v = k.sonar(6);
        nedensellik(d, v);
        DOGRULA(d, v.size() == 20, "50 ms aralıkla 20 ping");
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& p = p1d(v[i]);
            DOGRULA(d, p.ping_number == v[i].hdr.seq, "ping_number = seq");
            DOGRULA(d, v[i].hdr.t_meas_ns == v[i].hdr.t_tx_ns + p.tx_offset_ns, "t_meas = t_tx + tx_offset");
            YAKIN(d, p.distance, 2.0 - 0.0, 0.1, "havuz tabanı 2 m altta");
            if (i > 0) DOGRULA(d, v[i].hdr.t_tx_ns - v[i - 1].hdr.t_tx_ns == 50'000'000, "aralık 50 ms");
        }
    }
    {
        Ping1dYapilandirmasi w;
        w.scan_length = 80.0;
        w.otomatik = false;
        auto s = ping1d(w);
        std::get<Ping1dAyar>(s.ayar).islem_suresi_ns = 5'000'000;
        auto a = kume_ayari({s});
        Kosu k;
        if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 800)) return 1;
        const auto v = k.sonar(6);
        nedensellik(d, v);
        const std::int64_t dinleme = static_cast<std::int64_t>(std::llround(2.0 * 80.0 / k.kume.ses_hizi() * 1e9));
        for (std::size_t i = 1; i < v.size(); ++i) {
            DOGRULA(d, v[i].hdr.t_tx_ns - v[i - 1].hdr.t_tx_ns == ust(dinleme + 5'000'000),
                    "uzun pencerede aralık T_dinleme + T_işlem");
        }
        for (const auto& o : v) {
            DOGRULA(d, o.hdr.t_hazir_ns == ust(o.hdr.t_tx_ns + dinleme + 5'000'000), "üretim dinleme bitince");
        }
    }
    return d.hata;
}

int test_zaman_hareket()
{
    Denetci d;
    // 05 §2.2 / §2.4.5 md. 1: geometri gönderim anının durumuyla hesaplanır (dur-ve-atla); kayıt
    // dinleme bitince üretilir. Araç duvara 1 m/s ile yaklaşırken k. pingin profili, aracın
    // t_tx(k) konumunda durduğu koşunun aynı pingiyle bit düzeyinde aynıdır; t_teslim konumuyla
    // değildir.
    auto sahne = havuz_sahnesi();
    const Yorunge hareket = [](std::uint64_t n) {
        const double t = static_cast<double>(n) * 1.25e-3;
        return durum(n, {10.0 + 1.0 * t, 0.0, 1.0}, {1.0, 0.0, 0.0, 0.0}, {1.0, 0.0, 0.0});
    };
    auto a = kume_ayari({ping360(p360_yap(2222, 1200, 0, 0, 0 + 1))});   // 50 m: T_dinleme 66,7 ms
    Kosu k;
    if (!k.kur(a, sahne) || !k.kos(hareket, 0, 600)) return 1;
    const auto v = k.sonar(7);
    DOGRULA(d, v.size() >= 6, "pingler");
    for (std::size_t i : {std::size_t{2}, std::size_t{5}}) {
        const double t_tx = static_cast<double>(v[i].hdr.t_tx_ns) * 1e-9;
        const double t_tes = static_cast<double>(v[i].hdr.t_teslim_ns) * 1e-9;
        Kosu dur;
        if (!dur.kur(a, sahne) || !dur.kos(durgun({10.0 + t_tx, 0.0, 1.0}), 0, 600)) return 1;
        Kosu gec;
        if (!gec.kur(a, sahne) || !gec.kos(durgun({10.0 + t_tes, 0.0, 1.0}), 0, 600)) return 1;
        const auto vd = dur.sonar(7);
        const auto vg = gec.sonar(7);
        DOGRULA(d, vd[i].hdr.t_tx_ns == v[i].hdr.t_tx_ns, "aynı gönderim anı");
        DOGRULA(d, p360(vd[i]).tani.ham == p360(v[i]).tani.ham, "profil gönderim anının konumundan");
        DOGRULA(d, p360(vg[i]).tani.ham != p360(v[i]).tani.ham, "teslim anının konumundan değil");
        // Kuzey duvarı (x = 25 m) tepe menzili gönderim anındaki uzaklık; tolerans 2 kutu (düşey
        // açıklığın uzak menzile kuyruğu çekirdekle evrişince tepe bir kutu kayar).
        const std::size_t kk = en_buyuk(p360(v[i]).tani.ham, 100);
        YAKIN(d, kutu_menzili(k.kume.ses_hizi(), 2222, kk), 25.0 - (10.0 + t_tx), 0.09, "duvar menzili t_tx'te");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
