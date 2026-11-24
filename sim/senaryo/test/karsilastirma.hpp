// Alan alan karşılaştırma (W4-senaryo madde 11): dosyadan kurulan yapı ile koddaki İDA-0
// varsayılanı. Farklar yazdırılır; hiçbiri eşitlenmez (karar mimarındır).
#pragma once

#include "ida/senaryo/senaryo.hpp"

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace ida::senaryo::test {

class Karsilastirma {
public:
    std::vector<std::string> farklar;
    std::size_t alan = 0;

    void sayi(const std::string& ad, double dosya, double kod)
    {
        ++alan;
        if (!(dosya == kod)) {
            farklar.push_back(ad + ": dosya " + sayi_dizilimi(dosya) + ", kod " + sayi_dizilimi(kod));
        }
    }
    template <class T>
    void tam(const std::string& ad, T dosya, T kod)
    {
        ++alan;
        if (dosya != kod) {
            farklar.push_back(ad + ": dosya " + std::to_string(static_cast<long long>(dosya)) + ", kod " +
                              std::to_string(static_cast<long long>(kod)));
        }
    }
    void mantik(const std::string& ad, bool dosya, bool kod)
    {
        ++alan;
        if (dosya != kod) {
            farklar.push_back(ad + ": dosya " + (dosya ? "true" : "false") + ", kod " + (kod ? "true" : "false"));
        }
    }
    void dizgi(const std::string& ad, const std::string& dosya, const std::string& kod)
    {
        ++alan;
        if (dosya != kod) {
            farklar.push_back(ad + ": dosya \"" + dosya + "\", kod \"" + kod + "\"");
        }
    }
    template <std::size_t N>
    void dizi(const std::string& ad, const std::array<double, N>& dosya, const std::array<double, N>& kod)
    {
        for (std::size_t i = 0; i < N; ++i) {
            sayi(ad + "[" + std::to_string(i) + "]", dosya[i], kod[i]);
        }
    }
    void dizi(const std::string& ad, const std::vector<double>& dosya, const std::vector<double>& kod)
    {
        tam(ad + ".uzunluk", dosya.size(), kod.size());
        for (std::size_t i = 0; i < dosya.size() && i < kod.size(); ++i) {
            sayi(ad + "[" + std::to_string(i) + "]", dosya[i], kod[i]);
        }
    }
    void mat3(const std::string& ad, const std::array<std::array<double, 3>, 3>& dosya,
              const std::array<std::array<double, 3>, 3>& kod)
    {
        for (std::size_t i = 0; i < 3; ++i) {
            dizi(ad + "[" + std::to_string(i) + "]", dosya[i], kod[i]);
        }
    }
    template <class T>
    void secimli(const std::string& ad, const std::optional<T>& dosya, const std::optional<T>& kod)
    {
        ++alan;
        if (dosya.has_value() != kod.has_value()) {
            farklar.push_back(ad + ": dosya " + (dosya ? "var" : "yok") + ", kod " + (kod ? "var" : "yok"));
        }
    }
    void yazdir(const char* baslik) const
    {
        std::printf("  %s: %zu alan karşılaştırıldı, %zu fark\n", baslik, alan, farklar.size());
        for (const auto& f : farklar) {
            std::printf("    FARK %s\n", f.c_str());
        }
    }
};

}  // namespace ida::senaryo::test
