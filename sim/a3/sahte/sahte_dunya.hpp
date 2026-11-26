// sahte_dunya.hpp — A3 sunucusunu sınamak için sahte veri kaynağı ve komut alıcısı (W4-a3 kapsam 9).
//
// Değerler fizik değildir: araç sabit derinlikte bir çember çizer. Çalıştırma durum makinesi
// (Tablo 4.7a'nın canlı çalıştırma kısmı), emniyet revizyonu ve fizik sınırındaki emniyet sırası
// (A3-K95: önce kısıtlayanlar, aynı sınırda revizyon arttıysa gevşetmeler `kisitlama_hakim`) sim'in
// yerine taklit edilir; gerçek sim bağlandığında bu sınıfın yerini sim alır. Yalnız genel API'yi
// (ida/a3/*.hpp) kullanır.
#pragma once

#include "ida/a3/kaynak.hpp"
#include "ida/a3/komut.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ida::a3::sahte {

struct SahteAyar {
    ZamanKipi kip = ZamanKipi::sil;
    std::size_t genel_kuyruk = 256;     // A3-K59
    std::size_t kisitlayan_kuyruk = 16; // A3-K59 "ayrılmış yuva" (bu sahte sim'in seçimi)
    bool is_parcacigi = true;           // false: sınamalar adim() ile elle ilerletir
    bool veri = true;                   // telemetri üret
    std::uint32_t emniyet_rev_baslangic = 1;
    bool kisitlayani_reddet = false;    // yalnız sınama: kısıtlayanı adım sınırında reddet
};

// Uygulanan bir komutun izi (sınamalar için).
struct UygulamaIzi {
    std::uint32_t komut_no = 0;
    KomutTuru tur = KomutTuru::calistirma_baslat;
    bool kisitlayan = false;
    bool uygulandi = false;
    std::string hata_kodu;
    std::uint32_t emniyet_rev_sonra = 0;
};

class SahteDunya final : public KomutAlicisi {
public:
    SahteDunya(Yayinci& yayinci, SahteAyar ayar);
    ~SahteDunya() override;
    SahteDunya(const SahteDunya&) = delete;
    SahteDunya& operator=(const SahteDunya&) = delete;

    void baslat();
    void durdur();

    AnlikDurum anlik() override;
    AlimSonucu al(const Komut& k) override;
    std::optional<AlimSonucu> on_denetim(const Komut& k) override;   // W5-K57

    // Bir dünya döngüsü (5 ms, dört fizik adımı): önce adım sınırında komutlar, sonra yayın.
    void adim();
    // Sınamalar: dünya kuyruğu boşaltmayı bırakır (takılmış dünya benzetimi).
    void dondur(bool d);
    std::vector<UygulamaIzi> iz();
    std::vector<Komut> alinanlar();  // kuyruğa alınan komutlar, alınış sırasıyla
    // Ölçüm: yayın çağrılarının (Yayinci::*) en uzun süresi, ns; ve toplam çağrı sayısı.
    std::uint64_t en_kotu_yayin_ns() const noexcept { return en_kotu_ns_.load(std::memory_order_relaxed); }
    std::uint64_t yayin_sayisi() const noexcept { return yayin_sayisi_.load(std::memory_order_relaxed); }
    std::uint64_t toplam_yayin_ns() const noexcept { return toplam_ns_.load(std::memory_order_relaxed); }
    // 50 us'yi aşan yayın çağrısı sayısı (dağılımın kuyruğu için).
    std::uint64_t uzun_yayin() const noexcept { return uzun_.load(std::memory_order_relaxed); }
    void olcum_sifirla() noexcept
    {
        en_kotu_ns_.store(0);
        yayin_sayisi_.store(0);
        toplam_ns_.store(0);
        uzun_.store(0);
    }
    // Yük sınaması: her döngüde ek Ping360 satırı sayısı.
    void ek_ping360(int n) { ek_ping_.store(n, std::memory_order_relaxed); }
    // Dünya döngüsü (5 ms) sayısı; fizik/durum her 4. döngüde yayınlanır.
    std::uint64_t dongu_sayisi() const noexcept { return dongu_sayaci_.load(std::memory_order_relaxed); }

private:
    struct Durum;
    std::optional<AlimSonucu> calistirma_denetle_(const Komut& k);   // `al` ve `on_denetim` ortak denetimi
    void uygula_();
    void yayinla_();
    void olay_(std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint32_t komut_no, std::uint32_t istemci,
               const std::string& ayrinti);
    void sonuc_(const Komut& k, bool uygulandi, const std::string& kod, const std::string& ayrinti);
    void gecis_(std::uint8_t yeni, const char* neden);
    template <typename F>
    void olc_(F&& f);

    Yayinci& y_;
    SahteAyar ayar_;
    std::mutex kilit_;
    std::unique_ptr<Durum> d_;
    std::thread is_;
    std::atomic<bool> dur_{false};
    std::atomic<std::uint64_t> en_kotu_ns_{0};
    std::atomic<std::uint64_t> yayin_sayisi_{0};
    std::atomic<std::uint64_t> toplam_ns_{0};
    std::atomic<std::uint64_t> uzun_{0};
    std::atomic<int> ek_ping_{0};
    std::atomic<std::uint64_t> dongu_sayaci_{0};
};

}  // namespace ida::a3::sahte
