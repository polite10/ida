// W4-bagla-sonar bütünleme testleri: sonar modelinin `ida_sim`'e bağlanması (görev belgesi
// docs/gorevler/W4-bagla-sonar.md, kabul 1–6). Senaryolar depodaki scenarios/z1.toml ve
// scenarios/sonar-havuz.toml'dan bellekte türetilir (BellekKaynagi).
//
//   sonar_kapali_esitlik   kabul 1: sonar tanımı olmayan (ya da --sonar verilmeyen) koşu bu iş
//                          öncesiyle aynı yoldan: CSV ve A1 bayt akışı bayt bayt aynı
//   sonar_belirlenim       kabul 2: aynı senaryo ve tohumla iki koşu bayt bayt aynı; A3 açık/kapalı
//                          aynı; öteki sensörlerin kayıtları ve kontrol çıktısı sonardan bağımsız
//   sonar_ping1d           kabul 3: kartın aldığı menzil irtifayla (sonar eksenine göre) tutarlı;
//                          t_meas ve teslim partisi; kayıp ve donma arızaları
//   sonar_ping360          kabul 4: kartın (gerçek ida_link + çekirdek) birleştirdiği satır modelin
//                          dizisine bayt bayt eşit; her partide bayt ve üye bütçesi, en çok bir
//                          HEAD, IMU ve emniyet örneği gecikmez; `--ping360-ornek 600` ile de
//   sonar_red              kapsam 6: desteklenmeyen sonar arızası, tam kademe, yerleşik senaryo
//   sonar_a3               kabul 5: sonar kanalları yayınlanır, dünya tarafındaki A3 hizmetinde
//                          bellek ayırma 0
//   sonar_hil_olcum <n>    kabul 6 (ölçüm): HIL 1x 60 s, sözde uçbirim, Ping360 n örnek
#include "a3_istemci.hpp"
#include "ida/port.h"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "ida_link/tel.h"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24

#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
extern "C" {
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
}
#include "mavlink/ida/mavlink.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

void a3_ayirma_sifirla();             // test_a3.cpp (operator new kancası)
std::uint64_t a3_ayirma_sayisi();

namespace {

using namespace ida;

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

std::string s(std::uint64_t v) { return std::to_string(v); }

std::string dosya(const char* yol)
{
    senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    return d.oku(yol).value_or("");
}

void degistir(std::string& m, const std::string& a, const std::string& b)
{
    const std::size_t i = m.find(a);
    if (i == std::string::npos) {
        std::printf("  (sınama hatası: metinde yok: %s)\n", a.c_str());
        ++g_hata;
        return;
    }
    m.replace(i, a.size(), b);
}

std::shared_ptr<senaryo::BellekKaynagi> kaynak(const std::string& metin)
{
    senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    auto b = std::make_shared<senaryo::BellekKaynagi>();
    for (const char* y : {"scenarios/ida0.toml", "assets/ornek/manifest.json"}) {
        b->dosyalar[y] = d.oku(y).value_or("");
    }
    b->dosyalar["scenarios/t.toml"] = metin;
    return b;
}

sunucu::Ayar ayar(const std::string& metin, bool sonar)
{
    sunucu::Ayar a;
    a.gunluk = 0;
    a.senaryo_dosyasi = "scenarios/t.toml";
    a.senaryo_kaynagi = kaynak(metin);
    a.sonar_acik = sonar;
    a.csv_uret = true;
    return a;
}

// Sonar-havuz senaryosu, süresi kısaltılmış.
std::string sonar_metni(const char* sure = "30_000_000_000")
{
    std::string m = dosya("scenarios/sonar-havuz.toml");
    degistir(m, "sure_ns = 30_000_000_000", std::string("sure_ns = ") + sure);
    return m;
}

struct KartPing1d {
    ida_hdr_t hdr{};
    ida_ping1d_t p{};
    std::int64_t tf_t = 0;
};
struct KartPing360 {
    ida_hdr_t hdr{};
    std::uint16_t n = 0, angle = 0, sp = 0;
    std::vector<std::uint8_t> veri;
    std::int64_t tf_t = 0;
};

struct Gozlem {
    sunucu::Sonuc s;
    std::vector<std::uint8_t> giden, gelen;
    std::vector<sonar::SonarOlcumu> sonar;
    std::vector<sensor::Olcum> olcum;
    std::vector<sunucu::Ayar::SinirGozlemi> sinir;
    std::vector<KartPing1d> kart_p1d;
    std::vector<KartPing360> kart_p360;
    std::uint64_t kart_imu = 0, kart_cagri = 0;
};

Gozlem gozle(sunucu::Ayar a, bool kart_gozlemi = true)
{
    Gozlem g;
    a.a1_bayt_kancasi = [&g](bool giden, const std::uint8_t* p, std::size_t n) {
        auto& v = giden ? g.giden : g.gelen;
        v.insert(v.end(), p, p + n);
    };
    a.olcum_kancasi = [&g](const sensor::Olcum& o) { g.olcum.push_back(o); };
    a.sonar_kancasi = [&g](const sonar::SonarOlcumu& o) { g.sonar.push_back(o); };
    a.sinir_kancasi = [&g](const sunucu::Ayar::SinirGozlemi& x) { g.sinir.push_back(x); };
    if (kart_gozlemi) {
        a.ic_cagri_kancasi = [&g](int, const ida_in_t& in) {
            ++g.kart_cagri;
            g.kart_imu += in.imu_present ? 1u : 0u;
            for (std::uint8_t i = 0; i < in.n_samples; ++i) {
                if (in.samples[i].hdr.type == IDA_SENS_PING1D) {
                    g.kart_p1d.push_back({in.samples[i].hdr, in.samples[i].u.ping1d, in.tf.t.ns});
                }
            }
            if (in.ping360 != nullptr) {
                KartPing360 k;
                k.hdr = in.ping360->hdr;
                k.n = in.ping360->n_samples;
                k.angle = in.ping360->angle_grad;
                k.sp = in.ping360->sample_period;
                k.veri.assign(in.ping360->data, in.ping360->data + in.ping360->n_samples);
                k.tf_t = in.tf.t.ns;
                g.kart_p360.push_back(std::move(k));
            }
        };
    }
    g.s = sunucu::kos(a);
    return g;
}

// Sim → kart akışının partileri (kart rolünde çözülür).
struct Parti {
    std::size_t bayt = 0, uye = 0, head = 0, data = 0;
    bool imu = false, emniyet = false;
    std::uint8_t n_members = 0;
    std::uint8_t run_mode = 0;   // IDAW_RUN_MODE (4: hazırlık oturumu)
    std::int64_t t = 0;
};
std::vector<Parti> partiler(const std::vector<std::uint8_t>& v, std::size_t* sonar_bayt = nullptr)
{
    ida_link_alici_t a{};
    (void)ida_link_alici_kur(&a, IDA_LINK_ROL_KART);
    std::vector<Parti> r;
    Parti p;
    bool kosuyor = false;
    const std::uint8_t* q = v.data();
    std::size_t n = v.size();
    while (n > 0) {
        std::size_t tuk = 0;
        ida_link_cerceve_t c;
        const ida_link_rc_t rc = ida_link_al(&a, q, n, &tuk, &c);
        q += tuk;
        n -= tuk;
        if (rc != IDA_LINK_CERCEVE) {
            continue;
        }
        const std::size_t boy = static_cast<std::size_t>(c.len) + 12u;
        if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_ACCEPT) {
            kosuyor = true;   // yeni oturumun ilk partisi (hazırlık oturumu ayrı sayılır)
            p = Parti{};
            continue;
        }
        if (!kosuyor || c.msgid == MAVLINK_MSG_ID_IDA_SIM_HELLO || c.msgid == MAVLINK_MSG_ID_IDA_KEEPALIVE ||
            c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            continue;
        }
        p.bayt += boy;
        if (c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
            mavlink_ida_time_frame_t tf{};
            (void)ida_link_coz(&c, c.msgid, &tf, sizeof tf);
            p.n_members = tf.n_members;
            p.run_mode = tf.run_mode;
            p.t = tf.t_sim_ns;
            r.push_back(p);
            p = Parti{};
            continue;
        }
        ++p.uye;
        p.imu = p.imu || c.msgid == MAVLINK_MSG_ID_IDA_IMU;
        p.emniyet = p.emniyet || c.msgid == MAVLINK_MSG_ID_IDA_SAFETY;
        p.head += c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD ? 1u : 0u;
        p.data += c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA ? 1u : 0u;
        if (sonar_bayt != nullptr &&
            (c.msgid == MAVLINK_MSG_ID_IDA_PING360_HEAD || c.msgid == MAVLINK_MSG_ID_IDA_PING360_DATA)) {
            *sonar_bayt += boy;
        }
    }
    return r;
}

// ---------------------------------------------------------------- kabul 1

int test_kapali_esitlik()
{
    // z1.toml: araç dosyasından iki etkin sonar alır (ida0.toml); --sonar verilmezse kurulmaz.
    std::string z1 = dosya("scenarios/z1.toml");
    sunucu::Ayar a0 = ayar(z1, false);
    const Gozlem g0 = gozle(a0, false);
    // Aynı senaryo, sonarlar kapalı (sonar tanımı yok) + --sonar: küme kurulmaz.
    std::string z1k = z1;
    degistir(z1k, "[arac.sensorler.kamera0]", "[arac.sensorler.ping1d0]\netkin = false\n\n[arac.sensorler.ping360_0]\n"
                                              "etkin = false\n\n[arac.sensorler.kamera0]");
    const Gozlem g1 = gozle(ayar(z1k, true), false);
    const Gozlem g2 = gozle(ayar(z1k, false), false);
    dogrula(g0.s.tamam && g1.s.tamam && g2.s.tamam, "üç koşu tamam " + g0.s.hata + g1.s.hata + g2.s.hata);
    dogrula(!g0.s.sonar_acik && !g1.s.sonar_acik && g0.sonar.empty() && g1.sonar.empty(), "sonar kurulmadı");
    dogrula(g1.s.csv == g0.s.csv && g2.s.csv == g0.s.csv, "CSV bayt bayt aynı (" + s(g0.s.csv.size()) + " B)");
    dogrula(g1.giden == g0.giden && g2.giden == g0.giden, "A1 sim → kart bayt bayt aynı (" + s(g0.giden.size()) + " B)");
    dogrula(g1.gelen == g0.gelen && g2.gelen == g0.gelen, "A1 kart → sim bayt bayt aynı (" + s(g0.gelen.size()) + " B)");
    bool p360 = false;
    for (const Parti& p : partiler(g0.giden)) {
        p360 = p360 || p.head > 0u || p.data > 0u;
    }
    dogrula(!p360, "sonarsız akışta Ping360 çerçevesi yok");
    // Bu iş öncesiyle karşılaştırma için özetler (aynı ikili ve platformda belirlenimci; rapor):
    // scenarios/z1.toml ve yerleşik Z1, --sonar olmadan.
    auto ozet = [](const std::vector<std::uint8_t>& v) { return senaryo::sha256_hex(std::string(reinterpret_cast<const char*>(v.data()), v.size())); };
    sunucu::Ayar y;
    y.gunluk = 0;
    y.csv_uret = true;
    const Gozlem gy = gozle(y, false);
    std::printf("  ÖZET z1.toml: csv %s giden %s gelen %s\n", senaryo::sha256_hex(g0.s.csv).c_str(), ozet(g0.giden).c_str(),
                ozet(g0.gelen).c_str());
    std::printf("  ÖZET yerleşik z1: csv %s giden %s gelen %s\n", senaryo::sha256_hex(gy.s.csv).c_str(), ozet(gy.giden).c_str(),
                ozet(gy.gelen).c_str());
    return 0;
}

// ---------------------------------------------------------------- kabul 2

int test_belirlenim()
{
    const std::string m = sonar_metni("12_000_000_000");
    sunucu::Ayar a = ayar(m, true);
    const Gozlem x = gozle(a, false);
    const Gozlem y = gozle(a, false);
    sunucu::Ayar a3 = a;
    a3.a3_acik = true;
    a3.a3_port = 0;
    a3.a3_kapanis_bekleme_s = 0.1;
    const Gozlem z = gozle(a3, false);
    dogrula(x.s.tamam && y.s.tamam && z.s.tamam && x.s.sonar_acik, "koşular tamam " + x.s.hata + z.s.hata);
    dogrula(!x.sonar.empty() && x.s.sonar_ping360 > 0u && x.s.sonar_ping1d > 0u,
            "sonar kayıtları üretildi: Ping1D " + s(x.s.sonar_ping1d) + ", Ping360 " + s(x.s.sonar_ping360));
    dogrula(x.s.csv == y.s.csv && x.giden == y.giden && x.gelen == y.gelen, "aynı tohum: CSV ve A1 bayt bayt aynı");
    dogrula(z.s.csv == x.s.csv && z.giden == x.giden && z.gelen == x.gelen, "A3 açık/kapalı: CSV ve A1 bayt bayt aynı");
    bool sonar_esit = x.sonar.size() == y.sonar.size();
    for (std::size_t i = 0; sonar_esit && i < x.sonar.size(); ++i) {
        const auto& p = x.sonar[i];
        const auto& q = y.sonar[i];
        sonar_esit = p.hdr.seq == q.hdr.seq && p.hdr.t_meas_ns == q.hdr.t_meas_ns && p.hdr.flags == q.hdr.flags;
        if (const auto* u = std::get_if<sonar::Ping360Olcumu>(&p.yuk)) {
            sonar_esit = sonar_esit && u->data == std::get<sonar::Ping360Olcumu>(q.yuk).data;
        } else {
            const auto& v = std::get<sonar::Ping1dOlcumu>(p.yuk);
            const auto& w = std::get<sonar::Ping1dOlcumu>(q.yuk);
            sonar_esit = sonar_esit && v.distance == w.distance && v.confidence == w.confidence && v.profil == w.profil;
        }
    }
    dogrula(sonar_esit, "sonar kayıtları bayt bayt aynı (" + s(x.sonar.size()) + ")");
    // Sonarın eklenmesi öteki sensörlerin rastgele akışlarını değiştirmez (A5 Ek D): aynı senaryo
    // --sonar olmadan; sensör kayıtları ve kontrol çıktısı (CSV) aynı. Sonar arızaları sonarsız
    // koşuda kurulamaz (hedef sensör yok: A5-K65), iki koşuda da çıkarılır.
    std::string m2 = m;
    m2.erase(m2.find("# Sonar arızaları"));
    const Gozlem x2 = gozle(ayar(m2, true), false);
    const Gozlem k = gozle(ayar(m2, false), false);
    dogrula(x2.s.tamam && k.s.tamam && x2.s.sonar_acik && !k.s.sonar_acik, "arızasız koşular " + x2.s.hata + k.s.hata);
    bool olcum_esit = k.olcum.size() == x2.olcum.size();
    for (std::size_t i = 0; olcum_esit && i < k.olcum.size(); ++i) {
        const auto& p = k.olcum[i];
        const auto& q = x2.olcum[i];
        olcum_esit = p.hdr.sensor_id == q.hdr.sensor_id && p.hdr.seq == q.hdr.seq && p.hdr.t_meas_ns == q.hdr.t_meas_ns &&
                     p.hdr.flags == q.hdr.flags && p.yuk.index() == q.yuk.index();
        if (olcum_esit) {
            if (const auto* u = std::get_if<sensor::ImuOlcumu>(&p.yuk)) {
                const auto& v = std::get<sensor::ImuOlcumu>(q.yuk);
                olcum_esit = u->gyro == v.gyro && u->accel == v.accel;
            } else if (const auto* b = std::get_if<sensor::BaroOlcumu>(&p.yuk)) {
                olcum_esit = b->p_abs == std::get<sensor::BaroOlcumu>(q.yuk).p_abs;
            } else if (const auto* d = std::get_if<sensor::DvlOlcumu>(&p.yuk)) {
                olcum_esit = d->vel == std::get<sensor::DvlOlcumu>(q.yuk).vel;
            }
        }
    }
    dogrula(olcum_esit, "öteki sensörlerin kayıtları sonarla aynı (" + s(k.olcum.size()) + " kayıt)");
    dogrula(k.s.csv == x2.s.csv, "kontrol çıktısı (CSV) sonarla aynı: çekirdek sonar verisini bu dilimde kullanmıyor");
    return 0;
}

// ---------------------------------------------------------------- kabul 3

// R(q) · v (gövdeden NED'e).
std::array<double, 3> dondur(const std::array<double, 4>& q, const std::array<double, 3>& v)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    return {(1 - 2 * (y * y + z * z)) * v[0] + 2 * (x * y - w * z) * v[1] + 2 * (x * z + w * y) * v[2],
            2 * (x * y + w * z) * v[0] + (1 - 2 * (x * x + z * z)) * v[1] + 2 * (y * z - w * x) * v[2],
            2 * (x * z - w * y) * v[0] + 2 * (y * z + w * x) * v[1] + (1 - 2 * (x * x + y * y)) * v[2]};
}

int test_ping1d()
{
    const Gozlem g = gozle(ayar(sonar_metni(), true));
    dogrula(g.s.tamam, "koşu tamam " + g.s.hata);
    dogrula(g.s.karsi_gecerli_red == 0u, "VALID örnek reddedilmedi (çekirdek): " + s(g.s.karsi_gecerli_red));
    // Sim tarafında üretilen Ping1D kayıtları (seq → t_teslim, yük).
    std::map<std::uint32_t, const sonar::SonarOlcumu*> uretilen;
    for (const auto& o : g.sonar) {
        if (o.hdr.tur == sensor::SensorTuru::ping1d) {
            uretilen[o.hdr.seq] = &o;
        }
    }
    std::size_t bilinen = 0;
    for (const KartPing1d& k : g.kart_p1d) {
        bilinen += uretilen.count(k.hdr.seq);
    }
    // Koşunun son partisinden sonra üretilen (yolda kalan) en çok bir kayıt teslim edilmez.
    dogrula(!g.kart_p1d.empty() && bilinen == g.kart_p1d.size() && g.kart_p1d.size() + 1u >= uretilen.size(),
            "kart her Ping1D kaydını aldı (" + s(g.kart_p1d.size()) + " / " + s(uretilen.size()) + ")");
    // Montaj (ida0.toml ping1d0): konum (−0,08; 0,08; 0,14), aşağı bakar; taban 3,0 m.
    const std::array<double, 3> montaj{-0.08, 0.08, 0.14};
    std::size_t denenen = 0, tutarli = 0, sira = 0, parti = 0;
    double en_kotu = 0.0;
    std::uint32_t onceki_seq = 0;
    for (const KartPing1d& k : g.kart_p1d) {
        const auto it = uretilen.find(k.hdr.seq);
        if (it == uretilen.end()) {
            continue;
        }
        const sensor::OlcumBasligi& h = it->second->hdr;
        // Teslim: t_{n−1} < t_teslim ≤ t_n olan partide (A1-K35; W4-K9); t_meas aynen (A1-K54).
        parti += (k.tf_t - kopru::kP < h.t_teslim_ns && h.t_teslim_ns <= k.tf_t && k.hdr.t_meas.ns == h.t_meas_ns &&
                  k.hdr.t_meas.ns <= k.tf_t)
                     ? 1u
                     : 0u;
        sira += (onceki_seq == 0u || k.hdr.seq > onceki_seq) ? 1u : 0u;
        onceki_seq = k.hdr.seq;
        if ((k.hdr.flags & IDA_HF_VALID) == 0u) {
            continue;
        }
        // Gönderim anı t_tx = t_meas − tx_offset (A1-K54); o sınırın durumuyla irtifa.
        const std::int64_t t_tx = k.hdr.t_meas.ns - static_cast<std::int64_t>(k.p.tx_offset_ns);
        if (t_tx % kopru::kH != 0 || static_cast<std::size_t>(t_tx / kopru::kH) >= g.sinir.size()) {
            continue;
        }
        const auto& sg = g.sinir[static_cast<std::size_t>(t_tx / kopru::kH)];
        const auto r = dondur(sg.yonelim, montaj);
        const double irtifa = 3.0 - (sg.konum[2] + r[2]);
        const double tol = 3.0 * static_cast<double>(k.p.scan_length) / 200.0;   // sim.sonar.ping1d_duz_taban
        const double fark = std::abs(static_cast<double>(k.p.distance) - irtifa);
        ++denenen;
        tutarli += fark <= tol ? 1u : 0u;
        en_kotu = std::max(en_kotu, fark / tol);
    }
    dogrula(denenen > 400u && tutarli == denenen,
            "menzil irtifayla 3 kutu içinde (" + s(tutarli) + " / " + s(denenen) + "; en kötü fark/tolerans " +
                std::to_string(en_kotu) + ")");
    dogrula(parti == g.kart_p1d.size(), "teslim partisi t_hazır'ın partisi, t_meas aynen (" + s(parti) + ")");
    dogrula(sira == g.kart_p1d.size(), "seq sırası korunur");
    // Kayıp (10–11 s): bu aralıkta ölçülmüş kayıt karta gitmez, seq boşluğu kalır.
    std::size_t kayip_araligi = 0;
    std::uint32_t bosluk = 0;
    for (std::size_t i = 1; i < g.kart_p1d.size(); ++i) {
        const std::int64_t t = g.kart_p1d[i].hdr.t_meas.ns;
        kayip_araligi += (t >= 10'000'000'000 && t < 11'000'000'000) ? 1u : 0u;
        bosluk = std::max(bosluk, g.kart_p1d[i].hdr.seq - g.kart_p1d[i - 1].hdr.seq);
    }
    dogrula(kayip_araligi == 0u && bosluk >= 19u && g.s.sonar_kayip >= 19u,
            "kayıp: 10–11 s kaydı yok, seq boşluğu " + s(bosluk) + ", sayaç " + s(g.s.sonar_kayip));
    // Donma (15–16 s): yük son arızasız çıktıyla tekrarlanır; t_meas ve seq ilerler.
    const KartPing1d* son = nullptr;
    std::size_t donuk = 0, donuk_esit = 0;
    std::int64_t onceki_t = 0;
    for (const KartPing1d& k : g.kart_p1d) {
        const std::int64_t t = k.hdr.t_meas.ns;
        if (t < 15'000'000'000) {
            son = &k;
        } else if (t < 16'000'000'000 && son != nullptr) {
            ++donuk;
            donuk_esit += (k.p.distance == son->p.distance && k.p.confidence == son->p.confidence &&
                           k.p.scan_length == son->p.scan_length && t > onceki_t)
                              ? 1u
                              : 0u;
        }
        onceki_t = t;
    }
    dogrula(donuk >= 15u && donuk_esit == donuk, "donma: yük tekrarlanır, t_meas ilerler (" + s(donuk_esit) + " / " + s(donuk) + ")");
    std::printf("  Ping1D: %zu kayıt, irtifa farkı en kötü %.2f tolerans\n", g.kart_p1d.size(), en_kotu);
    return 0;
}

// ---------------------------------------------------------------- kabul 4

int ping360_birlesim(int ornek)
{
    sunucu::Ayar a = ayar(sonar_metni("24_000_000_000"), true);
    a.ping360_ornek = ornek;
    const Gozlem g = gozle(a);
    dogrula(g.s.tamam, "koşu tamam (" + s(static_cast<std::uint64_t>(ornek)) + " örnek) " + g.s.hata);
    std::map<std::uint32_t, const sonar::SonarOlcumu*> uretilen;
    for (const auto& o : g.sonar) {
        if (o.hdr.tur == sensor::SensorTuru::ping360) {
            uretilen[o.hdr.seq] = &o;
        }
    }
    const kopru::Sayaclar& c = g.s.sayac;
    // Koşu sonunda yolda kalan (başlamış ve bekleyen) en çok iki satır teslim edilmez.
    dogrula(!g.kart_p360.empty() && g.kart_p360.size() + c.ping360_seyreltildi + 2u >= uretilen.size() &&
                g.kart_p360.size() + c.ping360_seyreltildi <= uretilen.size(),
            "her satır karta ulaştı ya da A1-K37 ile seyreltildi (" + s(g.kart_p360.size()) + " + " +
                s(c.ping360_seyreltildi) + " / " + s(uretilen.size()) + ")");
    std::size_t esit = 0, bildirim = 0, bildirim_beklenen = 0;
    for (const KartPing360& k : g.kart_p360) {
        const auto it = uretilen.find(k.hdr.seq);
        if (it == uretilen.end()) {
            continue;
        }
        const auto& q = std::get<sonar::Ping360Olcumu>(it->second->yuk);
        esit += (k.veri == q.data && k.n == q.n_samples && k.angle == q.angle_grad && k.sp == q.sample_period &&
                 k.hdr.t_meas.ns == it->second->hdr.t_meas_ns && k.hdr.flags == it->second->hdr.flags)
                    ? 1u
                    : 0u;
        // kendini_bildiren (20–22 s): sensör arızası biti (A4 IDA_HF_*; t_meas = gönderim anı).
        if (k.hdr.t_meas.ns >= 20'000'000'000 && k.hdr.t_meas.ns < 22'000'000'000) {
            ++bildirim_beklenen;
            bildirim += (k.hdr.flags & sensor::kBayrakSensorArizasi) != 0u ? 1u : 0u;
        }
    }
    dogrula(esit == g.kart_p360.size(), "kartın birleştirdiği satır modelin dizisine bayt bayt eşit (" + s(esit) + ")");
    dogrula(bildirim_beklenen > 0u && bildirim == bildirim_beklenen, "kendini_bildiren arızası bayrakta (" + s(bildirim) + ")");
    std::size_t sonar_bayt = 0;
    const std::vector<Parti> p = partiler(g.giden, &sonar_bayt);
    std::size_t asim = 0, head = 0, imu_yok = 0, uye = 0, en_buyuk = 0, en_cok_uye = 0, emniyet = 0;
    std::int64_t en_uzun_emniyet = 0, son_emniyet = -1;
    for (std::size_t i = 1; i < p.size(); ++i) {   // ilk parti: el sıkışma sınırı
        if (p[i].run_mode == 4u) {
            continue;   // hazırlık oturumu (A1-K19): fizik yürümez, IMU yok
        }
        asim += p[i].bayt > 1200u ? 1u : 0u;
        head += p[i].head > 1u ? 1u : 0u;
        imu_yok += p[i].imu ? 0u : 1u;
        uye += p[i].uye == static_cast<std::size_t>(p[i].n_members) ? 0u : 1u;
        en_buyuk = std::max(en_buyuk, p[i].bayt);
        en_cok_uye = std::max(en_cok_uye, p[i].uye);
        if (p[i].emniyet) {
            ++emniyet;
            if (son_emniyet >= 0) {
                en_uzun_emniyet = std::max(en_uzun_emniyet, p[i].t - son_emniyet);
            }
            son_emniyet = p[i].t;
        }
    }
    dogrula(asim == 0u, "her partide bayt ≤ B_pencere (en büyük " + s(en_buyuk) + " B)");
    dogrula(head == 0u, "partide en çok bir HEAD (A1-K34)");
    dogrula(uye == 0u, "n_members = üye sayısı (A1-K30)");
    dogrula(imu_yok == 0u, "her partide IMU (kontrol üyesi gecikmez)");
    dogrula(emniyet > 0u && en_uzun_emniyet <= 100'000'000, "emniyet örneği en geç T_emniyet = 100 ms'de bir (en uzun " +
                                                                 std::to_string(1e-6 * static_cast<double>(en_uzun_emniyet)) + " ms)");
    dogrula(c.kritik_erteleme == 0u && c.pencere_tasma == 0u, "kritik erteleme ve pencere taşması 0");
    dogrula(g.s.karsi_gecerli_red == 0u, "VALID örnek reddedilmedi");
    std::printf("  ÖLÇÜM (bu Mac, SIL, %d örnek): satır %llu, seyreltme %llu, sonar baytı %.1f B/periyot, en büyük parti %zu B / "
                "%zu üye, teslim gecikmesi en çok %llu periyot\n",
                ornek == 0 ? 1200 : ornek, static_cast<unsigned long long>(c.ping360_satir),
                static_cast<unsigned long long>(c.ping360_seyreltildi),
                static_cast<double>(sonar_bayt) / static_cast<double>(std::max<std::size_t>(1, p.size())), en_buyuk,
                en_cok_uye, static_cast<unsigned long long>(c.ping360_gecikme_en_buyuk));
    return 0;
}

int test_ping360()
{
    (void)ping360_birlesim(0);
    (void)ping360_birlesim(600);
    return 0;
}

// ---------------------------------------------------------------- kapsam 6

int test_red()
{
    // Desteklenmeyen sonar arızaları: başlamadan açık hatayla (sessiz atlama yok).
    for (const char* tur : {"gurultu_artisi", "ofset", "motor_takilmasi"}) {
        std::string m = sonar_metni("1_000_000_000");
        const bool p360 = std::string(tur) == "motor_takilmasi";
        std::string ek = std::string("\n[[olaylar]]\nkimlik = \"red\"\nsinif = \"sensor\"\nzaman_ns = 500_000_000\ntur = \"") + tur +
                         "\"\nhedef = \"" + (p360 ? "ping360_0" : "ping1d0") + "\"\n";
        if (std::string(tur) == "gurultu_artisi") {
            ek += "siddet = { k = 2.0 }\n";
        } else if (std::string(tur) == "ofset") {
            ek += "siddet = { ofset = 0.5 }\n";
        }
        m += ek;
        const sunucu::Sonuc r = sunucu::kos(ayar(m, true));
        dogrula(!r.tamam && (r.hata_turu == sunucu::HataTuru::senaryo_yurutulemez ||
                             r.hata_turu == sunucu::HataTuru::senaryo_gecersiz),
                std::string("sonar arızası ") + tur + " başlamadan reddedilir: " + r.hata);
    }
    {
        std::string m = sonar_metni("1_000_000_000");
        degistir(m, "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = { isin_sayisi = 20000 }");
        const sunucu::Sonuc r = sunucu::kos(ayar(m, true));
        dogrula(!r.tamam && r.hata_turu == sunucu::HataTuru::senaryo_yurutulemez &&
                    r.hata.find("kalite.sonar = tam") != std::string::npos,
                "tam kademe yürütülemez: " + r.hata);
        m.erase(m.find("# Sonar arızaları"));   // sonarsız koşuda sonar arızasının hedefi yok (A5-K65)
        const sunucu::Sonuc r2 = sunucu::kos(ayar(m, false));
        dogrula(r2.tamam, "tam kademe --sonar olmadan değişmez (sonar kurulmaz) " + r2.hata);
    }
    {
        sunucu::Ayar a;
        a.gunluk = 0;
        a.sonar_acik = true;
        a.sure_s = 1.0;
        const sunucu::Sonuc r = sunucu::kos(a);
        dogrula(!r.tamam && r.hata_turu == sunucu::HataTuru::kullanim, "yerleşik senaryoda --sonar kullanım hatası");
        sunucu::Ayar b = ayar(sonar_metni("1_000_000_000"), false);
        b.ping360_ornek = 600;
        const sunucu::Sonuc r2 = sunucu::kos(b);
        dogrula(!r2.tamam && r2.hata_turu == sunucu::HataTuru::kullanim, "--ping360-ornek --sonar ister");
    }
    // Sonarın desteklediği arıza türleri (gövdeli) kurulur.
    {
        std::string m = sonar_metni("2_000_000_000");
        for (const char* tur : {"donma_zaman_damgasi", "yanlis_gecerlilik", "gec_teslim"}) {
            m += std::string("\n[[olaylar]]\nkimlik = \"") + tur + "\"\nsinif = \"sensor\"\nzaman_ns = 500_000_000\n"
                 "sure_ns = 500_000_000\ntur = \"" + tur + "\"\nhedef = \"ping360_0\"\n" +
                 (std::string(tur) == "gec_teslim" ? "siddet = { gecikme_ns = 20_000_000 }\n" : "");
        }
        m += "\n[[olaylar]]\nkimlik = \"kosullu\"\nsinif = \"sensor\"\nkosul = { buyukluk = \"derinlik\", "
             "karsilastirma = \"<\", deger = 1.99 }\nsure_ns = 200_000_000\ntur = \"kayip\"\nhedef = \"ping1d0\"\n";
        const sunucu::Sonuc r = sunucu::kos(ayar(m, true));
        std::size_t uyg = 0;
        for (const auto& o : r.olay_sonuclari) {
            uyg += o.sonuc == "uygulandi" ? 1u : 0u;
        }
        dogrula(r.tamam && uyg >= 4u, "desteklenen sonar arızaları (zamanlı ve koşullu) uygulanır: " + s(uyg) + " " + r.hata);
    }
    return 0;
}

// ---------------------------------------------------------------- kabul 5

int test_a3()
{
    const std::string m = sonar_metni("10_000_000_000");
    sunucu::Ayar a = ayar(m, true);
    a.a3_acik = true;
    a.a3_port = 0;
    a.a3_kapanis_bekleme_s = 0.2;
    std::atomic<bool> dur{false};
    std::atomic<bool> hazir{false};
    std::unique_ptr<ida::test::a3::Istemci> ist;
    std::thread is;
    bool baglandi = false;
    a.a3_hazir = [&](std::uint16_t port, const std::string& jeton) {
        is = std::thread([&, port, jeton] {
            ist = std::make_unique<ida::test::a3::Istemci>();
            baglandi = ist->baglan(port, jeton) &&
                       ist->abone({{"sonar/ping360", 0}, {"sonar/ping1d", 50}, {"calistirma/durum", 0}, {"sayac/gercek_zaman", 0}});
            (void)ist->oku(100);
            hazir = true;
            while (!dur && ist->oku(20)) {
            }
            (void)ist->oku(100);
        });
    };
    a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi&) {
        if (b == 0) {
            for (int i = 0; i < 300 && !hazir; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };
#if !defined(_MSC_VER)
    a3_ayirma_sifirla();
#endif
    std::uint64_t p360 = 0;
    a.sonar_kancasi = [&](const sonar::SonarOlcumu& o) { p360 += o.hdr.tur == sensor::SensorTuru::ping360 ? 1u : 0u; };
    const sunucu::Sonuc r = sunucu::kos(a);
#if !defined(_MSC_VER)
    const std::uint64_t ayirma = a3_ayirma_sayisi();
#endif
    dur = true;
    if (is.joinable()) {
        is.join();
    }
    dogrula(r.tamam && baglandi, "A3 açık sonar koşusu tamam, istemci bağlandı " + r.hata);
    const std::uint64_t n360 = ist ? ist->cerceve_sayisi[30] : 0u;
    const std::uint64_t n1d = ist ? ist->cerceve_sayisi[31] : 0u;
    dogrula(n360 > 0u && n360 <= p360, "sonar/ping360 çerçeveleri geldi (" + s(n360) + " / " + s(p360) + ")");
    dogrula(n1d > 0u, "sonar/ping1d çerçeveleri geldi (" + s(n1d) + ")");
#if defined(_MSC_VER)
    std::printf("  ATLANDI: MSVC'de genel operator new değiştirilmez (ayırma sayımı yalnız POSIX'te)\n");
#else
    dogrula(ayirma == 0u, "dünya tarafındaki A3 hizmetinde bellek ayırma yok (sonar kanalları dahil): " + s(ayirma));
#endif
    std::printf("  ÖLÇÜM (bu Mac, SIL, A3 açık): dünya tarafındaki A3 hizmeti sınır başına en çok %.1f µs, tek yayın çağrısı en çok "
                "%.1f µs (duvar)\n",
                1e-3 * static_cast<double>(r.a3_sinir_en_kotu_ns), 1e-3 * static_cast<double>(r.a3_cagri_en_kotu_ns));
    return 0;
}

// ---------------------------------------------------------------- kabul 6 (ölçüm)

int test_hil_olcum(int ornek)
{
    std::string m = sonar_metni("60_000_000_000");
    degistir(m, "kip = \"sil\"", "kip = \"hil_1x\"");
    degistir(m, "sil_azami_hiz = true\n", "");
    degistir(m, "konum = [0.0, 0.0, 2.0]", "konum = [0.0, 0.0, 2.6]");   // 60 s batık kalsın
    sunucu::Ayar a = ayar(m, true);
    a.karsi = sunucu::Karsi::pty;
    a.karsi_verildi = true;
    a.ping360_ornek = ornek;
    a.gunluk = 1;
    const Gozlem g = gozle(a, false);
    const sunucu::Sonuc& r = g.s;
    dogrula(r.tamam && r.sonar_acik, "HIL 1x (sözde uçbirim) sonar koşusu tamam " + r.hata);
    dogrula(r.zaman.tf_sayisi >= 11'990u, "60 s'de ≈ 12 000 TF (" + s(r.zaman.tf_sayisi) + ")");
    const kopru::Sayaclar& c = r.sayac;
    dogrula(c.ping360_satir > 0u && r.sonar_ping1d > 0u, "Ping360 satırı ve Ping1D gitti");
    dogrula(c.parti_bayt_en_buyuk <= 1200u && c.pencere_tasma == 0u, "parti bütçesi (en büyük " + s(c.parti_bayt_en_buyuk) + " B)");
    std::vector<std::int64_t> p = r.sonar_adim_ping_ns;
    std::sort(p.begin(), p.end());
    const auto yuzde = [&](double q) {
        return p.empty() ? 0.0 : 1e-3 * static_cast<double>(p[std::min(p.size() - 1u, static_cast<std::size_t>(q * static_cast<double>(p.size())))]);
    };
    std::string dagilim;
    for (std::size_t i = 0; i < c.parti_bayt_dagilim.size(); ++i) {
        if (c.parti_bayt_dagilim[i] != 0u) {
            dagilim += " [" + s(i * 100u) + (i + 1u < c.parti_bayt_dagilim.size() ? "–" + s(i * 100u + 99u) : "+") + "] " +
                       s(c.parti_bayt_dagilim[i]);
        }
    }
    std::printf("  ÖLÇÜM (bu Mac, HIL 1x, sözde uçbirim, 60 s, Ping360 %d örnek): TF %llu; gerçek zaman kaçırma %llu "
                "(sim gecikmesi %llu, kaçırılan parti %llu, son tarih %llu); çalıştırma geçersiz 0x%x; parti en büyük %llu B, "
                "%llu üye; parti baytı dağılımı:%s; Ping360 satır %llu, seyreltme %llu, teslim gecikmesi en çok %llu periyot; "
                "Ping1D %llu; sonar adımı (dünya iş parçacığı, duvar): en uzun %.1f µs, ping sınırlarında p50/p99 %.1f/%.1f µs "
                "(%zu sınır)\n",
                ornek == 0 ? 1200 : ornek, static_cast<unsigned long long>(r.zaman.tf_sayisi),
                static_cast<unsigned long long>(r.zaman.gercek_zaman_kacirma),
                static_cast<unsigned long long>(r.zaman.sim_gecikmesi), static_cast<unsigned long long>(c.parti_kacirildi),
                static_cast<unsigned long long>(c.son_tarih_kacirildi), c.calistirma_gecersiz,
                static_cast<unsigned long long>(c.parti_bayt_en_buyuk), static_cast<unsigned long long>(c.parti_uye_en_buyuk),
                dagilim.c_str(), static_cast<unsigned long long>(c.ping360_satir),
                static_cast<unsigned long long>(c.ping360_seyreltildi), static_cast<unsigned long long>(c.ping360_gecikme_en_buyuk),
                static_cast<unsigned long long>(r.sonar_ping1d), 1e-3 * static_cast<double>(r.sonar_adim_en_uzun_ns),
                yuzde(0.5), yuzde(0.99), p.size());
    {
        std::vector<std::int64_t> per = r.zaman.periyotlar_ns;
        std::sort(per.begin(), per.end());
        const double p999 = per.empty() ? 0.0 : 1e-6 * static_cast<double>(per[std::min(per.size() - 1u, per.size() * 999u / 1000u)]);
        std::printf("  ÖLÇÜM: TF geç yazım en çok %.3f ms; periyot p99,9 %.3f ms, en çok %.3f ms; eşzamansız güvenli çıktı %llu "
                    "(ilk parti öncesi %llu); uçtan uca yanıt p50/p99/en çok %.3f/%.3f/%.3f ms\n",
                    1e-6 * static_cast<double>(r.zaman.gec_yazim_en_cok_ns), p999,
                    1e-6 * static_cast<double>(r.zaman.periyot_en_cok_ns), static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                    static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi), 1e-6 * static_cast<double>(r.zaman.yanit_p50_ns),
                    1e-6 * static_cast<double>(r.zaman.yanit_p99_ns), 1e-6 * static_cast<double>(r.zaman.yanit_en_cok_ns));
    }
    // ZAMANLAMA (W4-K24): gerçek zaman kaçırma ve TF yazımının gecikmesi (sonar adımı TF'yi geciktirmez).
    // İyileştirmesiz derlemede (host-debug; NDEBUG yok) sonarın ping hesabı 3–7 ms sürer (ölçüm, bu Mac)
    // ve 1,25 ms'lik sınırı aşar: orada bu iki satır yalnız ölçümdür (mimar onayına; sanitizer'lı
    // derlemedeki W4-K24 muafiyetinin karşılığı). İşlevsel denetimler her derlemede ölçüttür.
    const bool kac = r.zaman.gercek_zaman_kacirma == 0u;
    const bool tf = r.zaman.gec_yazim_en_cok_ns <= 1'000'000;   // A1-K117 T_tf_gec (kart ızgara tick'i) 1 ms
#if defined(NDEBUG)
    dogrula(ida::test::zamanlama(kac, "sonar_hil_olcum.gercek_zaman_kacirma", "kaçırma " + s(r.zaman.gercek_zaman_kacirma)),
            "kaçırma 0 (zamanlama)");
    dogrula(ida::test::zamanlama(tf, "sonar_hil_olcum.tf_gec_yazim",
                                 "TF geç yazım en çok " + std::to_string(1e-6 * static_cast<double>(r.zaman.gec_yazim_en_cok_ns)) +
                                     " ms"),
            "TF geç yazım ≤ T_tf_gec (zamanlama)");
#else
    std::printf("  ZAMANLAMA [sonar_hil_olcum] iyileştirmesiz derleme: yalnız ölçüm (kaçırma %llu, TF geç yazım %s)\n",
                static_cast<unsigned long long>(r.zaman.gercek_zaman_kacirma), tf && kac ? "sınır içinde" : "sınır dışında");
#endif
    return 0;
}

}  // namespace

// test_butunleme.cpp'nin dağıtıcısından çağrılır; -1: bu dosyanın testi değil.
int sonar_testi(const std::string& ad, int argc, char** argv)
{
    g_hata = 0;
    if (ad == "sonar_kapali_esitlik") {
        (void)test_kapali_esitlik();
    } else if (ad == "sonar_belirlenim") {
        (void)test_belirlenim();
    } else if (ad == "sonar_ping1d") {
        (void)test_ping1d();
    } else if (ad == "sonar_ping360") {
        (void)test_ping360();
    } else if (ad == "sonar_red") {
        (void)test_red();
    } else if (ad == "sonar_a3") {
        (void)test_a3();
    } else if (ad == "sonar_hil_olcum") {
        (void)test_hil_olcum(argc > 2 ? std::atoi(argv[2]) : 0);
    } else {
        return -1;
    }
    return g_hata;
}
