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
// sınırıysa parti kapanır. A3 ve A2 yayınları (açıksa) sınırın son işidir, fizik adımından önce.
#pragma once

#include "ida/a2/sunucu.hpp"
#include "ida/a3/komut.hpp"
#include "ida/kopru/kopru.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/port.h"
#include "ida/sonar/olcum.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ida::senaryo {
class DosyaKaynagi;
}

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
    bool guided_sp = true;          // yalnız sınama: GUIDED_SP adımları (A5'te karşılığı yok)
};

struct Ayar {
    Kip kip = Kip::sil;
    Karsi karsi = Karsi::ic;
    std::uint16_t a1_port = 0;      // tcp: 0 → işletim sistemi seçer
    std::string sil_yolu;           // tcp: ida_sil programı (boşsa dışarıdan bağlanılması beklenir)
    std::string sil_depo;           // tcp + sil_yolu: koşucunun --depo yolu (boşsa bellek)
    std::string seri;               // seri: aygıt yolu
    std::string senaryo = "z1";     // yerleşik: z1 | bos (senaryo_dosyasi boşken)
    // A5 senaryo dosyası (W4-sim-senaryo): verilirse kurulum ve zaman çizelgesi dosyadan
    // (çözümleyici `ida_senaryo`); yol depo köküne göre göreli. Kip, süre ve tohum dosyadandır;
    // komut satırında ayrıca verilen ve dosyayla çelişen kip, ya da verilen tohum/süre hatadır.
    std::string senaryo_dosyasi;
    std::string depo_koku = ".";
    std::shared_ptr<const senaryo::DosyaKaynagi> senaryo_kaynagi;   // sınama: bellekteki dosyalar
    bool kip_verildi = false, karsi_verildi = false, tohum_verildi = false, sure_verildi = false;
    // Yalnız yerleşik senaryoda anlamı olan seçeneklerin verildiği (değerden ve yinelemeden
    // bağımsız kalıcı bayraklar; EMNİYET, W4-sim-senaryo sol #1 ve dar denetimi).
    bool ariza_verildi = false;      // --ariza (yalnız yerleşik z1)
    bool acil_verildi = false;       // --acil
    bool acil_kalk_verildi = false;  // --acil-kalk
    std::uint64_t tohum = 1;        // A5 rastgelelik.ana_tohum (sensörler, A1-K114)
    double sure_s = 90.0;
    Z1 z1{};
    // Yalnız yerleşik senaryoda (EMNİYET, W4-sim-senaryo sol #1): senaryo dosyasıyla birlikte
    // verilirse başlamadan kullanım hatasıdır (davranış özetine girmeyen emniyet olayı olmaz).
    double acil_durdurma_s = -1.0;  // senaryo: sanal acil durdurma basılır (< 0: yok)
    double acil_kalkar_s = -1.0;    // senaryo: acil durdurma kalkar
    std::string csv_yolu;           // boşsa dosya yazılmaz
    bool csv_uret = true;
    int gunluk = 1;                 // 0 sessiz, 1 özet, 2 ayrıntı (stderr)
    std::int64_t t_sil_ns = 5'000'000'000;   // A1-K76 T_SIL (duvar)
    // Bağlantı bekleme ve el sıkışma süre sınırı (duvar): her aşama kendi payını alır (bağlantının
    // kabulü; hazırlık oturumunun el sıkışması; çalıştırma oturumunun el sıkışması). Hazırlık
    // oturumunun kendi süresi buna girmez (W4-K26 mimar kararı; hedef Windows düşük donanımlı, K16).
    std::int64_t baglanti_bekleme_ns = 20'000'000'000;
    // Hazırlık oturumunun (A1-K19) duvar saati üst sınırı; aşılırsa koşu hatayla biter. 600 s bir
    // tahmindir (ölçülmedi): olağan hazırlık SIL'de saniyeler, HIL'de en çok 8000 periyot (40 s) sürer.
    std::int64_t hazirlik_azami_ns = 600'000'000'000;
    // Sol 4. denetim #2 (EMNİYET): bir oturum kurma işleminin (ilk kurulum ya da koşu ortasında
    // yeniden kurma; hazırlık gerekip gerekmediğinden bağımsız) mutlak duvar süresi ve en çok
    // denemesi (bağlantı kabulleri ve el sıkışma başlangıçları). Aşılırsa koşu hatayla ve ortak
    // güvenli kapanışla biter. Varsayılanlar tahmindir (ölçülmedi): 900 s hazırlığın 600 s'sini ve
    // el sıkışmaları kapsar; 32 deneme beş hazırlık denemesine ve yeniden bağlantılara pay bırakır.
    std::int64_t kurma_azami_ns = 900'000'000'000;
    int kurma_deneme_azami = 32;
    // pty (kartsız sınama; sol #8): aygıtın kalkması ve geri takılması. cikarma anında sözde
    // uçbirimin ana ucu kapatılır (işletim sistemi aygıtın kalktığını bildirir); takma anından
    // sonraki ilk yeniden açma denemesi yeni bir sözde uçbirim açar. < 0: yok.
    double pty_cikarma_s = -1.0;
    double pty_takma_s = -1.0;
    // Yalnız sınama (sol #13): ic kipinde karşı tarafın çekirdek çağrısı girdileri; tcp kipinde
    // dinleme başlayınca port ve karşı tarafın A1 tohumu (dışarıdan koşucu başlatmak için).
    std::function<void(int tur, const ida_in_t& girdi)> ic_cagri_kancasi;
    std::function<void(int tur, const ida_in_t& girdi, ida_out_t& cikti)> ic_cikti_kancasi;
    std::function<void(std::uint16_t port, std::uint64_t a1_tohum)> tcp_hazir;
    // Yalnız sınama (W4-sim-senaryo): her fizik sınırında fiziğe verilen darbe genişlikleri
    // (acil durdurma engeli ve itici arızasından sonra) ve sensör kümesinin teslim ettiği kayıtlar.
    struct SinirGozlemi {
        std::uint64_t b = 0;
        double derinlik = 0.0;                 // yer gerçeği O_b derinliği, sınır b'deki durum
        std::array<double, 8> pwm{};           // fiziğe verilen
        std::array<double, 8> itici_carpani{}; // etkin itici arızalarının çarpımı (1: arıza yok)
        bool acil_durdurma = false;
        std::array<double, 3> konum{};         // W4-bagla-sonar: yer gerçeği O_b konumu (NED), sınır b
        std::array<double, 4> yonelim{1.0, 0.0, 0.0, 0.0};   // gövdeden NED'e (w, x, y, z)
    };
    std::function<void(const SinirGozlemi&)> sinir_kancasi;
    // Yalnız sınama (W4-K26 (2)): sim'in karşı tarafa göndermeye karar verdiği baytlar (taşımaya
    // verilmeden önce, yazım başarısız olsa da), sırayla.
    std::function<void(const std::uint8_t*, std::size_t)> giden_kancasi;
    std::function<void(const sensor::Olcum& o)> olcum_kancasi;
    // Yalnız sınama: görev iletileri sonuç beklenmeden (sınır başına bir) gönderilir; köprünün ve
    // kartın dönüştürücüsünün alanları taşıdığını sınamak için (çekirdek görevi desteklemese de).
    bool gorev_beklemeden = false;

    // ---- A3 sunucusu (W4-bagla-a3; docs/sozlesmeler/A3-web.md) ----
    // Kapalıyken (varsayılan) sunucu hiç başlamaz ve koşu bugünkü davranışla bayt bayt aynıdır.
    bool a3_acik = false;
    std::uint16_t a3_port = 47100;  // A3-K01; 0: işletim sistemi atar
    std::string a3_kok;             // panellerin derleme çıktısı (web/dist); boşsa statik yollar 404
    // true: çalıştırma `yuklendi`'de bekler, panelden `calistirma/baslat` ile başlar (A3-K62).
    // false: çalıştırma kendiliğinden başlar (komut satırındaki koşunun davranışı).
    bool a3_bekle = false;
    // Açılış adresi (A3-K05) bir kez verilir; program standart çıktıya yazar.
    std::function<void(const std::string& adres)> a3_acilis;
    // Koşu bitince sunucunun son çerçeveleri yayınlaması için beklenen duvar süresi (s).
    double a3_kapanis_bekleme_s = 0.2;
    // Yalnız sınama: sunucu dinlemeye başladı (port, erişim jetonu).
    std::function<void(std::uint16_t port, const std::string& jeton)> a3_hazir;
    // Yalnız sınama: her fizik sınırının başında (dünya iş parçacığı), A3 komutları çekilmeden
    // önce; hazırlık yoklamalarında ve `a3_bekle`'de `yuklendi` beklemesinde (b = 0) de çağrılır. Sınama komut alıcısına doğrudan istek verebilir.
    std::function<void(std::uint64_t b, ida::a3::KomutAlicisi& alici)> a3_sinir_kancasi;
    // Yalnız sınama: A1 bayt akışı (sim → karşı taraf: true; karşı taraf → sim: false).
    std::function<void(bool sim_giden, const std::uint8_t* p, std::size_t n)> a1_bayt_kancasi;
    // Yalnız sınama (W4-K28 (3)): sim tarafının A1 taşımasını sarmalar (ör. kısmi yazım veren sahte
    // taşıma). Taşıma kurulunca (süreç içi, sözde uçbirim, seri; TCP'de bağlantı kabul edilince) çağrılır.
    std::function<std::unique_ptr<kopru::Tasima>(std::unique_ptr<kopru::Tasima>)> tasima_sarmalayici;

    // ---- Sonar (W4-bagla-sonar) ----
    // Kapalıyken (varsayılan) sonar kümesi kurulmaz; koşu bu iş öncesiyle bayt bayt aynıdır. Açıkken
    // (`--sonar`) senaryo dosyasının sonarları (A5 arac.sensorler, tur ping1d/ping360; sahne
    // senaryonun ortamından, `senaryo::sonar_kur`) kurulur, ölçümler A1'den karta (köprü: Ping1D
    // kritik üye, Ping360 A1-K49 parçalaması) ve A3 açıksa `sonar/ping1d`, `sonar/ping360`
    // kanallarından yayınlanır. Yalnız senaryo dosyasıyla (yerleşik senaryoda sahne tanımı yok).
    // Neden varsayılan kapalı (mimar onayına): scenarios/z1.toml araç dosyasından (ida0.toml) iki
    // etkin sonar alır; varsayılan açık olsaydı Z1/Z2 taban çizgisinin A1 akışı değişirdi.
    bool sonar_acik = false;
    // `--sonar`/`--sonar=acik|kapali` komut satırında verildi (W4-sunucu-kucuk kapsam 5): verilmemişse
    // ve senaryoda etkin sonar varsa başlangıçta tek satır bilgi yazılır.
    bool sonar_verildi = false;
    // Kalite kademesi (K16, Ö11): Ping360 tarama yapılandırması A5'te ve A1'de yok (W4-K18 (c));
    // kurulum alanının komut satırından kısılması. 0: modelin varsayılanı (1 200 örnek; A5-K78).
    // 200–1200: örnek sayısı, menzil korunarak (örnek periyodu orantılı).
    int ping360_ornek = 0;
    double ping360_bekleme_ms = 0.0;   // pingler arası bekleme (satır hızının kısılması), ≥ 0
    std::string sonar_csv_yolu;        // boşsa yazılmaz: sonar kayıtları (arıza zinciri sonrası)
    // Yalnız sınama: sınırda teslim edilen her sonar kaydı (köprüye verilmeden önce).
    std::function<void(const sonar::SonarOlcumu&)> sonar_kancasi;

    // ---- A2 sunucusu (W5-bagla-a2; docs/sozlesmeler/A2-gorsellestirici.md) ----
    // W5-K31: kapalıyken (varsayılan) sahiplik kilidi alınmaz, dinleyici açılmaz; koşu ve çıktılar
    // bu iş öncesiyle bayt bayt aynıdır. Açıkken (`--a2-port`) süreç başına bir A2 sunucusu başlar
    // ve dünyanın durumu her fizik sınırında görselleştiriciye yayınlanır (dünya hiçbir koşulda
    // beklemez: A2-K47). Yalnız senaryo dosyasıyla (varlık kümesi, ortam ve görünüm kademesi
    // senaryodandır; yerleşik senaryoda tanım yok). `kalite.gorunum = "kapali"` ise dinleyici açılmaz
    // (A2-K02). Kilit ya da port alınamazsa koşu A2'siz sürer (A2-K05, K79).
    bool a2_acik = false;
    std::uint16_t a2_port = 47210;     // A2-K01, K93: 1024 … 65 535 (aralık dışında koşu başlamaz)
    bool a2_sinama_bos_port = false;   // yalnız sınama: işletim sisteminin verdiği boş port
    bool a2_port_verildi = false;      // yalnız main.cpp: --a2-port verildi (sinama seçeneğiyle çakışma denetimi)
    std::string a2_kilit_dosyasi;      // yalnız sınama: sahiplik kilidinin yolu (boş: A2-K77'deki sabit yer)
    std::size_t a2_gonderme_kuyrugu_bayt = 0;   // yalnız sınama: 0 → A2-K26 (8 MiB)
    // Yalnız sınama: A2 sunucusu başlatıldı (dinlenen port; A2'sizken 0 ve dinliyor = false).
    std::function<void(std::uint16_t port, bool dinliyor)> a2_hazir;

    // ---- A6 kayıt (W5-bagla-kayit; docs/sozlesmeler/A6-kayit.md) ----
    // W5-K40: yalnız `--kayit-dizini <dizin>` verildiğinde açılır; yokken dosya açılmaz, iş parçacığı
    // başlamaz ve çıktılar bu iş öncesiyle bayt bayt aynıdır. Dosya `<dizin>/<run_id>.mcap`; kademe
    // senaryonun `kalite.kayit` alanından. Yalnız senaryo dosyasıyla (çözümlenmiş senaryo, A7 manifesti
    // ve doğrulama raporu A6 §3.6 ekleridir; yerleşik senaryoda yoktur ve uydurulmaz). Aynı adlı dosya
    // varsa çalıştırma başlamaz (A6-K01).
    bool kayit_acik = false;
    std::string kayit_dizini;
    // Yalnız sınama: dünya üreticisinin halka boyu (bayt; 0: 16 MiB, tahmin; ölçülmedi).
    std::size_t kayit_halka_bayt = 0;
    // Yalnız sınama: dosya adını (run_id; A3-K63 biçimi) sabitler (var olan dosyanın ezilmediği sınanabilsin).
    std::string kayit_run_id;
    // Yalnız sınama: kayıt dosyasındaki ilk `sequence` (A6-K12 sınırını enjekte eder; 1: olağan).
    std::uint32_t kayit_ilk_sira = 1;
    // Ön denetim `kayit_disk_alani` (A3 §3.8) alt sınırı, bayt: kayıt dizininde bundan az boş alan varsa
    // madde `kaldi` olur ve çalıştırma başlamaz. Varsayılan 1 GiB (mimari seçimi, tahmin); yalnız sınamada değişir.
    std::uint64_t kayit_disk_siniri_bayt = std::uint64_t{1} << 30;
    // Yalnız sınama: kayıt açıldı (dosya yolu, run_id); dünya döngüsü başlamadan önce.
    std::function<void(const std::string& yol, const std::string& run_id)> kayit_hazir;

    // SIL hız çarpanının başlangıç değeri (`--sil-hiz`; A3-K66 ile aynı düzenek: yalnız bekleme,
    // bayt akışı ve CSV değişmez). 0: olabildiğince hızlı (varsayılan). A3 açıkken panelin
    // `calistirma/hiz` komutu bu değerin yerine geçer. Yalnız SIL.
    double sil_hiz = 0.0;
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
    std::array<double, 8> itki_carpani{};     // fizikteki itici arızası çarpanı (W4-K20; CSV'ye yazılmaz)
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
    std::uint64_t rx_sira_disi_kosu = 0, rx_kayip_kosu = 0;   // seq tanısı (Sayaclar::rx_sira_disi, rx_kayip)
    std::vector<std::int64_t> sira_boslugu_t_ns;   // oturum içi ilk üç sıra boşluğunun sim zamanı
};

// A5 §3.15 kayıtları (taşıyıcıları A3/A6'nındır; burada Sonuc'ta tutulur).
struct OlaySonucu {
    std::string kimlik;
    std::string sonuc;               // uygulandi | reddedildi | uygulanmadi
    std::int64_t t_ns = -1;          // uygulandığı/reddedildiği sınır; uygulanmadi'da çalıştırma sonu
    std::string neden;               // gevsetme_reddedildi, desteklenmiyor, hedef_yok, kosul_saglanmadi;
                                     // A3 komutunda A3 §3.9 hata kodu
    // W4-bagla-a3 kapsam 4: kaynak. "senaryo" (zaman çizelgesi; kimlik olay kimliği) ya da "a3"
    // (panel komutu; kimlik "a3/<komut_no>/<tür>").
    std::string kaynak = "senaryo";
};

// Yetkili emniyet girdisi değişimi (A3-K96; kaynak "senaryo" ya da "operator").
struct EmniyetDegisimi {
    std::int64_t t_ns = 0;
    std::string alan;                // acil_durdurma | silahlandirma_fisi.f1 | … | emniyet_pimi.f2
    bool onceki = false, sonraki = false;   // true: basılı / takılı
    std::string kaynak;
    std::uint32_t komut_no = 0;      // operatörde A3 komut numarası
    std::uint32_t emniyet_rev = 0;   // değişimin uygulandığı sınırdan sonraki revizyon
};
struct AtamaCeliskisi {
    std::int64_t t_ns = 0;
    std::string alan;
    std::vector<bool> degerler;
    std::vector<std::string> olay_kimlikleri;
    bool uygulanan_deger = false;
};
struct GevsetmeReddi {
    std::int64_t t_ns = 0;
    std::string olay_kimligi;
    std::string alan;
    bool istenen_deger = false;
    std::uint32_t revizyon_adim_basi = 0;
    std::uint32_t revizyon_adim_3_sonu = 0;
};

enum class HataTuru { yok, calisma, kullanim, senaryo_gecersiz, senaryo_yurutulemez };

// A2 sunucusunun koşu sonundaki durumu ve sayaçları (W5-bagla-a2; kapanış özet satırına girer).
struct A2Ozeti {
    bool istendi = false;            // --a2-port verildi
    // kapali (istenmedi) | gorunum_kapali (A2-K02) | tanim_yok (çalıştırma tanımı kurulamadı) |
    // dinliyor | kilit_baskasinda | kilit_acilamadi | port_acilamadi | ic_hata (a2::A2Durumu adları;
    // sunucunun BAŞLANGIÇTAKİ durumu: kapanışta dinleyici zaten kapanır)
    std::string durum = "kapali";
    std::string aciklama;            // insan için (ör. "A2 sahiplik kilidi başka bir sim sürecinde")
    std::uint16_t port = 0;          // dinlenen port; A2'sizken 0
    std::uint8_t kademe = 0;         // 0 kapali, 1 sade, 2 tam (kalite.gorunum)
    bool calistirma_tanimlandi = false;   // yeni_calistirma kabul edildi
    std::uint32_t arac_kimligi = 0;  // aracın A2 gövde kimliği (W5-K32)
    std::uint64_t adim_siniri = 0;   // dünyanın verdiği adım sınırı (çağrı)
    std::uint64_t reddedilen_cagri = 0;   // kitaplığın uygulamadığı yayın çağrısı (0 beklenir)
    // Dünya iş parçacığında `adim_siniri` çağrısının süresi (duvar; ölçüm).
    std::int64_t cagri_toplam_ns = 0;
    std::int64_t cagri_en_kotu_ns = 0;
    a2::SunucuSayaclari sayac;       // kitaplığın sayaçları (yayın tarafı `sayac.yayin`)
};

// Doğrulama raporunun bir bulgusu (A5 §3.14; geçerli senaryonun uyarıları dahil, ör. A5-K71).
struct DogrulamaBulgusu {
    std::string kural;
    std::string onem;    // hata | uyari
    std::string dosya;
    std::string yol;
    std::int64_t satir = -1;   // −1: yok
    std::string ileti;
};

// A6 kayıt dosyasının koşu sonundaki durumu (W5-bagla-kayit; kapanış özet satırına girer).
struct KayitOzeti {
    bool istendi = false;            // --kayit-dizini verildi
    bool acildi = false;             // dosya oluşturuldu
    bool dosya_kapandi = false;      // Footer ve sondaki sihirli baytlar yazıldı
    std::string yol;                 // <dizin>/<run_id>.mcap
    std::string run_id;
    std::string kademe;              // olaylar | telemetri | tam
    std::string bitis_durumu;        // kapanış kaydındaki (A6-K45)
    std::string guvence;             // kapanış kaydındaki tekrar_guvencesi (yok | izleme | fizik | girdi)
    std::string guvence_nedenleri;
    std::string hata;                // açılamadı / kapanamadı nedeni
    std::vector<std::string> uretilen_kanallar;   // bu çalıştırmada üretici çağrısı alan kanallar
    std::vector<std::string> eksik_kanallar;      // kademe ve kipin gerektirdiği ama üretilmeyen kanallar (W5-K41)
    // Üretici ve yazıcı sayaçları (kitaplıktan).
    std::uint64_t uretilen = 0, kuyruklanan = 0, dusen_halka = 0, dusen_gecersiz = 0, suzulen = 0, kapali_atilan = 0;
    std::uint64_t yazilan_ileti = 0, eksik_kaydi = 0, bildirilen_dusen = 0, yazimda_kaybolan = 0, kapali_iken_atilan = 0;
    std::uint64_t kapanan_parca = 0, dosya_bayt = 0;
    // Dünya iş parçacığında üretici çağrılarının (kb_->yaz) süresi (duvar; ölçüm; zaman okuması dahil).
    std::uint64_t cagri_adet = 0;
    std::int64_t cagri_toplam_ns = 0, cagri_en_kotu_ns = 0;
    std::int64_t taslak_en_kotu_ns = 0;    // `kapanis_taslagi` çağrısının en uzun süresi
};

struct Sonuc {
    bool tamam = false;
    std::string hata;
    HataTuru hata_turu = HataTuru::yok;
    // Senaryo dosyası (W4-sim-senaryo).
    std::string dogrulama_raporu;                        // A5 §3.14 JSON, değiştirilmeden (geçerli ya da geçersiz)
    std::vector<DogrulamaBulgusu> dogrulama_bulgulari;   // aynı raporun bulguları (uyarılar dahil)
    Kip kip = Kip::sil;                                  // etkin kip (dosyadan seçilebilir; sol #5)
    std::string davranis_ozeti;                          // A5-K17
    std::map<std::string, std::string> kaynak_ozetleri;  // dosya → SHA-256
    std::vector<std::string> senaryo_uyarilari;
    std::vector<OlaySonucu> olay_sonuclari;
    std::vector<AtamaCeliskisi> atama_celiskileri;
    std::vector<GevsetmeReddi> gevsetme_retleri;
    std::string gorev_durumu = "yok";                    // yok | tamamlandi | reddedildi | bitmedi | gonderildi
    int gorev_basarisiz_adim = -1;                       // A3 mission_upload `adim` (0'dan)
    int gorev_ack = -1, gorev_neden = -1;                // başarısız adımın sonucu (IDAW_ACK, IDAW_REASON)
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
    std::uint64_t kapanis_yazim_eksik = 0;     // W4-K28 (3): kapanış çıkışı süre sınırında yazılamadı
    std::uint64_t sil_kuyruk_en_cok = 0;       // W4-K26 (3): SIL alım kuyruğunun en büyük boyu (B)
    std::uint64_t oturum_kur_derinlik_en_cok = 0;   // sol 3. denetim #2: oturum_kur iç içe çağrı derinliği (1 beklenir)
    // Sol 4. denetim #1, #3 (EMNİYET): kapanışta yerel daraltma (CSV'nin son satır tanımından ayrı).
    bool kapanis_daraltildi = false;        // ortak kapanışta daraltma yapıldı
    bool kapanis_oncesi_itki = false;       // daraltmadan önce yürürlükte itki komutu vardı
    bool kapanis_oncesi_izin = false;       // daraltmadan önce bir tüpün izni açıktı
    bool kapanis_izin_acik = false;         // daraltmadan sonra bir tüpün izni açık (false beklenir)
    bool kapanis_itki_yururlukte = false;   // daraltmadan sonra yürürlükte itki (false beklenir)
    std::uint64_t kapanis_istisna = 0;      // kapanış koruyucusunda yutulan istisna
    // W4-sunucu-kucuk (kapsam 1): koşu SIGINT/SIGTERM (Windows: konsol denetim olayı) ile kesildi.
    // Kapanış, hata kapanışıyla aynı işlevdir (`hata` = "sinyalle kesildi"); `tamam` false.
    bool sinyal_kesti = false;
    std::uint64_t sil_kuyruk_tasma = 0;        // W4-K26 (3): SIL bekleme tüketim sınırı (akış konumunda) aşıldı (PROTOCOL)
    std::uint32_t karsi_sample_rejected = 0;   // süreç içi karşı tarafın çekirdek sayaçları
    std::uint32_t karsi_sample_future = 0;
    std::uint32_t karsi_imu_rejected = 0;
    std::uint64_t karsi_gecerli_red = 0;
    std::string evre_ozeti;                    // HIL: döngü evrelerinin ölçümü (duvar)       // VALID olduğu halde reddedilen örnek (0 olmalı)
    // ---- A3 (W4-bagla-a3) ----
    std::string calistirma_sonu;               // A3 açıkken: bitti | durduruldu | hata
    std::vector<EmniyetDegisimi> emniyet_degisimleri;   // A3 açıkken
    std::uint64_t a3_yayin_cagrisi = 0;        // Yayinci çağrısı (dünya iş parçacığı)
    std::int64_t a3_cagri_en_kotu_ns = 0;      // tek çağrının en uzun süresi (duvar; ölçüm)
    // W4-K27 (5): dünya tarafındaki BÜTÜN A3 hizmetinin (istek çekme, karar, yayın hazırlığı ve
    // yayın; köprü çağrıları dahil) bir sınırdaki en uzun süresi.
    std::int64_t a3_sinir_en_kotu_ns = 0;
    std::uint64_t a3_halka_tasma = 0;          // A3-K38
    std::uint64_t a3_komut = 0;                // dünyaya ulaşan A3 komutu
    std::uint64_t a3_kayit_atilan = 0;         // kayıt tamponu dolduğu için Sonuc'a yazılmayan kayıt
    // W4-K27 (2) (EMNİYET): önce teslim edilmiş işlenmemiş bir kısıtlayan yüzünden reddedilen gevşetme.
    std::uint64_t a3_sira_korumasi = 0;
    // ---- Sonar (W4-bagla-sonar) ----
    bool sonar_acik = false;
    std::size_t senaryo_etkin_sonar = 0;   // senaryo dosyasındaki etkin Ping1D + Ping360 (sonar açık olsun olmasın)
    std::uint64_t sonar_ping1d = 0, sonar_ping360 = 0;   // teslim edilen kayıt (arıza zinciri sonrası)
    std::uint64_t sonar_ping = 0, sonar_isin = 0, sonar_havada = 0, sonar_kayip = 0, sonar_kuyruk_tasma = 0;
    // W4-K27 (5) (sol #5): sabit kapasiteli teslim deposuna sığmadığı için düşürülen kayıt ve yük
    // tamponu havuzu boşken ayrılan tampon (ikisi de 0 beklenir).
    std::uint64_t sonar_teslim_tasma = 0, sonar_tampon_yetersiz = 0;
    double sonar_a_sensor_bayt = 0.0;   // A1 §3.9 A_sensör ön denetiminin değeri (sol #1), B/parti
    // Dünya iş parçacığında sonar adımının (ping hesabı; sınır b'nin adımı fizik adımı b − 1'den hemen
    // sonra, sınırın bekleyişinden önce yapılır) süresi (duvar; ölçüm). Boşaltma ve köprüye verme
    // sınırın içindedir: sonar_bosaltma_en_uzun_ns.
    std::int64_t sonar_bosaltma_en_uzun_ns = 0;
    std::int64_t sonar_adim_en_uzun_ns = 0;
    std::int64_t sonar_adim_toplam_ns = 0;
    std::uint64_t sonar_adim_ping_sinir = 0;             // en az bir ping atılan sınır
    std::vector<std::int64_t> sonar_adim_ping_ns;        // ping atılan sınırların süreleri (ölçüm; sıralı değil)
    std::vector<std::string> sonar_uyarilari;
    std::string sonar_csv;
    // ---- A2 (W5-bagla-a2) ----
    A2Ozeti a2;
    // ---- A6 kayıt (W5-bagla-kayit) ----
    KayitOzeti kayit;
};

Sonuc kos(const Ayar& ayar);

// Yalnız sınama (W4-K27 (5)): çağıran iş parçacığı dünya tarafındaki A3 hizmetinin içindeyken
// (köprü ve fizik çağrıları hariç) sıfırdan büyük; bellek ayırma sayacı kancaları için.
int a3_ayirma_sayilir() noexcept;
// Yalnız sınama (W4-K27 (5); W4-bagla-sonar sol #5): çağıran iş parçacığı dünya tarafındaki sonar
// hesabının ya da boşaltmasının içindeyken sıfırdan büyük (A3 açık da kapalı da).
int sonar_ayirma_sayilir() noexcept;

// Yalnız sınama (W5-bagla-a2, W5-K31): bu süreçte A2 sunucusunu başlatma denemesi sayısı (sahiplik
// kilidini almayı ve dinlemeyi deneyen her çağrı). `--a2-port` verilmeyen koşu bu sayacı değiştirmez:
// kilit alınmadığının ve port açılmadığının kanıtı.
std::uint64_t a2_baslatma_sayisi() noexcept;

const char* csv_basligi();

// ---- Kapanış isteği (W4-sunucu-kucuk kapsam 1) ----
// Sinyal işleyicisi yalnız bir sayaç artırır (kilitsiz atomik; G/Ç, kilit ve bellek ayırma yok).
// Dünya döngüsü bayrağı bir sonraki sınırda (ve bağlantı, el sıkışma ve hazırlık beklemelerinin
// her turunda) görür ve var olan hata kapanışını işletir (ikinci bir kapanış yolu yoktur).
// Kartın güvenliği bu yola dayanmaz: kart kendi zaman aşımıyla güvenli duruma geçer.
// ida_sim çıkış kodları: 0 tamam, 1 komut satırı, 2 çalıştırma hatası, 3 geçersiz senaryo,
// 4 senaryo yürütülemez, 5 sinyalle düzgün kapanış, 6 ikinci sinyal (hemen sonlandırma).
constexpr int kCikisSinyal = 5;
constexpr int kCikisIkinciSinyal = 6;
// Kapanışı ister (programdan; ikinci sinyal gibi sonlandırmaz). Sinyal işleyicisinden çağrılabilir.
void kapanis_iste() noexcept;
bool kapanis_istendi() noexcept;
// Yalnız sınama: bayrağı sıfırlar.
void kapanis_sifirla() noexcept;
// İşleyicinin çekirdeği (asenkron sinyal güvenli): ilk çağrı bayrağı kurar; ikincisi süreci hemen
// sonlandırır (kCikisIkinciSinyal; bekleyen düzgün kapanışı da keser).
void kapanis_sinyali() noexcept;
// SIGINT ve SIGTERM (Windows'ta ayrıca konsol denetim olayları: Ctrl-C, Ctrl-Break, pencere
// kapatma) için işleyicileri kurar. false: kurulamadı.
bool sinyal_kur() noexcept;
// Düzgün kapanış bitti (Windows'ta pencere kapatma olayının işleyicisi bunu bekler).
void kapanis_tamamlandi() noexcept;

}  // namespace ida::sunucu
