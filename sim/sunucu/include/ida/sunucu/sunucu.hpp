// Başsız sim sunucusu `ida_sim` (W4-kopru): fizik + sensör kümesi + A1 köprüsü + zaman
// yöneticisi. Yapılandırma şimdilik koddan (İDA-0 ve örnek havuz varsayılanları) ve komut
// satırından gelir; senaryo dosyası (A5) okunmaz.
//
// Zaman yöneticisinin iki kipi (iskelet §4; A1-K75, K76, K108, K121):
//   SIL     kilitli adım: bir kontrol periyodunun fiziği koşar, parti gönderilir, karşı tarafın o
//           partiye yanıtı beklenir (sim zamanı ilerlemez; T_SIL = 5 s duvar gözetimi);
//           belirlenimci ve olabildiğince hızlı.
//   HIL 1x  duvar saatine kilitli 200 Hz: W(t) = W0 + t; sınır b, W(b·h) − C_sim'de hesaplanır,
//           TF(n) W(t_n)'den önce yazılmaz; yanıt beklenmez, a_n'de yuva boşsa parti kaçırılır;
//           gerçek zamanı kaçıran adımlar sayılır.
//
// Fizik sınırındaki sıra (her sınır b): köprünün emniyet ve sürekli çıktı işlemleri
// (A1-K85) → acil durdurma engeli (iskelet §5: etkinken itici girdisi nötr) → senaryonun
// itici arızası → fizik adımı b → b+1; sensör kümesi b'deki durumla örnekler; b kontrol
// sınırıysa parti kapanır.
#pragma once

#include "ida/kopru/kopru.hpp"
#include "ida/port.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ida::sunucu {

enum class Kip { sil, hil };

// Karşı taraf ve taşıma:
//   ic     SIL: süreç içi karşı taraf (ida_link + gerçek çekirdek), bellek taşıması
//   tcp    SIL: 127.0.0.1 TCP sunucusu (A1-K97); sil_yolu verilirse koşucuyu başlatır
//   pty    HIL: sözde uçbirim çifti + süreç içi kart rolünde karşı taraf (kartsız sınama)
//   seri   HIL: seri aygıt (kart; ST-LINK sanal seri portu)
enum class Karsi { ic, tcp, pty, seri };

// Z1 senaryosu (W3-zincir §3; tests/zincir/zincir.hpp ile aynı anlar ve hedefler).
struct Z1 {
    double t_arm_s = 3.0;
    double t_kip_s = 3.1;
    double t_sp_s = 3.2;
    double t_manevra_s = 43.2;
    double t_disarm_s = 83.2;
    double hedef_derinlik_m = 1.5;
    double hedef_yon_rad = 1.5707963267948966;
    double hedef2_derinlik_m = 2.5;
    double hedef2_yon_rad = -1.5707963267948966;
    std::vector<int> ariza_kanallari = {4, 0};
    double ariza_carpani = 0.4;
    double hb_ilk_s = 0.5;          // yer istasyonu canlılığı 1 Hz (A1-K70)
    double t_hb_kes_s = -1.0;
};

struct Ayar {
    Kip kip = Kip::sil;
    Karsi karsi = Karsi::ic;
    std::uint16_t a1_port = 0;      // tcp: 0 → işletim sistemi seçer
    std::string sil_yolu;           // tcp: ida_sil programı (boşsa dışarıdan bağlanılması beklenir)
    std::string sil_depo;           // tcp + sil_yolu: koşucunun --depo yolu (boşsa bellek)
    std::string seri;               // seri: aygıt yolu
    std::string senaryo = "z1";     // z1 | bos
    std::uint64_t tohum = 1;        // A5 rastgelelik.ana_tohum (sensörler, A1-K114)
    double sure_s = 90.0;
    Z1 z1{};
    double acil_durdurma_s = -1.0;  // senaryo: sanal acil durdurma basılır (< 0: yok)
    double acil_kalkar_s = -1.0;    // senaryo: acil durdurma kalkar
    std::string csv_yolu;           // boşsa dosya yazılmaz
    bool csv_uret = true;
    int gunluk = 1;                 // 0 sessiz, 1 özet, 2 ayrıntı (stderr)
    std::int64_t t_sil_ns = 5'000'000'000;   // A1-K76 T_SIL (duvar)
    std::int64_t baglanti_bekleme_ns = 20'000'000'000;
    // pty (kartsız sınama; sol #8): aygıtın kalkması ve geri takılması. cikarma anında sözde
    // uçbirimin ana ucu kapatılır (işletim sistemi aygıtın kalktığını bildirir); takma anından
    // sonraki ilk yeniden açma denemesi yeni bir sözde uçbirim açar. < 0: yok.
    double pty_cikarma_s = -1.0;
    double pty_takma_s = -1.0;
    // Yalnız sınama (sol #13): ic kipinde karşı tarafın çekirdek çağrısı girdileri; tcp kipinde
    // dinleme başlayınca port ve karşı tarafın A1 tohumu (dışarıdan koşucu başlatmak için).
    std::function<void(int tur, const ida_in_t& girdi)> ic_cagri_kancasi;
    std::function<void(std::uint16_t port, std::uint64_t a1_tohum)> tcp_hazir;
};

// Bir kontrol adımının kaydı (tests/zincir Satir ile aynı anlamlar; kart tarafı A1'den).
struct Satir {
    double t_s = 0.0;
    std::uint32_t parti = 0;
    bool parti_var = false;          // bu adımda parti gönderildi (batch_seq 0 da geçerlidir)
    double derinlik = 0.0, derinlik_k = 0.0;
    double yon = 0.0, yon_k = 0.0;
    double yalpa = 0.0, yalpa_k = 0.0;
    double bas_kic = 0.0, bas_kic_k = 0.0;
    std::array<std::int32_t, 8> pwm_uret{};   // kartın bu partiye yanıtındaki darbe genişliği
    std::array<std::int32_t, 8> pwm_yur{};    // (t − h, t] adımında yürürlükteki komut
    std::array<std::int32_t, 8> pwm_fiz{};    // aynı adımda fiziğin uyguladığı (engel ve arızadan sonra)
    std::array<double, 8> itki_istegi{};      // N
    double tau_z = 0.0, tau_n = 0.0;
    std::uint8_t kip = 0, armed = 0, fs_level = 0, est_level = 0;
    std::uint32_t fs_reasons = 0;
    std::uint8_t thr_valid = 0, is_safe = 0, yanit_var = 0;
    int rc = 0;
    std::uint8_t estop = 0;
    std::uint16_t sess = 0;
    std::uint64_t kacirilan = 0;
    std::uint32_t gecersiz = 0;
    std::int64_t yanit_suresi_ns = -1;        // TF yazımından bu partinin yanıtının alımına (HIL)
    std::uint8_t fc_state = 0;                // kartın IDA_FC_STATUS'u (son alınan; gözlem)
    std::uint32_t fc_inhibit = 0;
    std::uint8_t sim_izin = 0;                // sim'in yetkili izinleri: bit 0 tüp 0, bit 1 tüp 1
};

// HIL zamanlama ölçümü (duvar saati; bu makinedeki ölçüm).
struct ZamanOlcumu {
    std::uint64_t tf_sayisi = 0;
    std::int64_t periyot_en_az_ns = 0, periyot_en_cok_ns = 0;
    double periyot_ort_ns = 0.0;
    std::vector<std::int64_t> periyotlar_ns;  // ardışık TF yazım başlangıçları arası
    std::int64_t gec_yazim_en_cok_ns = 0;     // TF yazım başlangıcı − W(t_n)
    std::uint64_t sim_gecikmesi = 0;          // kontrol sınırı (a_n) W(a_n)'den sonra hesaplandı (A1-K108)
    std::uint64_t gercek_zaman_kacirma = 0;   // sim gecikmesi + kaçırılan parti + son tarih
    // Uçtan uca yanıt süresi: TF(n) yazımından yanıt(n) alımına (duvar; HIL'de anlamlı).
    std::uint64_t yanit_sayisi = 0;
    std::int64_t yanit_p50_ns = 0, yanit_p99_ns = 0, yanit_en_cok_ns = 0;
    // Karttan gelen akışın çerçeve hataları: toplam ve çalıştırma oturumu kurulduktan sonra
    // (el sıkışma, hazırlık ve REBOOT sırasındakiler hariç).
    // SESSION_CLOSE gönderiminde dondurulur.
    std::uint64_t rx_crc_kosu = 0, rx_resync_kosu = 0, rx_seq_gap_kosu = 0;
    std::vector<std::int64_t> sira_boslugu_t_ns;   // oturum içi ilk üç sıra boşluğunun sim zamanı
};

struct Sonuc {
    bool tamam = false;
    std::string hata;
    std::vector<Satir> satirlar;
    std::string csv;
    kopru::Sayaclar sayac;
    kopru::KartBilgisi kart;
    ZamanOlcumu zaman;
    double denge_derinligi_m = 0.0;
    // Senaryo komutlarının sonuçları (ack; −1: sonuç görülmedi).
    int arm_ack = -1, arm_neden = -1, kip_ack = -1, sp_ack = -1, sp2_ack = -1, disarm_ack = -1;
    std::int64_t t_disarm_ns = -1;
    std::int64_t t_pwm_notr_ns = -1;     // DISARM'dan sonra fiziğe giden darbe genişliğinin kalıcı nötr olduğu ilk adım
    std::int64_t t_itki_sifir_ns = -1;
    std::uint64_t ihlal_estop = 0;       // acil durdurma etkinken nötr dışı darbe genişliği uygulanan adım (0 olmalı)
    std::uint64_t estop_adim = 0;        // acil durdurma etkin geçen fizik adımı
    std::uint64_t hazirlik_oturumu = 0;  // A1-K19 hazırlık oturumu sayısı
    std::uint64_t yeniden_baglanti = 0;
    // HIL bağlantı kaybı (A1-K23, H29; sol #7, #8).
    std::uint64_t baglanti_kopma = 0;          // aygıt kalktı ya da yazma başarısız / ilerlemiyor
    std::uint64_t port_acma_denemesi = 0;      // 1 s aralıklı yeniden açma denemeleri
    std::uint64_t port_acildi = 0;
    std::uint64_t yazma_takilma = 0;           // T_bağ boyunca ilerlemeyen yazım
    std::uint32_t karsi_sample_rejected = 0;   // süreç içi karşı tarafın çekirdek sayaçları
    std::uint32_t karsi_sample_future = 0;
    std::uint32_t karsi_imu_rejected = 0;
    std::uint64_t karsi_gecerli_red = 0;
    std::string evre_ozeti;                    // HIL: döngü evrelerinin ölçümü (duvar)       // VALID olduğu halde reddedilen örnek (0 olmalı)
};

Sonuc kos(const Ayar& ayar);

const char* csv_basligi();

}  // namespace ida::sunucu
