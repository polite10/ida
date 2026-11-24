// statik.hpp — panellerin statik dosyalarını kök dışına çıkmadan okuma (A3-K11; W4-K23 1).
//
// Tehdit modeli (W4-K23): saldırgan aynı makinede çalışan, erişim jetonuna sahip olmayan ve statik
// köke (ve üst dizinlerine) yazamayan yerel bir süreçtir. Köke yazabilen biri paneli değiştirip
// jetonu zaten ele geçirebilir; o durum modelin dışındadır. Aşağıdakiler derinlemesine savunmadır.
//
// Kural: kök dizin sunucu başlarken bir kez açılır ve sunucu ömrü boyunca tutulur; her istekte
// yolun bileşenleri bu köke dayanarak, bağ izlenmeden açılır. Bağ, reparse noktası ve birden çok
// bağlantılı (sabit bağlı) dosya sunulmaz (404).
//   POSIX:   kök fd'sine göreli `openat(..., O_NOFOLLOW)` zinciri; son bileşen düzenli dosya ve
//            `st_nlink == 1`.
//   Windows: kök, silme/yeniden adlandırma paylaşımı verilmeden (FILE_SHARE_DELETE yok) açılıp
//            tutulur; tutulan kök yeniden adlandırılamaz ya da silinemez. Win32'de tanıtıcıya göreli
//            açılış olmadığından her istekte kökün gerçek yolu (`\\?\` önekli) yeniden açılır ve birim
//            seri numarası + dosya kimliğiyle tutulan kökle aynı nesne olduğu doğrulanır; ardından
//            her bileşen `FILE_FLAG_OPEN_REPARSE_POINT` ile açılır, reparse özniteliği reddedilir, ara
//            dizinler silme paylaşımı verilmeden okuma sonuna dek tutulur; son dosya
//            `nNumberOfLinks == 1`.
#pragma once

#include <cstddef>
#include <string>

namespace ida::a3 {

class StatikKok {
public:
    StatikKok() = default;
    ~StatikKok();
    StatikKok(const StatikKok&) = delete;
    StatikKok& operator=(const StatikKok&) = delete;

    // Kökü açar ve tutar (kökün kendisi yapılandırmadır; bağ olabilir, açılışta izlenir).
    bool ac(const std::string& kok, std::string& hata);
    bool acik() const noexcept;

    // `goreli` yalnız [A-Za-z0-9._-] ve '/' içerir, '.' ile başlayan ve boş bileşen yoktur (çağıran
    // denetler). Başarıda dosyanın içeriği; dosya yoksa, düzenli dosya değilse, `azami`'den büyükse,
    // yolda bağ/reparse noktası varsa ya da dosya birden çok bağlantılıysa false.
    bool oku(const std::string& goreli, std::size_t azami, std::string& icerik) const;

private:
#if defined(_WIN32)
    void* h_ = nullptr;  // HANDLE (INVALID_HANDLE_VALUE değil: nullptr = kapalı)
    std::wstring yol_;   // kökün gerçek yolu (`\\?\` önekli)
    unsigned long seri_ = 0;
    unsigned long kimlik_yuksek_ = 0;
    unsigned long kimlik_dusuk_ = 0;
#else
    int fd_ = -1;
#endif
};

}  // namespace ida::a3
