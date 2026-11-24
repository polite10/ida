// Sim sunucusunun A1 köprüsü (W4-kopru): sim rolünde oturum, parti kurulumu, kart çıktılarının
// alınıp doğrulanması, sürekli komutların uygulanması, tek seferlik olaylar ve sanal emniyet
// girdileri, operatör komutları.
//
// Sözleşme: docs/sozlesmeler/A1-kart-baglantisi.md (1.0), iskelet §3.2 ve §4. Kural
// numaraları yorumlarda verilir.
//
// Köprü G/Ç ve saat bilmez: her işlev zamanı argüman olarak alır. `duvar_ns` sim'in duvar
// saatidir (HIL: süreğen saat; SIL: sim zamanından türetilir, A1-K121 (i)); `t_ns` sim
// zamanıdır. Gidecek baytlar iç tampona yazılır, zaman yöneticisi `giden_al` ile alıp taşımaya
// verir. Belirlenimcidir: aynı girdi dizisi aynı bayt dizisini üretir.
//
// Zaman yöneticisinin her fizik sınırı `b` (t = b·h) için çağrı sırası:
//   sinir_isle(b, …)            emniyet işlemleri ve sürekli çıktılar (A1-K85 sırası); dönen
//                               darbe genişlikleri b'de başlayan fizik adımına verilir
//   (sensör kümesi adımı)       dis_durum() ile; ardından olcum_ekle(…) her yeni kayıt için
//   parti_kapat(t_n, …)         b kontrol periyodunun sınırıysa (b = 4n)
// Komutlar ve emniyet atamaları sınırlar arasında verilir; bir sonraki sinir_isle'de işlenir.
//
// Atış kontrol sınırı: köprü yalnız emniyet zincirini ve yetkili durumu uygular. Kabul edilen
// FIRE yalnız tüpün "dolu" bitini siler (torpido fiziği, ayrılma ve isabet bu işte yoktur);
// hedefleme ya da atış ölçütü yoktur.
#pragma once

#include "ida/sensor/olcum.hpp"
#include "ida/sensor/sensor_kumesi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ida::kopru {

inline constexpr std::int64_t kMs = 1'000'000;
inline constexpr std::int64_t kP = 5 * kMs;          // kontrol periyodu (A1 §3.9)
inline constexpr std::int64_t kH = 1'250'000;        // fizik adımı
inline constexpr std::int64_t kLact = 10 * kMs;      // uygulama gecikmesi
inline constexpr std::uint64_t kPeriyotAdim = 4;     // P / h
inline constexpr std::uint64_t kLactAdim = 8;        // L_act / h

// IDAW_RUN_MODE değerleri.
enum class CalistirmaKipi : std::uint8_t { sil = 1, hil_1x = 2, hil_bariyer = 3, hazirlik = 4 };

// Sim'in A1 durumu (A1 §4.2) ve el sıkışmanın ek alt durumları.
enum class OturumDurumu {
    kapali,              // el sıkışma başlatılmadı
    el_sikisma,          // SIM_HELLO yineleniyor (A1-K12)
    hazir,               // SESSION_ACCEPT(OK) gönderildi; kartın ilk oturum çerçevesi bekleniyor
    kosuyor,             // oturum kuruldu (A1-K27)
    reddedildi,          // REJECT_* gönderildi; çalıştırma başlamaz (A1-K14, H25)
};

struct KopruAyari {
    CalistirmaKipi kip = CalistirmaKipi::sil;
    // SIL türetimleri (A1-K114, K126; A5 Ek D.7): nonce, sess ve run_id `ana_tohum`dan.
    // HIL'de nonce ve sess gerçek rastgelelikten, run_id dışarıdan (A1-K126) gelir.
    std::uint64_t ana_tohum = 1;
    std::uint32_t run_id = 0;            // 0: SIL'de a1/run_id'den türet
    // Zamanlama ve emniyet parametreleri (A1 §3.9; A1-K104 ile denetlenir).
    std::int64_t valid_for_ns = 100 * kMs;
    std::int64_t t_bag_ns = 500 * kMs;
    std::int64_t t_bekle_ns = 30 * kMs;
    std::int64_t apply_by_ns = 20 * kMs;
    std::int64_t hold_ns = 200 * kMs;
    // ESC (A5 arac.esc): alt, nötr, üst (µs).
    std::uint16_t esc_alt_us = 1100;
    std::uint16_t esc_notr_us = 1500;
    std::uint16_t esc_ust_us = 1900;
    // El sıkışma denetimi (A1-K14): beklenen değerler. dialect_hash 0 ise derlenmiş değer.
    std::uint64_t dialect_hash = 0;
    std::uint64_t param_schema_hash = 0;  // 0: ida_core_info'dan
    // A1-K18: A5'ten kurulan parametre bloğunun CRC-32/ISO-HDLC değeri.
    std::uint32_t beklenen_blok_crc = 0;
    // İlk TF'nin batch_seq'i (çalıştırma boyunca kesintisiz artar, 2³²'de sarar; yalnız sınama
    // için değiştirilir: sol #12).
    std::uint32_t ilk_batch_seq = 1;
};

// Sim sayaçları (A1 Tablo 3.11a; bu dilimde tutulanlar).
struct Sayaclar {
    std::uint64_t parti_gonderilen = 0;
    std::uint64_t parti_kacirildi = 0;
    std::uint64_t parti_kacirildi_birlesik = 0;
    std::uint64_t tf_gec_yazim_ns_en_buyuk = 0;
    std::uint64_t yanit_ret_yinelenen = 0;
    std::uint64_t yanit_ret_gec = 0;
    std::uint64_t yanit_ret_oturum = 0;
    std::uint64_t yanit_ret_eski_uretim = 0;
    std::uint64_t yanit_iptal_esz = 0;
    std::uint64_t esz_guvenli_uygulanan = 0;
    std::uint64_t sil_kisit_erken = 0;          // W4-K26 (1): SIL bekleme noktasında erken uygulanan eşzamansız çıktı grubu
    std::uint64_t komut_suresi_doldu = 0;
    std::uint64_t olay_alinan = 0;
    std::uint64_t olay_gec = 0;
    std::uint64_t olay_eksik = 0;
    // W4-K15 (EMNİYET): olay kümesi.
    std::uint64_t olay_sayim_ihlali = 0;        // sayılmamış genişletici olay: REJ_SAFE + PROTOCOL
    std::uint64_t kume_reddedilen = 0;          // topluca doğrulamada reddedilen küme (yanıt + olaylar)
    std::uint64_t kume_bekleyen_tahliye = 0;    // tablo sınırında atılan bekleyen küme
    std::uint64_t kume_en_cok = 0;              // küme tablosunun en büyük boyu (sınır 256)
    std::array<std::uint64_t, 7> olay_sonuc{};   // dizin: IDA_EVR_* 1–6
    std::uint64_t olay_rev_ret = 0;
    std::uint64_t sonuc_kuyrugu_en_buyuk = 0;
    std::uint64_t sonuc_talebi_atildi = 0;
    std::uint64_t komut_birlestirildi = 0;
    std::uint64_t komut_ertelendi = 0;
    std::uint64_t komut_sonucsuz = 0;
    std::uint64_t komut_bayat_yetki = 0;
    std::uint64_t komut_oturumsuz = 0;
    std::uint64_t pencere_tasma = 0;
    std::uint64_t kritik_bayt_en_buyuk = 0;
    std::uint64_t kritik_erteleme = 0;           // A1-K113: kayan kritik üye
    std::uint64_t kritik_erteleme_en_buyuk = 0;  // en büyük kayma (parti)
    std::uint64_t eszamansiz_ilk_parti_oncesi = 0;  // W4-K6: uygulandı, A1-K79'a sayılmadı
    // Tanı (yinelenemeyen 0x20 gözlemi): A1-K79'a sayılan ilk dört eşzamansız güvenli çıktı.
    struct EszKaydi {
        std::uint64_t adim = 0;            // uygulandığı fizik sınırı
        std::uint32_t out_seq = 0;
        std::uint16_t act_flags = 0;
        std::uint32_t onceki_out_seq = 0;  // ondan önce uygulanan son çıktı
        bool ilk_yanit_alindi = false;
    };
    std::array<EszKaydi, 4> eszamansiz_kayit{};
    std::uint32_t eszamansiz_kayit_n = 0;
    // W4-K14 (SIL): ACTUATOR'dan sonraki çerçevelerin tamponu.
    std::uint64_t sil_tampon_en_cok = 0;         // en büyük doluluk (çerçeve)
    std::uint64_t sil_tampon_tasma = 0;          // sınır aşıldı: protokol hatasıyla oturum sonu
    std::uint64_t sil_tampon_atilan = 0;         // tampon işlenirken oturum bitti: kalan çerçeveler
    // Tanı (A1-K08'e göre davranış değişmez): karttan gelen başlık seq'inde sıra dışılık ile
    // kaybı ayırır. Son görülen en büyük seq'in gerisinde 32 numaralık pencere: atlanan numara
    // sonradan gelirse sira_disi, gelmeden pencereden çıkarsa kayip. Yeni oturumda sıfırlanır.
    std::uint64_t rx_sira_disi = 0, rx_kayip = 0;
    std::uint64_t estop_iptal = 0;              // W4-K3: acil durdurmayla iptal edilen komut ve yuvalar
    std::uint64_t gevsetme_reddedildi = 0;      // A5-K63: reddedilen gevşetme (sınır başına bir)
    std::uint64_t emniyet_rev_eslesmedi = 0;     // W4-K12: uygun revizyon bulunamadı
    std::uint64_t emniyet_rev_duzeltme = 0;      // W4-K12, W4-K15 (5): revizyonu kaydınkinden farklı seçilen emniyet örneği
    std::uint64_t gecersiz_ornek = 0;           // VALID = 0 gönderilen sensör üyesi (çekirdek reddeder)
    std::uint64_t emniyet_birlesti = 0;          // aynı pencerede birleştirilen emniyet kayıtları
    std::uint64_t emniyet_ertelendi = 0;         // A1-K56: açılma sonraki partiye
    std::uint64_t alan_ret = 0;                  // alan_ret_<mesaj> toplamı (V2, V4)
    std::uint64_t bag_ret = 0;                   // bag_ret_<mesaj> toplamı (V3)
    std::uint64_t rx_crc_err = 0, rx_resync_bytes = 0, rx_unknown_msgid = 0, rx_bad_len = 0,
                  rx_wrong_sys = 0, rx_iflag = 0, rx_mav1 = 0, rx_seq_gap = 0, rx_frames = 0;
    std::uint64_t el_sikisma_atilan = 0;         // A1-K12: el sıkışmada atılan çerçeve
    std::uint64_t oturum_kurulan = 0;
    std::uint64_t oturum_kopma = 0;
    std::uint64_t beklenmeyen_sifirlama = 0;
    std::uint64_t oturum_reddi = 0;
    std::uint64_t cagri_cakismasi = 0;
    std::uint64_t cagri_izi = 0;
    std::uint64_t bariyer_bekleme = 0;           // bu dilimde bariyer kipi yok: 0
    std::uint64_t son_tarih_kacirildi = 0;
    std::uint64_t sim_gecikmesi = 0;
    std::uint64_t batch_incomplete = 0;          // BATCH_INCOMPLETE bayraklı yanıt (H4)
    std::uint64_t emniyet_revizyonu = 1;
    bool emniyet_rev_doydu = false;
    // Zamanlama geçerliliği (A1-K79): neden bitleri (Gecersizlik).
    std::uint32_t calistirma_gecersiz = 0;
};

// A1-K79 neden bitleri.
enum Gecersizlik : std::uint32_t {
    kGecKacirilanParti = 1u << 0,
    kGecSonTarih = 1u << 1,
    kGecCagriCakismasi = 1u << 2,
    kGecBariyer = 1u << 3,
    kGecGecOlay = 1u << 4,
    kGecEszamansiz = 1u << 5,
    kGecGecYanit = 1u << 6,
    kGecPencereTasma = 1u << 7,
    kGecPlanlanmamisBitis = 1u << 8,
    kGecDuraklatma = 1u << 9,
    kGecSimGecikmesi = 1u << 10,
};

// Kartın bildirdikleri (son alınan, doğrulamadan geçmiş).
struct KartNav {
    bool var = false;
    std::int64_t t_ns = 0;
    std::uint32_t batch_seq = 0;
    std::array<float, 3> p_ned{};
    std::array<float, 4> q_nb{1.0F, 0.0F, 0.0F, 0.0F};
    float depth = 0.0F;
    std::uint8_t est_level = 0;
};

struct KartDurum {
    bool var = false;
    std::uint32_t batch_seq = 0;
    std::uint8_t mode = 0, armed = 0, fs_level = 0, health = 0, link_state = 0;
    std::uint8_t param_store = 0;
    std::uint32_t fs_reasons = 0;
    std::uint32_t param_blob_crc = 0;
    std::uint32_t boot_count = 0;
};

// IDA_CORE_COUNTERS (A4 ida_core_counters_t; ada göre).
struct KartSayaclari {
    bool var = false;
    std::uint32_t sample_rejected = 0, sample_future = 0, sample_conflict = 0,
                  sample_dup_dropped = 0, narrow_from_rejected = 0, imu_rejected = 0, imu_gap = 0,
                  bad_input = 0, dup_seq = 0, old_seq = 0, cmd_rejected = 0, widen_blocked = 0,
                  evres_ignored = 0, event_unknown = 0, step_miss = 0;
};

struct KartBilgisi {
    KartNav nav;
    KartDurum durum;
    KartSayaclari sayac;
    std::uint32_t boot_count = 0;
    std::uint16_t sessions_since_boot = 0;
    std::uint32_t param_blob_crc = 0;
    std::uint8_t init_rc = 0;
    std::uint8_t role = 0;
    std::uint8_t son_oturum_reddi = 0;           // son gönderilen IDAW_SESSION_RESULT
    std::uint8_t son_kapanis_nedeni = 0;         // son alınan IDA_SESSION_CLOSE.reason
    // IDA_FC_STATUS (yalnız gözlem; atış kontrol emniyet zinciri durumu, A4 §3.16).
    bool fc_var = false;
    std::uint8_t fc_state = 0;
    std::uint32_t fc_inhibit_mask = 0;
    std::uint8_t fc_son_olay_sonucu = 0;
};

// Bir komutun sonucu. `yerel` = true: sonucu sim kendisi verdi (oturum yok, birleştirildi,
// bayat yetki; A1-K61, K129, K23).
struct KomutSonucu {
    std::uint8_t ack = 0;        // IDAW_ACK
    std::uint16_t reason = 0;    // IDAW_REASON
    std::uint8_t kind = 0;       // IDAW_RES_KIND
    std::uint32_t value_bits = 0;
    std::uint32_t save_crc = 0;
    bool yerel = false;
    std::int64_t t_ns = 0;       // sonucun alındığı/verildiği sim zamanı
};

// GUIDED_SP hedefi (A1 §3.6 IDA_CMD_GUIDED; A4-K50).
struct GuidedHedef {
    std::uint8_t mask = 0;       // IDAW_SP_MASK
    std::array<float, 2> pos_ne{};
    float depth = 0.0F;
    float altitude = 0.0F;
    std::array<float, 3> vel_ned{};
    float yaw = 0.0F;
    float yaw_rate = 0.0F;
};

// Senaryonun emniyet ataması: verilen alanlar sonraki fizik sınırında uygulanır.
struct EmniyetAtama {
    std::optional<bool> acil_durdurma;            // true: basılı
    std::array<std::optional<bool>, 2> fis{};     // true: takılı
    std::array<std::optional<bool>, 2> pim{};     // true: takılı
};

// Kartın bir partiye verdiği olağan yanıt (IDA_ACTUATOR; A1-K109'dan geçmiş).
struct YanitBilgisi {
    std::uint16_t act_flags = 0;
    std::array<std::uint16_t, 8> pwm_us{};
    std::uint8_t fs_level = 0;
    std::uint8_t core_rc = 0;
    std::uint32_t out_seq = 0;
    // TF(n) yazımından bu yanıtın alınmasına geçen duvar süresi (HIL ölçümü; SIL'de sim zamanı
    // duvar sayıldığından anlamsız). < 0: bilinmiyor.
    std::int64_t yanit_suresi_ns = -1;
};

// Bir fizik sınırının sonucu: o sınırda başlayan adımın sürekli komutu (A1-K74, K77, K78).
struct SinirSonucu {
    std::array<double, 8> pwm_us{};   // yürürlükteki komut; nötr durumda hepsi ESC nötrü
    bool notr = true;                 // yürürlükte komut yok (süresi doldu ya da hiç gelmedi)
    bool yuva_uygulandi = false;      // bu sınırda bir yuva uygulandı
    bool yuva_kacirildi = false;      // bu sınırda bir yuva kaçırıldı (A1-K75)
    std::uint32_t yuva_seq = 0;       // bu sınırın yuvası (varsa)
    // Senaryo zaman çizelgesi (A5-K63, §3.15 `gevsetme_reddedildi`): bu sınırın gevşeten
    // atamaları revizyon 1–3. adımlarda arttığı için reddedildi; adım başı ve 3. adım sonu
    // revizyonları.
    bool gevsetme_reddedildi = false;
    std::uint32_t rev_adim_basi = 0;
    std::uint32_t rev_adim3_sonu = 0;
};

// W4-K26 (1): SIL'de bekleme noktasında işlenen kısıtlayan çerçevelerin erken uygulaması.
struct KisitSonucu {
    bool degisti = false;             // yürürlükteki komut ya da izinler değişti
    bool notr = true;
    std::array<double, 8> pwm_us{};   // yürürlükteki komut (nötrde ESC nötrü)
};

// Görev öğesi (A1 Tablo 3.6a; A5-K55): IDA_CMD_MISSION_ITEM alanları.
struct GorevOgesi {
    std::uint16_t index = 0;
    std::uint8_t item_type = 0;      // IDAW_MI_TYPE
    std::uint8_t frame = 0;          // IDAW_MI_FRAME
    std::array<float, 4> p{};
    std::int32_t lat_e7 = 0;
    std::int32_t lon_e7 = 0;
    float north_m = 0.0F;
    float east_m = 0.0F;
    float depth = 0.0F;
};

class Kopru {
public:
    explicit Kopru(const KopruAyari& ayar);
    ~Kopru();
    Kopru(Kopru&&) noexcept;
    Kopru& operator=(Kopru&&) noexcept;
    Kopru(const Kopru&) = delete;
    Kopru& operator=(const Kopru&) = delete;

    // ---- Oturum ----
    // Yeni el sıkışma (A1-K12): yeni nonce, SIM_HELLO; o ana kadar alınan baytlar atılır.
    void el_sikisma_baslat(std::int64_t duvar_ns);
    void al(const std::uint8_t* p, std::size_t n, std::int64_t duvar_ns);
    // SIL kilitli adım (A1-K121, K102): al() gibi, ama her teslim edilen çerçeveden sonra dur()
    // doğruysa o çerçevenin son baytında durur. Dönüş: tüketilen bayt sayısı (yarım çerçeve
    // ayrıştırıcıda tutulur ve tüketilmiş sayılır). Kalan baytları çağıran saklar ve sonra verir:
    // böylece çerçevelerin işlendiği an geliş zamanına değil akıştaki sırasına bağlı olur.
    std::size_t al(const std::uint8_t* p, std::size_t n, std::int64_t duvar_ns, const std::function<bool()>& dur);
    // Zamana bağlı işler: SIM_HELLO yinelemesi (T_hello), canlılık (T_canlı), T_bağ (yalnız
    // HIL), T_hazır, T_reboot.
    void zamanla(std::int64_t duvar_ns);
    // Planlı kapanış (A1-K21, K128): SESSION_CLOSE(reason) gönderir ve oturumu bitirir.
    void oturum_kapat(std::uint8_t reason, std::int64_t duvar_ns);
    // Taşıma kapandı (SIL TCP, aygıt kalktı; A1-K23, K99) ya da SIL gözetimi doldu (A1-K76).
    void akis_kapandi(std::int64_t duvar_ns);
    void sil_gozetimi_doldu(std::int64_t duvar_ns);
    // W4-K26 (3): karşı taraf kilitli adımı bozan sınırsız bir akış gönderdi (SIL alım sınırları):
    // oturum varsa SESSION_CLOSE(PROTOCOL) ile biter ve yeni el sıkışma başlar (A1-K23).
    void protokol_hatasi(std::int64_t duvar_ns);
    // Çalıştırma kipini değiştirir (hazırlık oturumundan sonra; A1-K19). Oturum yokken.
    void kip_ayarla(CalistirmaKipi kip);

    OturumDurumu durum() const;
    std::uint16_t sess() const;
    bool yeniden_baslatma_bekleniyor() const;     // REBOOT gönderildi (A1-K17)
    // A1-K18/K19: kart blok uyuşmazlığı ya da init_rc yüzünden hazırlık oturumu istiyor.
    bool hazirlik_gerekli() const;

    // ---- Giden baytlar ----
    void giden_al(std::vector<std::uint8_t>& v);
    bool giden_var() const;

    // ---- Parti (A1 §4.3, §4.4) ----
    void olcum_ekle(const sensor::Olcum& o);
    // Pencereyi kapatır: emniyet örneği, sonuçlar, TF(n). Dönüş: batch_seq (oturum yoksa 0).
    // w_tn: t_n'in planlanan duvar anı W(t_n) (HIL; A1-K43, K108); SIL'de < 0.
    // Oturum yoksa (A1-K28) parti gönderilmez: std::nullopt. batch_seq 0 geçerli bir değerdir.
    std::optional<std::uint32_t> parti_kapat(std::int64_t t_n, std::int64_t duvar_ns, std::int64_t w_tn = -1);
    // Yuva n tamamlandı mı (A1-K73).
    bool yuva_tamam(std::uint32_t seq) const;
    // Partinin doğrulamadan geçmiş olağan yanıtı (son 64 parti).
    std::optional<YanitBilgisi> yanit(std::uint32_t seq) const;
    // Bütün olağan yanıtların TF yazımından alımına süreleri (duvar, ns; ölçüm).
    const std::vector<std::int64_t>& yanit_sureleri() const;
    // Karttan gelen akışta her başlık sıra boşluğunun (A1-K08) sim zamanı damgası: son işlenen
    // fizik sınırının zamanı; köprü kurulduğundan beri, en çok ilk 1024.
    const std::vector<std::int64_t>& sira_boslugu_zamanlari() const;
    std::uint32_t son_parti() const;
    bool parti_gonderildi() const;   // bu oturumda en az bir TF gönderildi

    // ---- Fizik sınırı (A1-K85) ----
    SinirSonucu sinir_isle(std::uint64_t adim_no, std::int64_t duvar_ns);
    // W4-K26 (1) (EMNİYET, yalnız SIL; HIL'de etkisiz): sınır adim_no'nun yuva beklemesinde
    // işlenen kısıtlayan çerçevelerin (oturum sonu, eşzamansız güvenli çıktı) etkisini fizik o
    // sınırdan ilerlemeden uygular; dönen komut aynı adımın itici girdisidir.
    KisitSonucu sil_kisitlayan_uygula(std::uint64_t adim_no);

    // A1-K108 (sol #6): sim, a_n'de başlayan fizik adımını W(a_n)'den sonra hesapladı. Ortak
    // sayaç ve A1-K79 geçersizlik yolu.
    void sim_gecikmesi_bildir();

    // ---- Emniyet (iskelet §3.2, §5) ----
    void emniyet_ata(const EmniyetAtama& a);
    // Senaryonun başlangıç emniyet durumu (A5-K86, K87): yalnız ilk fizik sınırından önce;
    // revizyon 1 kalır ve yetkili geçmişin ilk kaydı bu durumla kurulur. false: geç çağrı.
    bool emniyet_baslangic(bool acil_durdurma, const std::array<bool, 2>& fis, const std::array<bool, 2>& pim,
                           const std::array<bool, 2>& dolu);
    sensor::DisDurum dis_durum() const;
    bool acil_durdurma() const;
    bool tup_dolu(int tup) const;
    bool izin_acik(int tup) const;

    // ---- Komutlar (A1 §3.6, §4.5) ----
    // Dönüş: cmd_id. t_ns komutun verildiği sim anı (t_hazır).
    std::uint32_t komut_basit(std::uint8_t type, std::uint16_t arg, std::int64_t t_ns,
                              std::uint8_t kaynak = 255);
    std::uint32_t komut_guided(const GuidedHedef& h, std::int64_t t_ns, std::uint8_t kaynak = 255);
    // Görev yüklemesinin öğesi (A1-K69; toplu sınıf). MISSION_BEGIN/END komut_basit'le gider.
    std::uint32_t komut_gorev_ogesi(const GorevOgesi& g, std::int64_t t_ns, std::uint8_t kaynak = 255);
    std::uint32_t komut_param_set(std::uint16_t param_id, std::uint8_t param_type,
                                  std::uint32_t value_bits, std::int64_t t_ns,
                                  std::uint8_t kaynak = 250);
    std::uint32_t komut_fc(std::uint8_t op, std::uint8_t tube, std::int64_t t_ns,
                           std::uint8_t kaynak = 255);
    std::optional<KomutSonucu> komut_sonucu(std::uint32_t cmd_id) const;
    std::size_t bekleyen_komut() const;

    // ---- Gözlem ----
    const Sayaclar& sayaclar() const;
    const KartBilgisi& kart() const;
    const KopruAyari& ayar() const;
    // A1-K104: ayar aralık dışıysa ilk ihlalin metni (boş: geçerli).
    std::string yapilandirma_hatasi() const;
    std::uint32_t run_id() const;
    // SIL koşucusuna verilecek --a1-seed (A1-K114; a1/board_tohum).
    std::uint64_t board_tohumu() const;

    struct Ic;

private:
    std::unique_ptr<Ic> ic_;
};

// A5 Ek D.7 türetimleri (A1-K114, K126; ana_tohum ve etiket).
std::uint32_t d7_u32(std::uint64_t tohum, const std::string& etiket, std::uint64_t n);
std::uint64_t d7_u64(std::uint64_t tohum, const std::string& etiket, std::uint64_t n);

// CRC-32/ISO-HDLC (A1-K68; parametre bloğu).
std::uint32_t crc32_iso_hdlc(const std::uint8_t* p, std::size_t n);

}  // namespace ida::kopru
