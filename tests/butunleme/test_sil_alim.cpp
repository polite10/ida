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
#include <functional>
#include <map>
#include <stdexcept>
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
enum class Ek { esz_guvenli, kart_kapanisi, log_fin, sifir_seli, sifir_act, sifir_fin };
constexpr std::int64_t kButce = std::int64_t{1} << 22U;   // sim'in bekleme tüketim sınırı (4 MiB)

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
    // Bütçe sınaması (Ek::sifir_act): ACTUATOR(n + 1)'in önüne konacak sıfır sayısı; göreli ise
    // kButce − (ACTUATOR boyu) + z.
    void sifir_ayarla(std::int64_t z, bool goreli)
    {
        sifir_ = z;
        sifir_goreli_ = goreli;
    }
    std::uint64_t sifir_sayisi() const { return sifir_sayisi_; }
    std::size_t act_boyu() const { return act_boyu_; }
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
        if (act_bekle_) {
            // Bütçe sınaması: ACTUATOR(n)'den sonra kartın öteki çerçeveleri atılır; ACTUATOR(n + 1)
            // gelince önüne Z sıfır konur (Z, çerçevenin boyuna göre: bütçeye göre konum kesin).
            if (c.msgid != MAVLINK_MSG_ID_IDA_ACTUATOR) {
                return;
            }
            act_bekle_ = false;
            const std::int64_t s0 = sifir_.load();
            const std::int64_t z = sifir_goreli_.load() ? kButce - static_cast<std::int64_t>(ham.size()) + s0 : s0;
            sifir_sayisi_ = static_cast<std::uint64_t>(z);
            act_boyu_ = ham.size();
            std::vector<std::uint8_t> sifirlar(static_cast<std::size_t>(z), 0);
            if (teslim_ == Teslim::ayni_okuma) {
                sifirlar.insert(sifirlar.end(), ham.begin(), ham.end());
                kuyruga(std::move(sifirlar), 0);
            } else {
                // Sıfırlar 64 KiB parçalarla, ACTUATOR ayrı ve 3 ms sonra.
                for (std::size_t i = 0; i < sifirlar.size(); i += 65536) {
                    kuyruga(std::vector<std::uint8_t>(
                                sifirlar.begin() + static_cast<std::ptrdiff_t>(i),
                                sifirlar.begin() + static_cast<std::ptrdiff_t>(std::min(sifirlar.size(), i + 65536))),
                            0);
                }
                kuyruga(std::move(ham), 3000);
            }
            return;
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
            if (ek_ == Ek::sifir_fin) {
                // Sol 3. denetim #1: ACTUATOR(n) → Z sıfır → FIN; son sıfırla FIN birlikte (aynı
                // parça) ya da FIN 5 ms sonra ayrı.
                std::vector<std::uint8_t> z(static_cast<std::size_t>(sifir_.load()), 0);
                sifir_sayisi_ = z.size();
                if (teslim_ == Teslim::ayni_okuma) {
                    ham.insert(ham.end(), z.begin(), z.end());
                    kuyruga(std::move(ham), 0, true);
                } else {
                    kuyruga(std::move(ham), 0);
                    kuyruga(std::move(z), 0);
                    kuyruga({}, 5000, true);
                }
                kesildi_ = true;
                sim_donuk_ = true;
                asama_ = "sıfır + FIN";
                return;
            }
            if (ek_ == Ek::sifir_act) {
                kuyruga(std::move(ham), 0);
                act_bekle_ = true;
                asama_ = "bütçe: ACTUATOR(n + 1) bekleniyor";
                return;
            }
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
    bool act_bekle_ = false;
    std::atomic<std::int64_t> sifir_{0};
    std::atomic<bool> sifir_goreli_{false};
    std::uint64_t sifir_sayisi_ = 0;
    std::size_t act_boyu_ = 0;
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
    std::uint64_t sifir_sayisi = 0;
    std::size_t act_boyu = 0;
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
Kosu kos(Teslim teslim, Ek ek, double sure_s, std::int64_t sifir = 0, bool goreli = false)
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
        ak->sifir_ayarla(sifir, goreli);
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
        k.sifir_sayisi = ak->sifir_sayisi();
        k.act_boyu = ak->act_boyu();
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

// W4-K26 (3), sol 2. denetim #3 (EMNİYET): bekleme tüketim sınırının kararı yalnız akıştaki bayt
// konumuna bağlıdır. Beklenen yuva boşken akış Z × 0x00 → ACTUATOR(n + 1): ACTUATOR'ın son baytı
// bütçenin (4 MiB) içindeyse geçerli, dışındaysa PROTOCOL; aynı okumada ya da ayrı teslimde aynı.
int test_sil_butce_konumu()
{
    struct Durum {
        const char* ad;
        std::int64_t z;
        bool goreli;
    };
    const Durum durumlar[] = {
        {"sınırda (son bayt = 4 MiB)", 0, true},
        {"sınırın bir bayt üstü", 1, true},
        {"4 194 304 sıfır", 4'194'304, false},
        {"4 194 305 sıfır", 4'194'305, false},
    };
    for (const Durum& d : durumlar) {
        std::vector<Kosu> kosular;
        for (const Teslim t : {Teslim::ayni_okuma, Teslim::ayri_gecikmeli}) {
            kosular.push_back(kos(t, Ek::sifir_act, 4.0, d.z, d.goreli));
            const Kosu& k = kosular.back();
            const auto kp = kapanislar(k.giden);
            const bool asim = k.sifir_sayisi + k.act_boyu > static_cast<std::uint64_t>(kButce);
            std::printf("  %s, %s: sıfır %llu + ACTUATOR %zu B, bütçe aşımı %d, tüketim sınırı %llu, kapanışlar %s, "
                        "karar dizisi %zu B\n",
                        d.ad, teslim_adi(t), static_cast<unsigned long long>(k.sifir_sayisi), k.act_boyu, asim ? 1 : 0,
                        static_cast<unsigned long long>(k.s.sil_kuyruk_tasma), nedenler(kp).c_str(), k.giden.size());
            const std::string on = std::string(d.ad) + ", " + teslim_adi(t) + ": ";
            dogrula_sa(k.tetik && k.act_boyu > 0u && k.s.tamam, on + "koşu tamamlandı " + k.s.hata);
            const auto protokol = static_cast<std::uint64_t>(std::count(kp.begin(), kp.end(), std::uint8_t{3}));
            dogrula_sa(k.s.sil_kuyruk_tasma == (asim ? 1u : 0u) && protokol == (asim ? 1u : 0u),
                       on + (asim ? "bütçe aşıldı: SESSION_CLOSE(PROTOCOL)" : "bütçe içinde: yuva tamam, PROTOCOL yok"));
        }
        dogrula_sa(kosular[0].giden == kosular[1].giden && kosular[0].girdi == kosular[1].girdi,
                   std::string(d.ad) + ": iki teslim biçiminde aynı karar, aynı sim çıktısı ve fizik girdisi");
    }
    return 0;
}

// Sol 3. denetim #1 (EMNİYET): akış ACTUATOR(n) → Z × 0x00 → FIN. Z = 4 194 304: bütçe tam son
// sıfırda biter → önce PROTOCOL, sonra bağlantı kaybı; Z = 4 194 303: bütçe içinde → PROTOCOL yok,
// bağlantı kaybı. Son sıfırla FIN aynı turda ya da ayrı görülse de aynı karar, aynı sim çıktısı.
int test_sil_eof_konumu()
{
    for (const std::int64_t z : {std::int64_t{4'194'304}, std::int64_t{4'194'303}}) {
        const bool asim = z >= kButce;
        std::vector<Kosu> kosular;
        for (const Teslim t : {Teslim::ayni_okuma, Teslim::ayri_gecikmeli}) {
            kosular.push_back(kos(t, Ek::sifir_fin, 4.0, z, false));
            const Kosu& k = kosular.back();
            const auto kp = kapanislar(k.giden);
            std::printf("  %lld sıfır + FIN, %s: tüketim sınırı %llu, bağlantı kopması %llu, kapanışlar %s, "
                        "karar dizisi %zu B\n",
                        static_cast<long long>(z), teslim_adi(t), static_cast<unsigned long long>(k.s.sil_kuyruk_tasma),
                        static_cast<unsigned long long>(k.s.baglanti_kopma), nedenler(kp).c_str(), k.giden.size());
            const std::string on = std::to_string(z) + " sıfır + FIN, " + teslim_adi(t) + ": ";
            dogrula_sa(k.tetik && k.s.tamam, on + "koşu (yeni bağlantıyla) tamamlandı " + k.s.hata);
            const auto protokol = static_cast<std::uint64_t>(std::count(kp.begin(), kp.end(), std::uint8_t{3}));
            dogrula_sa(k.s.sil_kuyruk_tasma == (asim ? 1u : 0u) && protokol == (asim ? 1u : 0u) &&
                           k.s.baglanti_kopma == 1u,
                       on + (asim ? "bütçe kararı EOF'tan önce: PROTOCOL, sonra bağlantı kaybı"
                                  : "bütçe içinde: PROTOCOL yok, bağlantı kaybı"));
        }
        dogrula_sa(kosular[0].giden == kosular[1].giden && kosular[0].girdi == kosular[1].girdi,
                   std::to_string(z) + " sıfır + FIN: iki teslim biçiminde aynı karar ve aynı sim çıktısı");
    }
    return 0;
}

// Hazırlık oturumunu (A1-K19) yavaşlatan aktarıcı: sim'in SESSION_CLOSE(RUN_END)'i görülene dek
// kartın her çerçevesi `gecikme` sonra teslim edilir (sıra korunur); `sonuc_yut` ise o süre boyunca
// kartın IDA_CMD_RESULT'ları atılır (hazırlık hiç bitmez). `kapat_tekrar` ise hazırlıkta sim'in
// komutları karta iletilmez (kart bloğu hiç kaydetmez, her el sıkışma yeniden hazırlık ister) ve
// kartın her ACTUATOR'ının ardından kart adına SESSION_CLOSE katılır (hazırlık oturumu her ilk
// TF'den sonra kapanır). Sim'in kart yönüne gönderdiği SESSION_CLOSE'lar ve kartın ondan sonraki
// IDA_CALL_TRACE'leri (oturum sonu çağrısının izi, A1-K95) kaydedilir.
class GeciktirenAktarici {
public:
    GeciktirenAktarici(std::uint16_t sim_port, std::chrono::microseconds gecikme, bool sonuc_yut,
                       bool kapat_tekrar = false)
        : sim_port_(sim_port), gecikme_(gecikme), sonuc_yut_(sonuc_yut), kapat_tekrar_(kapat_tekrar)
    {
        std::string hata;
        din_ = ida::kopru::tcp_dinle(0, hata);
        (void)ida_link_alici_kur(&alici_, IDA_LINK_ROL_SIM);
        (void)ida_link_alici_kur(&sim_alici_, IDA_LINK_ROL_KART);
        (void)ida_link_gonderici_kur(&g_, IDA_LINK_ROL_KART);
        g_.seq = 0x40;
        if (din_) {
            is_ = std::thread([this] { dongu(); });
        }
    }
    ~GeciktirenAktarici() { bekle(); }
    void bekle()
    {
        dur_ = true;
        if (is_.joinable()) {
            is_.join();
        }
    }
    std::uint16_t port() const { return din_ ? din_->port() : 0; }
    std::uint64_t geciktirilen() const { return geciktirilen_; }
    std::uint64_t yutulan() const { return yutulan_; }
    const std::vector<std::uint8_t>& sim_kapanislari() const { return sim_kapanislari_; }
    std::uint64_t kapanis_sonrasi_iz() const { return kapanis_sonrasi_iz_; }
    std::uint64_t katilan_kapanis() const { return katilan_kapanis_; }

private:
    using Saat = std::chrono::steady_clock;

    void dongu()
    {
        std::string hata;
        auto sim = ida::kopru::tcp_baglan(sim_port_, hata);
        auto kart = din_->kabul(60'000'000, hata);
        if (!sim || !kart) {
            return;
        }
        std::vector<std::uint8_t> t(65536);
        std::vector<std::uint8_t> ham;
        std::vector<std::uint8_t> sim_ham;
        std::deque<std::pair<std::vector<std::uint8_t>, Saat::time_point>> kuyruk;
        Saat::time_point son = Saat::now();
        bool hazirlikta = true;
        int bos = 0;
        bool sim_bitti = false;
        Saat::time_point bitis{};
        while (!dur_) {
            bool is_var = false;
            std::size_t n = 0;
            ida::kopru::OkumaSonucu r = sim_bitti ? ida::kopru::OkumaSonucu::zaman_asimi
                                                  : sim->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                // Sim kapattı: kartın kapanıştan sonraki çıktısı (oturum sonu izi) bir süre daha okunur.
                sim_bitti = true;
                bitis = Saat::now() + std::chrono::milliseconds(500);
                sim->kapat();
            }
            if (sim_bitti && Saat::now() > bitis) {
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                std::vector<std::uint8_t> ilet;
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
                    bool at = false;
                    if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
                        mavlink_ida_session_close_t s{};
                        (void)ida_link_coz(&c, c.msgid, &s, sizeof s);
                        sim_kapanislari_.push_back(s.reason);
                        kapanis_sonrasi_iz_ = 0;
                        if (s.reason == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END)) {
                            hazirlikta = false;
                        }
                    } else if (kapat_tekrar_ && hazirlikta &&
                               (c.msgid == MAVLINK_MSG_ID_IDA_CMD_PARAM_SET || c.msgid == MAVLINK_MSG_ID_IDA_CMD_SIMPLE)) {
                        at = true;   // kart bloğu kaydetmesin: her el sıkışma yeniden hazırlık ister
                    }
                    if (!at) {
                        ilet.insert(ilet.end(), sim_ham.begin(), sim_ham.end());
                    }
                    sim_ham.clear();
                }
                if (!ilet.empty() && !kart->yaz(ilet.data(), ilet.size())) {
                    break;
                }
            }
            r = kart->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
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
                    if (c.msgid == MAVLINK_MSG_ID_IDA_CALL_TRACE && !sim_kapanislari_.empty()) {
                        ++kapanis_sonrasi_iz_;
                    }
                    if (hazirlikta && sonuc_yut_ && c.msgid == MAVLINK_MSG_ID_IDA_CMD_RESULT) {
                        ++yutulan_;
                    } else {
                        Saat::time_point b = Saat::now();
                        if (hazirlikta) {
                            b += gecikme_;
                            ++geciktirilen_;
                        }
                        son = std::max(son, b);
                        const bool act = c.msgid == MAVLINK_MSG_ID_IDA_ACTUATOR;
                        std::uint16_t sess = 0;
                        if (act) {
                            mavlink_ida_actuator_t a{};
                            (void)ida_link_coz(&c, c.msgid, &a, sizeof a);
                            sess = a.sess;
                        }
                        kuyruk.emplace_back(std::move(ham), son);
                        if (act && kapat_tekrar_ && hazirlikta) {
                            mavlink_ida_session_close_t s{};
                            s.sess = sess;
                            s.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SAFE_STATE);
                            kuyruk.emplace_back(kodla(g_, MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &s, sizeof s), son);
                            ++katilan_kapanis_;
                        }
                    }
                    ham.clear();
                }
            }
            const auto simdi = Saat::now();
            while (!kuyruk.empty() && kuyruk.front().second <= simdi) {
                if (!sim_bitti && !sim->yaz(kuyruk.front().first.data(), kuyruk.front().first.size())) {
                    return;
                }
                kuyruk.pop_front();
                is_var = true;
            }
            if (is_var) {
                bos = 0;
            } else if (++bos < 2000) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    std::uint16_t sim_port_;
    std::chrono::microseconds gecikme_;
    bool sonuc_yut_;
    bool kapat_tekrar_;
    std::unique_ptr<ida::kopru::TcpDinleyici> din_;
    ida_link_alici_t alici_{};
    ida_link_alici_t sim_alici_{};
    ida_link_gonderici_t g_{};
    std::thread is_;
    std::atomic<bool> dur_{false};
    std::uint64_t geciktirilen_ = 0;
    std::uint64_t yutulan_ = 0;
    std::vector<std::uint8_t> sim_kapanislari_;
    std::uint64_t kapanis_sonrasi_iz_ = 0;
    std::uint64_t katilan_kapanis_ = 0;
};

struct HazirlikKosusu {
    ida::sunucu::Sonuc s;
    double sure_s = 0.0;
    std::uint64_t geciktirilen = 0;
    std::uint64_t yutulan = 0;
    std::vector<std::uint8_t> sim_kapanislari;
    std::uint64_t kapanis_sonrasi_iz = 0;
    std::uint64_t katilan_kapanis = 0;
};

HazirlikKosusu hazirlik_kos(std::int64_t baglanti_bekleme_ns, std::int64_t hazirlik_azami_ns, bool sonuc_yut,
                            std::int64_t gecikme_us = 25'000, bool kapat_tekrar = false, double sure_s = 1.0,
                            std::function<void(const ida::sunucu::Ayar::SinirGozlemi&)> kanca = {})
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = sure_s;
    a.sinir_kancasi = std::move(kanca);
    a.karsi = ida::sunucu::Karsi::tcp;
    a.baglanti_bekleme_ns = baglanti_bekleme_ns;
    a.hazirlik_azami_ns = hazirlik_azami_ns;
    std::unique_ptr<GeciktirenAktarici> ak;
    std::unique_ptr<ida::sil::Kosucu> kosucu;
    std::thread is;
    a.tcp_hazir = [&](std::uint16_t port, std::uint64_t tohum) {
        ak = std::make_unique<GeciktirenAktarici>(port, std::chrono::microseconds(gecikme_us), sonuc_yut, kapat_tekrar);
        ida::sil::Ayarlar ka;
        ka.a1_port = ak->port();
        ka.a1_tohum = tohum;
        ka.gunluk = 0;
        kosucu = std::make_unique<ida::sil::Kosucu>(ka);
        is = std::thread([&] { (void)kosucu->calistir(); });
    };
    HazirlikKosusu h;
    const auto bas = std::chrono::steady_clock::now();
    h.s = ida::sunucu::kos(a);
    h.sure_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    if (kosucu) {
        kosucu->durdur();
    }
    if (is.joinable()) {
        is.join();
    }
    if (ak) {
        ak->bekle();
        h.geciktirilen = ak->geciktirilen();
        h.yutulan = ak->yutulan();
        h.sim_kapanislari = ak->sim_kapanislari();
        h.kapanis_sonrasi_iz = ak->kapanis_sonrasi_iz();
        h.katilan_kapanis = ak->katilan_kapanis();
    }
    return h;
}

// Sol 3. denetim #2 (EMNİYET): hazırlık oturumu her ilk TF'den sonra kapanır ve her el sıkışma
// yeniden hazırlık ister. Yeniden kurma özyinelemesizdir (oturum_kur derinliği 1), deneme sayısı
// sınırlıdır (5; tahmin) ve koşu sınırlı sürede hatayla döner. Karar deneme sayısıyla verilir
// (duvar saatine bağlı değil; mutlak süre sınırı 600 s bu sınamada devreye girmez).
int test_sil_hazirlik_tekrar()
{
    const HazirlikKosusu h = hazirlik_kos(20'000'000'000, 600'000'000'000, false, 0, true);
    std::printf("  tekrarlayan hazırlık: tamam %d, hazırlık %llu, katılan kapanış %llu, derinlik en çok %llu, "
                "%.2f s, hata '%s'\n",
                h.s.tamam ? 1 : 0, static_cast<unsigned long long>(h.s.hazirlik_oturumu),
                static_cast<unsigned long long>(h.katilan_kapanis),
                static_cast<unsigned long long>(h.s.oturum_kur_derinlik_en_cok), h.sure_s, h.s.hata.c_str());
    dogrula_sa(!h.s.tamam && h.s.hata == "hazırlık oturumu 5 denemede tamamlanamadı",
               "deneme sınırında hatayla döndü: '" + h.s.hata + "'");
    dogrula_sa(h.s.hazirlik_oturumu == 5u && h.katilan_kapanis >= 5u, "beş hazırlık denemesi, her biri kart kapanışıyla bitti");
    dogrula_sa(h.s.oturum_kur_derinlik_en_cok == 1u, "oturum_kur özyinelemesiz (derinlik 1)");
    dogrula_sa(h.sure_s < 60.0, "asılma yok (" + std::to_string(h.sure_s) + " s)");
    return 0;
}

// Sol 3. denetim #3 (EMNİYET, A1-K21): kurulmuş oturumun hata çıkışlarında kart SESSION_CLOSE'u
// görür ve çekirdeğin oturum sonu çağrısı (izi: IDA_CALL_TRACE) yapılır.
int test_sil_hata_kapanisi()
{
    struct Durum {
        const char* ad;
        std::int64_t azami;
        std::int64_t gecikme_us;
        const char* hata;
    };
    const Durum durumlar[] = {
        {"hazırlık süre sınırı", 1'000'000'000, 25'000, "hazırlık oturumu süre sınırını aştı"},
        {"hazırlık 8000 periyotta bitmedi", 600'000'000'000, 0,
         "hazırlık oturumu tamamlanmadı (param_blob_crc eşleşmedi)"},
    };
    for (const Durum& d : durumlar) {
        const HazirlikKosusu h = hazirlik_kos(20'000'000'000, d.azami, true, d.gecikme_us);
        std::printf("  %s: tamam %d, hata '%s', sim'in kapanışları %s, kapanıştan sonra kartın izi %llu, "
                    "kapanış yazımı eksik %llu, %.2f s\n",
                    d.ad, h.s.tamam ? 1 : 0, h.s.hata.c_str(), nedenler(h.sim_kapanislari).c_str(),
                    static_cast<unsigned long long>(h.kapanis_sonrasi_iz),
                    static_cast<unsigned long long>(h.s.kapanis_yazim_eksik), h.sure_s);
        const std::string on = std::string(d.ad) + ": ";
        dogrula_sa(!h.s.tamam && h.s.hata == d.hata, on + "beklenen hata '" + h.s.hata + "'");
        dogrula_sa(!h.sim_kapanislari.empty() &&
                       h.sim_kapanislari.back() == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN) &&
                       h.s.kapanis_yazim_eksik == 0u,
                   on + "kart SESSION_CLOSE(SHUTDOWN)'u gördü (tamamen yazıldı)");
        dogrula_sa(h.kapanis_sonrasi_iz >= 1u, on + "kartın oturum sonu çağrısı yapıldı (IDA_CALL_TRACE)");
        dogrula_sa(h.s.kapanis_daraltildi && !h.s.kapanis_izin_acik && !h.s.kapanis_itki_yururlukte,
                   on + "sol 4. denetim #3: kapanışta izinler kapalı, itki nötr");
    }
    return 0;
}

// Sol 4. denetim #1, #3 (EMNİYET): oturum açıkken istisna (sınır kancasından enjekte) kos()'tan
// çıkmaz; koruyucu ortak kapanışı yapar (kart SESSION_CLOSE'u ve oturum sonu izini görür), yerel
// daraltma uygulanır, kos() hata döner. Olağan bitişte de daraltma: izin kapalı, itki nötr.
int test_sil_istisna_kapanisi()
{
    const auto kanca = [](const ida::sunucu::Ayar::SinirGozlemi& g) {
        if (g.b == 3600u) {   // 4,5 s: Z1'de itki yürürlükte
            throw std::runtime_error("enjekte hata");
        }
    };
    const HazirlikKosusu h = hazirlik_kos(20'000'000'000, 600'000'000'000, false, 0, false, 5.0, kanca);
    std::printf("  istisna: tamam %d, hata '%s', sim'in kapanışları %s, kapanıştan sonra kartın izi %llu, "
                "daraltma %d (önce itki %d, sonra izin %d, itki %d), yutulan istisna %llu\n",
                h.s.tamam ? 1 : 0, h.s.hata.c_str(), nedenler(h.sim_kapanislari).c_str(),
                static_cast<unsigned long long>(h.kapanis_sonrasi_iz), h.s.kapanis_daraltildi ? 1 : 0,
                h.s.kapanis_oncesi_itki ? 1 : 0, h.s.kapanis_izin_acik ? 1 : 0, h.s.kapanis_itki_yururlukte ? 1 : 0,
                static_cast<unsigned long long>(h.s.kapanis_istisna));
    dogrula_sa(!h.s.tamam && h.s.hata == "istisna: enjekte hata", "kos() istisnayı hataya çevirdi: '" + h.s.hata + "'");
    dogrula_sa(!h.sim_kapanislari.empty() &&
                   h.sim_kapanislari.back() == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN) &&
                   h.kapanis_sonrasi_iz >= 1u && h.s.kapanis_istisna == 0u,
               "kart SESSION_CLOSE(SHUTDOWN)'u gördü ve oturum sonu çağrısı yapıldı");
    dogrula_sa(h.s.kapanis_daraltildi && h.s.kapanis_oncesi_itki && !h.s.kapanis_izin_acik &&
                   !h.s.kapanis_itki_yururlukte,
               "istisna çıkışında daraltma: itki yürürlükteydi, sonra nötr; izinler kapalı");
    const HazirlikKosusu o = hazirlik_kos(20'000'000'000, 600'000'000'000, false, 0, false, 5.0);
    dogrula_sa(o.s.tamam && o.s.kapanis_daraltildi && o.s.kapanis_oncesi_itki && !o.s.kapanis_izin_acik &&
                   !o.s.kapanis_itki_yururlukte,
               "olağan bitişte daraltma: itki yürürlükteydi, sonra nötr; izinler kapalı " + o.s.hata);
    return 0;
}

// Sol 4. denetim #2 (EMNİYET): karşı uç her bağlantıda KOSUYOR olmadan kapatıp yeniden bağlanır.
// Kurma işleminin deneme sınırı (6) aşılınca koşu sınırlı sürede hatayla biter (deneme sayısıyla
// belirlenimci; ilk kurulum ve koşu ortası aynı işlev).
int test_sil_kurma_siniri()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 1.0;
    a.karsi = ida::sunucu::Karsi::tcp;
    a.kurma_deneme_azami = 6;
    std::atomic<bool> dur{false};
    std::atomic<int> baglanti{0};
    std::thread is;
    a.tcp_hazir = [&](std::uint16_t port, std::uint64_t) {
        is = std::thread([&dur, &baglanti, port] {
            std::vector<std::uint8_t> t(4096);
            while (!dur) {
                std::string hata;
                auto u = ida::kopru::tcp_baglan(port, hata);
                if (!u) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                ++baglanti;
                std::size_t n = 0;
                (void)u->oku(t.data(), t.size(), 20'000, n);   // SIM_HELLO'yu oku, yanıtlama
                u->kapat_duzgun(50'000);                      // FIN
            }
        });
    };
    const auto bas = std::chrono::steady_clock::now();
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const double sure = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    dur = true;
    if (is.joinable()) {
        is.join();
    }
    std::printf("  FIN/yeniden bağlanma: tamam %d, hata '%s', yeniden bağlantı %llu, karşı uç bağlantısı %d, %.2f s\n",
                s.tamam ? 1 : 0, s.hata.c_str(), static_cast<unsigned long long>(s.yeniden_baglanti),
                baglanti.load(), sure);
    dogrula_sa(!s.tamam && s.hata == "oturum kurulamadı (6 deneme)", "deneme sınırında hata: '" + s.hata + "'");
    dogrula_sa(s.yeniden_baglanti <= 6u, "sınırlı yeniden bağlantı");
    dogrula_sa(sure < 60.0, "asılma yok (" + std::to_string(sure) + " s)");
    return 0;
}

// W4-K26 mimar kararı: bağlantı/el sıkışma süre sınırı hazırlık oturumunun süresini kapsamaz;
// hazırlığın kendi sınırı ayrıdır ve asılma yoktur.
int test_sil_hazirlik_suresi()
{
    // (1) Hazırlık baglanti_bekleme_ns'den (500 ms) uzun sürer (kartın her çerçevesi 25 ms gecikir).
    const HazirlikKosusu h1 = hazirlik_kos(500'000'000, 600'000'000'000, false);
    std::printf("  yavaş hazırlık: tamam %d, hazırlık %llu, kurulan oturum %llu, geciktirilen %llu, %.2f s, hata '%s'\n",
                h1.s.tamam ? 1 : 0, static_cast<unsigned long long>(h1.s.hazirlik_oturumu),
                static_cast<unsigned long long>(h1.s.sayac.oturum_kurulan),
                static_cast<unsigned long long>(h1.geciktirilen), h1.sure_s, h1.s.hata.c_str());
    dogrula_sa(h1.s.hazirlik_oturumu == 1u && h1.geciktirilen > 0u, "hazırlık oturumu geciktirildi");
    dogrula_sa(h1.sure_s > 0.5, "hazırlık baglanti_bekleme_ns'den uzun sürdü (" + std::to_string(h1.sure_s) + " s)");
    dogrula_sa(h1.s.tamam && h1.s.sayac.oturum_kurulan >= 2u,
               "çalıştırma oturumu yine kuruldu ve koşu tamamlandı " + h1.s.hata);
    // (2) Hazırlık hiç bitmez (komut sonuçları yutulur): sınırlı sürede hata.
    const HazirlikKosusu h2 = hazirlik_kos(500'000'000, 1'000'000'000, true);
    std::printf("  bitmeyen hazırlık: tamam %d, yutulan %llu, %.2f s, hata '%s'\n", h2.s.tamam ? 1 : 0,
                static_cast<unsigned long long>(h2.yutulan), h2.sure_s, h2.s.hata.c_str());
    dogrula_sa(!h2.s.tamam && h2.s.hata == "hazırlık oturumu süre sınırını aştı",
               "bitmeyen hazırlık süre sınırıyla hata döndü: '" + h2.s.hata + "'");
    dogrula_sa(h2.sure_s < 60.0, "asılma yok (" + std::to_string(h2.sure_s) + " s)");
    return 0;
}

}  // namespace

int sil_alim_testi(const std::string& ad, int, char**)
{
    if (ad == "sil_butce_konumu") {
        (void)test_sil_butce_konumu();
        return g_hata_sa;
    }
    if (ad == "sil_istisna_kapanisi") {
        (void)test_sil_istisna_kapanisi();
        return g_hata_sa;
    }
    if (ad == "sil_kurma_siniri") {
        (void)test_sil_kurma_siniri();
        return g_hata_sa;
    }
    if (ad == "sil_hazirlik_tekrar") {
        (void)test_sil_hazirlik_tekrar();
        return g_hata_sa;
    }
    if (ad == "sil_hata_kapanisi") {
        (void)test_sil_hata_kapanisi();
        return g_hata_sa;
    }
    if (ad == "sil_eof_konumu") {
        (void)test_sil_eof_konumu();
        return g_hata_sa;
    }
    if (ad == "sil_hazirlik_suresi") {
        (void)test_sil_hazirlik_suresi();
        return g_hata_sa;
    }
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
