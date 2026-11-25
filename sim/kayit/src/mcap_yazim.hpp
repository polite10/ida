// mcap_yazim.hpp — `foxglove/mcap` C++ yazıcısının ince sarmalayıcısı (A6-K03).
//
// Kitaplığın başlıkları yalnız `mcap_yazim.cpp`'de içerilir (ayrı hedef `ida_kayit_mcap`; üçüncü
// taraf başlıkları İDA uyarı politikasıyla derlenmez). Bu başlık mcap tiplerini göstermez.
// Sarmalayıcı iş parçacığı güvenli değildir: yalnız kayıt yazıcısının iş parçacığı kullanır (A6-K05).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ida::kayit::ic {

using AnahtarDeger = std::vector<std::pair<std::string, std::string>>;

// Bayt hedefi. Kitaplığın yazım arayüzü hata döndürmez; hatayı uygulayan taraf izler.
class McapCikti {
public:
    virtual ~McapCikti() = default;
    virtual void yaz(const std::uint8_t* veri, std::uint64_t n) = 0;
    virtual std::uint64_t boyut() const = 0;  // o ana kadar `yaz`'a verilen toplam bayt
    virtual void bosalt() = 0;                // kitaplık her parçadan sonra çağırır
};

// Yazma hatasından sonra dosyanın elle yazılan kuyruğundaki `kayit/eksik` iletileri için.
struct KuyrukKanali {
    std::uint16_t sema_id = 0;
    std::uint16_t kanal_id = 0;
    std::string sema_adi;
    std::string sema_kodlamasi;
    std::string sema_verisi;
    std::string konu;
    std::string ileti_kodlamasi;
    AnahtarDeger metadata;
};
struct KuyrukIletisi {
    std::uint32_t sira = 0;
    std::uint64_t log_time = 0;
    std::uint64_t publish_time = 0;
    std::vector<std::uint8_t> veri;
};

class McapYazim {
public:
    McapYazim();
    ~McapYazim();
    McapYazim(const McapYazim&) = delete;
    McapYazim& operator=(const McapYazim&) = delete;

    // Sihirli baytları ve Header'ı yazar. Sıkıştırma zstd, parça CRC'si ve ek CRC'si açık
    // (A6-K51), ileti indeksleri ve özet kitaplık varsayılanlarıyla (A6-K03).
    void ac(McapCikti& cikti, std::string_view profil, std::uint64_t parca_hedefi_bayt);
    // Şema ve kanal kaydı; dönen değer MCAP kimliğidir (1'den).
    std::uint16_t sema_ekle(std::string_view ad, std::string_view kodlama, std::string_view veri);
    std::uint16_t kanal_ekle(std::string_view konu, std::string_view ileti_kodlamasi, std::uint16_t sema_id,
                             const AnahtarDeger& metadata);
    bool ileti_yaz(std::uint16_t kanal_id, std::uint32_t sira, std::uint64_t log_time, std::uint64_t publish_time,
                   const std::uint8_t* veri, std::size_t n);
    bool ust_veri_yaz(std::string_view ad, const AnahtarDeger& icerik);
    bool ek_yaz(std::string_view ad, std::string_view ortam_turu, std::uint64_t log_time, std::uint64_t create_time,
                const std::uint8_t* veri, std::uint64_t n);
    void parca_kapat();  // açık parçayı dosyaya yazar (`McapWriter::closeLastChunk`; A6-K04)
    void kapat();        // Data End, özet, Footer, sondaki sihirli baytlar (A6-K44)
    void birak();        // hiçbir şey yazmadan bırakır (yazma hatasından sonra)

    // Özet bölümü olmayan elle kapanış (yazma hatasından sonra; A6-K31): varsa `kayit/eksik`
    // iletilerini taşıyan sıkıştırmasız, CRC'li bir parça, `ida_kayit_kapanis` üst verisi, Data End,
    // Footer (özet konumları 0) ve sihirli baytlar.
    static void kuyruk_yaz(McapCikti& cikti, const KuyrukKanali& kanal, std::span<const KuyrukIletisi> iletiler,
                           std::string_view ust_veri_adi, const AnahtarDeger& ust_veri);

    static std::string mcap_surumu();  // kitaplığın `MCAP_LIBRARY_VERSION` değeri
    static std::string zstd_surumu();  // `ZSTD_versionString()`

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::kayit::ic
