// tests/kabul/kopru — küçük denetim düzeneği (C++20): vaka kaydı ve KONTROL makroları.
//
// Her vaka bir işlevdir; KONTROL başarısız olunca dosya, satır ve ileti yazılır, vaka düşer
// ama sürer (bir vakada birden çok bulgu görülebilsin). Çıkış kodu: 0 geçti, 1 düştü,
// 2 bilinmeyen vaka adı.
#pragma once

#include <cstdio>
#include <functional>
#include <map>
#include <ostream>
#include <sstream>
#include <string>

namespace kabul {

struct Durum {
    int dusen = 0;
    int denetim = 0;
};

inline Durum& durum() {
    static Durum d;
    return d;
}

inline void kaydet_hata(const char* dosya, int satir, const std::string& ileti) {
    ++durum().dusen;
    std::fprintf(stderr, "DÜŞTÜ %s:%d: %s\n", dosya, satir, ileti.c_str());
}

using Vaka = std::function<void()>;

inline std::map<std::string, Vaka>& vakalar() {
    static std::map<std::string, Vaka> v;
    return v;
}

struct Kayitci {
    Kayitci(const char* ad, Vaka f) { vakalar()[ad] = std::move(f); }
};

inline int calistir(int argc, char** argv) {
    if (argc < 2) {
        for (const auto& [ad, f] : vakalar()) {
            std::printf("%s\n", ad.c_str());
        }
        return 2;
    }
    const auto it = vakalar().find(argv[1]);
    if (it == vakalar().end()) {
        std::fprintf(stderr, "bilinmeyen vaka: %s\n", argv[1]);
        return 2;
    }
    it->second();
    if (durum().dusen != 0) {
        std::fprintf(stderr, "%s: %d denetim düştü (%d denetim)\n", argv[1], durum().dusen,
                     durum().denetim);
        return 1;
    }
    std::printf("%s: geçti (%d denetim)\n", argv[1], durum().denetim);
    return 0;
}

}  // namespace kabul

#define KABUL_BIRLES2(a, b) a##b
#define KABUL_BIRLES(a, b) KABUL_BIRLES2(a, b)
#define VAKA(ad)                                                                           \
    static void KABUL_BIRLES(vaka_, __LINE__)();                                           \
    static const kabul::Kayitci KABUL_BIRLES(kayit_, __LINE__)(ad, KABUL_BIRLES(vaka_, __LINE__)); \
    static void KABUL_BIRLES(vaka_, __LINE__)()

namespace kabul {

template <class F>
void kontrol_et(bool tamam, const char* ifade, const char* dosya, int satir, F&& ileti) {
    ++durum().denetim;
    if (!tamam) {
        std::ostringstream os;
        os << ifade << " | ";
        ileti(os);
        kaydet_hata(dosya, satir, os.str());
    }
}

template <class A, class B, class F>
void esit_et(const A& a, const B& b, const char* ia, const char* ib, const char* dosya, int satir,
             F&& ileti) {
    ++durum().denetim;
    if (!(a == b)) {
        std::ostringstream os;
        os << ia << " == " << ib << " beklendi; gözlenen " << +a << " ≠ " << +b << " | ";
        ileti(os);
        kaydet_hata(dosya, satir, os.str());
    }
}

}  // namespace kabul

// KONTROL(koşul, ileti-akışı): ileti `<<` ile kurulur (yalnız denetim düşerse değerlendirilir).
#define KONTROL(kosul, ileti)                                                               \
    kabul::kontrol_et(static_cast<bool>(kosul), #kosul, __FILE__, __LINE__,                  \
                      [&](std::ostream& kabul_os_) { kabul_os_ << ileti; })

#define KONTROL_ESIT(a, b, ileti)                                                           \
    kabul::esit_et((a), (b), #a, #b, __FILE__, __LINE__,                                    \
                   [&](std::ostream& kabul_os_) { kabul_os_ << ileti; })
