// viz birim testleri için küçük denetim düzeneği (bağımlılık eklememek için; Unity C içindir,
// GoogleTest bu depoya henüz eklenmedi). Her test çalıştırılabilir dosyası 0 ya da 1 döner.
// Makrolar sabit koşullu döngü kullanmaz (MSVC C4127; W3-K14).
#ifndef IDA_VIZ_DENEME_HPP
#define IDA_VIZ_DENEME_HPP

#include <cmath>
#include <cstdio>

namespace deneme {
inline int& hata_sayisi() {
  static int n = 0;
  return n;
}
inline int& denetim_sayisi() {
  static int n = 0;
  return n;
}
inline void denetle(bool kosul, const char* dosya, int satir, const char* metin) {
  ++denetim_sayisi();
  if (!kosul) {
    ++hata_sayisi();
    std::fprintf(stderr, "%s:%d: DENETİM BAŞARISIZ: %s\n", dosya, satir, metin);
  }
}
inline void yakin(double a, double b, double tol, const char* dosya, int satir, const char* ma, const char* mb) {
  ++denetim_sayisi();
  if (!(std::fabs(a - b) <= tol)) {
    ++hata_sayisi();
    std::fprintf(stderr, "%s:%d: YAKIN DEĞİL: %s = %.12g, %s = %.12g (tolerans %g)\n", dosya, satir, ma, a, mb, b, tol);
  }
}
inline int sonuc(const char* ad) {
  std::printf("%s: %d denetim, %d başarısız\n", ad, denetim_sayisi(), hata_sayisi());
  return hata_sayisi() == 0 ? 0 : 1;
}
}  // namespace deneme

#define DENETLE(kosul) deneme::denetle(static_cast<bool>(kosul), __FILE__, __LINE__, #kosul)
#define YAKIN(a, b, tol) \
  deneme::yakin(static_cast<double>(a), static_cast<double>(b), static_cast<double>(tol), __FILE__, __LINE__, #a, #b)
#define DENEME_SONUC(ad) return deneme::sonuc(ad)

#endif
