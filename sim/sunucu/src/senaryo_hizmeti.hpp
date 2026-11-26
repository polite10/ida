// Senaryo ve ön denetim uçlarının sim tarafı veri kaynağı (W5-a3-senaryo): A3 kitaplığının
// `a3::SenaryoHizmeti` arayüzünün `ida_sim` gerçeklemesi.
//
// Yapılan iş (A3 §3.6, §3.8, §4.9; A3-K76…K80):
//   - senaryo dizini (varsayılan `scenarios/`; depo köküne göre göreli): ad kalıbı A3-K78, dizin dışına
//     çıkan ya da düzgün dosya olmayan (simgesel bağ, dizin) hiçbir ad açılmaz;
//   - A5 doğrulaması (`senaryo_coz`): `dogrula` dosyayı ve çalıştırma durumunu değiştirmez;
//   - `senaryo/yukle` adayı: doğrulanmış ve yürütülebilirliği denenmiş senaryonun bayt görüntüsü. Dünya
//     (`Calistirici`) dosyayı yeniden okumaz, adayın görüntüsünü kullanır: doğrulanan bayt ile koşan bayt
//     aynıdır (okuma ile koşu arasında dosya değişse bile);
//   - yüklü çözümlenmiş senaryo (kanonik baytlar, davranış özeti) ve son 64 ön denetim raporu.
//
// İş parçacığı: arayüz işlevleri A3 G/Ç iş parçacığından çağrılır; `yuklu_ayarla`, `on_denetim_ekle`,
// `aday_al`, `yukleme_tamam` ve `yukleme_reddedildi` dünya (ya da sürücü) iş parçacığındandır. Hepsi tek
// kilit altındadır ve kısadır; dünya döngüsünün sınır turunda çağrılmaz (yalnız dünya kurulurken).
#pragma once

#include "ida/a3/senaryo.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ida::sunucu {

// Senaryo dizinindeki ya da panelden gelen metindeki dosyaların üstüne bindirilmiş kaynak: bindirilen yollar
// bellekten, ötekiler (araç dosyası, A7 manifesti) diskten okunur.
class KatmanliKaynak final : public senaryo::DosyaKaynagi {
public:
    KatmanliKaynak(std::filesystem::path kok, std::map<std::string, std::string> katman)
        : disk_(std::move(kok)), katman_(std::move(katman))
    {
    }
    std::optional<std::string> oku(const std::string& yol) const override;
    bool icinde_mi(const std::string& taban_dizin, const std::string& yol) const override
    {
        return disk_.icinde_mi(taban_dizin, yol);
    }

private:
    senaryo::DiskKaynagi disk_;
    std::map<std::string, std::string> katman_;
};

// Yüklenecek senaryonun dünya için görüntüsü.
struct SenaryoGoruntusu {
    std::uint64_t id = 0;
    std::string ad;                                       // dosya adı ya da boş (panelden metin)
    std::string yol;                                      // depo köküne göre göreli, `/` ayırıcılı
    std::string ozet;                                     // "sha256:<64 hex>"
    std::shared_ptr<const senaryo::DosyaKaynagi> kaynak;  // bindirmeli kaynak
};

class SenaryoHizmeti final : public a3::SenaryoHizmeti {
public:
    struct Ayar {
        std::string depo_koku = ".";
        std::string dizin = "scenarios";    // depo köküne göre göreli, `/` ayırıcılı
        double sil_hiz = 0.0;               // plan denemesi için (yalnız uyarı metnini etkiler)
    };
    explicit SenaryoHizmeti(Ayar ayar);

    // Dizin ayarının denetimi: göreli, `..` ve mutlak yol yok. Boş dönüş: geçerli; değilse neden.
    static std::string dizin_denetle(const std::string& dizin);

    // ---- a3::SenaryoHizmeti (A3 G/Ç iş parçacığı) ----
    std::vector<a3::SenaryoDosyasi> listele() override;
    a3::DosyaDurumu oku(std::string_view ad, std::string& icerik, std::string& ileti) override;
    a3::SenaryoDogrulama dogrula(const a3::SenaryoGirdisi& g) override;
    a3::SenaryoAdayi hazirla(const a3::SenaryoGirdisi& g) override;
    void aday_birak(std::uint64_t aday_id) override;
    a3::YuklemeSonucu yukleme_bekle(std::uint64_t aday_id, std::chrono::milliseconds sure) override;
    bool cozumlenmis(std::string& bayt, std::string& ozet) override;
    bool on_denetim(std::string_view kimlik, std::string& rapor_json) override;

    // ---- dünya / sürücü ----
    std::shared_ptr<const SenaryoGoruntusu> aday_al(std::uint64_t aday_id) const;
    void yukleme_tamam(std::uint64_t aday_id);
    void yukleme_reddedildi(std::uint64_t aday_id, std::string hata_kodu, std::string ileti);
    // Yüklü çözümlenmiş senaryo (boş `kanonik`: yok, ör. yerleşik senaryo).
    void yuklu_ayarla(std::string kanonik, std::string ozet);
    void on_denetim_ekle(const std::string& kimlik, const std::string& json);
    // Sunucu kapanıyor: `yukleme_bekle`de bekleyen çağrılar serbest kalır (ret, `mesgul`).
    void kapaniyor();

    // Yalnız sınama: dizin yolu.
    std::filesystem::path dizin_yolu() const { return dizin_; }
    // Yalnız sınama: son `hazirla` içinde A5 çözümlemesinin ve plan denemesinin duvar süresi (ns).
    std::int64_t son_hazirlama_ns() const;

private:
    struct Cozum;
    // `girdi` → dosya durumu, kaynak, yol ve A5 çözümü. Dosya sistemine yalnız ad kalıba uyuyorsa gidilir.
    a3::DosyaDurumu girdiyi_coz_(const a3::SenaryoGirdisi& g, std::string& ileti, std::string& yol, std::string& ad,
                                 std::shared_ptr<const senaryo::DosyaKaynagi>& kaynak, senaryo::CozumSonucu& cs);
    // Düzgün (simgesel bağ olmayan) bir dosyayı sınırlı boyda okur.
    a3::DosyaDurumu dosya_oku_(const std::string& ad, std::string& icerik, std::string& ileti) const;

    Ayar ayar_;
    std::filesystem::path dizin_;      // depo_koku / dizin

    mutable std::mutex m_;
    std::condition_variable cv_;
    std::uint64_t sonraki_aday_ = 1;
    std::map<std::uint64_t, std::shared_ptr<const SenaryoGoruntusu>> adaylar_;
    std::map<std::uint64_t, a3::YuklemeSonucu> sonuclar_;
    std::string yuklu_kanonik_;
    std::string yuklu_ozet_;
    std::deque<std::pair<std::string, std::string>> raporlar_;   // (kimlik, json); en yeni sonda; en çok 64
    std::int64_t son_hazirlama_ns_ = 0;
    bool kapaniyor_ = false;
};

}  // namespace ida::sunucu
