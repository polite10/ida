// Kopru'nun iç durumu (kopru.cpp, kopru_parti.cpp, kopru_sinir.cpp paylaşır).
#pragma once

#include "ida/kopru/kopru.hpp"
#include "ida_link/tel.h"
#include "mavlink_ic.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
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
    std::uint32_t ilk_seq = 0, son_seq = 0;
    std::int64_t ilk_t = 0;
    std::map<std::uint32_t, std::int64_t> tf_yazim;    // seq -> yazım duvarı (son 512)
    std::uint32_t sonraki_seq = 1;                     // çalıştırma boyunca kesintisiz

    // ---- Açık pencere ----
    std::uint16_t pen_uye = 0;
    bool pen_imu = false;
    std::size_t pen_kritik = 0;
    std::size_t pen_bayt = 0;
    std::optional<sensor::Olcum> pen_emniyet;
    std::optional<sensor::Olcum> sonraki_emniyet;

    // ---- Yuvalar ve sürekli çıktılar ----
    std::map<std::uint32_t, Yuva> yuvalar;
    std::map<std::uint64_t, std::uint32_t> adim_yuva;  // a_n adımı -> seq
    std::map<std::uint32_t, mavlink_ida_actuator_t> gecen_yanit;   // A1-K118 için (son 64)
    std::map<std::uint32_t, std::int64_t> yanit_sure;   // seq -> TF yazımından yanıt alımına (son 64)
    std::vector<std::int64_t> yanit_sureleri;           // çalıştırma boyunca (ölçüm)
    std::vector<mavlink_ida_actuator_t> esz_bekleyen;
    std::set<std::uint32_t> esz_muaf;       // W4-K6: ilk parti öncesi eşzamansız çıktıların out_seq'i
    bool ilk_yanit_alindi = false;          // bu oturumda olağan yanıt alındı
    bool sinir_var = false;
    std::uint64_t son_sinir = 0;
    bool out_seq_var = false;
    std::uint32_t son_out_seq = 0;          // V5
    bool uygulanan_var = false;
    std::uint32_t son_uygulanan_out_seq = 0;
    bool yururluk = false;
    std::array<double, 8> pwm{};
    std::int64_t yururluk_bitis = 0;
    std::set<std::uint32_t> birlesik_seq;

    // ---- Olaylar ----
    std::vector<BekleyenOlay> olay_bekleyen;
    std::set<std::uint64_t> karari_bekleyen;
    std::map<std::uint64_t, SakliSonuc> sakli;         // A1-K81 (çalıştırma boyunca)
    std::deque<mavlink_ida_event_result_t> sonuc_kuyrugu;
    std::deque<std::uint64_t> sakli_talep;

    // ---- Emniyet ----
    sensor::EmniyetDurumu emn{};
    std::array<std::int64_t, 2> izin_bitis{};
    std::vector<EmniyetAtama> atama_bekleyen;
    std::uint32_t dar_komut_bekleyen = 0;
    // W4-K4: her revizyonda emniyet girdilerinin (acil durdurma, fiş, pim) yetkili değeri;
    // bit 0 estop, bit 1–2 fiş takılı, bit 3–4 pim çıkık. Son 256 revizyon.
    std::map<std::uint32_t, std::uint8_t> rev_girdi;
    std::uint8_t girdi_kodu() const
    {
        std::uint8_t g = emn.acil_durdurma ? 1u : 0u;
        for (std::size_t k = 0; k < 2; ++k) {
            g = static_cast<std::uint8_t>(g | (emn.fis[k] ? (2u << k) : 0u) | (!emn.pim[k] ? (8u << k) : 0u));
        }
        return g;
    }
    bool estop_onceki = false;              // W4-K3
    bool estop_esik_var = false;
    std::uint32_t estop_esik = 0;

    // ---- Komutlar ----
    std::uint32_t c = 1;                    // A1-K66
    std::deque<Komut> toplu;
    std::array<std::optional<Komut>, kDarTurSayisi> dar_bekleyen{};
    std::array<bool, kDarTurSayisi> dar_bu_pencerede{};
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
    bool tf_gonderildi(std::uint32_t seq) const;
    std::int64_t tf_zamani(std::uint32_t seq) const;
    void yanit_al(const mavlink_ida_actuator_t& m);
    void olay_al(const mavlink_ida_event_t& m);
    void sonuc_al(const mavlink_ida_cmd_result_t& m);
    void yerel_sonuc(std::uint32_t cmd_id, std::uint8_t ack, std::uint16_t reason);
    void komut_yaz(Komut& k);
    std::uint32_t komut_ekle(Komut k);
    void pencere_ac(std::int64_t t_onceki);
    void rev_artir();
    void izin_kapat(int tup);
    void olay_karar(const BekleyenOlay& o, std::int64_t t);
    void sonuc_kuyruga(std::uint64_t event_id, std::uint8_t sonuc, std::int64_t t_apply);
    bool yuva_tamam(const Yuva& y) const;
    void yuva_tamamlandi_mi(std::uint32_t seq);
    std::uint32_t nonce_sec();
    std::uint16_t sess_sec(std::uint16_t prev_sess);
};

}  // namespace ida::kopru
