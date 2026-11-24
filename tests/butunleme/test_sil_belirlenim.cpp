// SIL belirlenimciliği, oturum düşmesiyle (A1-K102, K121; W4-K14): kart → sim akışına koşu
// ortasında protokol ihlali katan belirlenimci bir "kart" (ida_sil koşucusu + araya giren
// aktarıcı) ile aynı tohumda N koşunun iki yöndeki bayt akışı bayt bayt aynı olmalıdır.
//
// Aktarıcı, kartın belirli (k.) IDA_ACTUATOR'ından hemen sonra 150 geçerli IDA_LOG çerçevesi katar
// (W4-K14 tamponu 128'i aşar: sim oturumu SESSION_CLOSE(PROTOCOL) ile bitirir ve yeniden el
// sıkışır). Katılan çerçeveler ve kartın ACTUATOR sonrası bütün çıktıları rastgele (gerçek
// rastgelelik) gecikmeyle teslim edilir (partiye göre 0–1,5 ms pencereden); sıra korunur. Geliş zamanı her koşuda farklı
// olduğundan oturumun bitiş anı, kapanış çerçevesi ve yeniden başlama anı geliş zamanına
// bağlıysa akış koşudan koşuya değişir. Koşuların yarısı arka planda 6 meşgul döngüyle koşar.
// W4-K25 (5): aktarıcı sim'in SESSION_CLOSE(SHUTDOWN)'ını görünce karta yazdırır gibi 200
// çerçeve daha yollar; sim düzgün kapatmalıdır (yazım hata almaz, son çerçeve karşıya ulaşır).
//
//   sil_oturum_dusme_belirlenim [n]   n koşu yüksüz + n koşu yük altında (varsayılan 10 + 10)
#include "ida/kopru/tasima.hpp"
#include "ida/sil/kosucu.hpp"
#include "ida_link/tel.h"
#include "ida/sunucu/sunucu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_hata_sb = 0;

void dogrula_sb(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_sb;
    }
}

constexpr std::uint32_t kMsgSimHello = 46000;
constexpr std::uint32_t kMsgBoardHello = 46001;
constexpr std::uint32_t kMsgSessionClose = 46004;
constexpr std::uint32_t kMsgTimeFrame = 46010;
constexpr std::uint32_t kMsgActuator = 46060;
constexpr std::uint32_t kMsgLog = 46066;
constexpr std::uint8_t kCloseProtocol = 3;
constexpr std::uint8_t kCloseShutdown = 1;

// Sim ile ida_sil koşucusu arasına giren aktarıcı: iki yönü kaydeder, kart → sim yönüne
// belirlenimci ihlal katar ve ACTUATOR sonrası çerçeveleri rastgele gecikmeyle teslim eder.
class IhlalAktaricisi {
public:
    IhlalAktaricisi(std::uint16_t sim_port, std::vector<std::uint64_t> enjekte_act)
        : enjekte_(std::move(enjekte_act))
    {
        std::string hata;
        din_ = ida::kopru::tcp_dinle(0, hata);
        sim_port_ = sim_port;
        (void)ida_link_alici_kur(&alici_, IDA_LINK_ROL_SIM);
        (void)ida_link_alici_kur(&sim_alici_, IDA_LINK_ROL_KART);
        (void)ida_link_gonderici_kur(&g_, IDA_LINK_ROL_KART);
        g_.seq = 0x80;
        if (din_) {
            is_ = std::thread([this] { dongu(); });
        }
    }
    ~IhlalAktaricisi() { bitir(); }
    void bitir()
    {
        dur_ = true;
        if (is_.joinable()) {
            is_.join();
        }
    }
    // Sim bağlantıyı kapattıktan sonra kartın kalan çıktısı boşalınca kendiliğinden biter.
    void bekle()
    {
        if (is_.joinable()) {
            is_.join();
        }
    }
    std::uint16_t port() const { return din_ ? din_->port() : 0; }
    const std::vector<std::uint8_t>& sim_kart() const { return sim_kart_; }
    const std::vector<std::uint8_t>& kart_sim() const { return kart_sim_; }
    std::uint64_t enjekte_edilen() const { return enjekte_edilen_; }
    std::uint64_t sim_yazma_hatasi() const { return sim_yazma_hatasi_; }
    bool sim_eof() const { return sim_eof_; }

private:
    using Saat = std::chrono::steady_clock;
    struct Parca {
        std::vector<std::uint8_t> bayt;
        Saat::time_point birak;
    };

    void kuyruga(std::vector<std::uint8_t>&& b, bool geciktir)
    {
        kart_sim_.insert(kart_sim_.end(), b.begin(), b.end());
        Saat::time_point t = Saat::now();
        if (geciktir && (rng_() & 1u) != 0u) {
            t += std::chrono::microseconds(static_cast<std::int64_t>(rng_() % gecikme_us_));
        }
        son_birak_ = std::max(son_birak_, t);
        kuyruk_.push_back({std::move(b), son_birak_});
    }

    // Sim'in SESSION_CLOSE(SHUTDOWN)'ı görülünce kart çıktısını sürdürür (gerçek kart son
    // çağrının çıktısını yazarken sim kapanabilir): 200 IDA_LOG, gecikmesiz. Sim okunmamış
    // veriyle kapatırsa (RST) bu yazımlar hata alır (W4-K25 (5)).
    void kapanis_ara(const std::uint8_t* p, std::size_t n)
    {
        std::size_t k = 0;
        while (k < n) {
            std::size_t tuk = 0;
            ida_link_cerceve_t c{};
            const ida_link_rc_t rc = ida_link_al(&sim_alici_, p + k, n - k, &tuk, &c);
            k += tuk;
            if (rc != IDA_LINK_CERCEVE) {
                break;
            }
            if (c.msgid == kMsgSessionClose && c.yuk[2] == kCloseShutdown) {
                log_kat(200, false);
            }
        }
    }

    void enjekte_et() { log_kat(150, true); }

    void log_kat(int adet, bool geciktir)
    {
        ida_link_mesaj_bilgisi_t mb{};
        if (ida_link_mesaj_bilgisi(kMsgLog, &mb) != IDA_LINK_TAMAM) {
            return;
        }
        std::vector<std::uint8_t> yuk(mb.yapi_boyu, 0);
        for (int k = 0; k < adet; ++k) {
            yuk[0] = static_cast<std::uint8_t>(k + 1);   // kırpılmasın diye sıfırdan farklı ilk bayt
            std::uint8_t b[IDA_LINK_CERCEVE_AZAMI];
            std::size_t n = 0;
            if (ida_link_gonder(&g_, kMsgLog, yuk.data(), yuk.size(), b, sizeof b, &n) == IDA_LINK_TAMAM) {
                kuyruga(std::vector<std::uint8_t>(b, b + n), geciktir);
                enjekte_edilen_ += geciktir ? 1u : 0u;
            }
        }
    }

    void dongu()
    {
        std::string hata;
        auto sim = ida::kopru::tcp_baglan(sim_port_, hata);
        auto kart = din_->kabul(20'000'000, hata);
        if (!sim || !kart) {
            return;
        }
        std::vector<std::uint8_t> t(65536);
        std::vector<std::uint8_t> ham;
        bool act_sonrasi = false;
        bool sim_kapandi = false;
        Saat::time_point sessiz = Saat::now();
        while (!dur_) {
            bool is_var = false;
            std::size_t n = 0;
            if (!sim_kapandi) {
                const ida::kopru::OkumaSonucu r = sim->oku(t.data(), t.size(), 0, n);
                if (r == ida::kopru::OkumaSonucu::kapandi) {
                    sim_kapandi = true;
                    sim_eof_ = true;   // sim kapattı ve okunacak bayt kalmadı (yazım hatasız)
                    sessiz = Saat::now();
                } else if (r == ida::kopru::OkumaSonucu::tamam) {
                    is_var = true;
                    sim_kart_.insert(sim_kart_.end(), t.data(), t.data() + n);
                    kapanis_ara(t.data(), n);
                    if (!kart->yaz(t.data(), n)) {
                        break;
                    }
                }
            }
            const ida::kopru::OkumaSonucu r = kart->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                sessiz = Saat::now();
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
                    kuyruga(std::move(ham), act_sonrasi);
                    ham.clear();
                    if (c.msgid == kMsgActuator) {
                        act_sonrasi = true;
                        ++act_;
                        // Partiden partiye değişen pencere: sim'in fizik adımlarıyla (onlarca µs)
                        // ve yuva beklemesiyle (bir sonraki ACTUATOR'a dek) çakışır.
                        static constexpr std::uint32_t kPencere[] = {1u, 15u, 30u, 60u, 120u, 250u, 1500u};
                        gecikme_us_ = kPencere[rng_() % (sizeof kPencere / sizeof kPencere[0])];
                        if (std::find(enjekte_.begin(), enjekte_.end(), act_) != enjekte_.end()) {
                            enjekte_et();
                        }
                    }
                }
            }
            const auto simdi = Saat::now();
            while (!kuyruk_.empty() && kuyruk_.front().birak <= simdi) {
                if (!sim_kapandi && !sim->yaz(kuyruk_.front().bayt.data(), kuyruk_.front().bayt.size())) {
                    // Sim okunmamış veriyle kapattı (RST): taşıma soketi kapatır, sim'in son
                    // baytları okunmadan kalabilir (W4-K25 (5)).
                    sim_kapandi = true;
                    ++sim_yazma_hatasi_;
                }
                kuyruk_.pop_front();
                is_var = true;
            }
            if (sim_kapandi && kuyruk_.empty() && simdi - sessiz > std::chrono::milliseconds(300)) {
                break;
            }
            if (!is_var) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    std::vector<std::uint64_t> enjekte_;
    std::unique_ptr<ida::kopru::TcpDinleyici> din_;
    std::uint16_t sim_port_ = 0;
    ida_link_alici_t alici_{};
    ida_link_alici_t sim_alici_{};   // sim → kart yönü (kapanış çerçevesini görmek için)
    ida_link_gonderici_t g_{};
    std::thread is_;
    std::atomic<bool> dur_{false};
    std::mt19937 rng_{std::random_device{}()};
    std::uint32_t gecikme_us_ = 1;   // bu partinin en büyük gecikmesi (+1), µs
    std::deque<Parca> kuyruk_;
    Saat::time_point son_birak_ = Saat::now();
    std::vector<std::uint8_t> sim_kart_;
    std::vector<std::uint8_t> kart_sim_;
    std::uint64_t act_ = 0;
    std::uint64_t enjekte_edilen_ = 0;
    std::uint64_t sim_yazma_hatasi_ = 0;
    bool sim_eof_ = false;
};

struct KosuSonucu {
    ida::sunucu::Sonuc s;
    std::vector<std::uint8_t> sim_kart;
    std::vector<std::uint8_t> kart_sim;
    std::uint64_t enjekte = 0;
    std::uint64_t yazma_hatasi = 0;
    bool eof = false;
};

KosuSonucu ihlalli_kos(const ida::sunucu::Ayar& a, const std::vector<std::uint64_t>& enjekte)
{
    ida::sunucu::Ayar b = a;
    b.karsi = ida::sunucu::Karsi::tcp;
    std::unique_ptr<IhlalAktaricisi> ak;
    std::unique_ptr<ida::sil::Kosucu> kosucu;
    std::thread is;
    b.tcp_hazir = [&](std::uint16_t port, std::uint64_t tohum) {
        ak = std::make_unique<IhlalAktaricisi>(port, enjekte);
        ida::sil::Ayarlar ka;
        ka.a1_port = ak->port();
        ka.a1_tohum = tohum;
        ka.gunluk = 0;
        kosucu = std::make_unique<ida::sil::Kosucu>(ka);
        is = std::thread([&] { (void)kosucu->calistir(); });
    };
    KosuSonucu k;
    k.s = ida::sunucu::kos(b);
    if (ak) {
        ak->bekle();
    }
    if (kosucu) {
        kosucu->durdur();
    }
    if (is.joinable()) {
        is.join();
    }
    if (ak) {
        k.sim_kart = ak->sim_kart();
        k.kart_sim = ak->kart_sim();
        k.enjekte = ak->enjekte_edilen();
        k.yazma_hatasi = ak->sim_yazma_hatasi();
        k.eof = ak->sim_eof();
    }
    return k;
}

// Akıştaki çerçeveler: (msgid, ilk 8 yük baytı) — tanı için.
struct Cerceve {
    std::uint32_t msgid;
    std::size_t konum;
    std::uint8_t yuk[8];
};

std::vector<Cerceve> cerceveler(const std::vector<std::uint8_t>& v, ida_link_rol_t rol)
{
    std::vector<Cerceve> c;
    ida_link_alici_t al{};
    (void)ida_link_alici_kur(&al, rol);
    std::size_t k = 0;
    while (k < v.size()) {
        std::size_t tuk = 0;
        ida_link_cerceve_t f{};
        const ida_link_rc_t rc = ida_link_al(&al, v.data() + k, v.size() - k, &tuk, &f);
        k += tuk;
        if (rc != IDA_LINK_CERCEVE) {
            break;
        }
        Cerceve x{f.msgid, k, {}};
        std::memcpy(x.yuk, f.yuk, sizeof x.yuk);
        c.push_back(x);
    }
    return c;
}

std::int64_t tf_t(const Cerceve& c)
{
    std::int64_t t = 0;
    std::memcpy(&t, c.yuk, sizeof t);   // t_sim_ns yükün ilk alanı (en büyük tür)
    return t;
}

// sim → kart akışının oturum özeti: her SESSION_CLOSE'tan önceki son TF ve sonraki ilk TF.
std::string ozet(const std::vector<std::uint8_t>& sim_kart)
{
    const std::vector<Cerceve> c = cerceveler(sim_kart, IDA_LINK_ROL_KART);
    std::string o;
    std::int64_t son_tf = -1;
    bool kapanis_sonrasi = false;
    bool hello_sonrasi = false;
    for (const Cerceve& x : c) {
        if (x.msgid == kMsgTimeFrame) {
            if (kapanis_sonrasi || hello_sonrasi) {
                o += " ilk TF " + std::to_string(tf_t(x) / 1'000'000) + " ms;";
                kapanis_sonrasi = false;
                hello_sonrasi = false;
            }
            son_tf = tf_t(x);
        } else if (x.msgid == kMsgSessionClose) {
            o += " CLOSE(" + std::to_string(x.yuk[2]) + ") son TF " + std::to_string(son_tf / 1'000'000) + " ms;";
            kapanis_sonrasi = true;
        } else if (x.msgid == kMsgSimHello && son_tf >= 0 && !kapanis_sonrasi && !hello_sonrasi) {
            o += " HELLO (kapanışsız) son TF " + std::to_string(son_tf / 1'000'000) + " ms;";
            hello_sonrasi = true;
        }
    }
    return o;
}

std::size_t ilk_fark_konumu(const std::vector<std::uint8_t>& x, const std::vector<std::uint8_t>& y)
{
    std::size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) {
        ++i;
    }
    return i;
}

int test_sil_oturum_dusme_belirlenim(int n)
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 3.0;
    const std::vector<std::uint64_t> enjekte{100u, 180u, 260u, 340u, 420u, 500u};
    std::vector<KosuSonucu> kosular;
    std::atomic<bool> yuk_dur{false};
    std::vector<std::thread> yuk;
    for (int r = 0; r < 2 * n; ++r) {
        if (r == n) {
            for (int j = 0; j < 6; ++j) {   // arka planda 6 meşgul döngü
                yuk.emplace_back([&yuk_dur] {
                    volatile std::uint64_t x = 0;
                    while (!yuk_dur.load(std::memory_order_relaxed)) {
                        x = x + 1;
                    }
                });
            }
        }
        kosular.push_back(ihlalli_kos(a, enjekte));
        const KosuSonucu& k = kosular.back();
        std::printf("koşu %2d%s: tamam=%d sim→kart %zu B, kart→sim %zu B, katılan %llu, tampon taşması %llu,"
                    " oturum kopması %llu;%s\n",
                    r, r >= n ? " (yük)" : "", k.s.tamam ? 1 : 0, k.sim_kart.size(), k.kart_sim.size(),
                    static_cast<unsigned long long>(k.enjekte),
                    static_cast<unsigned long long>(k.s.sayac.sil_tampon_tasma),
                    static_cast<unsigned long long>(k.s.sayac.oturum_kopma), ozet(k.sim_kart).c_str());
    }
    yuk_dur = true;
    for (std::thread& t : yuk) {
        t.join();
    }
    const KosuSonucu& k0 = kosular.front();
    // Her koşu: iki ihlal, iki PROTOCOL kapanışı; her el sıkışma tek SIM_HELLO (A1-K121 (iv)).
    int ayni_sk = 0;
    int ayni_ks = 0;
    for (std::size_t r = 0; r < kosular.size(); ++r) {
        const KosuSonucu& k = kosular[r];
        const std::vector<Cerceve> sk = cerceveler(k.sim_kart, IDA_LINK_ROL_KART);
        const std::vector<Cerceve> ks = cerceveler(k.kart_sim, IDA_LINK_ROL_SIM);
        int kapanis_p = 0;
        int hello = 0;
        int board_hello = 0;
        for (const Cerceve& x : sk) {
            kapanis_p += x.msgid == kMsgSessionClose && x.yuk[2] == kCloseProtocol ? 1 : 0;
            hello += x.msgid == kMsgSimHello ? 1 : 0;
        }
        for (const Cerceve& x : ks) {
            board_hello += x.msgid == kMsgBoardHello ? 1 : 0;
        }
        const std::string ad = "koşu " + std::to_string(r) + ": ";
        dogrula_sb(k.s.tamam, ad + "tamamlandı " + k.s.hata);
        dogrula_sb(k.enjekte == 150u * enjekte.size() && k.s.sayac.sil_tampon_tasma == enjekte.size(),
                   ad + "altı ihlal katıldı ve hepsi tampon taşması olarak algılandı");
        dogrula_sb(kapanis_p == static_cast<int>(enjekte.size()), ad + "her ihlalde SESSION_CLOSE(PROTOCOL) gönderildi (" + std::to_string(kapanis_p) + ")");
        // W4-K25 (5): son çerçeve SESSION_CLOSE(SHUTDOWN) karşı tarafa ulaşır; sim düzgün
        // kapatır (karşı tarafın yazımı hata almaz, okuması EOF ile biter).
        const bool son_kapanis = !sk.empty() && sk.back().msgid == kMsgSessionClose && sk.back().yuk[2] == kCloseShutdown;
        dogrula_sb(son_kapanis && k.yazma_hatasi == 0u && k.eof,
                   ad + "son SESSION_CLOSE(SHUTDOWN) karşı tarafa ulaştı, kapanış düzgün (yazma hatası " +
                       std::to_string(k.yazma_hatasi) + ", EOF " + std::to_string(k.eof ? 1 : 0) + ")");
        dogrula_sb(hello == board_hello, ad + "her el sıkışma tek SIM_HELLO (SIM_HELLO " + std::to_string(hello) +
                                             ", BOARD_HELLO " + std::to_string(board_hello) + ")");
        if (r == 0) {
            continue;
        }
        if (k.sim_kart == k0.sim_kart) {
            ++ayni_sk;
        } else {
            std::printf("  koşu %zu sim → kart ilk fark: bayt %zu\n", r, ilk_fark_konumu(k0.sim_kart, k.sim_kart));
        }
        if (k.kart_sim == k0.kart_sim) {
            ++ayni_ks;
        } else {
            std::printf("  koşu %zu kart → sim ilk fark: bayt %zu\n", r, ilk_fark_konumu(k0.kart_sim, k.kart_sim));
        }
    }
    const int m = static_cast<int>(kosular.size()) - 1;
    dogrula_sb(ayni_sk == m, "sim → kart akışı bütün koşularda aynı (" + std::to_string(ayni_sk) + "/" +
                                 std::to_string(m) + ", " + std::to_string(k0.sim_kart.size()) + " B)");
    dogrula_sb(ayni_ks == m, "kart → sim akışı bütün koşularda aynı (" + std::to_string(ayni_ks) + "/" +
                                 std::to_string(m) + ", " + std::to_string(k0.kart_sim.size()) + " B)");
    return 0;
}

}  // namespace

int sil_belirlenim_testi(const std::string& ad, int argc, char** argv)
{
    if (ad == "sil_oturum_dusme_belirlenim") {
        int n = 10;
        if (argc > 2) {
            n = std::max(1, static_cast<int>(std::strtol(argv[2], nullptr, 10)));
        }
        (void)test_sil_oturum_dusme_belirlenim(n);
        return g_hata_sb;
    }
    return -1;   // bu dosyanın testi değil
}
