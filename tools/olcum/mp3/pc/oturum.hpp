// W0-MP3a ölçüm prototipi: PC tarafı koşu akışı (atılabilir).
//
// Bir koşu: denetim hızında (115200) KIMLIK -> KUR/KUR_YANIT; işletim sisteminde yeni hız; yeni hızda ESLE
// (yinelemeli, sınırlı süre) -> ESLE_YANIT; t0; trafik (akış ya da echo, sabit takvim); boşaltma (sınırlı);
// BITIR -> OZET (sınırlı yineleme); DONUS; denetim hızında KIMLIK ile yeniden eşleme. Her bekleme süre sınırlıdır.
// Sonuç her zaman kayıt dosyasına yazılır ve hükmü kayit.hpp'deki çözümleyici verir.
//
// PC sayaç kapsamı: okuyucu iş parçacığının okuduğu bütün baytlar rx_okunan_bayt. Pencere ilk ESLE_YANIT'tan SONRA
// başlar, ilk OZET dahil biter; pencere sayaçları (rx_*) o anda dondurulur. Öncesi esle_oncesi_bayt (ilk ESLE_YANIT
// dahil), sonrası ozet_sonrasi_bayt (ayrıştırıcıda kalan yarım bayt dahil). rx_kontrol: penceredeki yinelenen
// ESLE_YANIT'lar ve OZET. tx_cerceve/tx_bayt yalnız veri çerçeveleridir.
#pragma once

#include "kayit.hpp"
#include "seri.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace mp3 {

struct Ayar {
    bool echo = false;
    uint32_t baud = 0;
    uint16_t yuk = 256;
    double sure_s = 10.0;
    double akis_oran = AKIS_ORAN_SINIR;   // akış: ≤ %80 (sabit sınır); %80 altı "deneysel" profil
    double echo_hiz = 200.0;              // echo: istek/s
    int zaman_asimi_ms = 1000;            // echo yanıtı, yazma sonundan
    uint32_t azami_bekleyen = 64;         // echo: gönderilmiş, yanıtı gelmemiş ve zaman aşımına uğramamış istek sınırı
    int bosaltma_ms = 2000;               // akış: kartın son planlı çerçevesinden sonra en çok bekleme
    int yazma_ms = 1000;                  // tek yazma çağrısının üst sınırı
    int esle_pc_ms = 3000;
    uint16_t esle_kart_ms = 5000;
    uint16_t bos_kart_ms = 5000;
    int donus_ms = 6000;
    int64_t takvim_kes_ns = 1000000000;   // gecikme bunu aşarsa gönderim kesilir (profil zaten GECMEDI; 10 ms sınırı çözümleyicide)
    std::string kayit;
};

// Echo bekleyen kuyruğu: yalnız GERÇEKTEN gönderilmiş istekler girer; yanıtı gelen ya da (yazma sonundan) zaman
// aşımına uğrayan istek tek kez çıkar. Atlanan sıra kuyruğa girmez.
// Zaman çizelgesi (düzeltme turu 2, sol Y4): k isteğinin karar anı t_k'da (yazma başından hemen önce) j < k bekler
// ⇔ j gönderilmiş, yanıtı t_k'da görünür değil (yanıt_j > t_k; eşitlikte yanıt önce sayılır) ve
// t_k − yazma_sonu_j ≤ zaman_aşımı (eşitlikte hâlâ bekler). Bekleyen ≥ sınır ise k gönderilmez (atlanır).
// Ölçülen doluluk: k gönderildikten SONRA kuyruk boyu (k dahil); azami_bekleyen_gozlenen bunun azamisidir ve
// sınırı aşamaz. Yanıt yazma çağrısı dönmeden gelebilir (yanıt < yazma_sonu); bu durumda k hemen sonraki kararda
// çıkar. Çözümleyici R'den aynı tanımı t_k = yazma_başı ile yeniden üretir; okuyucu iş parçacığının yanıtı
// görünür kılması gecikebildiği için çevrim içi ölçüm yeniden üretilenden KÜÇÜK olamaz, büyük olabilir.
class BekleyenKuyruk {
public:
    explicit BekleyenKuyruk(uint32_t azami) : azami_(azami) {}
    void gonderildi(uint32_t sira, int64_t yazma_sonu_ns) { q_.push_back({sira, yazma_sonu_ns}); }
    void cozumle(int64_t simdi_ns, int64_t zaman_asimi_ns, const std::function<bool(uint32_t)> &yanit_var);
    size_t boyut() const { return q_.size(); }
    bool dolu() const { return q_.size() >= azami_; }

private:
    struct Oge {
        uint32_t sira;
        int64_t ye;
    };
    uint32_t azami_;
    std::deque<Oge> q_;
};

bool kimlik_sor(Port &p, int sure_ms, std::map<std::string, std::string> &kimlik, std::string &ham);

AnalizSonuc kos(Port &p, const Ayar &a, uint32_t kosu, const AnalizBeklenti &bek, std::string &hata);

// Ön tarama: her baud için kısa akış (mp3-akis-v1) ve kısa echo (64 B, 200/s; mp3-echo-v1). Her koşu ayrı kayıt.
// Dönüş: 0 bütün koşular GECTI; 1 tarama tamamlandı ama en az bir aday GECMEDI/BASLAMADI; 2 en az bir koşu GECERSIZ.
struct TaramaSatiri {
    uint32_t baud;
    Hukum akis, echo;
};
int tara(Port &p, const std::vector<uint32_t> &baudlar, double akis_sn, double echo_sn, const std::string &dizin,
         const std::function<uint32_t()> &kosu_uret, std::vector<TaramaSatiri> &tablo, const Ayar &taban);

}  // namespace mp3
