// mcap_kitaplik_okuma.hpp — bir dosyanın `foxglove/mcap` C++ kitaplığının KENDİ okuyucusuyla
// okunmuş dökümü (yalnız testler; kabul 1: yazılan dosya kitaplığın okuyucusuyla hatasız açılır).
// Ürünün okuyucusu bu kitaplığı kullanmaz; burada iki bağımsız okuyucu birbirini sınar.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ida::kayit::test {

struct KitaplikSema {
    std::uint16_t id = 0;
    std::string ad;
    std::string kodlama;
    std::string veri;
};
struct KitaplikKanal {
    std::uint16_t id = 0;
    std::uint16_t sema_id = 0;
    std::string konu;
    std::string ileti_kodlamasi;
    std::map<std::string, std::string> metadata;
};
struct KitaplikIleti {
    std::string konu;
    std::uint32_t sira = 0;
    std::uint64_t log_time = 0;
    std::uint64_t publish_time = 0;
    std::vector<std::uint8_t> veri;
};
struct KitaplikEk {
    std::string ad;
    std::string ortam_turu;
    std::vector<std::uint8_t> veri;
};
struct KitaplikParca {
    std::uint64_t acik_boy = 0;
    std::uint32_t crc = 0;
    std::string sikistirma;
};

struct KitaplikDokumu {
    bool acildi = false;        // open() ve özet okuması (yedek tarama OLMADAN) başarılı
    std::string hata;
    std::vector<std::string> sorunlar;  // kitaplığın bildirdiği sorunlar (ileti okuma sırasında)
    std::string profil;
    std::string kitaplik;
    std::vector<KitaplikSema> semalar;   // özet bölümünden
    std::vector<KitaplikKanal> kanallar; // özet bölümünden
    std::uint64_t istatistik_ileti = 0;
    std::uint64_t istatistik_parca = 0;
    std::vector<KitaplikIleti> iletiler; // kitaplığın `readMessages` sırasıyla
    std::vector<std::pair<std::string, std::map<std::string, std::string>>> ust_veri;  // dosya sırasıyla
    std::vector<KitaplikEk> ekler;
    std::vector<KitaplikParca> parcalar;  // dosya sırasıyla
};

KitaplikDokumu kitaplik_ile_oku(const std::string& yol);

}  // namespace ida::kayit::test
