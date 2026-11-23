// depo.hpp — SIL koşucusunun kalıcı deposu: açılış sayacı (A4-K86) ve kalıcı parametre bloğu
// (A4-K82 … K84). Kartta flash'ın yaptığını dosyayla yapar; yol verilmezse bellekte tutar
// (süreç içinde REBOOT'a dayanır, süreç bitince kaybolur).
//
// Dosya biçimi (küçük uçlu; bu koşucunun seçimi, sözleşme değildir):
//   [0,8)   "IDA-SIL1"
//   [8,12)  sürüm = 1
//   [12,16) bayraklar: bit 0 = açılış sayacı geçerli
//   [16,20) son verilen boot_count
//   [20,24) CRC-32/ISO-HDLC [0,20)                       -> sayaç kesimi
//   [24,28) blok var (0/1)
//   [28,32) gen
//   [32,36) len (<= IDA_PARAM_BLOB_MAX)
//   [36,40) CRC-32/ISO-HDLC(blob)  (A1-K68 param_blob_crc)
//   [40,44) CRC-32/ISO-HDLC [24,40)                      -> blok kesimi
//   [44,44+len) blob
// İki kesim ayrı denetlenir: bozuk blok sayacı, bozuk sayaç bloğu geçersiz kılmaz.
//
// Yazım kesintiye dayanıklıdır (A4-K84): içerik "<yol>.yeni"ye yazılır ve aygıta kadar
// kalıcılaştırılır (macOS F_FULLFSYNC, yoksa fsync; Windows FlushFileBuffers), geri okunup
// doğrulanır, eskinin üzerine adlandırılır (POSIX rename; Windows MoveFileExW
// REPLACE_EXISTING | WRITE_THROUGH) ve POSIX'te dizin kalıcılaştırılır. Herhangi bir adım
// başarısızsa OK verilmez; adlandırmadan önceki bir hatada eski dosya aynen kalır.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ida::sil {

// CRC-32/ISO-HDLC (yansıtılmış 0xEDB88320, başlangıç ve son XOR 0xFFFFFFFF).
std::uint32_t crc32(const std::uint8_t* p, std::size_t n);

class Depo {
public:
    // Dosyanın okunma sonucu.
    enum class Yukleme {
        DosyaYok,     // yol verilmedi ya da dosya yok: sayaç onceki_acilis'ten, blok yok
        Tamam,        // sayaç ve (varsa) blok geçerli
        BlokBozuk,    // sayaç geçerli, blok kesimi bozuk: blok yok sayılır
        SayacBozuk,   // sayaç kesimi bozuk (blok geçerli olabilir)
        DosyaBozuk    // tanınmayan dosya: sayaç ve blok geçersiz
    };

    struct Acilis {
        std::uint32_t boot_count = 0;
        bool ok = false;
    };

    struct Blok {
        std::vector<std::uint8_t> veri;
        std::uint32_t gen = 0;
        std::uint32_t crc = 0;
    };

    // param_store / param_fail değerleri (IDA_PSTORE_*, IDA_PSFAIL_*).
    struct KayitSonucu {
        std::uint8_t store = 0;
        std::uint8_t fail = 0;
    };

    // yol boşsa bellek içi depo. onceki_acilis: dosya yoksa son verilen boot_count (A1-K114).
    Depo(std::string yol, std::uint32_t onceki_acilis);

    Yukleme yukle();

    // A4-K86: yeni boot_count'u ayırır ve (dosyalı depoda) yazar. Sayaç geçersizse, tükendiyse
    // ya da yazılamadıysa ok = false (olay kimliği kullanılmaz; kısıtlayan okuma).
    Acilis acilis_ayir();

    const std::optional<Blok>& blok() const { return blok_; }

    // PARAM_SAVE (A4-K82, K83): bloğu bayt bayt saklar. TOO_BIG, FLASH (yazılamadı), VERIFY
    // (geri okunan farklı) ya da OK. Başarısızlıkta eski blok aynen kalır.
    KayitSonucu kaydet(const std::uint8_t* veri, std::uint32_t len, std::uint32_t gen);

    // IDA_BOARD_HELLO / IDA_STATUS için (A1-K68); blok yoksa 0, 0, 0.
    void kalici_blok(std::uint32_t* crc, std::uint32_t* len, std::uint32_t* gen) const;

    bool dosyali() const { return !yol_.empty(); }

    // Dosyaya yazımın adımları (sırasıyla). Sınama: adımların izi ve tek adımda hata
    // enjeksiyonu (o adım başarısız sayılır).
    enum class Adim { Yazma, Kalicilastirma, Dogrulama, YenidenAdlandirma, DizinKaliciligi };
    void hata_enjekte(std::optional<Adim> a) { hata_ = a; }
    const std::vector<Adim>& adim_izi() const { return iz_; }
    const std::string& yol() const { return yol_; }

private:
    std::vector<std::uint8_t> kodla(const std::optional<Blok>& b) const;
    bool atomik_yaz(const std::vector<std::uint8_t>& icerik, bool* dogrulama_hatasi);
    bool adim(Adim a);

    std::string yol_;
    std::optional<std::uint32_t> sayac_;  // son verilen boot_count; yok: geçersiz
    std::optional<Blok> blok_;
    std::optional<Adim> hata_;
    std::vector<Adim> iz_;
};

}  // namespace ida::sil
