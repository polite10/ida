// statik.hpp — panellerin statik dosyalarını kök dışına çıkmadan okuma (A3-K11; EMNİYET erişim sınırı).
//
// En kısıtlayan okuma: statik kök altında simgesel bağ ya da (Windows) reparse noktası içeren hiçbir
// yol sunulmaz — bağ kök içini gösterse de. Denetim açılan tanıtıcı üzerinde yapılır (denetim ile
// açma arasında yarış yok):
//   POSIX:   kökten başlayarak her bileşen `openat(..., O_NOFOLLOW)` ile açılır; son bileşen düzenli dosya.
//   Windows: dosya açılır, `GetFinalPathNameByHandleW` ile gerçek yolu alınır ve kökün gerçek yolu +
//            istenen göreli yolla birebir (büyük/küçük harf duyarsız) karşılaştırılır; herhangi bir
//            bileşendeki reparse noktası gerçek yolu değiştireceğinden reddedilir.
#pragma once

#include <cstddef>
#include <string>

namespace ida::a3 {

// `goreli` yalnız [A-Za-z0-9._-] ve '/' içerir, '.' ile başlayan bileşen ve boş bileşen yoktur
// (çağıran denetler). Başarıda dosyanın içeriği; dosya yoksa, düzenli dosya değilse, `azami`'den
// büyükse ya da yol bir bağ/reparse noktası içeriyorsa false.
bool statik_oku(const std::string& kok, const std::string& goreli, std::size_t azami, std::string& icerik);

}  // namespace ida::a3
