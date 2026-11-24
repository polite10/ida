// W5-gorev-yurutucu kabul 6: scenarios/gorev-havuz.toml'un SIL koşusu (süreç içi karşı taraf,
// gerçek çekirdek): yüzeyde iki yol noktası → DIVE_TO → su altında yol noktası → SURFACE.
// Görev COMPLETE olmalı; yer gerçeğine (fiziğin O_b konumu ve derinliği) göre yol noktası
// kaçırma uzaklıkları, bacak boyunca çapraz hata ve su altı bacağındaki derinlik hatası ölçülüp
// yazdırılır; aynı tohumla iki koşu bayt bayt aynı olmalı (CSV, görev izi, yer gerçeği izi).
//
//   gorev_sil      koşu, ölçüm ve belirlenimcilik
//
// Ölçüm sınırları bu testin seçimidir (TAHMİN; gerçek araçta ölçülmedi): kestirilen konuma göre
// kabul yarıçapına girme ve çapraz hata ≤ 1,5 m (güdüm); yer gerçeğinde kaçırma ≤ 3 m (kaba sınır:
// kestirim hatasını içerir, havuzda GNSS'le 1,4–2,7 m ölçüldü); su altı derinlik hatası ≤ 0,3 m.
#include "ida/port.h"
#include "ida/sunucu/sunucu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

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

struct Oge {
    bool yol;          // WAYPOINT
    double n, e, d;    // hedef (yol noktası) ya da derinlik (DIVE_TO)
    double r;          // kabul yarıçapı
};

// scenarios/gorev-havuz.toml'un öğeleri (dosyayla aynı tutulur).
const std::array<Oge, 5> OGELER = {{
    {true, 6.0, 0.0, 0.0, 0.5},
    {true, 6.0, 4.0, 0.0, 0.5},
    {false, 0.0, 0.0, 1.5, 0.0},
    {true, 0.0, 4.0, 1.5, 0.5},
    {false, 0.0, 0.0, 0.0, 0.0},
}};

struct Iz {
    std::vector<std::uint8_t> durum, oge;   // kartın status'u (çağrı başına)
    std::vector<double> n, e, d;            // yer gerçeği (fizik sınırı başına)
    std::vector<int> sinir_oge;             // sınırdaki etkin öğe (son kart çıktısından)
    std::vector<int> sinir_durum;
    std::vector<double> kn, ke;             // kestirilen konum, sınırdaki son kart çıktısından
    double son_kn = 0.0, son_ke = 0.0;
    std::uint8_t en_kotu_fs = 0;
    std::uint32_t fs_nedenleri = 0;
    int son_oge = -1, son_durum = -1;
};

sunucu::Sonuc kos(Iz& iz)
{
    sunucu::Ayar a;
    a.gunluk = 0;
    a.depo_koku = IDA_DEPO_KOKU;
    a.senaryo_dosyasi = "scenarios/gorev-havuz.toml";
    a.ic_cikti_kancasi = [&iz](int, const ida_in_t&, ida_out_t& o) {
        iz.durum.push_back(o.status.mission_state);
        iz.oge.push_back(static_cast<std::uint8_t>(o.status.mission_item));
        iz.son_oge = o.status.mission_item;
        if (o.nav_valid) {
            iz.son_kn = static_cast<double>(o.nav.p_ned[0]);
            iz.son_ke = static_cast<double>(o.nav.p_ned[1]);
        }
        iz.son_durum = o.status.mission_state;
        if (o.status.fs_level > iz.en_kotu_fs) {
            iz.en_kotu_fs = o.status.fs_level;
        }
        iz.fs_nedenleri |= o.status.fs_reasons & ~static_cast<std::uint32_t>(IDA_FSR_IMU_LAG);
    };
    a.sinir_kancasi = [&iz](const sunucu::Ayar::SinirGozlemi& g) {
        iz.n.push_back(g.konum[0]);
        iz.e.push_back(g.konum[1]);
        iz.d.push_back(g.derinlik);
        iz.sinir_oge.push_back(iz.son_oge);
        iz.sinir_durum.push_back(iz.son_durum);
        iz.kn.push_back(iz.son_kn);
        iz.ke.push_back(iz.son_ke);
    };
    return sunucu::kos(a);
}

int test_gorev_sil()
{
    Iz iz;
    const sunucu::Sonuc s = kos(iz);
    dogrula(s.tamam, "koşu " + s.hata);
    dogrula(s.gorev_durumu == "tamamlandi", "görev yüklendi (" + s.gorev_durumu + ")");
    // Durum izi: RUNNING'e geçti, PAUSED ve ABORTED görülmedi, COMPLETE ile bitti.
    bool kostu = false, durdu = false, iptal = false;
    int en_cok_oge = -1;
    std::size_t tamam_i = iz.durum.size();
    for (std::size_t i = 0; i < iz.durum.size(); ++i) {
        kostu = kostu || iz.durum[i] == IDA_MS_RUNNING;
        durdu = durdu || iz.durum[i] == IDA_MS_PAUSED;
        iptal = iptal || iz.durum[i] == IDA_MS_ABORTED;
        if (iz.durum[i] == IDA_MS_RUNNING && iz.oge[i] > en_cok_oge) {
            en_cok_oge = iz.oge[i];
        }
        if (iz.durum[i] == IDA_MS_COMPLETE && tamam_i == iz.durum.size()) {
            tamam_i = i;
        }
    }
    dogrula(kostu && !durdu && !iptal, "görev RUNNING oldu; PAUSED ve ABORTED görülmedi");
    dogrula(en_cok_oge == 4, "bütün öğeler yürütüldü (son öğe " + std::to_string(en_cok_oge) + ")");
    dogrula(tamam_i < iz.durum.size() && iz.durum.back() == IDA_MS_COMPLETE, "görev COMPLETE");
    std::printf("  RAPOR: en kötü fs_level %u, nedenler 0x%X; COMPLETE çağrısı %zu / %zu\n",
                static_cast<unsigned>(iz.en_kotu_fs), static_cast<unsigned>(iz.fs_nedenleri), tamam_i,
                iz.durum.size());
    // Ölçümler (fizik sınırı başına; sınırdaki etkin öğe ve kestirim son kart çıktısından):
    // yer gerçeğine göre (ölçüm; kestirim hatasını da içerir) ve kestirilen konuma göre (güdümün
    // gördüğü). Bacağın başı önceki yol noktasıdır (yoksa öğe başındaki konum).
    for (std::size_t k = 0; k < OGELER.size(); ++k) {
        const Oge& og = OGELER[k];
        double g_en_az = 1e9, k_en_az = 1e9, g_capraz = 0.0, k_capraz = 0.0, g_kare = 0.0, d_en = 0.0, d_kare = 0.0,
               kes_en = 0.0;
        std::size_t say = 0, ilk = iz.n.size(), son = 0;
        double bn = 0.0, be = 0.0, kbn = 0.0, kbe = 0.0;
        const bool bas_yol = k > 0 && OGELER[k - 1].yol;
        for (std::size_t i = 0; i < iz.n.size(); ++i) {
            if (iz.sinir_oge[i] != static_cast<int>(k) || iz.sinir_durum[i] != IDA_MS_RUNNING) {
                continue;
            }
            if (ilk == iz.n.size()) {
                ilk = i;
                if (bas_yol) {
                    const Oge& onceki = OGELER[k - 1];
                    bn = onceki.n;
                    be = onceki.e;
                    kbn = onceki.n;
                    kbe = onceki.e;
                } else {
                    bn = iz.n[i];
                    be = iz.e[i];
                    kbn = iz.kn[i];
                    kbe = iz.ke[i];
                }
            }
            son = i;
            ++say;
            kes_en = std::max(kes_en, std::hypot(iz.n[i] - iz.kn[i], iz.e[i] - iz.ke[i]));
            if (og.yol) {
                g_en_az = std::min(g_en_az, std::hypot(iz.n[i] - og.n, iz.e[i] - og.e));
                k_en_az = std::min(k_en_az, std::hypot(iz.kn[i] - og.n, iz.ke[i] - og.e));
                const double ln = og.n - bn, le = og.e - be, l = std::hypot(ln, le);
                const double y = l > 1e-6 ? (-(iz.n[i] - bn) * le + (iz.e[i] - be) * ln) / l : 0.0;
                g_capraz = std::max(g_capraz, std::fabs(y));
                g_kare += y * y;
                const double kln = og.n - kbn, kle = og.e - kbe, kl = std::hypot(kln, kle);
                const double ky = kl > 1e-6 ? (-(iz.kn[i] - kbn) * kle + (iz.ke[i] - kbe) * kln) / kl : 0.0;
                k_capraz = std::max(k_capraz, std::fabs(ky));
                if (og.d > 0.3) {
                    const double e = iz.d[i] - og.d;
                    d_en = std::max(d_en, std::fabs(e));
                    d_kare += e * e;
                }
            }
        }
        if (say == 0) {
            dogrula(false, "öğe " + std::to_string(k) + " yürütülmedi");
            continue;
        }
        const double sure_s = static_cast<double>(son - ilk + 1) * 0.00125;   // fizik adımı 1,25 ms
        char b[512];
        if (og.yol) {
            std::snprintf(b, sizeof b,
                          "  RAPOR: öğe %zu WAYPOINT (%.1f, %.1f, %.1f m), %.1f s: yer gerçeği bitişte %.3f m, en yakın "
                          "%.3f m, çapraz en çok %.3f m (RMS %.3f); kestirime göre en yakın %.3f m, çapraz en çok %.3f m; "
                          "kestirim hatası en çok %.3f m",
                          k, og.n, og.e, og.d, sure_s, std::hypot(iz.n[son] - og.n, iz.e[son] - og.e), g_en_az, g_capraz,
                          std::sqrt(g_kare / static_cast<double>(say)), k_en_az, k_capraz, kes_en);
            std::printf("%s\n", b);
            // Güdüm (kestirilen konum): kabul yarıçapı ve çapraz hata. Yer gerçeği ölçümdür; kaba
            // bozulmaya karşı geniş sınır (kestirim hatasını içerir; W5-K24).
            dogrula(k_en_az <= og.r + 0.05, "öğe " + std::to_string(k) + ": kestirime göre kabul yarıçapına girdi");
            // Kestirilen konum GNSS düzeltmeleriyle sıçrayabildiği için bacak başındaki çapraz hata
            // kestirim gürültüsünü de taşır (ilk koşuda en çok 1,15 m ölçüldü).
            dogrula(k_capraz <= 1.5, "öğe " + std::to_string(k) + ": kestirime göre çapraz hata ≤ 1,5 m");
            dogrula(g_en_az <= 3.0, "öğe " + std::to_string(k) + ": yer gerçeğinde kaçırma ≤ 3 m (kaba sınır)");
            if (og.d > 0.3) {
                std::snprintf(b, sizeof b, "  RAPOR: öğe %zu derinlik hatası (yer gerçeği) en çok %.3f m, RMS %.3f m", k,
                              d_en, std::sqrt(d_kare / static_cast<double>(say)));
                std::printf("%s\n", b);
                dogrula(d_en <= 0.3, "öğe " + std::to_string(k) + ": su altı derinlik hatası ≤ 0,3 m");
            }
        } else {
            std::snprintf(b, sizeof b,
                          "  RAPOR: öğe %zu %s, %.1f s: bitişte derinlik %.3f m (hedef %.2f), yatay kayma %.3f m; "
                          "kestirim hatası en çok %.3f m",
                          k, og.d > 0.0 ? "DIVE_TO" : "SURFACE", sure_s, iz.d[son], og.d,
                          std::hypot(iz.n[son] - iz.n[ilk], iz.e[son] - iz.e[ilk]), kes_en);
            std::printf("%s\n", b);
        }
    }
    // Belirlenimcilik: ikinci koşu bayt bayt aynı.
    Iz iz2;
    const sunucu::Sonuc s2 = kos(iz2);
    dogrula(s2.tamam && s.csv == s2.csv && !s.csv.empty(), "iki koşunun CSV'si bayt bayt aynı");
    dogrula(iz.durum == iz2.durum && iz.oge == iz2.oge, "iki koşunun görev izi aynı");
    dogrula(iz.n == iz2.n && iz.e == iz2.e && iz.d == iz2.d, "iki koşunun yer gerçeği izi aynı");
    dogrula(s.davranis_ozeti == s2.davranis_ozeti, "davranış özeti aynı");
    return 0;
}

}  // namespace

int gorev_sil_testi(const std::string& ad, int argc, char** argv)
{
    (void)argc;
    (void)argv;
    if (ad != "gorev_sil") {
        return -1;
    }
    g_hata = 0;
    (void)test_gorev_sil();
    return g_hata;
}
