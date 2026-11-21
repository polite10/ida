// tests/kabul/kopru — sahte kart (A1'in kart rolü; bağımsız kabul testleri için).
//
// Gerçek çekirdek ve ida_link üst katmanı kullanılmaz: kart çerçeveleri yalnız tel
// katmanının (protocol/ida_link/include/ida_link/tel.h) kart rolündeki kodlayıcısıyla
// tipli yapılardan üretilir, sim çerçeveleri aynı katmanın alıcısıyla ayrıştırılır. Böylece
// test, gerçek bir kartın göndermeyeceği bozuk, bayat, yinelenen ya da tutarsız çerçeveleri
// de gönderebilir.
//
// Sahte kart yalnız el sıkışmayı (A1-K13, K16, K17) kendiliğinden yürütür; zaman çerçevesine
// verilecek yanıtı, olayları ve her şeyi test belirler (tf_geldi geri çağrısı).
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include "ida_link/tel.h"
#include "mavlink/ida/mavlink.h"

namespace kabul {

inline constexpr std::int64_t kMs = 1'000'000;
inline constexpr std::int64_t kH = 1'250'000;     // fizik adımı (A1 §2)
inline constexpr std::int64_t kP = 5 * kMs;       // kontrol periyodu
inline constexpr std::int64_t kLact = 10 * kMs;   // uygulama gecikmesi

// IDAW_* değerleri (protocol/ida.xml; burada sözleşmeden yazıldı).
namespace w {
inline constexpr std::uint8_t kOlaySafetyOpen = 1, kOlaySafetyClose = 2, kOlayFire = 3;
inline constexpr std::uint8_t kSonucApplied = 1, kSonucDuplicate = 2, kSonucRejExpired = 3,
                              kSonucRejSafe = 4, kSonucRejTubeEmpty = 5, kSonucRejSession = 6;
inline constexpr std::uint16_t kThrValid = 1, kIsSafe = 2, kArmed = 4, kSrcTick = 8, kAsync = 16;
inline constexpr std::uint8_t kTupDolu = 1, kTupFis = 2, kTupPimCikik = 4, kTupIzin = 8;
inline constexpr std::uint8_t kCagriStep = 1, kCagriTickTf = 2, kCagriTickGrid = 3;
inline constexpr std::uint8_t kRcOk = 0, kRcBadInput = 3, kRcImuRejected = 8;
inline constexpr std::uint8_t kRolKart = 1, kRolSil = 2;
inline constexpr std::uint8_t kOturumOk = 0, kOturumReboot = 1;
inline constexpr std::uint8_t kLinkRunning = 2;
inline constexpr std::uint8_t kKapanisShutdown = 1;
inline constexpr std::uint8_t kRunModePrep = 4;
inline constexpr std::uint8_t kTfImu = 1;
}  // namespace w

struct GelenTf {
    std::uint32_t seq = 0;
    std::int64_t t = 0;
    std::uint16_t sess = 0;
    std::uint8_t n_members = 0;
    std::uint8_t flags = 0;
    std::uint8_t run_mode = 0;
};

// Bir partinin (TF(n−1) ile TF(n) arası; A1-K28) kartın gördüğü içeriği.
struct Parti {
    GelenTf tf;
    std::vector<mavlink_ida_safety_t> emniyet;
    std::vector<mavlink_ida_event_result_t> sonuc;
    std::size_t uye = 0;
};

struct GelenKomut {
    std::uint32_t msgid = 0;
    std::uint32_t cmd_id = 0;
    std::uint16_t sess = 0;
    std::uint8_t type = 0;         // CMD_SIMPLE.type (diğerlerinde 0)
    std::uint16_t param_id = 0;    // CMD_PARAM_SET
    std::uint8_t param_type = 0;
    std::uint32_t value_bits = 0;
};

class SahteKart {
public:
    struct Ayar {
        std::uint8_t rol = w::kRolSil;
        std::uint32_t boot_count = 1;
        std::uint32_t blok_crc = 0x5A17C0DEu;
        std::uint16_t blok_len = 32;
        std::uint32_t valid_for_ns = 100'000'000u;
        std::uint16_t esc_notr = 1500;
        std::uint32_t apply_by_ns = 20'000'000u;
    };

    struct Giden {
        std::int64_t t_birak = 0;
        std::vector<std::uint8_t> bayt;
    };

    explicit SahteKart(const Ayar& ayar);

    // Sim → kart baytları (t: şimdiki sim zamanı; kartın kendiliğinden ürettiği çerçeveler bu
    // anda bırakılır).
    void al(const std::uint8_t* p, std::size_t n, std::int64_t t);

    // ---- Gönderim (kart → sim) ----
    // Tipli yapıyı tel katmanıyla kodlar ve t_birak anında bırakılmak üzere kuyruğa koyar.
    void gonder(std::uint32_t msgid, const void* m, std::size_t boy, std::int64_t t_birak);
    // Ham bayt (bozuk çerçeve, kesik çerçeve, çöp).
    void ham_gonder(std::vector<std::uint8_t> b, std::int64_t t_birak);
    // Tipli yapıyı kodlayıp baytlarını döndürür (kuyruğa koymaz).
    std::vector<std::uint8_t> kodla(std::uint32_t msgid, const void* m, std::size_t boy);

    // Olay: event_id = (boot_count << 32) | sayaç (A4-K61); t_decide/batch_seq TF'den;
    // safety_rev = bu oturumda alınan son IDA_SAFETY'ninki (A1-K106).
    mavlink_ida_event_t olay(std::uint8_t kind, std::uint8_t tube, const GelenTf& tf,
                             std::uint32_t hold_ns = 0);
    // Olağan yanıt (STEP, OK): THR_VALID | ARMED, IS_SAFE = 0; out_seq gönderimde atanır.
    mavlink_ida_actuator_t yanit(const GelenTf& tf, std::uint16_t pwm, std::uint8_t n_events);
    // Geçerli komut taşımayan yanıt (A4 4.14: IMU_REJECTED, THR_VALID = 0).
    mavlink_ida_actuator_t gecersiz_yanit(const GelenTf& tf, std::uint8_t n_events);
    // Yanıtı yeni out_seq ile gönderir (A1-K105); atanan out_seq döner.
    std::uint32_t yanit_gonder(mavlink_ida_actuator_t a, std::int64_t t_birak);
    void olay_gonder(const mavlink_ida_event_t& e, std::int64_t t_birak);
    void keepalive_gonder(std::int64_t t_birak);
    void oturum_kapat_gonder(std::uint8_t reason, std::int64_t t_birak);
    // Kartın planlanmamış sıfırlanması: yeni açılış (boot_count + 1), oturum sayısı ve olay
    // sayacı 0, duyuru (A1-K11); sim bunu KOSUYOR'da alınca oturumu bitirir (A1-K23).
    void sifirlan(std::int64_t t);

    // ---- Durum ----
    bool kosuyor() const { return kosuyor_; }
    std::uint16_t sess() const { return sess_; }
    std::uint32_t boot_count() const { return ayar_.boot_count; }
    void boot_count_ata(std::uint32_t b) { ayar_.boot_count = b; }
    std::uint32_t son_rev() const { return son_rev_; }
    std::uint32_t out_seq() const { return out_seq_; }
    void out_seq_ata(std::uint32_t s) { out_seq_ = s; }
    const Ayar& ayar() const { return ayar_; }
    Ayar& ayar_degistir() { return ayar_; }
    const ida_link_sayaclar_t& rx_sayac() const { return alici_.sayac; }

    std::deque<Giden> giden;
    std::vector<Parti> partiler;                       // kapanan partiler, sırayla
    Parti acik;                                        // açık pencere
    std::vector<mavlink_ida_event_result_t> sonuclar;  // bütün olay sonuçları, geliş sırasıyla
    std::vector<mavlink_ida_session_accept_t> kabuller;
    std::vector<GelenKomut> komutlar;
    std::uint64_t sim_hello_sayisi = 0;
    std::uint64_t oturum_kapanis_alindi = 0;
    std::uint64_t yeniden_baslatma = 0;

    // TF(n) alındığında (parti kapandıktan sonra) çağrılır; yanıtı test verir.
    std::function<void(const GelenTf&, std::int64_t t)> tf_geldi;
    // Komut alındığında (yalnız sunucu düzeyindeki vakalar kullanır).
    std::function<void(const GelenKomut&, std::int64_t t)> komut_geldi;
    // false: SIM_HELLO yanıtlanmaz.
    bool el_sikisma_yanitla = true;

    // Bir olayın ilk sonucu (yoksa boş).
    std::optional<mavlink_ida_event_result_t> sonuc(std::uint64_t event_id) const;
    std::size_t sonuc_sayisi(std::uint64_t event_id) const;

private:
    void cerceve_isle(const ida_link_cerceve_t& c, std::int64_t t);
    void board_hello_gonder(std::uint32_t peer_nonce, std::int64_t t);

    Ayar ayar_;
    ida_link_alici_t alici_{};
    ida_link_gonderici_t gonderici_{};
    bool kosuyor_ = false;
    bool el_sikismada_ = false;
    std::uint16_t sess_ = 0;
    std::uint16_t prev_sess_ = 0;
    std::uint16_t oturum_sayisi_ = 0;
    std::uint32_t son_sim_nonce_ = 0;
    std::uint32_t nonce_ = 0;
    std::uint32_t nonce_sayaci_ = 0;
    std::uint32_t son_rev_ = 0;
    std::uint32_t out_seq_ = 0;
    std::uint32_t olay_sayaci_ = 0;
};

}  // namespace kabul
