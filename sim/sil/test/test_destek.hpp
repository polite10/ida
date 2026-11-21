// test_destek.hpp — sim/sil testlerinin ortak yardımcıları (test kodu).
//
// Sahte sim: gerçek TCP üzerinden (127.0.0.1) koşucuyla konuşur; çerçeveleri tel katmanının sim
// rolündeki kodlayıcısıyla kurar ve alıcısıyla çözer. Alınan baytlar ayrı bir iş parçacığında
// okunur (iki yönde aynı anda yazma tıkanmasın); iki yöndeki bütün baytlar kaydedilir.
// Koşucu ipliği: Kosucu'yu ayrı iş parçacığında çalıştırır; yıkıcı durdurur ve bekler.
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ida/sil/kosucu.hpp"
#include "ida_link/tel.h"
#include "ida_link/ust.h"
#include "mavlink_ic.hpp"
#include "tasima.hpp"

namespace ida::sil::test {

// ---- Denetim -------------------------------------------------------------------------------

int& hata_sayisi();
bool kontrol(bool kosul, const char* ifade, const char* dosya, int satir);
#define KONTROL(k) ::ida::sil::test::kontrol(static_cast<bool>(k), #k, __FILE__, __LINE__)

struct Baglam {
    std::string ad;
    std::filesystem::path gecici;   // bu teste ait boş dizin
    std::string program;            // ida_sil programının yolu (yalnız program testi)
};

// ---- Sahte sim -----------------------------------------------------------------------------

struct Cerceve {
    ida_link_cerceve_t ham{};
    std::uint32_t msgid() const { return ham.msgid; }
    template <class T>
    T coz() const
    {
        T t{};
        (void)ida_link_coz(&ham, ham.msgid, &t, sizeof t);
        return t;
    }
};

class SahteSim {
public:
    explicit SahteSim(tasima::Soket s);
    ~SahteSim();
    SahteSim(const SahteSim&) = delete;
    SahteSim& operator=(const SahteSim&) = delete;

    template <class T>
    void gonder(std::uint32_t msgid, const T& m)
    {
        mesaj_gonder(msgid, &m, sizeof m);
    }
    void mesaj_gonder(std::uint32_t msgid, const void* m, std::size_t boy);
    // Çerçeveyi yalnız kodlar (gönderilen kayda girmez).
    std::vector<std::uint8_t> kodla(std::uint32_t msgid, const void* m, std::size_t boy);
    void bayt_gonder(const std::uint8_t* p, std::size_t n);
    // Durdurulabilir gönderim (gönderilen kayda girmez); başarısız ya da durdurulduysa false.
    bool bayt_gonder_durdurulabilir(const std::uint8_t* p, std::size_t n,
                                    const std::atomic<bool>* dur);
    // Okuyucu iş parçacığını durdurur; bağlantı açık kalır, alınmayan baytlar birikir.
    void okumayi_durdur();
    void alim_tamponu(int bayt) { soket_.alim_tamponu(bayt); }

    // Okuma imlecinden sonraki, msgid'li ve koşulu tutan ilk çerçeveyi bekler; bulunca imleç
    // onun ardına geçer. Zaman aşımında false.
    bool bekle(std::uint32_t msgid, Cerceve* c, int ms = 5000,
               const std::function<bool(const Cerceve&)>& kosul = {});
    std::vector<Cerceve> cerceveler() const;
    std::size_t imlec() const { return imlec_; }
    std::vector<std::uint8_t> alinan() const;
    std::vector<std::uint8_t> gonderilen() const { return gonderilen_; }
    // En az n bayt alınana ya da bağlantı kapanana kadar bekler.
    bool bayt_bekle(std::size_t n, int ms);
    bool kapandi() const { return kapandi_.load(); }
    bool kapanmayi_bekle(int ms);
    void kapat();

    std::uint16_t sess = 0;

private:
    void okuyucu();

    tasima::Soket soket_;
    ida_link_gonderici_t gon_{};
    ida_link_alici_t al_{};
    std::vector<std::uint8_t> gonderilen_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::vector<Cerceve> cerceveler_;
    std::vector<std::uint8_t> alinan_;
    std::size_t imlec_ = 0;
    std::atomic<bool> dur_{false};
    std::atomic<bool> kapandi_{false};
    std::thread iplik_;
};

// ---- Koşucu ipliği -------------------------------------------------------------------------

class KosucuIpligi {
public:
    explicit KosucuIpligi(Ayarlar a, Kancalar k = {});
    ~KosucuIpligi();
    KosucuIpligi(const KosucuIpligi&) = delete;
    KosucuIpligi& operator=(const KosucuIpligi&) = delete;

    // Kendiliğinden bitmesini bekler; zaman aşımında false.
    bool bitmesini_bekle(int ms);
    // durdur() ve bekle; çıkış kodu.
    Cikis bitir();
    Cikis cikis() const { return cikis_; }
    const Istatistik& istatistik() const { return k_.istatistik(); }

private:
    Kosucu k_;
    std::atomic<bool> bitti_{false};
    Cikis cikis_ = Cikis::Tamam;
    std::thread t_;
};

// Testlerin ortak ayarı: port, sessiz günlük, kısa bağlantı beklemesi.
Ayarlar test_ayari(std::uint16_t port);

// Dinleyici açar, koşucuyu başlatır ve ilk bağlantıyı kabul eder.
struct Duzen {
    tasima::Dinleyici dinleyici;
    std::unique_ptr<KosucuIpligi> kosucu;
    std::unique_ptr<SahteSim> sim;
};
bool duzen_kur(Duzen* d, const std::function<void(Ayarlar&)>& ayarla = {}, Kancalar k = {});
// Koşucunun yeniden bağlanmasını kabul eder (sim değişir).
bool yeniden_kabul(Duzen* d);

// ---- Mesajlar ------------------------------------------------------------------------------

constexpr std::int64_t kP = 5000000;   // 5 ms (A1 P)
inline std::int64_t parti_zamani(std::uint32_t seq)
{
    return static_cast<std::int64_t>(seq) * kP;
}

// SIM_HELLO(nonce) -> BOARD_HELLO -> SESSION_ACCEPT(OK, sess) -> KEEPALIVE(sess, RUNNING).
bool el_sikis(SahteSim& sim, std::uint32_t nonce, std::uint16_t sess,
              mavlink_ida_board_hello_t* hello = nullptr);

class PartiKurucu {
public:
    explicit PartiKurucu(SahteSim& sim) : sim_(sim) {}
    void imu(std::uint32_t seq, std::int64_t t);
    void baro(std::uint32_t seq, std::int64_t t);
    void mag(std::uint32_t seq, std::int64_t t);
    void simple(std::uint32_t cmd_id, std::uint8_t type, std::uint16_t arg);
    void param_set(std::uint32_t cmd_id, std::uint16_t id, std::uint8_t type, std::uint32_t bits);
    void guided(std::uint32_t cmd_id, std::uint8_t mask, float depth, float yaw);
    void emniyet(std::uint32_t seq, std::int64_t t, std::uint32_t rev);
    void batarya(std::uint32_t seq, std::int64_t t);   // 16 V (eşikler 13,2 / 12,4 V)
    void sizinti(std::uint32_t seq, std::int64_t t);   // kuru
    // Kamera algılama partisi başlığı ve tek kaydı (A8).
    void algilama(std::uint32_t batch_seq, std::uint32_t det_seq, std::int64_t t);
    // Ping360 satırı: 240 örnek, tek parça (önce DATA, sonra HEAD).
    void ping360(std::uint32_t seq, std::int64_t t);
    // Sayılan üyelerle TF'yi gönderir ve sayacı sıfırlar.
    void tf(std::uint32_t seq, std::int64_t t, bool imu_var);

private:
    SahteSim& sim_;
    std::uint8_t uye_ = 0;
    bool imu_var_ = false;
};

// Olağan parti: IMU, her 4'te BARO, her 8'de MAG, ek üyeler, TF; ACTUATOR(seq) beklenir.
bool parti(SahteSim& sim, std::uint32_t seq, Cerceve* act = nullptr,
           const std::function<void(PartiKurucu&)>& ek = {});

// cmd_id'nin nihai (IN_PROGRESS dışı) sonucunu bekler (imleçten bağımsız, baştan arar).
bool sonuc_bekle(SahteSim& sim, std::uint32_t cmd_id, mavlink_ida_cmd_result_t* r,
                 int ms = 5000);

// Son IDA_STATUS (yoksa false).
bool son_durum(const SahteSim& sim, mavlink_ida_status_t* s);

}  // namespace ida::sil::test
