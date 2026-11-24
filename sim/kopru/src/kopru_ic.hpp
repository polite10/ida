// Kopru'nun iç durumu (kopru.cpp, kopru_parti.cpp, kopru_sinir.cpp paylaşır).
#pragma once

#include "ida/kopru/kopru.hpp"
#include "ida_link/tel.h"
#include "mavlink_ic.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace ida::kopru {

// A1 §3.9 sabitleri (bu işte kullanılanlar).
inline constexpr std::int64_t kTHello = 100 * kMs;
inline constexpr std::int64_t kTCanli = 100 * kMs;
inline constexpr std::int64_t kTHazir = 1000 * kMs;
inline constexpr std::int64_t kTReboot = 5000 * kMs;
inline constexpr std::int64_t kCSim = 400'000;             // A1 §3.9 bütçe kalemi k (tahmin)
inline constexpr std::size_t kBKrit = 520;                  // A1-K113
inline constexpr std::size_t kBPencere = 1200;              // A1-K34
inline constexpr std::int64_t kRc = 60;                     // A1-K112 komut kredisi
inline constexpr std::int64_t kCcMax = 236;
inline constexpr std::size_t kQSonuc = 8;                   // A1-K111
inline constexpr std::size_t kPartiSonuc = 4;
inline constexpr std::size_t kPartiTopluKomut = 4;          // A1-K34
inline constexpr std::uint32_t kRevAzami = 0xFFFFFFFFu;
inline constexpr std::int64_t kRs = 260;                    // A1-K112 sonar kredisi (B/pencere)
inline constexpr std::int64_t kCsMax = 600;                 // C_s,max
inline constexpr std::size_t kPingParca = 240;              // A1 §3.4 PING360_DATA parça boyu
inline constexpr std::size_t kPingOrnekAzami = 1200;

// Daraltıcı türler (A1-K61): DISARM, daraltıcı SET_MODE, FC_DISARM, FC_ABORT.
inline constexpr int kDarTurSayisi = 4;

enum class Sinif { notr, daraltir, genisletir };

struct Komut {
    std::uint32_t cmd_id = 0;
    std::uint32_t msgid = 0;
    Sinif sinif = Sinif::notr;
    int dar_tur = -1;
    std::int64_t t_hazir = 0;
    std::uint32_t rev = 0;                 // kabul edildiği emniyet revizyonu (A1-K129)
    // Tipli yapı (sess yazımda doldurulur).
    mavlink_ida_cmd_simple_t basit{};
    mavlink_ida_cmd_guided_t guided{};
    mavlink_ida_cmd_param_set_t param{};
    mavlink_ida_cmd_fc_t fc{};
    mavlink_ida_cmd_mission_item_t gorev{};
};

struct Yuva {
    bool act_var = false;
    mavlink_ida_actuator_t act{};
    std::set<std::uint64_t> olaylar;       // sayılan yeni olaylar (A1-K73)
    bool reddedildi = false;               // A1-K118 tutarsızlığı
    bool iptal = false;                    // eşzamansız güvenli çıktıyla iptal (A1-K78)
    bool islendi = false;                  // uygulama anı geçti
    bool birlesik = false;                 // sonradan gelen bir yanıtın n_merged'i kapsıyor
    std::int64_t t_n = 0;
    std::uint64_t adim = 0;                // a_n'in fizik adımı
    std::int64_t son_tarih = -1;           // D_n (A1-K108); < 0: yok (SIL)
    std::int64_t tamam_duvar = -1;
};

struct BekleyenOlay {
    mavlink_ida_event_t ev{};
    std::uint64_t adim = 0;                // uygulamaya çalışılacağı sınır
    bool gec = false;                      // A1-K83: kendi sınırından sonra uygulanacak (sayıldı)
};

// W4-K15 (EMNİYET): partinin olay kümesi = IDA_ACTUATOR(n) + batch_seq = n taşıyan, TICK_GRID
// olmayan n_events farklı event_id (A1-K73). Durum makinesi (BENIOKU "Olay kümesi"):
//   bekliyor    yanıt yok ya da küme eksik; yanıt hiçbir geçmişe girmez, canlılığı yenilemez;
//               genişletici olay bekler (son geçerlilikte REJ_EXPIRED)
//   dogrulandi  küme tam ve topluca tutarlı (A1-K118, Tablo 4.6b): yanıt kabul edildi
//   uygulandi   yanıt a_n'de uygulandı
//   reddedildi  küme tutarsız: yanıt ve sayılan genişletici olaylar birlikte reddedilir
// Karar verilmiş kümeye yeni kimlikli genişletici olay ya da yanıt geldiğinde sayıyı aşan
// genişletici olay: sayım ihlali (REJ_SAFE + PROTOCOL oturum sonu). SAFETY_CLOSE beklemez.
enum class KumeDurumu : std::uint8_t { bekliyor, dogrulandi, uygulandi, reddedildi };

// W4-K15 ek (7): bir kart çerçevesinin canlılık (T_bağ) katkısı. yok: katkı yok (reddedildi
// ya da reddedilmiş/bekleyen kümenin yinelemesi); aninda: alındığı anda yenilenir (beklemeden
// uygulanan çerçeveler); kumeye_bagli: alım zamanı kümede bekler, küme topluca doğrulanınca
// işlenir, reddedilir ya da tahliye edilirse atılır.
enum class Canlilik : std::uint8_t { yok, aninda, kumeye_bagli };

struct PartiKumesi {
    KumeDurumu durum = KumeDurumu::bekliyor;
    bool act_var = false;                  // alınmış yanıt (bekleyen ya da karara bağlanmış)
    mavlink_ida_actuator_t act{};
    std::int64_t act_duvar = 0;            // yanıtın alım duvarı
    std::set<std::uint64_t> olaylar;       // partinin ızgara dışı olayları (geliş zamanından bağımsız)
    std::vector<std::uint64_t> acan_id;    // genişletici olayların kimlikleri
    std::uint8_t acan = 0;                 // bit 0 SAFETY_OPEN, bit 1 FIRE
    std::int64_t canlilik_bekleyen = -1;   // kümeye bağlı çerçevelerin en son alımı (W4-K15 ek (7))
};

// W4-K12: bir revizyonun yetkili emniyet durumu. girdi: bit 0 estop, 1–2 fiş takılı, 3–4 pim
// çıkık; izin_acilan: bit 5–6, bu revizyon sürerken açılmış izinler (izin açılması revizyonu
// artırmaz, kapanması artırır).
struct RevDurum {
    std::uint8_t girdi = 0;
    std::uint8_t izin_acilan = 0;
};

std::uint8_t emniyet_kodu(const sensor::EmniyetOlcumu& y);
bool en_az_kisitlayici(std::uint8_t kod, const RevDurum& r);

// Ekleme sırasıyla sınırlı anahtar–değer tablosu (sol #12): eski kayıt sayısal anahtara göre
// değil gönderim sırasına göre atılır; 32 bitlik sıra numarasının sarmasında doğru kalır.
template <typename V>
struct SiraliHarita {
    std::map<std::uint32_t, V> m;
    std::deque<std::uint32_t> sira;
    std::size_t sinir = 64;
    explicit SiraliHarita(std::size_t s) : sinir(s) {}
    void koy(std::uint32_t k, const V& v)
    {
        if (m.count(k) == 0u) {
            sira.push_back(k);
        }
        m[k] = v;
        while (sira.size() > sinir) {
            m.erase(sira.front());
            sira.pop_front();
        }
    }
    const V* bul(std::uint32_t k) const
    {
        const auto it = m.find(k);
        return it == m.end() ? nullptr : &it->second;
    }
    V* bul(std::uint32_t k)
    {
        const auto it = m.find(k);
        return it == m.end() ? nullptr : &it->second;
    }
    // Yoksa varsayılanla ekler.
    V& al(std::uint32_t k)
    {
        if (m.count(k) == 0u) {
            koy(k, V{});
        }
        return m.find(k)->second;
    }
    void temizle()
    {
        m.clear();
        sira.clear();
    }
};

// Pencere kapanışında yazılacak sensör üyesi (A1-K113 öncelikli kaydırma için tutulur;
// MAVLink başlık seq'i yazım anında verilir).
struct SensorUye {
    std::uint32_t msgid = 0;
    std::variant<mavlink_ida_imu_t, mavlink_ida_mag_t, mavlink_ida_baro_t, mavlink_ida_gnss_t, mavlink_ida_dvl_t,
                 mavlink_ida_batt_t, mavlink_ida_leak_t, mavlink_ida_ping1d_t>
        m;
    std::int64_t t_hazir = 0;   // t_teslim (bosalt'ın atama anı)
    std::uint8_t tur = 0;       // IDA_SENS_*
    std::uint8_t inst = 0;
    bool gecersiz = false;      // VALID = 0
    std::uint32_t kayma = 0;    // kaç parti kaydı
    std::size_t boy = 0;        // kodlanmış çerçeve boyu
};

// Gönderilecek Ping360 satırı (A1-K49): DATA 0 … n_frag − 1, sonra HEAD.
struct Ping360Satir {
    bool var = false;
    mavlink_ida_ping360_head_t bas{};
    std::array<std::uint8_t, kPingOrnekAzami> veri{};   // n_samples'tan sonrası 0 (A1 §3.4)
    std::uint8_t sonraki = 0;                           // sıradaki parça; n_frag: HEAD
    std::int64_t t_hazir = 0;                           // t_teslim (A1-K36 atama anı)
};

struct TfKaydi {
    std::int64_t t = 0;       // t_sim_ns
    std::int64_t duvar = 0;   // yazım duvarı
};

struct SakliSonuc {
    std::uint8_t sonuc = 0;
    std::int64_t t_apply = 0;
};

struct Kopru::Ic {
    KopruAyari a;
    bool belirlenimci = true;              // SIL: nonce ve sess tohumdan (A1-K114)
    Sayaclar say;
    KartBilgisi kart;
    ida_link_alici_t alici{};
    ida_link_gonderici_t gonderici{};
    std::vector<std::uint8_t> giden;
    std::int64_t duvar = 0;                // son çağrının duvar saati
    std::int64_t son_tx = 0;

    // ---- Oturum ----
    OturumDurumu durum = OturumDurumu::kapali;
    std::uint32_t nonce = 0;
    std::uint32_t nonce_deneme = 0;        // A1-K114 <deneme>
    std::int64_t son_hello = 0;
    std::uint32_t board_nonce = 0;
    std::uint16_t sess_ = 0;
    std::uint32_t oturum_sirasi = 0;       // A1-K114 <oturum_sırası>
    std::deque<std::uint16_t> son_oturumlar;   // A1-K15: son 16
    std::int64_t hazir_bas = 0;
    std::int64_t son_rx_gecerli = 0;       // T_bağ
    bool reboot_bekleniyor = false;
    std::int64_t reboot_duvar = 0;
    std::uint32_t reboot_onceki_boot = 0;
    bool hazirlik_gerekli = false;
    bool calistirma_basladi = false;       // bu çalıştırmada OK oturum kuruldu (A1-K18)
    bool oturum_sonu_bekliyor = false;     // sınırın (1). adımı
    bool yururluk_sil = false;
    std::uint32_t run_id = 0;
    std::uint64_t board_tohum = 0;

    // ---- Bu oturumda gönderilen TF'ler ----
    bool tf_var = false;
    std::uint32_t son_seq = 0;
    SiraliHarita<TfKaydi> tf_kayit{512};               // bu oturumda gönderilen TF'ler (son 512)
    std::uint32_t sonraki_seq = 1;                     // çalıştırma boyunca kesintisiz; 0 da geçerli
    std::deque<std::uint32_t> yuva_sira;               // yuvalar gönderim sırasıyla

    // ---- Açık pencere ----
    std::uint16_t pen_uye = 0;
    bool pen_imu = false;
    std::size_t pen_kritik = 0;     // pencerede yazılmış daraltıcı komut baytları (kritik; A1-K113)
    std::size_t pen_bayt = 0;       // pencerede yazılmış toplu üyelerin baytları (A1-K34)
    std::vector<SensorUye> pen_sensor;   // pencere kapanışında yazılacak sensör üyeleri
    std::vector<SensorUye> kayan;        // B_krit ya da A1-K34 yüzünden sonraki partiye kayanlar
    std::optional<sensor::Olcum> pen_emniyet;
    std::optional<sensor::Olcum> sonraki_emniyet;
    // ---- Sonar (W4-bagla-sonar; A1-K36, K37, K49, K112) ----
    Ping360Satir p360_aktif;            // gönderimi başlamış satır
    Ping360Satir p360_bekleyen;         // gönderimi başlamamış en çok bir satır (A1-K37)
    std::int64_t sonar_kredi = kCsMax;  // C_s
    bool p360_bas_bu_pencerede = false; // A1-K34: partide en çok bir HEAD
    std::int64_t pen_t_kapanis = 0;     // açık pencerenin kapanacağı sim anı (t_n)
    void sonar_hizmet(bool ayrilmis);   // A1-K36 adım (3) (ayrilmis) ve (5)

    // ---- Yuvalar ve sürekli çıktılar ----
    std::map<std::uint32_t, Yuva> yuvalar;
    std::map<std::uint64_t, std::uint32_t> adim_yuva;  // a_n adımı -> seq
    SiraliHarita<mavlink_ida_actuator_t> gecen_yanit{64};   // kabul edilen yanıtlar (A1-K118)
    SiraliHarita<std::int64_t> yanit_sure{64};              // TF yazımından yanıt alımına
    std::vector<std::int64_t> yanit_sureleri;           // çalıştırma boyunca (ölçüm)
    std::vector<std::int64_t> sira_boslugu_t;           // A1-K08 boşluklarının sim zamanı
    std::vector<mavlink_ida_actuator_t> esz_bekleyen;   // en çok 1 (W4-K27 (3))
    // W4-K6, W4-K28 (3): tutulan tek eşzamansız çıktının ilk parti öncesi muafiyeti. Muafiyet
    // çıktıyla birlikte saklanır; birleşen çıktıların kimlikleri biriktirilmez (bellek sabit).
    bool esz_bekleyen_muaf = false;
    bool ilk_yanit_alindi = false;          // bu oturumda olağan yanıt alındı
    // W4-K14 (SIL; A1-K121 (ii) okuması): KOSUYOR'da ACTUATOR, SESSION_CLOSE ve BOARD_HELLO
    // dışındaki çerçeveler tamponlanır; sonraki ACTUATOR alındığında ondan önce, oturum sonunda
    // kapanışta işlenir. Sınır: bir çağrının ACTUATOR dışı çıktısı en çok Q_ii = 64 zorunlu-
    // güvenilir çerçeve + R_iii = 500 B kaybedilebilir sınıf (≥ 12 B'lık çerçevelerle < 42);
    // 128 bunun üstündedir. Aşılırsa protokol hatasıyla oturum sonu (atılmaz).
    static constexpr std::size_t kSilTamponAzami = 128;
    std::vector<ida_link_cerceve_t> sil_tampon;
    std::vector<ida_link_cerceve_t> sil_isle;
    bool sil_tampon_isleniyor = false;
    // Seq tanısı (Sayaclar::rx_sira_disi, rx_kayip).
    bool sira_var = false;
    std::uint8_t sira_en = 0;
    std::uint32_t sira_maske = 0;   // bit k: seq (sira_en − 1 − k) bekleniyor
    bool sinir_var = false;
    std::uint64_t son_sinir = 0;
    bool out_seq_var = false;
    std::uint32_t son_out_seq = 0;          // V5
    bool uygulanan_var = false;
    std::uint32_t son_uygulanan_out_seq = 0;
    std::uint32_t uygulanan_boot = 0;       // son uygulanan çıktının kart açılışı (sol #4)
    bool yururluk = false;
    std::array<double, 8> pwm{};
    std::int64_t yururluk_bitis = 0;
    std::set<std::uint32_t> birlesik_seq;

    // ---- Olaylar ----
    std::vector<BekleyenOlay> olay_bekleyen;
    std::set<std::uint64_t> karari_bekleyen;
    std::map<std::uint64_t, SakliSonuc> sakli;         // A1-K81 (çalıştırma boyunca)
    // W4-K15: partilerin olay kümeleri (ekleme sırasıyla son 256 parti; bekleyen küme
    // tahliyede kaybolur: genişletici olayları REJ_SAFE alır, kısıtlayan sonuç).
    SiraliHarita<PartiKumesi> kume{256};
    PartiKumesi& kume_al(std::uint32_t seq);
    bool kume_tamamla(std::uint32_t seq);   // true: yanıt şimdi kabul edildi
    void yanit_kabul(std::uint32_t seq, const PartiKumesi& p);
    void sayim_ihlali(const std::vector<std::uint64_t>& idler);
    std::deque<mavlink_ida_event_result_t> sonuc_kuyrugu;
    std::deque<std::uint64_t> sakli_talep;
    // W4-bagla-a3 (A3-K98): verilen olay kararları, A3'e aktarılana dek (en çok 1024).
    std::array<OlayKarari, 1024> olay_kararlari{};
    std::size_t olay_karari_bas = 0, olay_karari_n = 0;
    std::uint64_t olay_karari_atilan = 0;

    // ---- Emniyet ----
    sensor::EmniyetDurumu emn{};
    std::array<std::int64_t, 2> izin_bitis{};
    std::vector<EmniyetAtama> atama_bekleyen;
    std::uint32_t dar_komut_bekleyen = 0;
    // W4-duraklat: sınırın duran kipi (Kopru::duraklatma_ayarla). true iken üye gönderimi yok
    // (daraltıcı bekler) ve SAFETY_CLOSE yuva sınırını beklemez.
    bool duran = false;
    // W4-K4: her revizyonda emniyet girdilerinin (acil durdurma, fiş, pim) yetkili değeri;
    // bit 0 estop, bit 1–2 fiş takılı, bit 3–4 pim çıkık. Son 256 revizyon.
    std::map<std::uint32_t, RevDurum> rev_girdi;
    std::uint8_t girdi_kodu() const
    {
        std::uint8_t g = emn.acil_durdurma ? 1u : 0u;
        for (std::size_t k = 0; k < 2; ++k) {
            g = static_cast<std::uint8_t>(g | (emn.fis[k] ? (2u << k) : 0u) | (!emn.pim[k] ? (8u << k) : 0u));
        }
        return g;
    }
    std::uint8_t izin_kodu() const
    {
        return static_cast<std::uint8_t>((emn.izin_acik[0] ? 32u : 0u) | (emn.izin_acik[1] ? 64u : 0u));
    }
    bool estop_onceki = false;              // W4-K3
    bool estop_esik_var = false;
    std::uint32_t estop_esik = 0;

    // ---- Komutlar ----
    std::uint32_t c = 1;                    // A1-K66
    std::deque<Komut> toplu;
    std::array<std::optional<Komut>, kDarTurSayisi> dar_bekleyen{};
    std::array<bool, kDarTurSayisi> dar_bu_pencerede{};
    // A1-K61, K119 (sol #10): daraltıcı tür başına duvar saatli kova (patlama 1, 5 ms'de 1).
    std::array<double, kDarTurSayisi> dar_kova{};
    std::array<std::int64_t, kDarTurSayisi> dar_kova_t{};
    std::array<bool, kDarTurSayisi> dar_kova_var{};
    void dar_doldur(std::size_t t)
    {
        if (!dar_kova_var[t]) {
            dar_kova_var[t] = true;
            dar_kova[t] = 1.0;
        } else {
            dar_kova[t] = std::min(1.0, dar_kova[t] + static_cast<double>(duvar - dar_kova_t[t]) / static_cast<double>(kP));
        }
        dar_kova_t[t] = duvar;
    }
    void dar_hizmet();
    std::map<std::uint32_t, KomutSonucu> sonuclar;
    std::set<std::uint32_t> oturum_komutlari;
    std::set<std::uint32_t> sonuc_bekleyen;
    std::int64_t kredi = kCcMax;
    double kova = 8.0;
    std::int64_t kova_duvar = 0;
    bool kova_var = false;

    // ---- Yardımcılar ----
    void cerceve(std::uint32_t msgid, const void* m, std::size_t boy, std::size_t* yazilan = nullptr);
    void oturum_bitti(bool planli, bool yeniden);
    void el_sikis(std::int64_t duvar_ns);
    void hello_isle(const mavlink_ida_board_hello_t& h);
    void kart_cercevesi(const ida_link_cerceve_t& c);
    void cerceve_al(const ida_link_cerceve_t& c);
    void sil_tampon_bosalt();
    void sira_izle(std::uint8_t seq);
    bool tf_gonderildi(std::uint32_t seq) const;
    std::int64_t tf_zamani(std::uint32_t seq) const;
    Canlilik yanit_al(const mavlink_ida_actuator_t& m);   // A1-K109, K72; W4-K15 ek (7)
    Canlilik olay_al(const mavlink_ida_event_t& m);       // A1-K109, K72; W4-K15 ek (7)
    void canlilik_isle(std::int64_t t, bool sess_tasir);  // T_bağ ve HAZIR → KOSUYOR (A1-K27)
    Canlilik kume_canliligi(std::uint32_t seq);           // bekleyen kümeye alım zamanı ekler
    bool sonuc_al(const mavlink_ida_cmd_result_t& m);   // false: reddedildi (A1-K109, K72)
    void yerel_sonuc(std::uint32_t cmd_id, std::uint8_t ack, std::uint16_t reason);
    std::size_t komut_yaz(Komut& k);
    std::size_t uye_yaz(const SensorUye& u);
    std::size_t uye_boy(const SensorUye& u) const;
    std::uint32_t komut_ekle(Komut k);
    void pencere_ac(std::int64_t t_onceki);
    void rev_artir();
    void izin_kapat(int tup);
    // Emniyet adımlarının kodu (sinir_isle'nin iki kipi; W4-duraklat).
    bool kisitla(const EmniyetAtama& at, bool& degisti);
    bool operator_kisitla(const EmniyetAtama& at, OperatorSonucu& o);
    OperatorSonucu operator_gevset(const EmniyetAtama& at, bool kisitlama_hakim, std::uint32_t rev_bas);
    void estop_isle();
    void olay_karar(const BekleyenOlay& o, std::int64_t t, std::uint64_t adim_no);
    void cikti_uygula(const mavlink_ida_actuator_t& m, bool esz, bool muaf, std::uint64_t adim_no, std::int64_t t,
                      SinirSonucu& s);
    bool oturum_sonu_daralt();
    void sonuc_kuyruga(std::uint64_t event_id, std::uint8_t sonuc, std::int64_t t_apply, std::uint8_t kind = 255,
                       std::uint8_t tube = 255);
    bool yuva_tamam(const Yuva& y) const;
    void yuva_tamamlandi_mi(std::uint32_t seq);
    std::uint32_t nonce_sec();
    std::uint16_t sess_sec(std::uint16_t prev_sess);
};

}  // namespace ida::kopru
