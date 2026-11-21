// Kütle durumları (A5-K23/K24, Ek A) ve ida0_arac() ile scenarios/ida0.toml eşitliği.
#include "test_destek.hpp"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace ida::fizik::test {

namespace {

struct EkASatiri {
    const char* ad;
    std::array<bool, 2> yuklu;
    bool trim;
    double m;
    Vek3 rg;
    double hacim;
    Vek3 rb;
    double Ixx, Iyy, Izz, Ixz, Iyz;
};

// A5 Ek A (ida0.toml değerleriyle; trimli satırlar trim 0,5 kg @ (0,10; 0; 0,05) m).
const EkASatiri kEkA[] = {
    {"bos", {false, false}, false, 13.5, {0, 0, 0}, 0.0137, {0, 0, -0.01}, 0.26, 0.23, 0.37, 0, 0},
    {"f1", {true, false}, false, 13.675, {0, 0.0012797, 0.0012797}, 0.0138676,
     {0, 0.0012082, -0.0086709}, 0.263528, 0.232114, 0.372114, 0, -0.00175},
    {"iki", {true, true}, false, 13.85, {0, 0, 0.0025271}, 0.0140351, {0, 0, -0.0073736}, 0.267056,
     0.234228, 0.374228, 0, 0},
    {"bos+trim", {false, false}, true, 14.0, {0.0035714, 0, 0.0017857}, 0.0137, {0, 0, -0.01}, 0.26125,
     0.23625, 0.375, -0.0025, 0},
    {"f1+trim", {true, false}, true, 14.175, {0.0035273, 0.0012346, 0.0029982}, 0.0138676,
     {0, 0.0012082, -0.0086709}, 0.264778, 0.238364, 0.377114, -0.0025, -0.00175},
    {"iki+trim", {true, true}, true, 14.35, {0.0034843, 0, 0.0041812}, 0.0140351,
     {0, 0, -0.0073736}, 0.268306, 0.240478, 0.379228, -0.0025, 0},
};

}  // namespace

int test_kutle_durumlari(const TestBaglami&)
{
    Denetci d;
    // Tolerans: A5-K23 "Ek A'daki test vektörleriyle 1e-4 bağıl hata içinde"; sıfır beklenen
    // değerlerde (Ek A'da tam sıfır yazılı) mutlak 1e-12 (yalnız yuvarlama).
    const double kBagil = 1e-4;
    const double kSifir = 1e-12;
    for (const auto& s : kEkA) {
        AracParametreleri a = ida0_arac();
        if (s.trim) {
            a.kutle.trim_kutlesi = 0.5;
            a.kutle.trim_konumu = {0.10, 0.0, 0.05};
        }
        const KutleDurumu k = kutle_durumu_turet(a, s.yuklu);
        std::printf("  %s: m %.6f ∇ %.7f r_g (%.7f %.7f %.7f) r_b (%.7f %.7f %.7f)\n", s.ad, k.kutle,
                    k.hacim, k.r_g[0], k.r_g[1], k.r_g[2], k.r_b[0], k.r_b[1], k.r_b[2]);
        YAKIN(d, k.kutle, s.m, kBagil, 0.0, "m");
        YAKIN(d, k.hacim, s.hacim, kBagil, 0.0, "∇");
        for (int i = 0; i < 3; ++i) {
            const auto u = static_cast<std::size_t>(i);
            YAKIN(d, k.r_g[u], s.rg[u], kBagil, s.rg[u] == 0.0 ? kSifir : 0.0, "r_g");
            YAKIN(d, k.r_b[u], s.rb[u], kBagil, s.rb[u] == 0.0 ? kSifir : 0.0, "r_b");
        }
        YAKIN(d, k.atalet[0][0], s.Ixx, kBagil, 0.0, "I_xx");
        YAKIN(d, k.atalet[1][1], s.Iyy, kBagil, 0.0, "I_yy");
        YAKIN(d, k.atalet[2][2], s.Izz, kBagil, 0.0, "I_zz");
        YAKIN(d, k.atalet[0][2], s.Ixz, kBagil, s.Ixz == 0.0 ? kSifir : 0.0, "I_xz");
        YAKIN(d, k.atalet[2][0], s.Ixz, kBagil, s.Ixz == 0.0 ? kSifir : 0.0, "I_zx");
        YAKIN(d, k.atalet[1][2], s.Iyz, kBagil, s.Iyz == 0.0 ? kSifir : 0.0, "I_yz");
        YAKIN(d, k.atalet[0][1], 0.0, 0.0, kSifir, "I_xy");

        // Yüzerlik ağları (yer tutucu kutu + sferoidler) tam batıkta aynı ∇ ve r_b'yi verir.
        // Tolerans: kutu tam, sferoid ağı analitik hacme ölçeklenir; fark yalnız yuvarlama.
        if (!s.trim) {
            Dunya dn;
            std::vector<std::string> yuk;
            if (s.yuklu[0]) yuk.push_back("f1");
            if (s.yuklu[1]) yuk.push_back("f2");
            if (!kur(dn, a, havuz_ortami(), baslangic(5.0, yuk))) {
                return 1;
            }
            const auto& t = dn.turetilmis();
            YAKIN(d, t.tam_hacim_ag, k.hacim, 1e-12, 0.0, "ağ hacmi");
            for (int i = 0; i < 3; ++i) {
                const auto u = static_cast<std::size_t>(i);
                YAKIN(d, t.tam_yuzerlik_merkezi_ag[u], k.r_b[u], 0.0, 1e-12, "ağ yüzerlik merkezi");
            }
        }
    }
    return d.hata;
}

// ------------------------------------------------------------------ ida0.toml eşitliği
namespace {

// Yalnız ida0.toml'daki sayısal anahtarlar için en küçük TOML okuyucu: bölüm başlıkları,
// `anahtar = sayı` ve (çok satırlı olabilen) sayı dizileri. Dizgi ve satır içi tablolar atlanır.
std::map<std::string, std::vector<double>> toml_sayilari(const std::string& yol)
{
    std::map<std::string, std::vector<double>> sonuc;
    std::ifstream f(yol);
    if (!f) {
        return sonuc;
    }
    std::string bolum, satir, bekleyen_anahtar, birikim;
    int derinlik = 0;
    auto yorum_at = [](const std::string& s) {
        bool dizgide = false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '"') dizgide = !dizgide;
            if (s[i] == '#' && !dizgide) return s.substr(0, i);
        }
        return s;
    };
    auto sayilar = [](const std::string& s) {
        std::vector<double> v;
        std::string t;
        for (char c : s) {
            if (c != '_') t.push_back(c == '[' || c == ']' || c == ',' ? ' ' : c);
        }
        std::istringstream is(t);
        std::string parca;
        while (is >> parca) {
            char* son = nullptr;
            const double x = std::strtod(parca.c_str(), &son);
            if (son != nullptr && *son == '\0') v.push_back(x);
        }
        return v;
    };
    while (std::getline(f, satir)) {
        satir = yorum_at(satir);
        if (!bekleyen_anahtar.empty()) {
            birikim += " " + satir;
            for (char c : satir) {
                derinlik += (c == '[') - (c == ']');
            }
            if (derinlik == 0) {
                sonuc[bekleyen_anahtar] = sayilar(birikim);
                bekleyen_anahtar.clear();
            }
            continue;
        }
        const auto bas = satir.find_first_not_of(" \t");
        if (bas == std::string::npos) continue;
        if (satir[bas] == '[') {
            const auto kap = satir.find(']');
            bolum = satir.substr(bas + 1, kap - bas - 1);
            continue;
        }
        const auto esit = satir.find('=');
        if (esit == std::string::npos) continue;
        std::string anahtar = satir.substr(bas, esit - bas);
        anahtar.erase(anahtar.find_last_not_of(" \t") + 1);
        std::string deger = satir.substr(esit + 1);
        const auto db = deger.find_first_not_of(" \t");
        if (db == std::string::npos) continue;
        const std::string tam = bolum + "." + anahtar;
        if (deger[db] == '[') {
            derinlik = 0;
            for (char c : deger) {
                derinlik += (c == '[') - (c == ']');
            }
            if (derinlik == 0) {
                sonuc[tam] = sayilar(deger);
            } else {
                bekleyen_anahtar = tam;
                birikim = deger;
            }
        } else if (deger[db] != '"' && deger[db] != '{' && deger.find("true") == std::string::npos &&
                   deger.find("false") == std::string::npos) {
            const auto v = sayilar(deger);
            if (!v.empty()) sonuc[tam] = v;
        }
    }
    return sonuc;
}

template <std::size_t N>
std::vector<double> v(const std::array<double, N>& a)
{
    return std::vector<double>(a.begin(), a.end());
}

}  // namespace

int test_ida0_toml_esitligi(const TestBaglami& b)
{
    Denetci d;
    const auto t = toml_sayilari(b.ida0_toml_yolu);
    DOGRULA(d, !t.empty(), "ida0.toml okunamadı: " + b.ida0_toml_yolu);
    if (t.empty()) return 1;
    const AracParametreleri a = ida0_arac();
    std::vector<std::pair<std::string, std::vector<double>>> beklenen = {
        {"arac.kutle.kutle", {a.kutle.kutle}},
        {"arac.kutle.hacim", {a.kutle.hacim}},
        {"arac.kutle.r_g", v(a.kutle.r_g)},
        {"arac.kutle.r_b", v(a.kutle.r_b)},
        {"arac.kutle.atalet", {a.kutle.atalet[0][0], a.kutle.atalet[0][1], a.kutle.atalet[0][2],
                               a.kutle.atalet[1][0], a.kutle.atalet[1][1], a.kutle.atalet[1][2],
                               a.kutle.atalet[2][0], a.kutle.atalet[2][1], a.kutle.atalet[2][2]}},
        {"arac.kutle.trim_kutlesi", {a.kutle.trim_kutlesi}},
        {"arac.hidrodinamik.eklenmis_kutle", v(a.hidrodinamik.eklenmis_kutle)},
        {"arac.hidrodinamik.dogrusal_sonum", v(a.hidrodinamik.dogrusal_sonum)},
        {"arac.hidrodinamik.karesel_sonum", v(a.hidrodinamik.karesel_sonum)},
        {"arac.hidrodinamik.yuzey_sonum_orani", v(a.hidrodinamik.yuzey_sonum_orani)},
        {"arac.hidrodinamik.ruzgar_direnc_katsayisi", {a.hidrodinamik.ruzgar_direnc_katsayisi}},
        {"arac.hidrodinamik.belirsizlik.oteleme", {a.hidrodinamik.belirsizlik.oteleme}},
        {"arac.hidrodinamik.belirsizlik.donme", {a.hidrodinamik.belirsizlik.donme}},
        {"arac.itici_modeli.sekil_polinomu", v(a.itici_modeli.sekil_polinomu)},
        {"arac.itici_modeli.gerilim_noktalari", a.itici_modeli.gerilim_noktalari},
        {"arac.itici_modeli.azami_itki_ileri", a.itici_modeli.azami_itki_ileri},
        {"arac.itici_modeli.azami_itki_geri", a.itici_modeli.azami_itki_geri},
        {"arac.itici_modeli.tam_gaz_akimi", a.itici_modeli.tam_gaz_akimi},
        {"arac.itici_modeli.akim_ussu", {a.itici_modeli.akim_ussu}},
        {"arac.itici_modeli.transfer_pay", a.itici_modeli.transfer_pay},
        {"arac.itici_modeli.transfer_payda", a.itici_modeli.transfer_payda},
        {"arac.itici_modeli.pervane_yaricapi", {a.itici_modeli.pervane_yaricapi}},
        {"arac.itici_modeli.havalanma_h_bolu_r", a.itici_modeli.havalanma_h_bolu_r},
        {"arac.itici_modeli.havalanma_beta", a.itici_modeli.havalanma_beta},
        {"arac.esc.pwm_notr_us", {static_cast<double>(a.esc.pwm_notr_us)}},
        {"arac.esc.olu_bant_us", {static_cast<double>(a.esc.olu_bant_us)}},
        {"arac.esc.pwm_min_us", {static_cast<double>(a.esc.pwm_min_us)}},
        {"arac.esc.pwm_max_us", {static_cast<double>(a.esc.pwm_max_us)}},
        {"arac.esc.baslatma_suresi_ns", {static_cast<double>(a.esc.baslatma_suresi_ns)}},
        {"arac.batarya.v_sabit", {a.batarya.v_sabit}},
        {"arac.batarya.seri", {static_cast<double>(a.batarya.seri)}},
        {"arac.batarya.paralel", {static_cast<double>(a.batarya.paralel)}},
        {"arac.batarya.kapasite", {a.batarya.kapasite}},
        {"arac.batarya.r_kablo", {a.batarya.r_kablo}},
        {"arac.batarya.aviyonik_guc", {a.batarya.aviyonik_guc}},
        {"arac.temas.yay_sabiti", {a.temas.yay_sabiti}},
        {"arac.temas.sonum_orani", {a.temas.sonum_orani}},
        {"arac.temas.surtunme_katsayisi", {a.temas.surtunme_katsayisi}},
        {"arac.torpido.boy", {a.torpido.boy}},
        {"arac.torpido.cap", {a.torpido.cap}},
        {"arac.torpido.kutle", {a.torpido.kutle}},
        {"arac.torpido.r_g", v(a.torpido.r_g)},
        {"arac.torpido.kanat_alani_duzlem", {a.torpido.kanat_alani_duzlem}},
        {"arac.torpido.kanat_en_boy_orani", {a.torpido.kanat_en_boy_orani}},
        {"arac.torpido.kanat_x", {a.torpido.kanat_x}},
        {"arac.torpido.surukleme_katsayisi", {a.torpido.surukleme_katsayisi}},
        {"arac.torpido.ayrilma_hizi", {a.torpido.ayrilma_hizi}},
    };
    for (int f = 0; f < kFirlaticiSayisi; ++f) {
        const auto& fr = a.firlaticilar[static_cast<std::size_t>(f)];
        const std::string ad = "arac.firlaticilar.f" + std::to_string(f + 1);
        beklenen.push_back({ad + ".tup_konumu", v(fr.tup_konumu)});
        beklenen.push_back({ad + ".tup_ekseni", v(fr.tup_ekseni)});
        beklenen.push_back({ad + ".tup_su_kutlesi", {fr.tup_su_kutlesi}});
    }
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto& it = a.iticiler[static_cast<std::size_t>(i)];
        const std::string ad = "arac.iticiler.t" + std::to_string(i + 1);
        beklenen.push_back({ad + ".kanal", {static_cast<double>(it.kanal)}});
        beklenen.push_back({ad + ".konum", v(it.konum)});
        beklenen.push_back({ad + ".yon", v(it.yon)});
    }
    // Tolerans: tam eşitlik (aynı ondalık dizgi aynı binary64'e çözülür).
    for (const auto& [anahtar, deger] : beklenen) {
        const auto it = t.find(anahtar);
        if (it == t.end()) {
            DOGRULA(d, false, "ida0.toml'da yok: " + anahtar);
            continue;
        }
        DOGRULA(d, it->second == deger, "ida0.toml ile farklı: " + anahtar);
    }
    std::printf("  %zu anahtar karşılaştırıldı\n", beklenen.size());
    return d.hata;
}

}  // namespace ida::fizik::test
