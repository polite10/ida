// W4-bagla-sonar: köprünün sonar üyeleri (A1 §3.4, A1-K34, K36, K37, K49–K54, K112, K113).
//
//   sonar_parcalama       A1-K49: DATA 0 … n_frag − 1 sonra HEAD; yeniden birleşim bayt bayt eşit;
//                         her partide bayt bütçesi, en çok bir HEAD, n_members; sonar kredisi
//   sonar_seyreltme       A1-K37: bekleyen en çok bir satır; başlamış satır tamamlanır; teslim gecikmesi
//   sonar_kritik_oncelik  kritik üyeler (TF, IMU, emniyet, sensörler) sonarla aynen; komutlar gecikmez
//   sonar_oturum_sonu     oturum biterse yarıdaki satır atılır, yeni oturumda yeni satır baştan
//   sonar_eksik_parca     gerçek ida_link üst katmanı: düşürülen DATA'nın satırı teslim edilmez,
//                         kart `ping360_incomplete` sayar (A1-K50); öteki satırlar bayt bayt eşit
//   ping1d_uye            Ping1D kritik üye: alanlar dönüşümsüz, A1-K113 sırası, t_hazır penceresi
#include "test_destek.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

// Sim → kart akışını kart rolünde çözüp partilere ayıran kayıtçı.
struct Cer {
    std::uint32_t msgid = 0;
    std::size_t boy = 0;   // teldeki çerçeve boyu
    ida_link_cerceve_t c{};
};
struct Kayitci {
    ida_link_alici_t a{};
    std::vector<Cer> acik;
    std::vector<std::vector<Cer>> partiler;   // her biri TF ile biter
    Kayitci() { (void)ida_link_alici_kur(&a, IDA_LINK_ROL_KART); }
    void al(const std::vector<std::uint8_t>& v)
    {
        const std::uint8_t* p = v.data();
        std::size_t n = v.size();
        while (n > 0) {
            std::size_t tuk = 0;
            Cer c;
            const ida_link_rc_t rc = ida_link_al(&a, p, n, &tuk, &c.c);
            p += tuk;
            n -= tuk;
            if (rc != IDA_LINK_CERCEVE) {
                break;
            }
            c.msgid = c.c.msgid;
            c.boy = static_cast<std::size_t>(c.c.len) + 12u;   // MAVLink 2, imzasız
            acik.push_back(c);
            if (c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
                partiler.push_back(acik);
                acik.clear();
            }
        }
    }
};

struct Duzenek {
    Kopru k;
    SahteKart sk;
    Kayitci kay;
    std::uint64_t sinir = 0;
    std::int64_t t = 0;
    std::uint32_t imu_seq = 0;
    explicit Duzenek(const KopruAyari& a = KopruAyari{}) : k(a) { (void)k.sinir_isle(0, 0); }
    void pompa()
    {
        for (int tur = 0; tur < 8; ++tur) {
            std::vector<std::uint8_t> v;
            k.giden_al(v);
            if (!v.empty()) {
                kay.al(v);
                sk.al(v);
            }
            if (sk.giden.empty() && v.empty()) {
                return;
            }
            if (!sk.giden.empty()) {
                std::vector<std::uint8_t> w;
                w.swap(sk.giden);
                k.al(w.data(), w.size(), t);
            }
        }
    }
    bool el_sikis()
    {
        k.el_sikisma_baslat(t);
        pompa();
        return k.durum() == OturumDurumu::kosuyor;
    }
    // Bir kontrol periyodu: dört sınır, `ekle` (sınır 4'te), IMU, TF.
    void periyot(const std::function<void()>& ekle = {})
    {
        for (int j = 0; j < 4; ++j) {
            ++sinir;
            t = static_cast<std::int64_t>(sinir) * kH;
            (void)k.sinir_isle(sinir, t);
            pompa();
        }
        if (ekle) {
            ekle();
        }
        k.olcum_ekle(imu_olcumu(++imu_seq, t));
        (void)k.parti_kapat(t, t);
        pompa();
    }
};

// Belirlenimci örnek dizisi; `sifir_kuyruk` sondaki o kadar örnek 0 (MAVLink kırpması sınanır).
std::vector<std::uint8_t> ornekler(std::uint16_t n, std::uint32_t tohum, std::size_t sifir_kuyruk = 0)
{
    std::vector<std::uint8_t> v(n);
    std::uint32_t x = tohum * 2654435761u + 1u;
    for (std::size_t i = 0; i < n; ++i) {
        x = x * 1664525u + 1013904223u;
        v[i] = static_cast<std::uint8_t>(1u + (x >> 24) % 255u);
    }
    for (std::size_t i = 0; i < sifir_kuyruk && i < n; ++i) {
        v[n - 1u - i] = 0u;
    }
    return v;
}

ida::sensor::OlcumBasligi p360_baslik(std::uint32_t seq, std::int64_t t)
{
    ida::sensor::OlcumBasligi h;
    h.tur = ida::sensor::SensorTuru::ping360;
    h.sensor_id = 7;
    h.inst = 0;
    h.seq = seq;
    h.t_meas_ns = t;
    h.t_hazir_ns = t;
    h.t_teslim_ns = t;
    h.flags = ida::sensor::kBayrakGecerli;
    return h;
}

Ping360Verisi p360_veri(const std::vector<std::uint8_t>& v, std::uint16_t aci)
{
    Ping360Verisi p;
    p.angle_grad = aci;
    p.n_samples = static_cast<std::uint16_t>(v.size());
    p.sample_period = 2222;
    p.tx_duration_us = 222;
    p.tx_freq_khz = 750;
    p.gain = 1;
    p.data = v.data();
    return p;
}

struct Satir {
    mavlink_ida_ping360_head_t bas{};
    std::vector<int> parcalar;                 // geliş sırasıyla frag_idx
    std::array<std::uint8_t, 1200> veri{};
    std::size_t ilk_parti = 0, bas_partisi = 0;
};

// Kayıtçının partilerinden satırları kurar; sıra ve parti denetimi `d`'ye.
std::map<std::uint32_t, Satir> satirlar(Denetci& d, const Kayitci& kay)
{
    std::map<std::uint32_t, Satir> m;
    std::optional<std::uint32_t> suren;   // parçaları gelen, HEAD'i gelmemiş satır
    for (std::size_t p = 0; p < kay.partiler.size(); ++p) {
        for (const Cer& c : kay.partiler[p]) {
            if (c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA) {
                mavlink_ida_ping360_data_t x{};
                (void)ida_link_coz(&c.c, c.msgid, &x, sizeof x);
                DOGRULA(d, !suren || *suren == x.seq, "satırlar iç içe geçmez (seq " + s(x.seq) + ")");
                Satir& st = m[x.seq];
                if (st.parcalar.empty()) {
                    st.ilk_parti = p;
                }
                suren = x.seq;
                st.parcalar.push_back(x.frag_idx);
                for (std::size_t j = 0; j < 240u; ++j) {
                    st.veri[x.frag_idx * 240u + j] = x.data[j];
                }
            } else if (c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD) {
                mavlink_ida_ping360_head_t h{};
                (void)ida_link_coz(&c.c, c.msgid, &h, sizeof h);
                DOGRULA(d, suren && *suren == h.seq, "HEAD satırın son çerçevesi (seq " + s(h.seq) + ")");
                Satir& st = m[h.seq];
                st.bas = h;
                st.bas_partisi = p;
                suren.reset();
            }
        }
    }
    return m;
}

// Her partinin bayt, üye ve HEAD denetimi; sonar kredisinin benzetimi (A1-K112).
void parti_denetimi(Denetci& d, const Kayitci& kay, std::size_t bas = 0)
{
    std::int64_t kredi = 600;
    std::size_t en_buyuk = 0;
    for (std::size_t p = bas; p < kay.partiler.size(); ++p) {
        const auto& v = kay.partiler[p];
        std::size_t bayt = 0, sonar = 0, uye = 0, head = 0;
        for (const Cer& c : v) {
            bayt += c.boy;
            if (c.msgid != MAVLINK_MSG_ID_IDA_TIME_FRAME) {
                ++uye;
            }
            if (c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA || c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD) {
                sonar += c.boy;
            }
            head += c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD ? 1u : 0u;
        }
        mavlink_ida_time_frame_t tf{};
        (void)ida_link_coz(&v.back().c, MAVLINK_MSG_ID_IDA_TIME_FRAME, &tf, sizeof tf);
        DOGRULA(d, bayt <= 1200u, "parti " + s(p) + " baytı ≤ B_pencere: " + s(bayt));
        DOGRULA(d, head <= 1u, "partide en çok bir HEAD (A1-K34)");
        DOGRULA(d, tf.n_members == uye, "n_members = üye sayısı (A1-K30): " + s(tf.n_members) + " / " + s(uye));
        if (p > bas) {
            kredi = std::min<std::int64_t>(kredi + 260, 600);
        }
        kredi -= static_cast<std::int64_t>(sonar);
        DOGRULA(d, kredi >= 0, "sonar kredisi aşılmadı (A1-K112), parti " + s(p));
        en_buyuk = std::max(en_buyuk, bayt);
    }
    std::printf("  en büyük parti %zu B\n", en_buyuk);
}

}  // namespace

int test_sonar_parcalama()
{
    Denetci d;
    Duzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.periyot();
    const std::size_t bas = z.kay.partiler.size();
    struct Giden {
        std::uint32_t seq;
        std::vector<std::uint8_t> v;
    };
    std::vector<Giden> giden;
    const std::vector<std::pair<std::uint16_t, std::size_t>> vakalar = {
        {200, 0}, {239, 0}, {240, 0}, {241, 0}, {600, 0}, {1199, 0}, {1200, 0},
        {1200, 100}, {480, 239}, {1200, 1200}, {600, 361}};
    std::uint32_t seq = 100;
    for (std::size_t i = 0; i < vakalar.size(); ++i) {
        const auto [n, sifir] = vakalar[i];
        Giden g{++seq, ornekler(n, seq, sifir)};
        giden.push_back(g);
        z.periyot([&] { z.k.ping360_ekle(p360_baslik(g.seq, z.t), p360_veri(giden.back().v, static_cast<std::uint16_t>(i * 37u))); });
        for (int j = 0; j < 8; ++j) {
            z.periyot();
        }
    }
    const auto m = satirlar(d, z.kay);
    DOGRULA(d, m.size() == giden.size(), "bütün satırlar gitti: " + s(m.size()));
    for (const Giden& g : giden) {
        const auto it = m.find(g.seq);
        if (it == m.end()) {
            DOGRULA(d, false, "satır yok: " + s(g.seq));
            continue;
        }
        const Satir& st = it->second;
        const std::size_t n_frag = (g.v.size() + 239u) / 240u;
        DOGRULA(d, st.bas.n_samples == g.v.size() && st.bas.n_frag == n_frag, "HEAD n_samples ve n_frag (seq " + s(g.seq) + ")");
        bool sira = st.parcalar.size() == n_frag;
        for (std::size_t j = 0; j < st.parcalar.size(); ++j) {
            sira = sira && st.parcalar[j] == static_cast<int>(j);
        }
        DOGRULA(d, sira, "parçalar 0 … n_frag − 1 sırasıyla (seq " + s(g.seq) + ")");
        bool esit = true;
        for (std::size_t j = 0; j < 1200u; ++j) {
            esit = esit && st.veri[j] == (j < g.v.size() ? g.v[j] : 0u);
        }
        DOGRULA(d, esit, "birleşim modelin dizisine bayt bayt eşit, kuyruk 0 (seq " + s(g.seq) + ", n " + s(g.v.size()) + ")");
        DOGRULA(d, st.bas_partisi - st.ilk_parti <= 6u, "satır en çok 6 pencerede (E12): " + s(st.bas_partisi - st.ilk_parti));
    }
    parti_denetimi(d, z.kay, bas);
    const Sayaclar& c = z.k.sayaclar();
    DOGRULA(d, c.ping360_satir == giden.size() && c.ping360_seyreltildi == 0u && c.pencere_tasma == 0u,
            "sayaçlar: satır " + s(c.ping360_satir) + ", seyreltme " + s(c.ping360_seyreltildi) + ", taşma " + s(c.pencere_tasma));
    DOGRULA(d, c.parti_bayt_en_buyuk <= 1200u, "parti_bayt_en_buyuk ≤ 1200: " + s(c.parti_bayt_en_buyuk));
    return d.hata;
}

int test_sonar_seyreltme()
{
    Denetci d;
    Duzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.periyot();
    const std::size_t bas = z.kay.partiler.size();
    std::uint32_t seq = 0;
    std::vector<std::vector<std::uint8_t>> v;
    v.reserve(64);
    std::size_t en_cok_kuyruk = 0;
    for (int p = 0; p < 40; ++p) {
        v.push_back(ornekler(1200, static_cast<std::uint32_t>(p)));
        z.periyot([&] { z.k.ping360_ekle(p360_baslik(++seq, z.t), p360_veri(v.back(), static_cast<std::uint16_t>(p))); });
        en_cok_kuyruk = std::max(en_cok_kuyruk, z.k.ping360_kuyruk());
    }
    for (int p = 0; p < 12; ++p) {
        z.periyot();
    }
    const auto m = satirlar(d, z.kay);
    const Sayaclar& c = z.k.sayaclar();
    DOGRULA(d, en_cok_kuyruk <= 2u, "kuyrukta en çok bir başlamış ve bir bekleyen satır (A1-K37)");
    DOGRULA(d, c.ping360_seyreltildi > 0u, "pay hızını aşan satırlar seyreltildi: " + s(c.ping360_seyreltildi));
    DOGRULA(d, c.ping360_satir + c.ping360_seyreltildi == 40u,
            "her satır ya teslim ya seyreltme (" + s(c.ping360_satir) + " + " + s(c.ping360_seyreltildi) + ")");
    DOGRULA(d, m.size() == c.ping360_satir, "kart tarafında görülen satır = gönderilen");
    for (const auto& [sq, st] : m) {
        bool esit = st.parcalar.size() == 5u;
        for (std::size_t j = 0; j < 1200u; ++j) {
            esit = esit && st.veri[j] == v[sq - 1u][j];
        }
        DOGRULA(d, esit, "başlamış satır tamamlandı ve eşit (seq " + s(sq) + ")");
    }
    // A1 3.10: başlamış 1 200 örnekli satır en geç 6 pencerede; t_hazır'dan teslime ≤ 13 pencere.
    DOGRULA(d, c.ping360_gecikme_en_buyuk <= 13u, "teslim gecikmesi ≤ 13 periyot: " + s(c.ping360_gecikme_en_buyuk));
    parti_denetimi(d, z.kay, bas);
    std::printf("  satır %llu, seyreltme %llu, en büyük gecikme %llu periyot\n",
                static_cast<unsigned long long>(c.ping360_satir), static_cast<unsigned long long>(c.ping360_seyreltildi),
                static_cast<unsigned long long>(c.ping360_gecikme_en_buyuk));
    return d.hata;
}

namespace {

ida::sensor::Olcum mag_olcumu(std::uint32_t seq, std::int64_t t)
{
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::manyetometre;
    o.hdr.seq = seq;
    o.hdr.t_meas_ns = t;
    o.hdr.t_hazir_ns = t;
    o.hdr.t_teslim_ns = t;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    o.yuk = ida::sensor::MagOlcumu{{1e-5F, 2e-6F, 4e-5F}};
    return o;
}

ida::sensor::Olcum emniyet_olcumu(std::uint32_t seq, std::int64_t t)
{
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::firlatici_geri_okuma;
    o.hdr.seq = seq;
    o.hdr.t_meas_ns = t;
    o.hdr.t_hazir_ns = t;
    o.hdr.t_teslim_ns = t;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    ida::sensor::EmniyetOlcumu e;
    e.tube = {ida::sensor::kTupDolu, ida::sensor::kTupDolu};
    e.safety_rev = 1;
    o.yuk = e;
    return o;
}

ida::sensor::OlcumBasligi p1d_baslik(std::uint32_t seq, std::int64_t t_meas, std::int64_t t_teslim)
{
    ida::sensor::OlcumBasligi h;
    h.tur = ida::sensor::SensorTuru::ping1d;
    h.sensor_id = 6;
    h.seq = seq;
    h.t_meas_ns = t_meas;
    h.t_hazir_ns = t_teslim;
    h.t_teslim_ns = t_teslim;
    h.flags = ida::sensor::kBayrakGecerli;
    return h;
}

// Kritik üyeler ve komutlar (sonar dışı her çerçeve) yük olarak; MAVLink başlık seq'i hariç.
std::vector<std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>> sonarsiz(const Kayitci& kay)
{
    std::vector<std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>> r;
    for (const auto& p : kay.partiler) {
        auto& x = r.emplace_back();
        for (const Cer& c : p) {
            if (c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA || c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD) {
                continue;
            }
            std::vector<std::uint8_t> y(c.c.yuk, c.c.yuk + c.c.len);
            if (c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
                y.clear();   // n_members sonarla değişir; TF'nin varlığı ve sırası karşılaştırılır
            }
            x.emplace_back(c.msgid, y);
        }
    }
    return r;
}

}  // namespace

int test_sonar_kritik_oncelik()
{
    Denetci d;
    auto kos = [&](bool sonar, Kayitci& kay, Sayaclar& say) {
        Duzenek z;
        DOGRULA(d, z.el_sikis(), "el sıkışma");
        std::uint32_t mag = 0, emn = 0, p1d = 0, p360 = 0;
        std::vector<std::vector<std::uint8_t>> v;
        v.reserve(400);
        for (int p = 0; p < 400; ++p) {
            z.periyot([&] {
                z.k.olcum_ekle(mag_olcumu(++mag, z.t - kMs));
                z.k.olcum_ekle(mag_olcumu(++mag, z.t));
                if (p % 20 == 0) {
                    z.k.olcum_ekle(emniyet_olcumu(++emn, z.t));
                }
                if (p % 10 == 0) {
                    Ping1dVerisi q;
                    q.distance = 2.5F;
                    q.scan_length = 4.0F;
                    q.confidence = 90;
                    q.tx_offset_ns = 1'700'000u;
                    z.k.ping1d_ekle(p1d_baslik(++p1d, z.t - 2 * kMs, z.t), q);
                }
                // Toplu komutlar: partide 4 (A1-K34) ve kredi sınırında.
                for (int j = 0; j < 2; ++j) {
                    (void)z.k.komut_basit(static_cast<std::uint8_t>(IDAW_CMD_TYPE_HEARTBEAT), 0, z.t - kP);
                }
                if (sonar) {
                    v.push_back(ornekler(1200, static_cast<std::uint32_t>(p)));
                    z.k.ping360_ekle(p360_baslik(++p360, z.t), p360_veri(v.back(), static_cast<std::uint16_t>(p % 400)));
                }
            });
        }
        kay = z.kay;
        say = z.k.sayaclar();
    };
    Kayitci a, b;
    Sayaclar sa, sb;
    kos(false, a, sa);
    kos(true, b, sb);
    const auto xa = sonarsiz(a);
    const auto xb = sonarsiz(b);
    DOGRULA(d, xa.size() == xb.size(), "parti sayısı aynı");
    std::size_t fark = 0;
    for (std::size_t p = 0; p < std::min(xa.size(), xb.size()); ++p) {
        fark += xa[p] == xb[p] ? 0u : 1u;
    }
    DOGRULA(d, fark == 0u, "sonar dışı bütün üyeler (TF, IMU, emniyet, sensörler, Ping1D, komutlar) her partide aynı: farklı parti " + s(fark));
    DOGRULA(d, sb.kritik_erteleme == 0u && sa.kritik_erteleme == 0u, "kritik üye kaymadı");
    DOGRULA(d, sb.ping360_cerceve > 0u && sb.ping360_satir > 50u, "sonar yükü vardı: satır " + s(sb.ping360_satir));
    DOGRULA(d, sb.komut_ertelendi == sa.komut_ertelendi, "komut ertelemesi sonardan bağımsız");
    parti_denetimi(d, b, 1);
    std::size_t imu_yok = 0;
    for (std::size_t p = 1; p < b.partiler.size(); ++p) {
        bool imu = false;
        for (const Cer& c : b.partiler[p]) {
            imu = imu || c.msgid == MAVLINK_MSG_ID_IDA_IMU;
        }
        imu_yok += imu ? 0u : 1u;
    }
    DOGRULA(d, imu_yok == 0u, "her partide IMU (kontrol üyesi)");
    return d.hata;
}

int test_sonar_oturum_sonu()
{
    Denetci d;
    Duzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.periyot();
    const auto v1 = ornekler(1200, 1);
    z.periyot([&] { z.k.ping360_ekle(p360_baslik(1, z.t), p360_veri(v1, 10)); });
    z.periyot();
    DOGRULA(d, z.k.ping360_kuyruk() == 1u, "satır yarıda");
    z.k.akis_kapandi(z.t);   // taşıma koptu (A1-K23, K99)
    DOGRULA(d, z.k.sayaclar().ping360_oturum_atilan == 1u && z.k.ping360_kuyruk() == 0u, "yarıdaki satır atıldı");
    // Oturum yokken gelen satır da atılır.
    z.k.ping360_ekle(p360_baslik(2, z.t), p360_veri(v1, 11));
    DOGRULA(d, z.k.sayaclar().ping360_oturum_atilan == 2u, "oturumsuz satır atıldı");
    z.kay.acik.clear();   // eski oturumun son penceresi (kart oturum sonunda siler; A1-K22)
    DOGRULA(d, z.el_sikis(), "yeni oturum");
    const std::size_t bas = z.kay.partiler.size();
    const auto v3 = ornekler(600, 3);
    z.periyot([&] { z.k.ping360_ekle(p360_baslik(3, z.t), p360_veri(v3, 12)); });
    for (int j = 0; j < 5; ++j) {
        z.periyot();
    }
    std::vector<std::pair<std::uint32_t, int>> yeni;
    for (std::size_t p = bas; p < z.kay.partiler.size(); ++p) {
        for (const Cer& c : z.kay.partiler[p]) {
            if (c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA) {
                mavlink_ida_ping360_data_t x{};
                (void)ida_link_coz(&c.c, c.msgid, &x, sizeof x);
                yeni.emplace_back(x.seq, x.frag_idx);
            }
        }
    }
    DOGRULA(d, !yeni.empty() && yeni.front().first == 3u && yeni.front().second == 0,
            "yeni oturumda yalnız yeni satır, parça 0'dan");
    DOGRULA(d, z.k.sayaclar().ping360_satir == 1u, "yeni satır tamamlandı");
    return d.hata;
}

int test_ping1d_uye()
{
    Denetci d;
    Duzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.periyot();
    const std::size_t bas = z.kay.partiler.size();
    Ping1dVerisi q;
    q.distance = 2.75F;
    q.scan_start = 0.25F;
    q.scan_length = 4.125F;
    q.ping_number = 77;
    q.tx_offset_ns = 3'710'000u;
    q.confidence = 87;
    // t_teslim bu pencerenin içinde (t_{n−1} < t_hazır ≤ t_n): bu partide; MAG ve BATT'la sırası.
    z.periyot([&] {
        auto b = mag_olcumu(1, z.t - kMs);
        z.k.olcum_ekle(b);
        z.k.ping1d_ekle(p1d_baslik(5, z.t - 4 * kMs, z.t - kMs), q);
    });
    bool bulundu = false;
    std::vector<std::uint32_t> sira;
    for (const Cer& c : z.kay.partiler[bas]) {
        sira.push_back(c.msgid);
        if (c.msgid == MAVLINK_MSG_ID_IDA_PING1D) {
            mavlink_ida_ping1d_t x{};
            (void)ida_link_coz(&c.c, c.msgid, &x, sizeof x);
            bulundu = x.distance == q.distance && x.scan_start == q.scan_start && x.scan_length == q.scan_length &&
                      x.ping_number == 77u && x.tx_offset_ns == q.tx_offset_ns && x.confidence == 87u && x.seq == 5u &&
                      x.t_meas_ns == z.t - 4 * kMs && x.flags == ida::sensor::kBayrakGecerli;
        }
    }
    DOGRULA(d, bulundu, "Ping1D alanları dönüşümsüz, t_hazır'ın partisinde (A1-K35, K54)");
    const auto im = std::find(sira.begin(), sira.end(), MAVLINK_MSG_ID_IDA_MAG);
    const auto ip = std::find(sira.begin(), sira.end(), MAVLINK_MSG_ID_IDA_PING1D);
    DOGRULA(d, im != sira.end() && ip != sira.end() && im < ip, "A1-K113 sırası: MAG önce, PING1D sonra");
    DOGRULA(d, z.k.sayaclar().ping1d_uye == 1u, "sayaç");
    return d.hata;
}

// Gerçek ida_link üst katmanı ve çekirdekli karşı taraf (SIL): bir DATA düşürülür.
int test_sonar_eksik_parca()
{
    Denetci d;
    std::map<std::uint32_t, std::vector<std::uint8_t>> alinan;
    KarsiAyari kk;
    kk.cagri_kancasi = [&](int, const ida_in_t& in) {
        if (in.ping360 != nullptr) {
            alinan[in.ping360->hdr.seq] = std::vector<std::uint8_t>(in.ping360->data, in.ping360->data + in.ping360->n_samples);
        }
    };
    IcDuzenek z({}, kk);
    // Pompa: sim → karşı taraf baytları çerçevelere ayrılır; seq 2'nin parça 1'i düşürülür.
    ida_link_alici_t ayir{};
    (void)ida_link_alici_kur(&ayir, IDA_LINK_ROL_KART);
    ida_link_alici_t sim_alici{};
    (void)ida_link_alici_kur(&sim_alici, IDA_LINK_ROL_SIM);
    std::uint32_t eksik_sayaci = 0;
    std::size_t dusen = 0;
    auto pompa = [&] {
        for (int tur = 0; tur < 16; ++tur) {
            std::vector<std::uint8_t> v;
            z.k->giden_al(v);
            std::vector<std::uint8_t> gecen;
            const std::uint8_t* p = v.data();
            std::size_t n = v.size();
            while (n > 0) {
                std::size_t tuk = 0;
                ida_link_cerceve_t c;
                const ida_link_rc_t rc = ida_link_al(&ayir, p, n, &tuk, &c);
                bool at = false;
                if (rc == IDA_LINK_CERCEVE && c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA) {
                    mavlink_ida_ping360_data_t x{};
                    (void)ida_link_coz(&c, c.msgid, &x, sizeof x);
                    at = x.seq == 2u && x.frag_idx == 1u;
                    dusen += at ? 1u : 0u;
                }
                if (!at) {
                    gecen.insert(gecen.end(), p, p + tuk);
                }
                p += tuk;
                n -= tuk;
                if (rc != IDA_LINK_CERCEVE) {
                    break;
                }
            }
            if (!gecen.empty()) {
                (void)z.sim_ucu->yaz(gecen.data(), gecen.size());
            }
            (void)z.karsi->pompala();
            std::uint8_t b[8192];
            std::size_t k = 0;
            bool geldi = false;
            while (z.sim_ucu->oku(b, sizeof b, 0, k) == OkumaSonucu::tamam) {
                // Kartın IDA_LINK_STATS'ındaki ping360_incomplete (A1 Tablo 3.11b #21).
                const std::uint8_t* q = b;
                std::size_t m = k;
                while (m > 0) {
                    std::size_t tk = 0;
                    ida_link_cerceve_t c;
                    const ida_link_rc_t rc = ida_link_al(&sim_alici, q, m, &tk, &c);
                    q += tk;
                    m -= tk;
                    if (rc != IDA_LINK_CERCEVE) {
                        break;
                    }
                    if (c.msgid == MAVLINK_MSG_ID_IDA_LINK_STATS) {
                        mavlink_ida_link_stats_t ls{};
                        (void)ida_link_coz(&c, c.msgid, &ls, sizeof ls);
                        eksik_sayaci = ls.cnt[21];
                    }
                }
                z.k->al(b, k, z.t);
                geldi = true;
            }
            if (!geldi && !z.k->giden_var()) {
                return;
            }
        }
    };
    z.k->el_sikisma_baslat(z.t);
    pompa();
    DOGRULA(d, z.k->durum() == OturumDurumu::kosuyor, "el sıkışma");
    std::vector<std::vector<std::uint8_t>> v;
    v.reserve(8);
    std::uint32_t seq = 0;
    for (int p = 0; p < 400; ++p) {
        for (int j = 0; j < 4; ++j) {
            ++z.sinir;
            z.t = static_cast<std::int64_t>(z.sinir) * kH;
            (void)z.k->sinir_isle(z.sinir, z.t);
        }
        if (p % 20 == 5 && seq < 4u) {
            v.push_back(ornekler(seq == 2u ? 600 : 1200, seq + 50u, seq == 3u ? 300u : 0u));
            const std::uint32_t sq = ++seq;
            z.k->ping360_ekle(p360_baslik(sq, z.t), p360_veri(v.back(), static_cast<std::uint16_t>(sq * 10u)));
        }
        z.k->olcum_ekle(imu_olcumu(++z.imu_seq, z.t));
        (void)z.k->parti_kapat(z.t, z.t);
        pompa();
    }
    DOGRULA(d, dusen == 1u, "bir DATA düşürüldü");
    DOGRULA(d, alinan.count(2) == 0u, "eksik satır çekirdeğe verilmedi (A1-K50)");
    for (std::uint32_t sq : {1u, 3u, 4u}) {
        const auto it = alinan.find(sq);
        DOGRULA(d, it != alinan.end() && it->second == v[sq - 1u], "satır " + s(sq) + " çekirdeğe bayt bayt eşit ulaştı");
    }
    DOGRULA(d, eksik_sayaci == 1u, "kart ping360_incomplete sayar (IDA_LINK_STATS cnt[21]): " + s(eksik_sayaci));
    return d.hata;
}
