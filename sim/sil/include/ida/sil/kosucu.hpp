// ida/sil/kosucu.hpp — kartsız (SIL) koşucu: ortak çekirdeği (ida_core) ve ida_link üst
// katmanını PC'de, kartın yerine çalıştırır (görev: docs/gorevler/W4-sil.md).
//
// Sim sunucusuna kartla aynı A1 mesajlarıyla, 127.0.0.1 üzerindeki TCP ile istemci olarak
// bağlanır (A1-K97 … K101); kopunca yeniden dener. Platformun kart yazılımından farkı:
//   - saat sim zamanıdır: son kabul edilen zaman çerçevesinin t_sim_ns'i (A4-K11, A1-K121);
//     TF alındığı an güncellenir, parti üyeleri önceki TF'nin zamanıyla damgalanır;
//   - ızgara tick'i, T_bağ ve duyuru yoktur (A1-K100; ida_link_ust SIL rolü);
//   - her step ve tick'ten önce tam N_bg arka plan çağrısı, oturum sonu çağrısından önce hiç
//     (A1-K121 (iii), A4-K95);
//   - BOARD_HELLO.nonce --a1-seed tohumundan türetilir (A1-K114, A5-K118);
//   - açılış sayacı ve kalıcı parametre bloğu dosyadadır (A4-K82 … K86).
// Çıktı baytları yalnız alınan bayt akışına ve başlangıç durumuna bağlıdır; duvar saatine ve
// TCP parçalanmasına bağlı değildir (A1-K102): alınan baytlar çerçeve çerçeve işlenir ve
// her çerçeveden sonra gönderilecekler yazılır.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ida/port.h"

namespace ida::sil {

inline constexpr const char* kSurum = "0.1.0";
// IDA_BOARD_HELLO.fw_version: (ana << 16) | (ara << 8) | yama.
inline constexpr std::uint32_t kSurumSayisi = 0x000100u;
// Çekirdek örneği için ayrılan alan (A4-K98); gerçek boy ida_core_info ile denetlenir.
inline constexpr std::uint32_t kCekirdekAlani = 64u * 1024u;
inline constexpr std::uint32_t kCekirdekHizasi = 64u;

// Çıkış kodları (BENIOKU.md).
enum class Cikis : int {
    Tamam = 0,           // durduruldu ya da bağlantı bitti ve bekleme süresinde yeniden kurulmadı
    Kullanim = 1,        // komut satırı hatası (main)
    Cekirdek = 2,        // çekirdek kurulamadı: boy, hizalama, port sürümü ya da init dönüşü
    GuvenliBekleme = 3,  // çekirdek iç tutarlılığını kaybetti (health.core_ok = 0)
    Baglanamadi = 4      // ağ başlatılamadı ya da ilk bağlantı bekleme süresinde kurulamadı
};

const char* cikis_aciklamasi(Cikis c);

struct Ayarlar {
    std::uint16_t a1_port = 0;              // --a1-port (zorunlu; A1-K97)
    std::uint64_t a1_tohum = 0;             // --a1-seed (A1-K114); verilmezse 0
    std::string depo_yolu;                  // --depo; boş: bellek içi depo
    std::uint32_t n_bg = 1;                 // --n-bg (A1-K121 N_bg)
    int gunluk = 1;                         // --gunluk: 0 sessiz, 1 hata, 2 bilgi, 3 ayrıntı
    std::uint16_t esc_alt_us = 1100;        // --esc (scenarios/ida0.toml arac.esc)
    std::uint16_t esc_notr_us = 1500;
    std::uint16_t esc_ust_us = 1900;
    // Kalıcı başlangıç durumu (A1-K114; sınama girdisi). Yalnız ilk init'te kullanılır.
    std::uint32_t onceki_acilis = 0;        // --onceki-acilis: depo dosyası yoksa
    std::uint16_t prev_sess = 0;            // --prev-sess
    std::uint16_t oturum_sayisi = 0;        // --oturum-sayisi (sessions_since_boot)
    std::uint32_t last_cmd_id = 0;          // --last-cmd-id
    // Bağlantı yokken en uzun bekleme ve deneme aralığı (duvar saati; çıktıyı etkilemez).
    std::uint32_t bekleme_ms = 10000;       // --bekleme-ms
    std::uint32_t deneme_araligi_ms = 100;
    bool olcum = false;                     // --olcum: çıkışta süre istatistiği (stderr)
};

// Süre istatistiği (ns; duvar saati). Yalnız ölçümdür; çekirdeğe verilmez.
struct SureIstatistigi {
    std::uint64_t sayi = 0;
    std::uint64_t toplam_ns = 0;
    std::uint64_t en_az_ns = 0;
    std::uint64_t en_cok_ns = 0;
    // 1 µs çözünürlüklü histogram, 0 … 9999 µs; son kova taşma.
    static constexpr std::size_t kKova = 10001u;
    std::vector<std::uint32_t> kova;
    void ekle(std::uint64_t ns);
    // Yüzdelik (0 < p < 100), µs; boşsa 0.
    double yuzdelik_us(double p) const;
};

struct Istatistik {
    std::uint64_t cagri[5] = {};            // ida_link_cagri_t türüne göre (1..4)
    std::uint64_t arka_plan = 0;            // ida_core_background çağrısı
    std::uint64_t pencere_ihlali = 0;       // çağrı penceresinde rx denemesi (W3-K10); 0 olmalı
    std::uint32_t baglanti = 0;             // kurulan TCP bağlantısı
    std::uint32_t yeniden_kurulum = 0;      // REBOOT ve sim zamanı geri dönüşü
    std::uint32_t geri_donus = 0;           // sim zamanının geri başlaması (A4-K11)
    std::uint32_t kayit_ok = 0;             // PARAM_SAVE sonucu OK
    std::uint32_t kayit_fail = 0;
    std::uint32_t boot_count = 0;           // son init'in değerleri
    bool boot_count_ok = false;
    std::uint8_t init_rc = 0;
    std::uint32_t core_size = 0;
    SureIstatistigi tur;                    // take → put_out (arka plan çağrıları dahil)
    SureIstatistigi cekirdek;               // yalnız ida_core_step / ida_core_tick
};

// Sınama kancaları. cagri_icinde çağrı penceresinde (take ile put_out arası, çekirdek
// çağrısından hemen önce) koşucunun iş parçacığından çağrılır.
struct Kancalar {
    std::function<void(int tur, const ida_in_t& girdi)> cagri_icinde;
};

class Kosucu {
public:
    explicit Kosucu(Ayarlar ayar, Kancalar kancalar = {});
    ~Kosucu();
    Kosucu(const Kosucu&) = delete;
    Kosucu& operator=(const Kosucu&) = delete;

    // Ana döngü; durdur() çağrılana ya da bağlantı kalıcı olarak bitene kadar döner.
    Cikis calistir();

    // Başka bir iş parçacığından çağrılabilir; döngü en geç ~100 ms içinde fark eder.
    void durdur();

    // calistir() döndükten sonra okunur.
    const Istatistik& istatistik() const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::sil
