// Duvar saatine bağlı denetimlerin ortak yardımcısı (W4-K24, W4-K29). Gerçek zamanlı koşunun kaçırma
// sayısı, periyot ve gecikme üst sınırları gibi eşikler yalnız geliştirme Mac'inde, hedef Windows
// bilgisayarında ve kartta, en iyileştirilmiş (`NDEBUG` tanımlı, release) derlemede kabul ölçütüdür.
// İyileştirmesiz (debug; `NDEBUG` yok) derlemede (W4-K29), paylaşılan CI koşucusunda
// (`GITHUB_ACTIONS` ortam değişkeni tanımlı) ve sanitizer'lı derlemede (ASan/TSan koşuyu
// yavaşlatır) ölçülür ve yazdırılır ama ölçüt sayılmaz (W4-K24). Muafiyet kararı yalnız buradan
// okunur; testler kendi içinde ayrıca karar vermez. İşlevsel denetimler (çerçeve sayısı, koşunun
// tamamlanması, sıra ve emniyet denetimleri) bu yardımcıdan geçmez ve hiçbir ortamda gevşemez.
//
// Kullanan testler (dosya:satır W4-mac-ci raporunda): tests/butunleme (hil_cikarma_takma,
// senaryo_hil_pty, sonar_hil_olcum_*) ve sim/kopru/test (yazma_sinirli).
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace ida::test {

inline bool paylasilan_ci()
{
#if defined(_MSC_VER)
    char* v = nullptr;
    std::size_t n = 0;
    const bool var = _dupenv_s(&v, &n, "GITHUB_ACTIONS") == 0 && v != nullptr && v[0] != '\0';
    std::free(v);
    return var;
#else
    const char* v = std::getenv("GITHUB_ACTIONS");
    return v != nullptr && v[0] != '\0';
#endif
}

inline bool sanitizer_derlemesi()
{
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

// W4-K29: iyileştirmesiz (debug) derleme. Yalnız en iyileştirilmiş (`NDEBUG`) derlemede zamanlama ölçüttür.
inline bool iyilestirmesiz_derleme()
{
#if defined(NDEBUG)
    return false;
#else
    return true;
#endif
}

// Zamanlama eşikleri bu ortamda ölçüt mü.
inline bool zamanlama_olcut_mu() { return !paylasilan_ci() && !sanitizer_derlemesi() && !iyilestirmesiz_derleme(); }

// Ölçüt değilse nedeni (yazdırma için; ölçütse boş).
inline const char* zamanlama_muafiyeti()
{
    if (paylasilan_ci()) {
        return " (GITHUB_ACTIONS: yalnız ölçüm, W4-K24)";
    }
    if (sanitizer_derlemesi()) {
        return " (sanitizer'lı derleme: yalnız ölçüm, W4-K24)";
    }
    if (iyilestirmesiz_derleme()) {
        return " (iyileştirmesiz derleme, NDEBUG yok: yalnız ölçüm, W4-K29)";
    }
    return "";
}

// Adıyla işaretlenmiş bir zamanlama denetimi: sonucu ve ölçümü her zaman yazdırır; ölçüt değilse
// (CI ya da sanitizer) true döner. Dönüş, çağıranın kendi denetim makrosuna verilir.
inline bool zamanlama(bool kosul, const std::string& ad, const std::string& olcum)
{
    const bool olcut = zamanlama_olcut_mu();
    std::printf("  ZAMANLAMA [%s] %s: %s%s\n", ad.c_str(), kosul ? "sağlandı" : "SAĞLANMADI", olcum.c_str(),
                zamanlama_muafiyeti());
    return olcut ? kosul : true;
}

}  // namespace ida::test
