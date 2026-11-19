// W0-MP3a ölçüm prototipi: koşu kaydı, ölçüm profilleri ve çözümleyici (atılabilir).
//
// Kayıt biçimi (metin, satır başına bir kayıt, her satır kendi CRC'siyle):
//   <tür> <satır_no> <gövde...> *<CRC-32 8 büyük onaltılık>
// CRC, " *"dan önceki her şeyin (tür ve satır numarası dahil) CRC-32/ISO-HDLC'sidir. Satır numaraları 0'dan
// kesintisizdir. Sıra: B (başlık; tam bir kez, ilk), R (yalnız echo; istek başına), P (PC sayaçları; tam bir kez),
// K (kart özeti; P.asama = tamam ise zorunlu), S (son; satir=<önceki satır sayısı>; en son). S'den sonra bayt yok.
// B/P/K/S gövdesi anahtar=değer; R gövdesi konumsaldır:
//   R <no> <sıra> <plan_ns> <yazma_başı_ns> <yazma_sonu_ns> <yanıt_ns>
// Zamanlar t0'a (ESLE_YANIT alındıktan sonra) göre host steady_clock ns; gönderilmeyen istekte yazma alanları,
// yanıtı gelmeyende yanıt alanı "-".
//
// Hüküm yalnız bu çözümleyiciden çıkar (canlı araç da kaydı yazıp bununla okur):
//   GECTI     bütünlük tamam ve bütün ölçütler sağlandı
//   GECMEDI   kayıt bütün ve tutarlı; en az bir ölçüt sağlanmadı (kayıp/hata sayacı, iki uç eşleşmesi, takvim,
//             süre, kapasite, zaman aşımı, kampanya profili istenip verilmemesi)
//   GECERSIZ  kayıt bozuk/eksik/kendi içinde tutarsız ya da koşu kesildi (özet yok, kopma, yazma zaman aşımı,
//             koşu kimliği, BRR/baud geri hesabı, hız kabulü, R takvimi, bayt/çerçeve muhasebesi)
//   BASLAMADI kart ya da işletim sistemi hızı kabul etmedi veya yeni hızda eşleme olmadı (hız doğrulanmadı)
//
// Ölçüm profilleri (mimarın sabitlediği koşullar, W0-MP3a-duzeltme-1):
//   mp3-akis-v1  akış, 256 B yük, yön başına 8N1 kapasitesinin %80'i (etkin hız: istenen ile kartın BRR'den
//                hesapladığı hızın küçüğü). %80 üst sınırı SABİTTİR; daha düşük oran "deneysel" profildir.
//   mp3-echo-v1  echo, 64 ya da 256 B, 200 ya da 400 istek/s; fiziksel kapasite sınırı %100.
//   deneysel     başka ayarlar (akışta oran < %80); hüküm verilir ama dondurulmuş kampanya sonucu sayılmaz.
// Takvim toleransı (ölçüm geçerliliği için mimar seçimi; kontrol gecikme gereği değil): her yazma başlangıcının
// planlanan zamandan gecikmesi ≤ 10 ms; son yazmanın tamamlanması planlanan trafik penceresini (adet × aralık)
// en çok max(10 ms, pencerenin %1'i) aşabilir. Kartın akış gönderiminde aynı iki sınır (son çerçevenin gönderim
// halkasına konduğu an).
#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace mp3 {

constexpr double AKIS_ORAN_SINIR = 0.80;       // akış kampanyası, sabit
constexpr double ECHO_KAPASITE_SINIR = 1.00;   // echo, fiziksel
constexpr int64_t TAKVIM_GEC_AZAMI_NS = 10000000;   // 10 ms
constexpr int64_t TAKVIM_PAY_TABAN_NS = 10000000;   // max(10 ms, pencerenin %1'i)

using Alanlar = std::vector<std::pair<std::string, std::string>>;

std::string satir_imzala(const std::string &govde);   // "<govde> *XXXXXXXX"

// Akış için yön başına çerçeve aralığı (ns): oran × (etkin_baud / 10) B/s'yi aşmayan en küçük tam ns.
uint64_t akis_araligi(uint32_t etkin_baud, uint16_t yuk, double oran);
// Echo aralığı (ns): 10¹² / hiz_milli (tam bölünmeli; değilse 0).
uint64_t echo_araligi(uint64_t hiz_milli);
// Profil adı (mp3-akis-v1, mp3-echo-v1, deneysel)
std::string profil_adi(bool echo, uint16_t yuk, uint64_t oran_ppm, uint64_t hiz_milli);
// Takvim payı: max(10 ms, pencere / 100)
int64_t takvim_payi(int64_t pencere_ns);

class KayitYazici {
public:
    bool ac(const std::string &yol, std::string &hata);   // yalnız yoksa oluşturur (O_EXCL)
    void alanli(const std::string &tur, const Alanlar &a);
    void konumsal(const std::string &tur, const std::string &govde);
    bool kapat();
    bool acik() const { return acik_; }

private:
    void yaz(const std::string &tur, const std::string &govde);
    std::ofstream o_;
    uint64_t no_ = 0;
    bool acik_ = false;
};

enum class Hukum { Gecti = 0, Gecmedi = 1, Gecersiz = 2, Baslamadi = 3 };
const char *hukum_ad(Hukum h);

struct AnalizBeklenti {
    double en_az_sn = 0.0;        // koşunun kapsaması gereken en kısa süre (0: denetlenmez)
    bool kampanya = false;        // dondurulmuş kampanya profili zorunlu (deneysel profil GECMEDI)
};

struct AnalizSonuc {
    Hukum hukum = Hukum::Gecersiz;
    std::string profil;
    std::vector<std::string> nedenler;
    std::string rapor;
};

AnalizSonuc analiz_metin(const std::string &icerik, const AnalizBeklenti &b);
AnalizSonuc analiz_dosya(const std::string &yol, const AnalizBeklenti &b);

// Yüzdelik: sıralı dizide ⌈q·n⌉'inci öğe (en yakın sıra yöntemi), q = pay/payda.
int64_t yuzdelik(const std::vector<int64_t> &sirali, uint64_t pay, uint64_t payda);

}  // namespace mp3
