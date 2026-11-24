// Sınama için küçük GLB yazıcısı (W4-sonar-sahne): kutu ağlı A7 varlıkları bellekte üretir.
// Yalnız testlerde kullanılır (sim/senaryo/test, tests/butunleme); ürün kodu GLB yazmaz.
//
// Kutular gövde çerçevesinde verilir (x ileri, y sancak, z aşağı) ve glTF'e A7-K14'teki M'nin
// tersiyle yazılır: (X, Y, Z)_gltf = (−y, −z, x). REF düğümü 0'dır (glTF orijininde, sahne kökü);
// öteki düğümler sırayla 1, 2, ...; hepsi sahne köküdür. Olumsuz durumlar için JSON metni ve BIN
// parçası yazılmadan önce değiştirilebilir.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <string>
#include <vector>

namespace ida::senaryo::test {

struct GlbDugum {
    std::string ad;
    bool ag = true;                       // false: ağsız düğüm (bağlantı noktası gibi)
    std::array<double, 3> mn{}, mx{};     // gövde çerçevesinde kutu
    std::uint32_t malzeme = 0;            // glTF malzeme indeksi
    bool ters = false;                    // üçgen yönünü çevir (içe dönük; olumsuz sınama)
    bool delik = false;                   // bir üçgeni çıkar (kapalı değil; olumsuz sınama)
    bool ikiz = false;                    // aynı ağda `kaydir` kadar ötede ikinci kutu (dışbükey değil)
    std::array<double, 3> kaydir{};
};

struct GlbAyar {
    std::vector<GlbDugum> dugumler;
    std::uint32_t malzeme_sayisi = 1;
    std::function<void(std::string&)> json_degistir;   // JSON metni (dolgudan önce)
};

inline void glb_u32(std::string& s, std::uint32_t v)
{
    char b[4];
    std::memcpy(b, &v, 4);
    s.append(b, 4);
}

inline std::string glb_yaz(const GlbAyar& a)
{
    std::string bin;
    std::string erisimci, gorunum, ag, dugum = "{\"name\":\"REF\"}", kokler = "0";
    int ei = 0, gi = 0, mi = 0;
    auto gorunum_ekle = [&](const std::string& veri, int hedef) {
        while (bin.size() % 4u != 0u) bin.push_back('\0');
        if (!gorunum.empty()) gorunum += ",";
        gorunum += "{\"buffer\":0,\"byteOffset\":" + std::to_string(bin.size()) + ",\"byteLength\":" +
                   std::to_string(veri.size()) + ",\"target\":" + std::to_string(hedef) + "}";
        bin += veri;
        return gi++;
    };
    for (std::size_t k = 0; k < a.dugumler.size(); ++k) {
        const GlbDugum& d = a.dugumler[k];
        dugum += ",{\"name\":\"" + d.ad + "\"";
        kokler += "," + std::to_string(k + 1u);
        if (d.ag) {
            // Kutunun 8 köşesi (gövde): indeks = 4 x + 2 y + z; ikizde ikinci kutu 8–15.
            std::string kb;
            const int kutu_sayisi = d.ikiz ? 2 : 1;
            for (int c = 0; c < kutu_sayisi; ++c) {
                for (int x = 0; x < 2; ++x) {
                    for (int y = 0; y < 2; ++y) {
                        for (int z = 0; z < 2; ++z) {
                            double p[3] = {x ? d.mx[0] : d.mn[0], y ? d.mx[1] : d.mn[1], z ? d.mx[2] : d.mn[2]};
                            for (int e = 0; e < 3 && c == 1; ++e) p[e] += d.kaydir[static_cast<std::size_t>(e)];
                            const float g[3] = {static_cast<float>(-p[1]), static_cast<float>(-p[2]), static_cast<float>(p[0])};
                            kb.append(reinterpret_cast<const char*>(g), 12);
                        }
                    }
                }
            }
            // Dışa dönük yüzler (gövde çerçevesinde; M bir dönüş olduğundan yön korunur).
            static const std::uint16_t yuz[12][3] = {{0, 1, 3}, {0, 3, 2}, {4, 6, 7}, {4, 7, 5}, {0, 4, 5}, {0, 5, 1},
                                                    {2, 3, 7}, {2, 7, 6}, {0, 2, 6}, {0, 6, 4}, {1, 5, 7}, {1, 7, 3}};
            std::string ib;
            int n = 0;
            for (int c = 0; c < kutu_sayisi; ++c) {
                const int son = (d.delik && c == 0) ? 11 : 12;
                for (int t = 0; t < son; ++t) {
                    std::uint16_t u[3] = {static_cast<std::uint16_t>(yuz[t][0] + 8 * c), static_cast<std::uint16_t>(yuz[t][1] + 8 * c),
                                          static_cast<std::uint16_t>(yuz[t][2] + 8 * c)};
                    if (d.ters) std::swap(u[1], u[2]);
                    ib.append(reinterpret_cast<const char*>(u), 6);
                    ++n;
                }
            }
            const int kv = gorunum_ekle(kb, 34962);
            const int iv = gorunum_ekle(ib, 34963);
            if (!erisimci.empty()) erisimci += ",";
            erisimci += "{\"bufferView\":" + std::to_string(kv) + ",\"componentType\":5126,\"count\":" +
                        std::to_string(8 * kutu_sayisi) + ",\"type\":\"VEC3\"},";
            erisimci += "{\"bufferView\":" + std::to_string(iv) + ",\"componentType\":5123,\"count\":" + std::to_string(3 * n) +
                        ",\"type\":\"SCALAR\"}";
            if (!ag.empty()) ag += ",";
            ag += "{\"name\":\"" + d.ad + "\",\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(ei) +
                  "},\"indices\":" + std::to_string(ei + 1) + ",\"material\":" + std::to_string(d.malzeme) + ",\"mode\":4}]}";
            ei += 2;
            dugum += ",\"mesh\":" + std::to_string(mi++);
        }
        dugum += "}";
    }
    while (bin.size() % 4u != 0u) bin.push_back('\0');
    std::string malzemeler;
    for (std::uint32_t m = 0; m < a.malzeme_sayisi; ++m) {
        malzemeler += (m ? ",{\"name\":\"m" : "{\"name\":\"m") + std::to_string(m) + "\"}";
    }
    std::string js = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"glb_yazici.hpp\"},\"scene\":0,\"scenes\":[{\"nodes\":[" +
                     kokler + "]}],\"nodes\":[" + dugum + "],\"meshes\":[" + ag + "],\"materials\":[" + malzemeler +
                     "],\"accessors\":[" + erisimci + "],\"bufferViews\":[" + gorunum + "],\"buffers\":[{\"byteLength\":" +
                     std::to_string(bin.size()) + "}]}";
    if (a.json_degistir) a.json_degistir(js);
    while (js.size() % 4u != 0u) js.push_back(' ');
    std::string out;
    glb_u32(out, 0x46546C67u);
    glb_u32(out, 2u);
    glb_u32(out, static_cast<std::uint32_t>(12u + 8u + js.size() + 8u + bin.size()));
    glb_u32(out, static_cast<std::uint32_t>(js.size()));
    glb_u32(out, 0x4E4F534Au);
    out += js;
    glb_u32(out, static_cast<std::uint32_t>(bin.size()));
    glb_u32(out, 0x004E4942u);
    out += bin;
    return out;
}

}  // namespace ida::senaryo::test
