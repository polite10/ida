// A6 kayıt kitaplığının (`sim/kayit`, `ida_kayit`) `ida_sim`'e bağlanması (W5-bagla-kayit): açılış üst
// verisi ve ekleri, tek üretici (dünya iş parçacığı), kapanış alanları ve güvence tavanı.
//
// Sözleşme: docs/sozlesmeler/A6-kayit.md (A6-K01, K05, K06, K12, K18, K33, K44–K46, K53; §3.5, §3.6).
// Mimar kararları: W5-K40 (kayıt yalnız `--kayit-dizini` ile), W5-K41 (üretilmeyen kanal için güvence
// iddia edilmez), W5-K42 (A3'e giden yapı aynen kayda da verilir).
//
// İş parçacığı: `ac`, `taslak_guncelle`, `guncelle` ve `kapat` dünya iş parçacığındandır; yazım hedefi
// kitaplığın kendi iş parçacığıdır. Üretici çağrısı (`uretici().yaz`) beklemez, kilit almaz, bellek
// ayırmaz ve dosyaya dokunmaz (kitaplığın sözü; `sim.kayit.uretici_ayirmaz`, `uretici_beklemez`).
#pragma once

#include "ida/a3/kodlayici.hpp"
#include "ida/kayit/yazici.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ida::sunucu {

// Senaryo dosyasından gelen, çalıştırma boyunca değişmeyen kısım (yalnız kayıt açıkken kurulur).
struct KayitGirdisi {
    std::string kademe = "telemetri";         // kalite.kayit
    std::string kamera_kareleri = "kapali";   // kalite.kamera_kareleri
    std::string kademeler;                    // gorunum=..,kamera=..,sonar=..,kayit=..
    std::string a5_sema_surumu;               // meta.sema_surumu
    std::string kanonik;                      // cozumlenmis_senaryo.json (RFC 8785 baytları)
    std::string dogrulama_raporu;             // dogrulama_raporu.json
    std::string davranis_ozeti;               // 64 onaltılık (A5-K17)
    std::map<std::string, std::string> kaynak_ozetleri;   // dosya → SHA-256 (64 onaltılık)
    std::string manifest;                     // A7 manifesti, ham baytlar
    std::string manifest_ozeti;               // SHA-256 (64 onaltılık)
    std::string kume;                         // varliklar.kume
    std::string kume_surumu;                  // manifestin kume_surumu'u (onluk)
};

// Doğrulanmış senaryodan `KayitGirdisi`. false: `hata` dolu (manifest okunamadı, özet uyuşmuyor vb.).
bool kayit_girdisi_kur(const senaryo::DogrulanmisSenaryo& cozumlenmis, const senaryo::DogrulamaRaporu& rapor,
                       const senaryo::DosyaKaynagi& kaynak, KayitGirdisi& girdi, std::string& hata);

struct KayitBaglantiAyari {
    std::filesystem::path dizin;
    std::string run_id;       // A3-K63 metin kimliği (dosya adı)
    std::string a1_run_id;    // onluk u32 (k_->run_id())
    std::string kip;          // sil | hil_1x
    std::string tohum;        // onluk
    std::string sil_n_bg;     // SIL'de zorunlu (onluk)
    std::string depo_koku;    // protocol/komutlar.json ve protocol/ida.xml için
    std::string a2_port;      // A2 gerçekten dinliyorsa onluk port; yoksa boş (koşullu anahtar)
    bool a3_acik = false;     // A3 durum makinesine bağlı kanallar yalnız A3 açıkken bağlıdır
    bool sonar_acik = false;
    std::size_t halka_bayt = 0;   // 0: 16 MiB (tahmin)
    KayitGirdisi girdi;
};

class KayitBaglanti {
public:
    // nullptr: açılamadı (`hata` dolu; ör. aynı adlı dosya var: A6-K01, ek okunamadı).
    static std::unique_ptr<KayitBaglanti> ac(const KayitBaglantiAyari& ayar, std::string& hata);
    ~KayitBaglanti();
    KayitBaglanti(const KayitBaglanti&) = delete;
    KayitBaglanti& operator=(const KayitBaglanti&) = delete;

    // Dünya iş parçacığının üreticisi. Hiçbir çağrı beklemez.
    template <kayit::Kanal K>
    void yaz(std::int64_t sim_t_ns, std::uint64_t adim, const typename kayit::KanalYuku<K>::tip& y) noexcept
    {
        (void)u_->yaz<K>(sim_t_ns, adim, y);
        isaretle_(K);
    }

    // Kapanış taslağı (A6 §4 kapanış; A6-K53): 1 Hz'te ve güvenceyi değiştiren bir olayda hemen.
    void taslak_guncelle(kayit::KapanisAlanlari a);

    // Kapanış (A6-K44): A1 kapanışından sonra, dünya iş parçacığından. İkinci çağrı aynı sonucu verir.
    kayit::KapanisSonucu kapat(kayit::KapanisAlanlari alanlar);

    kayit::YaziciDurumu durum() const noexcept { return y_->durum(); }
    kayit::YaziciSayaclari yazici_sayaclari() const noexcept { return y_->sayaclar(); }
    kayit::UreticiSayaclari uretici_sayaclari() const noexcept { return u_->sayaclar(); }
    const std::filesystem::path& yol() const noexcept { return y_->yol(); }
    kayit::Kademe kademe() const noexcept { return y_->kademe(); }
    bool kapandi() const noexcept { return kapandi_; }

    // Üretici çağrısı almış ve kademe ile kipe göre dosyaya yazılan kanalların adları (katalog sırası; bağlı
    // olsa da çalıştırmada hiç olay olmayan kanal görünmez).
    std::vector<std::string> uretilen_kanallar() const;
    // Kademe ve kipin gerektirdiği (`kanal_etkin`) ama bu bağlamada KODDA BAĞLI OLMAYAN kanallar (statik
    // küme: A3 açık/kapalı ve sonar açık/kapalı; çalıştırmada olay olup olmadığına bağlı değildir).
    // `yalniz_tekrar_duzeyi`: yalnız `fizik` ya da `girdi` düzeyinin gerektirdiği kanallar (W5-K41).
    std::vector<std::string> eksik_kanallar(bool yalniz_tekrar_duzeyi = false) const;

    // Kapanış alanlarına konan tavan (W5-K41): üretilmeyen kanal varsa `izleme`.
    static void tavan_uygula(const std::vector<std::string>& eksik, kayit::KapanisAlanlari& a);

private:
    KayitBaglanti() = default;
    void isaretle_(kayit::Kanal k) noexcept { uretildi_[static_cast<std::size_t>(k)] = true; }

    std::unique_ptr<kayit::Yazici> y_;
    kayit::Uretici* u_ = nullptr;
    std::string kip_;
    bool uretildi_[kayit::kKanalSayisi] = {};
    bool a3_acik_ = false;
    bool sonar_acik_ = false;
    bool kapandi_ = false;
    kayit::KapanisSonucu sonuc_;
};

// Kapanış kaydının `sayaclar` alanı: `yuk_sayac_gercek_zaman`'ın üst düzey tekil alanları, `ad=deger`,
// virgülle ayrılmış (A6 §3.5).
std::string kayit_sayac_listesi(const a3::yuk_sayac_gercek_zaman& y);

}  // namespace ida::sunucu
