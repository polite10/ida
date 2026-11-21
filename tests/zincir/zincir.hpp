// İDA kapalı çevrim düzeneği (W3-zincir, Z1): fizik (ida::fizik::Dunya) + sade sensör üretimi
// + gerçek ida_core + platformun en küçük taklidi, tek süreçte. Ağ, protokol ve kart yoktur.
//
// Zaman (SIL, A4-K11): kontrol periyodu P = 5 ms (200 Hz); k. çağrının zaman çerçevesi
// tf.t = k·P, now_w = tf.t, alım damgaları = now_w. Fizik adımı 1,25 ms; çağrılar arasında
// dört fizik adımı. Çağrının itki isteği A4-K54'e göre darbe genişliğine çevrilir ve iki
// kontrol periyodu sonra (L_act = 10 ms) fiziğe uygulanır.
//
// Sensörler (sade; ayrıntılı modeller 05'in işidir): IMU 200 Hz (5 ms pencere ortalaması,
// montaj kolu, sabit sapma, beyaz gürültü), manyetometre 100 Hz (yerel alan sabit), basınç
// 25 Hz (18,75 ms pencere), batarya ve sızıntı 10 Hz (sağlıklı sabit), emniyet örneği 10 Hz.
// Gürültü tohumu sabittir; çıktı aynı ikili ve aynı makinede bit düzeyinde yinelenir.
//
// Senaryo (Z1): 0–2 s nötr (motor sürücüsü beklemesi), hizalama; ARM; SET_MODE(GUIDED);
// GUIDED_SP derinlik 1,5 m, yön +90°; 40 s tutma; manevra: GUIDED_SP derinlik 2,5 m, yön −90°
// ve aynı anda seçilen iticilerde itki × 0,4; 40 s tutma; DISARM.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ida::zincir {

// Senaryo ayarı. Varsayılanlar Z1'dir (W3-zincir §3).
struct Ayar {
    std::uint64_t tohum = 0x5A31'0001ULL;
    double sure_s = 90.0;
    double t_arm_s = 3.0;        // ARM
    double t_kip_s = 3.1;        // SET_MODE(GUIDED)
    double t_sp_s = 3.2;         // GUIDED_SP 1 (derinlik, yön)
    double t_manevra_s = 43.2;   // GUIDED_SP 2 ve itici arızası (40 s tutmadan sonra)
    double t_disarm_s = 83.2;    // DISARM (manevradan 40 s sonra)
    double hedef_derinlik_m = 1.5;
    double hedef_yon_rad = 1.5707963267948966;    // +90°
    double hedef2_derinlik_m = 2.5;
    double hedef2_yon_rad = -1.5707963267948966;  // −90°
    // Arızalı iticiler (A5 itici sırası, kanal 0–7); boş: arızasız. Varsayılan: t5 (kanal 4,
    // ön iskele dikey) ve t1 (kanal 0, ön iskele yatay) birlikte.
    std::vector<int> ariza_kanallari = {4, 0};
    double ariza_carpani = 0.4;  // itki × 0,4
    bool csv_uret = true;
    // Yer istasyonu canlılığının (HEARTBEAT) kesildiği an; < 0: kesilmez. Manevra sırasında
    // kesilirse GCS_KAYIP_MS sonra HOLD arıza eylemi başlar (yön ve derinlik geçişi testi).
    double t_hb_kes_s = -1.0;
    // Yalnız testin karşı örneği: platform DISARM çağrısının çıktısını kuyruğa koymaz (hata
    // taklidi); fizik tarafındaki nötrlük denetiminin bunu yakaladığı sınanır.
    bool karsi_ornek_disarm_yuvasi_atlanir = false;
    // Ek parametre değerleri (ad, F32 değer): blokta çekirdeğin varsayılanının yerine geçer.
    // Ayar denemeleri içindir; Z1 testi boş bırakır (varsayılanlar sınanır).
    std::vector<std::pair<std::string, float>> parametreler;
};

// Bir kontrol adımının kaydı.
struct Satir {
    double t_s = 0.0;
    double derinlik = 0.0, derinlik_k = 0.0;   // gerçek ve kestirilen (O_b, m)
    double yon = 0.0, yon_k = 0.0;             // rad
    double yalpa = 0.0, yalpa_k = 0.0;
    double bas_kic = 0.0, bas_kic_k = 0.0;
    float thr[8] = {};
    std::int32_t pwm_uret[8] = {};   // bu çağrının çıktısından üretilen, t + L_act için kuyruğa konan (µs)
    std::int32_t pwm_yur[8] = {};    // (t − h, t] fizik adımında yürürlükteki komut (arızadan önce)
    std::int32_t pwm_fiz[8] = {};    // aynı adımda fiziğin uyguladığı (arızadan sonra; DurumAnlik.pwm_us)
    double tau_z = 0.0, tau_n = 0.0;           // iticilerin gövde Z kuvveti (N, aşağı +) ve N momenti (N·m)
    std::uint8_t kip = 0, armed = 0, fs_level = 0, est_level = 0;
    std::uint32_t fs_reasons = 0;
    std::uint8_t thr_valid = 0, is_safe = 0;
    int rc = 0;
};

struct Sonuc {
    bool kurulum_tamam = false;
    std::string hata;
    std::vector<Satir> satirlar;
    std::string csv;
    // Komut sonuçları (ack; −1: sonuç görülmedi).
    int arm_ack = -1, arm_neden = -1;
    int kip_ack = -1, kip_neden = -1;
    int sp_ack = -1, sp_neden = -1;
    int sp2_ack = -1, sp2_neden = -1;
    int disarm_ack = -1;
    double t_arm_kabul_s = -1.0;
    // Emniyet değişmezlerinin ihlalleri (W3-zincir kabul md.4) ve platform sayaçları.
    std::vector<std::string> ihlaller;
    std::uint32_t cikti_duzeltildi = 0;   // A4-K59
    std::uint32_t sayisal_hata_fizik = 0;
    double denge_derinligi_m = 0.0;       // başlangıçtaki yüzer denge (O_b)
    // DISARM (A1-K74, K77; EMNİYET): DISARM'ı taşıyan çağrının tf.t'si, o anda kuyrukta nötr
    // dışı komut bulunup bulunmadığı, fiziğe giden darbe genişliğinin (kalıcı olarak) nötr olduğu
    // ilk fizik adımının başı ve en büyük gerçekleşen itkinin 0,05 N altına indiği an.
    std::int64_t t_disarm_ns = -1;
    bool disarm_kuyrukta_notr_disi = false;
    std::int64_t t_pwm_notr_ns = -1;
    std::int64_t t_itki_sifir_ns = -1;
};

Sonuc kos(const Ayar& ayar);

// Bir hedefe gidiş ve tutma evresinin ölçüleri (ölçüt gerçek, fizik durumu).
struct Evre {
    double oturma_s = -1.0;        // GUIDED_SP'den |derinlik hatası| ≤ bant olup evre sonuna dek
                                   // kaldığı ana; −1: oturmadı
    double yon_oturma_s = -1.0;    // aynı tanım, |yön hatası| ≤ yön bandı
    double asim_m = 0.0;           // hedefi geçme (derinlik yönünde), m
    double yon_asim_rad = 0.0;     // yön hedefini dönüş yönünde geçme, rad
    double tutum_rad = 0.0;        // evre boyunca en büyük |yalpa|, |baş-kıç|
    double tutum_oturma_s = -1.0;  // |yalpa|, |baş-kıç| ≤ tutum bandı olup evre sonuna dek kaldığı an
    double tutma_derinlik_m = 0.0; // evrenin son 20 s'sinde en büyük |derinlik hatası|
    double tutma_yon_rad = 0.0;    // aynı aralıkta en büyük |yön hatası|
};

// Z1 özeti (W3-zincir kabul md.3).
struct Ozet {
    double bant_m = 0.10;
    double yon_bant_rad = 0.05;
    double tutum_bant_rad = 0.02;
    Evre dalis;     // GUIDED_SP 1 → manevra
    Evre manevra;   // GUIDED_SP 2 (ve arıza) → DISARM
    // Arızanın etkinliği: manevra evresinde arızalı kanalların yürürlükteki (arızadan önceki)
    // darbe genişliği ölü bandın (±25 µs) dışındayken geçen kontrol adımı sayısı (kanal başına
    // en küçüğü).
    std::uint32_t ariza_etkin_adim = 0;
    // Aynı adımlarda arızanın fiziğe giden darbe genişliğini gerçekten değiştirdiği adım sayısı
    // (kanal başına en az; pwm_fiz ≠ pwm_yur).
    std::uint32_t ariza_degistirdi_adim = 0;
    int en_yuksek_fs = 0;            // ARM'dan DISARM'a en yüksek arıza düzeyi
    int en_yuksek_est = 0;           // aynı aralıkta en kötü kestirim düzeyi (sayısal)
    bool sayisal_cokme = false;      // est FAULT ya da fs_reasons NUMERIC
    bool disarm_sonrasi_sifir = false;   // DISARM'dan sonra bütün thr = 0 ve pwm nötr
    double kestirim_derinlik_hata_m = 0.0;  // ARM'dan DISARM'a |derinlik_k − derinlik| en büyüğü
    double kestirim_yon_hata_rad = 0.0;
};

Ozet ozetle(const Sonuc& s, const Ayar& a);

}  // namespace ida::zincir
