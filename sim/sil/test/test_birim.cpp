// test_birim.cpp — türetme vektörleri, CRC, kalıcı depo ve komut satırı.
#include <cstdio>
#include <fstream>
#include <string>

#include "depo.hpp"
#include "komut_satiri.hpp"
#include "test_destek.hpp"
#include "turetme.hpp"

namespace ida::sil::test {

int test_turetme_vektorleri(const Baglam&)
{
    // SHA-256("abc") (FIPS 180-4 örneği).
    {
        const std::uint8_t abc[] = {'a', 'b', 'c'};
        const auto o = turetme::sha256(abc, sizeof abc);
        KONTROL(o[0] == 0xbau && o[1] == 0x78u && o[2] == 0x16u && o[3] == 0xbfu &&
                o[31] == 0xadu);
        // 56 baytlık iki bloklu örnek: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
        const std::string s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        const auto o2 =
            turetme::sha256(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
        KONTROL(o2[0] == 0x24u && o2[1] == 0x8du && o2[2] == 0x6au && o2[31] == 0xc1u);
    }
    // A5 Ek D.2: Philox4x32-10 bilinen yanıtları (Random123 kat_vectors).
    {
        const auto a = turetme::philox({0u, 0u, 0u, 0u}, {0u, 0u});
        KONTROL(a[0] == 0x6627e8d5u && a[1] == 0xe169c58du && a[2] == 0xbc57ac4cu &&
                a[3] == 0x9b00dbd8u);
        const auto b = turetme::philox({0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu},
                                       {0xffffffffu, 0xffffffffu});
        KONTROL(b[0] == 0x408f276du && b[1] == 0x41c83b0eu && b[2] == 0xa20bc7c6u &&
                b[3] == 0x6d5451fdu);
        const auto c = turetme::philox({0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u},
                                       {0xa4093822u, 0x299f31d0u});
        KONTROL(c[0] == 0xd16cfe09u && c[1] == 0x94fdccebu && c[2] == 0x5001e420u &&
                c[3] == 0x24126ea1u);
    }
    // A5 Ek D.7 (ana_tohum = 1).
    {
        const auto k = turetme::anahtar(1u, "a1/board_tohum");
        KONTROL(k[0] == 0xca3d4094u && k[1] == 0x4d0ec9efu);
        KONTROL(turetme::u64(1u, "a1/board_tohum") == UINT64_C(0xb1536801d0cf436c));
        KONTROL(turetme::u32_sifirsiz(1u, "a1/run_id") == 0x3f25d322u);
        KONTROL(turetme::u32_sifirsiz(1u, "ida/a1/nonce_sim/0") == 0xe6a501d3u);
        const std::uint64_t tohum = UINT64_C(0xb1536801d0cf436c);
        const auto kb = turetme::anahtar(tohum, turetme::nonce_board_etiketi(1u, 0u));
        KONTROL(kb[0] == 0x6c316b29u && kb[1] == 0xd1b8de2fu);
        KONTROL(turetme::nonce_board_etiketi(1u, 0u) == "ida/a1/nonce_board/1/0");
        KONTROL(turetme::u32_sifirsiz(tohum, turetme::nonce_board_etiketi(1u, 0u)) ==
                0x0d2c29dcu);
        // sess: w0 & 0xFFFF; n = 0 -> 18101, n = 1 -> 6699.
        KONTROL((turetme::cek(1u, "ida/a1/sess/0", 0u)[0] & 0xFFFFu) == 18101u);
        KONTROL((turetme::cek(1u, "ida/a1/sess/0", 1u)[0] & 0xFFFFu) == 6699u);
    }
    return hata_sayisi();
}

int test_crc32(const Baglam&)
{
    const std::string s = "123456789";
    KONTROL(crc32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()) == 0xCBF43926u);
    KONTROL(crc32(nullptr, 0u) == 0u);
    return hata_sayisi();
}

namespace {

std::vector<std::uint8_t> dosya(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f),
                                     std::istreambuf_iterator<char>());
}

void dosya_yaz(const std::filesystem::path& p, const std::vector<std::uint8_t>& v)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size()));
}

}  // namespace

int test_depo(const Baglam& b)
{
    const std::filesystem::path yol = b.gecici / "depo.bin";
    const std::vector<std::uint8_t> blob = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    // Yeni dosya: sayaç onceki_acilis'ten.
    {
        Depo d(yol.string(), 4u);
        KONTROL(d.yukle() == Depo::Yukleme::DosyaYok);
        const auto a = d.acilis_ayir();
        KONTROL(a.ok && a.boot_count == 5u);
        std::uint32_t crc = 1, len = 1, gen = 1;
        d.kalici_blok(&crc, &len, &gen);
        KONTROL(crc == 0u && len == 0u && gen == 0u);
        const auto s = d.kaydet(blob.data(), static_cast<std::uint32_t>(blob.size()), 7u);
        KONTROL(s.store == IDA_PSTORE_OK && s.fail == IDA_PSFAIL_NONE);
        const auto big = d.kaydet(blob.data(), IDA_PARAM_BLOB_MAX + 1u, 8u);
        KONTROL(big.store == IDA_PSTORE_FAIL && big.fail == IDA_PSFAIL_TOO_BIG);
        d.kalici_blok(&crc, &len, &gen);
        KONTROL(len == blob.size() && gen == 7u && crc == crc32(blob.data(), blob.size()));
    }
    // Yeniden açılış: sayaç ve blok korunur.
    {
        Depo d(yol.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::Tamam);
        KONTROL(d.blok().has_value() && d.blok()->veri == blob && d.blok()->gen == 7u);
        const auto a = d.acilis_ayir();
        KONTROL(a.ok && a.boot_count == 6u);
    }
    // Blok baytı bozuk: sayaç geçerli, blok yok sayılır.
    {
        auto v = dosya(yol);
        KONTROL(v.size() == 44u + blob.size());
        v[46] = static_cast<std::uint8_t>(v[46] ^ 0x40u);
        dosya_yaz(yol, v);
        Depo d(yol.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::BlokBozuk);
        KONTROL(!d.blok().has_value());
        const auto a = d.acilis_ayir();
        KONTROL(a.ok && a.boot_count == 7u);
    }
    // Sayaç kesimi bozuk: boot_count_ok = 0 ve öyle kalır (kısıtlayan).
    {
        auto v = dosya(yol);
        v[17] = static_cast<std::uint8_t>(v[17] ^ 0x01u);
        dosya_yaz(yol, v);
        Depo d(yol.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::SayacBozuk);
        KONTROL(!d.acilis_ayir().ok);
        Depo d2(yol.string(), 0u);
        (void)d2.yukle();
        KONTROL(!d2.acilis_ayir().ok);
    }
    // Tanınmayan dosya.
    {
        dosya_yaz(yol, {'b', 'o', 'z', 'u', 'k'});
        Depo d(yol.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::DosyaBozuk);
        KONTROL(!d.blok().has_value());
        KONTROL(!d.acilis_ayir().ok);
    }
    // Yazılamayan yol: sayaç ayrılamaz, kayıt FAIL/FLASH; bellek içi blok değişmez.
    {
        const std::filesystem::path yok = b.gecici / "olmayan_dizin" / "depo.bin";
        Depo d(yok.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::DosyaYok);
        KONTROL(!d.acilis_ayir().ok);
        const auto s = d.kaydet(blob.data(), static_cast<std::uint32_t>(blob.size()), 1u);
        KONTROL(s.store == IDA_PSTORE_FAIL && s.fail == IDA_PSFAIL_FLASH);
        KONTROL(!d.blok().has_value());
    }
    // Bellek içi depo.
    {
        Depo d("", 0u);
        KONTROL(d.yukle() == Depo::Yukleme::DosyaYok);
        KONTROL(d.acilis_ayir().boot_count == 1u);
        KONTROL(d.acilis_ayir().boot_count == 2u);
        KONTROL(d.kaydet(blob.data(), 3u, 2u).store == IDA_PSTORE_OK);
        KONTROL(d.blok().has_value() && d.blok()->veri.size() == 3u);
    }
    return hata_sayisi();
}

int test_komut_satiri(const Baglam&)
{
    {
        const char* a[] = {"ida_sil", "--a1-port", "4567", "--a1-seed", "b1536801D0CF436C",
                           "--n-bg", "3", "--depo", "x.bin", "--esc", "1000,1500,2000",
                           "--gunluk", "2", "--olcum"};
        const auto k = komut_satiri_ayristir(static_cast<int>(sizeof a / sizeof a[0]), a);
        KONTROL(k.hata.empty());
        KONTROL(k.ayar.a1_port == 4567u && k.ayar.a1_tohum == UINT64_C(0xb1536801d0cf436c));
        KONTROL(k.ayar.n_bg == 3u && k.ayar.depo_yolu == "x.bin" && k.ayar.gunluk == 2);
        KONTROL(k.ayar.esc_alt_us == 1000u && k.ayar.esc_notr_us == 1500u &&
                k.ayar.esc_ust_us == 2000u && k.ayar.olcum);
    }
    const auto hatali = [](std::vector<const char*> v) {
        return !komut_satiri_ayristir(static_cast<int>(v.size()), v.data()).hata.empty();
    };
    KONTROL(hatali({"ida_sil"}));                                    // port zorunlu
    KONTROL(hatali({"ida_sil", "--a1-port", "0"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "65536"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "-1"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "1", "--a1-seed", "123"}));   // 16 hane değil
    KONTROL(hatali({"ida_sil", "--a1-port", "1", "--a1-seed", "g000000000000000"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "1", "--esc", "1500,1500,1900"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "1", "--gunluk", "4"}));
    KONTROL(hatali({"ida_sil", "--a1-port", "1", "--bilinmeyen", "1"}));
    KONTROL(hatali({"ida_sil", "--a1-port"}));
    {
        const char* a[] = {"ida_sil", "--surum"};
        const auto k = komut_satiri_ayristir(2, a);
        KONTROL(k.hata.empty() && k.surum);
    }
    return hata_sayisi();
}

}  // namespace ida::sil::test
