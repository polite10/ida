// SIL'de alım anı ve akışın sonu (W4-K26; EMNİYET). Sim (ida_sunucu, SIL, TCP) ile ida_sil
// koşucusu arasına giren aktarıcı, kartın k. itkili IDA_ACTUATOR(n)'ından hemen sonra kart → sim
// akışına bir ek katar ve ACTUATOR(n) ile eki farklı teslim biçimleriyle (aynı okumada, ayrı ve
// gecikmeli, parçalı, ince parçalı) gönderir. Aynı bayt dizisi her teslim biçiminde aynı sonucu
// vermelidir.
//
//   sil_kisitlayan_alim_ani   W4-K26 (1): ek = eşzamansız güvenli çıktı (ASYNC + IS_SAFE) ya da
//                             kartın SESSION_CLOSE'u. Nötrleme, yuva n + 1'in sınırındaki fizik
//                             adımında (b* = t_n / h + 4) başlar; b* − 1'de itki nötr değildir;
//                             A1-K78 iptali (yanit_iptal_esz) ve sim → kart baytları her biçimde aynı.
//   sil_akis_sonu_belirlenim  W4-K26 (2): ek = 150 IDA_LOG, ardından FIN (aktarıcı yazma yönünü
//                             kapatır). Dört teslim biçiminde aynı bitiş nedeni (tampon taşması →
//                             SESSION_CLOSE(PROTOCOL)) ve sim'in göndermeye karar verdiği aynı dizi.
//                             Sim kapatınca aktarıcı yeni bağlantıyla sürdürür; koşu tamamlanır.
//   sil_sifir_seli            W4-K26 (3): ek = kesintisiz 0x00 akışı. Bellek sınırlı, oturum
//                             protokol hatasıyla biter; sel sim'in kapanışıyla durur, koşu tamamlanır.
// FIN ve selde tetikten sonra kartın girdisi dondurulur (sim → kart iletilmez): kartın gördüğü
// akış teslim biçiminden bağımsızdır. Hiçbir sınama sim'in duvar saati süre sınırlarına dayanmaz
// (W4-mac-ci Windows CI: kısaltılmış baglanti_bekleme_ns hazırlık oturumunu da kapsıyordu).
#include "ida/kopru/tasima.hpp"
#include "ida/sil/kosucu.hpp"
#include "ida_link/tel.h"
#include "ida/sunucu/sunucu.hpp"
#include "../../sim/kopru/src/mavlink_ic.hpp"   // yalnız mesaj yapıları (A1-K101)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_hata_sa = 0;

void dogrula_sa(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_sa;
    }
}

constexpr std::int64_t kH = 1'250'000;
constexpr std::uint16_t kNotr = 1500;

enum class Teslim { ayni_okuma, ayri_gecikmeli, parcali, ince_parcali };
enum class Ek { esz_guvenli, kart_kapanisi, log_fin, sifir_seli };

const char* teslim_adi(Teslim t)
{
    switch (t) {
    case Teslim::ayni_okuma:
        return "aynı okuma";
    case Teslim::ayri_gecikmeli:
        return "ayrı, 3 ms gecikmeli";
    case Teslim::parcali:
        return "parçalı (2 + 3 parça, 1 ms arayla)";
    case Teslim::ince_parcali:
        return "ince parçalı (97 B, 0,3 ms arayla)";
    }
    return "?";
}

std::vector<std::uint8_t> kodla(ida_link_gonderici_t& g, std::uint32_t msgid, const void* m, std::size_t boy)
{
    std::uint8_t b[IDA_LINK_CERCEVE_AZAMI];
    std::size_t n = 0;
    if (ida_link_gonder(&g, msgid, m, boy, b, sizeof b, &n) != IDA_LINK_TAMAM) {
        return {};
    }
    return std::vector<std::uint8_t>(b, b + n);
}

class EkAktarici {
public:
    EkAktarici(std::uint16_t sim_port, Teslim teslim, Ek ek) : teslim_(teslim), ek_(ek), sim_port_(sim_port)
    {
        std::string hata;
        din_ = ida::kopru::tcp_dinle(0, hata);
        (void)ida_link_alici_kur(&alici_, IDA_LINK_ROL_SIM);
        (void)ida_link_alici_kur(&sim_alici_, IDA_LINK_ROL_KART);
        (void)ida_link_gonderici_kur(&g_, IDA_LINK_ROL_KART);
        g_.seq = 0x80;
        if (din_) {
            is_ = std::thread([this] { dongu(); });
        }
    }
    ~EkAktarici() { bekle(); }
    void bekle()
    {
        dur_ = true;
        if (is_.joinable()) {
            is_.join();
        }
    }
    std::uint16_t port() const { return din_ ? din_->port() : 0; }
    const std::vector<std::uint8_t>& sim_kart() const { return sim_kart_; }
    bool tetiklendi() const { return tetik_; }
    std::string tani() const
    {
        return "aktarıcı: aşama '" + asama_ + "', sim bağlantısı " + std::to_string(sim_baglanti_) +
               ", sim → aktarıcı " + std::to_string(sim_okunan_) + " B (karta iletilen " +
               std::to_string(sim_kart_iletilen_) + " B), kart → aktarıcı " + std::to_string(kart_okunan_) +
               " B (sim'e yazılan " + std::to_string(kart_sim_yazilan_) + " B), itkili ACTUATOR " +
               std::to_string(itkili_sayi_) + ", soket hata kodu sim " + std::to_string(sim_hata_) + " kart " +
               std::to_string(kart_hata_);
    }
    // Tetikleyen ACTUATOR'ın partisinin TF zamanı (sim zamanı, ns); yoksa −1.
    std::int64_t t_n() const
    {
        const auto it = tf_.find(tetik_seq_);
        return tetik_ && it != tf_.end() ? it->second : -1;
    }

private:
    using Saat = std::chrono::steady_clock;
    struct Parca {
        std::vector<std::uint8_t> bayt;
        Saat::time_point birak;
        bool fin = false;   // bu parçadan sonra sim'e FIN
    };

    void kuyruga(std::vector<std::uint8_t> b, std::int64_t gecikme_us, bool fin = false)
    {
        const Saat::time_point t = Saat::now() + std::chrono::microseconds(gecikme_us);
        son_ = std::max(son_, t);
        kuyruk_.push_back({std::move(b), son_, fin});
    }

    // sim → kart yönünün bir çerçevesi: TF izlenir; donukken karta iletilmez. Selde sim'in
    // SESSION_CLOSE'u görülünce sel durur ve akış (o kapanıştan itibaren) yeniden iletilir.
    void sim_cercevesi(const std::vector<std::uint8_t>& ham, const ida_link_cerceve_t& c, ida::kopru::Tasima& kart)
    {
        if (c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
            mavlink_ida_time_frame_t tf{};
            (void)ida_link_coz(&c, c.msgid, &tf, sizeof tf);
            tf_[tf.batch_seq] = tf.t_sim_ns;
        }
        if (sim_donuk_ && sel_ && c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            sim_donuk_ = false;
            sel_ = false;
            kesildi_ = false;
            asama_ = "sel bitti: sim'in kapanışından itibaren iletiliyor";
        }
        if (sim_donuk_) {
            return;
        }
        if (!kart.yaz(ham.data(), ham.size())) {
            kart_hata_ = kart.son_hata_kodu();
            asama_ = "karta yazım başarısız";
            return;
        }
        sim_kart_iletilen_ += ham.size();
    }

    std::vector<std::uint8_t> ek_baytlari(const mavlink_ida_actuator_t& a)
    {
        std::vector<std::uint8_t> v;
        if (ek_ == Ek::esz_guvenli) {
            // A1-K44, K78: ızgara tick'inin eşzamansız güvenli çıktısı; nötr, kendinden önce
            // üretilmiş uygulanmamış yanıtları iptal eder. Kartın sonraki çıktılarının out_seq'i
            // bir kaydırılır (gerçek kartta ASYNC de bir out_seq tüketir).
            mavlink_ida_actuator_t e{};
            e.batch_seq = a.batch_seq;
            e.valid_for_ns = a.valid_for_ns;
            e.out_seq = a.out_seq + 1u;
            for (auto& p : e.pwm_us) {
                p = kNotr;
            }
            e.sess = a.sess;
            e.act_flags = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                                     IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC);
            e.core_rc = static_cast<std::uint8_t>(IDA_RC_OK);
            v = kodla(g_, MAVLINK_MSG_ID_IDA_ACTUATOR, &e, sizeof e);
            kaydir_ = true;
        } else if (ek_ == Ek::kart_kapanisi) {
            mavlink_ida_session_close_t c{};
            c.sess = a.sess;
            c.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SAFE_STATE);
            v = kodla(g_, MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, sizeof c);
        } else if (ek_ == Ek::log_fin) {
            mavlink_ida_log_t l{};
            for (int k = 0; k < 150; ++k) {
                const std::uint8_t ilk = static_cast<std::uint8_t>(k + 1);   // kırpılmasın diye
                std::memset(&l, 0, sizeof l);
                std::memcpy(&l, &ilk, 1u);
                const auto f = kodla(g_, MAVLINK_MSG_ID_IDA_LOG, &l, sizeof l);
                v.insert(v.end(), f.begin(), f.end());
            }
        }
        return v;
    }

    // ACTUATOR(n) ve ek, teslim biçimine göre parçalara bölünüp kuyruğa girer.
    void teslim_et(std::vector<std::uint8_t> act, std::vector<std::uint8_t> ek, bool fin)
    {
        auto bol = [](const std::vector<std::uint8_t>& v, std::size_t parca) {
            std::vector<std::vector<std::uint8_t>> p;
            for (std::size_t i = 0; i < v.size(); i += parca) {
                p.emplace_back(v.begin() + static_cast<std::ptrdiff_t>(i),
                               v.begin() + static_cast<std::ptrdiff_t>(std::min(v.size(), i + parca)));
            }
            return p;
        };
        switch (teslim_) {
        case Teslim::ayni_okuma: {
            act.insert(act.end(), ek.begin(), ek.end());
            kuyruga(std::move(act), 0, fin);
            break;
        }
        case Teslim::ayri_gecikmeli:
            kuyruga(std::move(act), 0);
            kuyruga(std::move(ek), 3000, fin);
            break;
        case Teslim::parcali: {
            const std::size_t y = act.size() / 2;
            kuyruga(std::vector<std::uint8_t>(act.begin(), act.begin() + static_cast<std::ptrdiff_t>(y)), 0);
            kuyruga(std::vector<std::uint8_t>(act.begin() + static_cast<std::ptrdiff_t>(y), act.end()), 1000);
            const auto p = bol(ek, ek.size() / 3 + 1);
            for (std::size_t i = 0; i < p.size(); ++i) {
                kuyruga(p[i], static_cast<std::int64_t>(2000 + 1000 * i), fin && i + 1 == p.size());
            }
            break;
        }
        case Teslim::ince_parcali: {
            kuyruga(std::move(act), 0);
            const auto p = bol(ek, 97);
            for (std::size_t i = 0; i < p.size(); ++i) {
                kuyruga(p[i], static_cast<std::int64_t>(300 * (i + 1)), fin && i + 1 == p.size());
            }
            break;
        }
        }
        if (fin && ek.empty() && kuyruk_.empty()) {
            kuyruga({}, 0, true);
        }
    }

    void kart_cercevesi(std::vector<std::uint8_t> ham, const ida_link_cerceve_t& c)
    {
        if (kesildi_) {
            return;   // FIN ya da sel: kartın sonraki çıktısı iletilmez
        }
        if (c.msgid != MAVLINK_MSG_ID_IDA_ACTUATOR) {
            kuyruga(std::move(ham), 0);
            return;
        }
        mavlink_ida_actuator_t a{};
        (void)ida_link_coz(&c, c.msgid, &a, sizeof a);
        if (kaydir_) {
            ++a.out_seq;
            ida_link_gonderici_t g2{};
            (void)ida_link_gonderici_kur(&g2, IDA_LINK_ROL_KART);
            g2.seq = c.seq;
            ham = kodla(g2, MAVLINK_MSG_ID_IDA_ACTUATOR, &a, sizeof a);
        }
        bool itkili = (a.act_flags & IDAW_ACT_FLAGS_THR_VALID) != 0u && (a.act_flags & IDAW_ACT_FLAGS_IS_SAFE) == 0u;
        bool notr_disi = false;
        for (std::uint16_t p : a.pwm_us) {
            notr_disi = notr_disi || p != kNotr;
        }
        itkili = itkili && notr_disi;
        if (!tetik_ && itkili && ++itkili_sayi_ == 5u) {
            tetik_ = true;
            tetik_seq_ = a.batch_seq;
            // FIN ve selde tetikten sonra kartın girdisi dondurulur (sim → kart iletilmez): kartın
            // gördüğü akış teslim biçiminden bağımsız kalır (tetik anında sim TF(n)'den sonrasını
            // göndermemiştir; kilitli adım).
            if (ek_ == Ek::sifir_seli) {
                kuyruga(std::move(ham), 0);
                sel_ = true;
                kesildi_ = true;
                sim_donuk_ = true;
                asama_ = "sel";
                return;
            }
            const bool fin = ek_ == Ek::log_fin;
            teslim_et(std::move(ham), ek_baytlari(a), fin);
            kesildi_ = fin;
            sim_donuk_ = fin;
            return;
        }
        kuyruga(std::move(ham), 0);
    }

    void dongu()
    {
        std::string hata;
        auto sim = ida::kopru::tcp_baglan(sim_port_, hata);
        auto kart = din_->kabul(60'000'000, hata);
        if (!sim || !kart) {
            asama_ = std::string("kurulum başarısız: ") + (sim ? "" : "sim'e bağlanılamadı ") +
                     (kart ? "" : "kart bağlanmadı ") + hata;
            return;
        }
        sim_baglanti_ = 1;
        asama_ = "aktarılıyor";
        std::vector<std::uint8_t> t(65536);
        std::vector<std::uint8_t> sifir(65536, 0);
        std::vector<std::uint8_t> ham;
        std::vector<std::uint8_t> sim_ham;
        int bos = 0;
        while (!dur_) {
            bool is_var = false;
            std::size_t n = 0;
            ida::kopru::OkumaSonucu r = sim->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                sim_hata_ = sim->son_hata_kodu();
                asama_ = "sim bağlantıyı kapattı";
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                sim_okunan_ += n;
                sim_kart_.insert(sim_kart_.end(), t.data(), t.data() + n);
                std::size_t k = 0;
                while (k < n) {
                    std::size_t tuk = 0;
                    ida_link_cerceve_t c{};
                    const ida_link_rc_t rc = ida_link_al(&sim_alici_, t.data() + k, n - k, &tuk, &c);
                    sim_ham.insert(sim_ham.end(), t.data() + k, t.data() + k + tuk);
                    k += tuk;
                    if (rc != IDA_LINK_CERCEVE) {
                        break;
                    }
                    sim_cercevesi(sim_ham, c, *kart);
                    sim_ham.clear();
                }
            }
            r = kart->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                kart_hata_ = kart->son_hata_kodu();
                asama_ = "kart bağlantıyı kapattı";
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                kart_okunan_ += n;
                std::size_t k = 0;
                while (k < n) {
                    std::size_t tuk = 0;
                    ida_link_cerceve_t c{};
                    const ida_link_rc_t rc = ida_link_al(&alici_, t.data() + k, n - k, &tuk, &c);
                    ham.insert(ham.end(), t.data() + k, t.data() + k + tuk);
                    k += tuk;
                    if (rc != IDA_LINK_CERCEVE) {
                        break;
                    }
                    kart_cercevesi(std::move(ham), c);
                    ham.clear();
                }
            }
            const auto simdi = Saat::now();
            bool fin_oldu = false;
            while (!kuyruk_.empty() && kuyruk_.front().birak <= simdi) {
                Parca p = std::move(kuyruk_.front());
                kuyruk_.pop_front();
                is_var = true;
                if (!p.bayt.empty()) {
                    if (!sim->yaz(p.bayt.data(), p.bayt.size())) {
                        sim_hata_ = sim->son_hata_kodu();
                        asama_ = "sim'e yazım başarısız";
                        return;
                    }
                    kart_sim_yazilan_ += p.bayt.size();
                }
                if (p.fin) {
                    fin_oldu = true;
                    break;
                }
            }
            if (fin_oldu) {
                // FIN: yazma yönü kapanır; sim kapatana dek (en çok 30 s) okunur. Sonra sim'in
                // beklediği yeni bağlantı kurulur (A1-K99: yeni akış) ve aktarım sürer; kartın
                // girdisi tetikten beri donuktu, yeni akışın ilk baytından itibaren iletilir.
                asama_ = "FIN gönderildi, sim'in kapatması bekleniyor";
                sim->kapat_duzgun(30'000'000);
                sim.reset();
                for (int i = 0; i < 6000 && !sim && !dur_; ++i) {
                    sim = ida::kopru::tcp_baglan(sim_port_, hata);
                    if (!sim) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                }
                if (!sim) {
                    asama_ = "FIN sonrası sim'e yeniden bağlanılamadı: " + hata;
                    return;
                }
                ++sim_baglanti_;
                (void)ida_link_alici_kur(&sim_alici_, IDA_LINK_ROL_KART);
                sim_ham.clear();
                sim_donuk_ = false;
                kesildi_ = false;
                asama_ = "FIN sonrası yeni bağlantı";
                continue;
            }
            if (sel_ && kuyruk_.empty()) {
                const long long w = sim->yaz_biraz(sifir.data(), sifir.size());
                if (w < 0) {
                    sim_hata_ = sim->son_hata_kodu();
                    asama_ = "selde sim'e yazım başarısız";
                    break;
                }
                kart_sim_yazilan_ += static_cast<std::uint64_t>(w);
                is_var = is_var || w > 0;
            }
            // Windows'ta uyku çözünürlüğü kaba (~15,6 ms): boşta önce iş parçacığı sırası bırakılır.
            if (is_var) {
                bos = 0;
            } else if (++bos < 2000) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    Teslim teslim_;
    Ek ek_;
    std::uint16_t sim_port_;
    std::unique_ptr<ida::kopru::TcpDinleyici> din_;
    ida_link_alici_t alici_{};
    ida_link_alici_t sim_alici_{};
    ida_link_gonderici_t g_{};
    std::thread is_;
    std::atomic<bool> dur_{false};
    std::deque<Parca> kuyruk_;
    Saat::time_point son_ = Saat::now();
    std::vector<std::uint8_t> sim_kart_;
    std::map<std::uint32_t, std::int64_t> tf_;
    bool tetik_ = false;
    std::uint32_t tetik_seq_ = 0;
    std::uint32_t itkili_sayi_ = 0;
    bool kaydir_ = false;
    bool kesildi_ = false;
    bool sel_ = false;
    bool sim_donuk_ = false;   // tetikten sonra sim → kart iletilmez (kartın girdisi belirli kalır)
    // Tanı (kurulamayan oturumda nedeni göstermek için).
    std::string asama_ = "başlamadı";
    int sim_baglanti_ = 0;
    std::uint64_t sim_okunan_ = 0;
    std::uint64_t sim_kart_iletilen_ = 0;
    std::uint64_t kart_okunan_ = 0;
    std::uint64_t kart_sim_yazilan_ = 0;
    int sim_hata_ = 0;
    int kart_hata_ = 0;
};

struct Kosu {
    ida::sunucu::Sonuc s;
    std::vector<std::pair<std::uint64_t, std::array<double, 8>>> girdi;   // (b, fiziğe verilen)
    std::vector<std::uint8_t> giden;                                      // sim'in karar verdiği dizi
    std::vector<std::uint8_t> sim_kart;                                   // aktarıcının okuduğu
    std::int64_t t_n = -1;
    bool tetik = false;
    double sure_s = 0.0;
    std::string tani;
};

// Oturum kurulamadığında ya da ek katılmadığında nedeni yazar (CI günlüğü belirsiz kalmasın).
void tani_yaz(const Kosu& k)
{
    if (k.tetik && k.s.tamam) {
        return;
    }
    std::printf("  TANI: tamam %d, hata '%s', hazırlık oturumu %llu, kurulan oturum %llu, yeniden bağlantı %llu, "
                "bağlantı kopması %llu, el sıkışmada atılan %llu, kart açılışı %u, son oturum reddi %u, "
                "karar dizisi %zu B, aktarıcının okuduğu %zu B, %.2f s\n  TANI: %s\n",
                k.s.tamam ? 1 : 0, k.s.hata.c_str(), static_cast<unsigned long long>(k.s.hazirlik_oturumu),
                static_cast<unsigned long long>(k.s.sayac.oturum_kurulan),
                static_cast<unsigned long long>(k.s.yeniden_baglanti),
                static_cast<unsigned long long>(k.s.baglanti_kopma),
                static_cast<unsigned long long>(k.s.sayac.el_sikisma_atilan), static_cast<unsigned>(k.s.kart.boot_count),
                static_cast<unsigned>(k.s.kart.son_oturum_reddi), k.giden.size(), k.sim_kart.size(), k.sure_s,
                k.tani.c_str());
}

// baglanti_bekleme_ns varsayılandır (20 s): sim'in el sıkışma süre sınırı hazırlık oturumunu da
// kapsar ve yavaş koşucuda (Windows CI, hata ayıklama/ASan) yüzlerce ms'yi aşar. Sınamalar bu
// süreye dayanmaz: FIN'den sonra aktarıcı yeni bağlantıyla sürdürür, sel sim'in kapanışıyla durur.
Kosu kos(Teslim teslim, Ek ek, double sure_s)
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = sure_s;
    a.karsi = ida::sunucu::Karsi::tcp;
    Kosu k;
    a.sinir_kancasi = [&k](const ida::sunucu::Ayar::SinirGozlemi& g) { k.girdi.emplace_back(g.b, g.pwm); };
    a.giden_kancasi = [&k](const std::uint8_t* p, std::size_t n) { k.giden.insert(k.giden.end(), p, p + n); };
    std::unique_ptr<EkAktarici> ak;
    std::unique_ptr<ida::sil::Kosucu> kosucu;
    std::thread is;
    a.tcp_hazir = [&](std::uint16_t port, std::uint64_t tohum) {
        ak = std::make_unique<EkAktarici>(port, teslim, ek);
        ida::sil::Ayarlar ka;
        ka.a1_port = ak->port();
        ka.a1_tohum = tohum;
        ka.gunluk = 0;
        kosucu = std::make_unique<ida::sil::Kosucu>(ka);
        is = std::thread([&] { (void)kosucu->calistir(); });
    };
    const auto bas = std::chrono::steady_clock::now();
    k.s = ida::sunucu::kos(a);
    k.sure_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    if (kosucu) {
        kosucu->durdur();
    }
    if (is.joinable()) {
        is.join();
    }
    if (ak) {
        ak->bekle();
        k.sim_kart = ak->sim_kart();
        k.t_n = ak->t_n();
        k.tetik = ak->tetiklendi();
        k.tani = ak->tani();
    }
    tani_yaz(k);
    return k;
}

bool notr(const std::array<double, 8>& p)
{
    return std::all_of(p.begin(), p.end(), [](double x) { return x == static_cast<double>(kNotr); });
}

const std::array<double, 8>* girdi_b(const Kosu& k, std::uint64_t b)
{
    for (const auto& [bb, p] : k.girdi) {
        if (bb == b) {
            return &p;
        }
    }
    return nullptr;
}

// Akıştaki SESSION_CLOSE nedenleri (sim → kart yönü).
std::vector<std::uint8_t> kapanislar(const std::vector<std::uint8_t>& v)
{
    std::vector<std::uint8_t> r;
    ida_link_alici_t al{};
    (void)ida_link_alici_kur(&al, IDA_LINK_ROL_KART);
    std::size_t k = 0;
    while (k < v.size()) {
        std::size_t tuk = 0;
        ida_link_cerceve_t c{};
        const ida_link_rc_t rc = ida_link_al(&al, v.data() + k, v.size() - k, &tuk, &c);
        k += tuk;
        if (rc != IDA_LINK_CERCEVE) {
            break;
        }
        if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            mavlink_ida_session_close_t s{};
            (void)ida_link_coz(&c, c.msgid, &s, sizeof s);
            r.push_back(s.reason);
        }
    }
    return r;
}

std::string nedenler(const std::vector<std::uint8_t>& v)
{
    std::string s;
    for (std::uint8_t x : v) {
        s += (s.empty() ? "" : ",") + std::to_string(x);
    }
    return "[" + s + "]";
}

constexpr Teslim kTeslimler[] = {Teslim::ayni_okuma, Teslim::ayri_gecikmeli, Teslim::parcali, Teslim::ince_parcali};

int test_sil_kisitlayan_alim_ani()
{
    for (const Ek ek : {Ek::esz_guvenli, Ek::kart_kapanisi}) {
        const char* ad = ek == Ek::esz_guvenli ? "eşzamansız güvenli çıktı" : "kartın SESSION_CLOSE'u";
        std::vector<Kosu> kosular;
        for (const Teslim t : kTeslimler) {
            kosular.push_back(kos(t, ek, 4.0));
            const Kosu& k = kosular.back();
            const std::uint64_t bs = k.t_n >= 0 ? static_cast<std::uint64_t>(k.t_n / kH) + 4u : 0u;
            const auto* once = girdi_b(k, bs - 1u);
            const auto* simdi = girdi_b(k, bs);
            std::uint64_t ilk_notr = 0;
            for (const auto& [b, p] : k.girdi) {
                if (b >= bs - 1u && notr(p)) {
                    ilk_notr = b;
                    break;
                }
            }
            std::printf("  %s, %s: t_n %lld ns, b* %llu, ilk nötr %llu, yanit_iptal_esz %llu, esz uygulanan %llu, "
                        "oturum kopması %llu, sim→kart %zu B\n",
                        ad, teslim_adi(t), static_cast<long long>(k.t_n), static_cast<unsigned long long>(bs),
                        static_cast<unsigned long long>(ilk_notr),
                        static_cast<unsigned long long>(k.s.sayac.yanit_iptal_esz),
                        static_cast<unsigned long long>(k.s.sayac.esz_guvenli_uygulanan),
                        static_cast<unsigned long long>(k.s.sayac.oturum_kopma), k.sim_kart.size());
            const std::string on = std::string(ad) + ", " + teslim_adi(t) + ": ";
            dogrula_sa(k.s.tamam && k.tetik && k.t_n > 0, on + "koşu tamamlandı ve ek katıldı " + k.s.hata);
            dogrula_sa(once != nullptr && !notr(*once), on + "b* − 1'de itki nötr değil (sınama anlamlı)");
            dogrula_sa(simdi != nullptr && notr(*simdi),
                       on + "W4-K26 (1): nötrleme yuva n + 1'in sınırındaki fizik adımında (b*)");
            if (ek == Ek::esz_guvenli) {
                dogrula_sa(k.s.sayac.yanit_iptal_esz >= 1u && k.s.sayac.esz_guvenli_uygulanan == 1u,
                           on + "A1-K78: eşzamansız çıktı uygulandı ve eski yanıtı iptal etti");
            } else {
                dogrula_sa(k.s.sayac.oturum_kopma >= 1u, on + "A1-K23: oturum bitti");
            }
        }
        const Kosu& k0 = kosular.front();
        bool ayni = true;
        for (const Kosu& k : kosular) {
            ayni = ayni && k.sim_kart == k0.sim_kart && k.s.sayac.yanit_iptal_esz == k0.s.sayac.yanit_iptal_esz &&
                   k.girdi == k0.girdi;
        }
        dogrula_sa(ayni, std::string(ad) + ": dört teslim biçiminde fiziğe giden girdi, A1-K78 iptali ve sim → kart "
                                           "baytları aynı");
    }
    return 0;
}

int test_sil_akis_sonu_belirlenim()
{
    std::vector<Kosu> kosular;
    for (const Teslim t : kTeslimler) {
        kosular.push_back(kos(t, Ek::log_fin, 4.0));
        const Kosu& k = kosular.back();
        const auto kp = kapanislar(k.giden);
        std::printf("  %s: tampon taşması %llu, oturum kopması %llu, bağlantı kopması %llu, kapanışlar %s, "
                    "karar dizisi %zu B, hata '%s'\n",
                    teslim_adi(t), static_cast<unsigned long long>(k.s.sayac.sil_tampon_tasma),
                    static_cast<unsigned long long>(k.s.sayac.oturum_kopma),
                    static_cast<unsigned long long>(k.s.baglanti_kopma), nedenler(kp).c_str(), k.giden.size(),
                    k.s.hata.c_str());
        const std::string on = std::string(teslim_adi(t)) + ": ";
        dogrula_sa(k.tetik, on + "ek katıldı");
        dogrula_sa(k.s.sayac.sil_tampon_tasma == 1u && std::count(kp.begin(), kp.end(), std::uint8_t{3}) == 1,
                   on + "W4-K26 (2): kuyruktaki LOG taşması FIN'den önce işlendi: SESSION_CLOSE(PROTOCOL)");
        dogrula_sa(k.s.tamam && k.s.baglanti_kopma == 1u,
                   on + "FIN bağlantı kaybı olarak kuyruk bitince işlendi; yeni bağlantıyla koşu tamamlandı");
    }
    const Kosu& k0 = kosular.front();
    bool ayni = true;
    for (const Kosu& k : kosular) {
        ayni = ayni && k.giden == k0.giden && k.s.hata == k0.s.hata && k.s.sayac.oturum_kopma == k0.s.sayac.oturum_kopma &&
               k.s.baglanti_kopma == k0.s.baglanti_kopma;
    }
    dogrula_sa(ayni, "dört teslim biçiminde aynı bitiş nedeni ve sim'in aynı karar dizisi (" +
                         std::to_string(k0.giden.size()) + " B)");
    return 0;
}

int test_sil_sifir_seli()
{
    const Kosu k = kos(Teslim::ayni_okuma, Ek::sifir_seli, 4.0);
    const auto kp = kapanislar(k.giden);
    std::printf("  sıfır seli: kuyruk taşması %llu, kuyruk en çok %llu B, kapanışlar %s, %.2f s, hata '%s'\n",
                static_cast<unsigned long long>(k.s.sil_kuyruk_tasma),
                static_cast<unsigned long long>(k.s.sil_kuyruk_en_cok), nedenler(kp).c_str(), k.sure_s,
                k.s.hata.c_str());
    dogrula_sa(k.tetik, "sel başladı");
    dogrula_sa(k.s.sil_kuyruk_tasma >= 1u && std::count(kp.begin(), kp.end(), std::uint8_t{3}) >= 1,
               "W4-K26 (3): oturum protokol hatasıyla bitti (SESSION_CLOSE(PROTOCOL))");
    dogrula_sa(k.s.sil_kuyruk_en_cok <= (std::uint64_t{1} << 26U) + 65536u, "alım kuyruğu sınırlı (≤ 64 MiB + bir okuma)");
    dogrula_sa(k.s.tamam, "koşu asılmadan sürdü: sel durunca yeni oturum kuruldu ve koşu tamamlandı");
    return 0;
}

}  // namespace

int sil_alim_testi(const std::string& ad, int, char**)
{
    if (ad == "sil_kisitlayan_alim_ani") {
        (void)test_sil_kisitlayan_alim_ani();
        return g_hata_sa;
    }
    if (ad == "sil_akis_sonu_belirlenim") {
        (void)test_sil_akis_sonu_belirlenim();
        return g_hata_sa;
    }
    if (ad == "sil_sifir_seli") {
        (void)test_sil_sifir_seli();
        return g_hata_sa;
    }
    return -1;   // bu dosyanın testi değil
}
