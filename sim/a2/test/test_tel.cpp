// test_tel.cpp — kabul 1 (A2-U01): sunucunun ürettiği her ileti türünün bayt düzeni.
//
// Üç bağımsız dayanak: (1) `a2_wire.h` yapıları (boyut ve ofsetler orada static_assert ile sabit);
// (2) `viz/a2` kodeği: sim kodlayıcısının çıktısını çözer, aynı değerleri yeniden kodlayınca aynı
// baytları üretir; (3) elle yazılmış onaltılık dökümler ve sözleşme tablolarındaki ofsetler.
#include "kodlayici.hpp"
#include "sha256.hpp"
#include "takvim.hpp"
#include "test_destek.hpp"

#include "ida/viz/sha256.hpp"

#include <cstring>
#include <random>

namespace ida::a2::test {

namespace {

template <class T>
T oku_le(const std::vector<std::uint8_t>& b, std::size_t ofset)
{
    T v{};
    std::memcpy(&v, b.data() + ofset, sizeof v);  // ana makine küçük sonlu (a2_wire.h static_assert)
    return v;
}

Baslik baslik_oku(const ic::Baytlar& b)
{
    Baslik h{};
    std::memcpy(&h, b.data(), sizeof h);
    return h;
}

viz::BaytGorunumu yuk_gor(const ic::Baytlar& b) { return viz::BaytGorunumu(b.data() + 16, b.size() - 16); }

void baslik_denetle(Denetci& d, const ic::Baytlar& b, IletiTuru tur, std::uint32_t sira, std::size_t beklenen_yuk,
                    const char* ad)
{
    DOGRULA(d, b.size() == 16 + beklenen_yuk, std::string(ad) + ": toplam boy");
    const Baslik h = baslik_oku(b);
    DOGRULA(d, h.sihirli == 0x32414449u, std::string(ad) + ": sihirli sayı");
    DOGRULA(d, b[0] == 0x49 && b[1] == 0x44 && b[2] == 0x41 && b[3] == 0x32, std::string(ad) + ": \"IDA2\" bayt sırası");
    DOGRULA(d, h.tur == static_cast<std::uint16_t>(tur), std::string(ad) + ": tür");
    DOGRULA(d, h.bayraklar == 0, std::string(ad) + ": bayraklar");
    DOGRULA(d, h.yuk_bayt == beklenen_yuk, std::string(ad) + ": yuk_bayt");
    DOGRULA(d, h.sira == sira, std::string(ad) + ": sira");
}

GovdeDurumu ornek_govde(std::uint32_t kimlik)
{
    GovdeDurumu g{};
    g.govde_kimligi = kimlik;
    g.sureklilik_no = kimlik + 3;
    g.govde_turu = 1;
    g.dolu_tupler = 2;
    g.bayraklar = kGovdeGecerli;
    for (int i = 0; i < 3; ++i) {
        g.p_n[i] = 1.5 * kimlik + i;
        g.v_n[i] = -0.25 * kimlik - i;
        g.w_b[i] = 0.125 * (i + 1);
    }
    g.q_nb[0] = 0.5;
    g.q_nb[1] = -0.5;
    g.q_nb[2] = 0.5;
    g.q_nb[3] = -0.5;
    return g;
}

}  // namespace

int test_sha256()
{
    Denetci d;
    // FIPS 180-4 örnekleri (NIST "SHA-256 Example Values").
    const auto onaltilik = [](const ic::Ozet& o) {
        static const char* h = "0123456789abcdef";
        std::string s;
        for (std::uint8_t b : o) {
            s.push_back(h[b >> 4u]);
            s.push_back(h[b & 0xFu]);
        }
        return s;
    };
    DOGRULA(d, onaltilik(ic::sha256("", 0)) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "boş");
    DOGRULA(d, onaltilik(ic::sha256("abc", 3)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
    const char* uzun = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    DOGRULA(d, onaltilik(ic::sha256(uzun, 56)) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            "56 baytlık örnek");
    // İkinci bağımsız gerçekleştirim (viz) ile: 0…300 baytlık rastgele girdiler, parçalı ekleme dahil.
    std::mt19937 rng(20261125u);
    for (std::size_t n = 0; n <= 300; ++n) {
        std::vector<std::uint8_t> v(n);
        for (auto& b : v) {
            b = static_cast<std::uint8_t>(rng());
        }
        const ic::Ozet a = ic::sha256(v.data(), n);
        const viz::Ozet b = viz::sha256(v.data(), n);
        DOGRULA(d, std::memcmp(a.data(), b.data(), 32) == 0, "viz sha256 ile aynı, n=" + std::to_string(n));
        ic::Sha256 s;
        const std::size_t yari = n / 3;
        s.ekle(v.data(), yari);
        s.ekle(v.data() + yari, n - yari);
        DOGRULA(d, s.bitir() == a, "parçalı ekleme");
    }
    return d.hata;
}

int test_takvim_birim()
{
    Denetci d;
    // takvimde(adim) ⇔ ∃k: takvim_adimi(k) = adim; ardışık aralık 13/14; sapma ≤ 0,625 ms.
    std::uint64_t k = 0;
    std::uint64_t onceki = 0;
    for (std::uint64_t adim = 0; adim < 200'000; ++adim) {
        const bool t = ic::takvimde(adim);
        const bool beklenen = ic::takvim_adimi(k) == adim;
        DOGRULA(d, t == beklenen, "takvimde adım " + std::to_string(adim));
        if (beklenen) {
            if (k > 0) {
                DOGRULA(d, adim - onceki == 13 || adim - onceki == 14, "aralık 13/14");
            }
            // ideal zaman k/60 s = k · 16 666 666,67 ns; adımın zamanı adim · 1 250 000 ns
            // 3 ile çarpılmış tamsayı karşılaştırması: |3·adim·1 250 000 − k·50 000 000| ≤ 3·625 000
            const std::int64_t fark = 3 * static_cast<std::int64_t>(adim) * 1'250'000 -
                                      static_cast<std::int64_t>(k) * 50'000'000;
            DOGRULA(d, fark <= 3 * 625'000 && fark >= -3 * 625'000, "sapma ≤ 0,625 ms");
            onceki = adim;
            ++k;
        }
    }
    DOGRULA(d, k == 15'000, "200 000 adımda 15 000 takvim iletisi (250 s × 60 Hz)");
    return d.hata;
}

int test_gelen_okuyucu()
{
    Denetci d;
    viz::Baytlar akis;
    Merhaba m{};
    m.surum_ana = 1;
    m.platform = 1;
    m.cizici = 1;
    viz::kodla_merhaba(akis, 0, m);
    GoruntuleyiciNabzi nb{};
    nb.gonderim_duvar_ns = 77;
    viz::kodla_goruntuleyici_nabzi(akis, 1, nb);
    viz::kodla_veda(akis, 2, VedaNedeni::olagan);
    // Her parça boyuyla (1 bayttan hepsine) aynı üç ileti çıkar.
    for (std::size_t parca = 1; parca <= akis.size(); parca += (parca < 40 ? 1 : 37)) {
        ic::GelenOkuyucu o;
        std::vector<std::uint16_t> turler;
        std::vector<std::uint32_t> siralar;
        for (std::size_t i = 0; i < akis.size(); i += parca) {
            const std::uint8_t* p = akis.data() + i;
            std::size_t n = akis.size() - i < parca ? akis.size() - i : parca;
            while (n > 0) {
                const auto r = o.besle(p, n);
                if (r == ic::GelenOkuyucu::Sonuc::ileti) {
                    turler.push_back(o.baslik().tur);
                    siralar.push_back(o.baslik().sira);
                    if (o.baslik().tur == static_cast<std::uint16_t>(IletiTuru::goruntuleyici_nabzi)) {
                        GoruntuleyiciNabzi g{};
                        std::memcpy(&g, o.yuk(), sizeof g);
                        DOGRULA(d, g.gonderim_duvar_ns == 77, "yük içeriği");
                    }
                } else if (r == ic::GelenOkuyucu::Sonuc::hata) {
                    DOGRULA(d, false, "geçerli akışta hata");
                    n = 0;
                }
            }
        }
        DOGRULA(d, turler.size() == 3 && turler[0] == 0x0001 && turler[1] == 0x0103 && turler[2] == 0x00FF,
                "üç ileti, parça " + std::to_string(parca));
        DOGRULA(d, siralar.size() == 3 && siralar[2] == 2, "sıralar");
    }
    // Başlık hataları yük beklenmeden bulunur (A2-K06).
    const auto hata_bekle = [&](viz::Baytlar b, ic::GelenHata beklenen, const char* ad) {
        ic::GelenOkuyucu o;
        const std::uint8_t* p = b.data();
        std::size_t n = 16;  // yalnız başlık
        const auto r = o.besle(p, n);
        DOGRULA(d, r == ic::GelenOkuyucu::Sonuc::hata && o.hata() == beklenen, ad);
    };
    {
        viz::Baytlar b;
        viz::kodla_veda(b, 0, VedaNedeni::olagan);
        b[0] ^= 0xFFu;
        hata_bekle(b, ic::GelenHata::sihirli, "bozuk sihirli sayı");
    }
    {
        viz::Baytlar b;
        viz::kodla_veda(b, 0, VedaNedeni::olagan);
        b[4] = 0x77;
        b[5] = 0x77;
        hata_bekle(b, ic::GelenHata::tur, "bilinmeyen tür");
    }
    {
        viz::Baytlar b;
        viz::kodla_sim_nabzi(b, 0, SimNabzi{});  // sim → görselleştirici türü ters yönde
        hata_bekle(b, ic::GelenHata::tur, "yön yanlış");
    }
    {
        viz::Baytlar b;
        viz::kodla_veda(b, 0, VedaNedeni::olagan);
        b[6] = 1;
        hata_bekle(b, ic::GelenHata::bayraklar, "bayraklar ≠ 0");
    }
    {
        viz::Baytlar b;
        viz::kodla_veda(b, 0, VedaNedeni::olagan);
        b[8] = 9;
        hata_bekle(b, ic::GelenHata::uzunluk, "yanlış uzunluk");
    }
    {
        viz::Baytlar b;
        viz::kodla_veda(b, 0, VedaNedeni::olagan);
        b[8] = 0;
        b[9] = 0;
        b[10] = 0x20;  // 2 MiB > 1 MiB
        hata_bekle(b, ic::GelenHata::uzunluk, "yük 1 MiB'den büyük");
    }
    return d.hata;
}

// Her ileti türü: sim kodlayıcısı → viz çözücüsü (değerler) → viz kodlayıcısı (aynı baytlar).
int test_tel_kodek()
{
    Denetci d;
    // ---- veda ve ret ----
    {
        ic::Baytlar a;
        ic::kodla_veda(a, 7, VedaNedeni::tampon_tasti);
        baslik_denetle(d, a, IletiTuru::veda, 7, 8, "veda");
        Veda v{};
        DOGRULA(d, viz::coz_veda(yuk_gor(a), v) == viz::KodHata::yok && v.neden == 2, "veda çözüldü");
        viz::Baytlar b;
        viz::kodla_veda(b, 7, VedaNedeni::tampon_tasti);
        DOGRULA(d, a == b, "veda: viz kodlayıcısıyla aynı baytlar");
        // Onaltılık döküm (§3.1, §3.3): başlık 16 B + neden u32 + dolgu u32.
        const std::uint8_t beklenen[24] = {0x49, 0x44, 0x41, 0x32, 0xFF, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
                                           0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        DOGRULA(d, a.size() == 24 && std::memcmp(a.data(), beklenen, 24) == 0, "veda onaltılık dökümü");
    }
    {
        ic::Baytlar a;
        ic::kodla_ret(a, 0, RetNedeni::mesgul, "meşgul: ğüşiöç");
        baslik_denetle(d, a, IletiTuru::ret, 0, 128, "ret");
        Ret r{};
        DOGRULA(d, viz::coz_ret(yuk_gor(a), r) == viz::KodHata::yok && r.neden == 4, "ret çözüldü");
        DOGRULA(d, viz::metin_oku(r.ayrinti, sizeof r.ayrinti) == "meşgul: ğüşiöç", "ret ayrıntısı UTF-8");
        viz::Baytlar b;
        viz::kodla_ret(b, 0, RetNedeni::mesgul, "meşgul: ğüşiöç");
        DOGRULA(d, a == b, "ret: viz kodlayıcısıyla aynı baytlar");
        // Sığmayan metin: son bayt NUL, UTF-8 dizisi bölünmez, NUL'dan sonrası sıfır.
        std::string uzun(200, 'x');
        uzun.replace(122, 2, "ş");  // 2 baytlık dizi 123 baytlık sınırı ortalar (bayt 122 ve 123)
        ic::Baytlar u;
        ic::kodla_ret(u, 0, RetNedeni::protokol, uzun);
        DOGRULA(d, viz::coz_ret(yuk_gor(u), r) == viz::KodHata::yok, "uzun ret NUL ile biter");
        viz::Baytlar ub;
        viz::kodla_ret(ub, 0, RetNedeni::protokol, uzun);
        DOGRULA(d, u == ub, "uzun ret: kesme kuralı viz ile aynı");
    }
    // ---- sim_nabzi: tam onaltılık döküm (§3.5) ----
    {
        SimNabzi n{};
        n.t_ns = 0x0102030405060708;
        n.adim_no = 0x1112131415161718ull;
        n.gonderim_duvar_ns = 0x2122232425262728;
        n.calisma_durumu = 2;
        n.sim_kipi = 3;
        ic::Baytlar a;
        ic::kodla_sim_nabzi(a, 0x11223344u, n);
        baslik_denetle(d, a, IletiTuru::sim_nabzi, 0x11223344u, 32, "sim_nabzi");
        const std::uint8_t beklenen[48] = {
            0x49, 0x44, 0x41, 0x32, 0x08, 0x02, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11,
            0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
            0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        DOGRULA(d, a.size() == 48 && std::memcmp(a.data(), beklenen, 48) == 0, "sim_nabzi onaltılık dökümü");
        SimNabzi c{};
        DOGRULA(d, viz::coz_sim_nabzi(yuk_gor(a), c) == viz::KodHata::yok && std::memcmp(&c, &n, sizeof n) == 0,
                "sim_nabzi çözüldü");
        viz::Baytlar b;
        viz::kodla_sim_nabzi(b, 0x11223344u, n);
        DOGRULA(d, a == b, "sim_nabzi: viz kodlayıcısıyla aynı baytlar");
    }
    // ---- hosgeldin (600 + 16·n; kamera yok) ----
    {
        Hosgeldin h{};
        h.oturum_kimligi = 0xA1A2A3A4A5A6A7A8ull;
        h.surum_ana = 1;
        h.surum_alt = 0;
        h.sim_kipi = 2;
        h.kademe_3b = 1;
        h.t_ns = 1'250'000ll * 4321;
        h.adim_no = 4321;
        h.adim_ns = 1'250'000;
        h.durum_hiz_pay = 60;
        h.durum_hiz_payda = 1;
        const auto oz = ornek_ozet(0x10);
        std::memcpy(h.manifest_sha256, oz.data(), 32);
        ic::metin_yaz(h.manifest_yolu, sizeof h.manifest_yolu, "/veri/sahne/manifest.json");
        h.calisma_nesli = 9;
        std::array<GovdeBilgisi, 3> g{};
        for (std::uint32_t i = 0; i < 3; ++i) {
            g[i].govde_kimligi = 100 + i;
            g[i].sureklilik_no = i;
            g[i].varlik_indeksi = static_cast<std::uint16_t>(i);
            g[i].govde_turu = static_cast<std::uint8_t>(i == 0 ? 1 : 3);
            g[i].dolu_tupler = static_cast<std::uint8_t>(i == 0 ? 3 : 0);
        }
        ic::Baytlar a;
        ic::kodla_hosgeldin(a, 0, h, g);
        baslik_denetle(d, a, IletiTuru::hosgeldin, 0, 600 + 16 * 3, "hosgeldin");
        viz::HosgeldinIleti c;
        DOGRULA(d, viz::coz_hosgeldin(yuk_gor(a), c) == viz::KodHata::yok, "hosgeldin çözüldü");
        DOGRULA(d, c.h.oturum_kimligi == h.oturum_kimligi && c.h.adim_no == 4321 && c.h.calisma_nesli == 9 &&
                       c.h.kamera_sayisi == 0 && c.h.kademe_kamera == 0 && c.h.govde_sayisi == 3,
                "hosgeldin alanları");
        DOGRULA(d, c.govdeler.size() == 3 && c.govdeler[2].govde_kimligi == 102 && c.govdeler[0].dolu_tupler == 3 &&
                       c.kameralar.empty() && c.halkalar.empty(),
                "hosgeldin gövde tablosu");
        viz::HosgeldinIleti v;
        v.h = h;
        v.govdeler.assign(g.begin(), g.end());
        viz::Baytlar b;
        viz::kodla_hosgeldin(b, 0, v);
        DOGRULA(d, a == b, "hosgeldin: viz kodlayıcısıyla aynı baytlar");
        // Sözleşme tablosundaki ofsetler (yük başından; a2_wire.h static_assert'leriyle aynı).
        const std::vector<std::uint8_t> y(a.begin() + 16, a.end());
        DOGRULA(d, oku_le<std::uint64_t>(y, 0) == h.oturum_kimligi, "ofset 0 oturum_kimligi");
        DOGRULA(d, oku_le<std::uint16_t>(y, 8) == 1 && oku_le<std::uint16_t>(y, 10) == 0, "ofset 8/10 sürüm");
        DOGRULA(d, y[12] == 2 && y[13] == 1 && y[14] == 0 && y[15] == 0, "ofset 12–15 kip, kademe, kamera");
        DOGRULA(d, oku_le<std::int64_t>(y, 16) == h.t_ns && oku_le<std::uint64_t>(y, 24) == 4321 &&
                       oku_le<std::int64_t>(y, 32) == 1'250'000,
                "ofset 16/24/32 zaman");
        DOGRULA(d, oku_le<std::uint32_t>(y, 40) == 60 && oku_le<std::uint32_t>(y, 44) == 1, "ofset 40/44 hız");
        DOGRULA(d, std::memcmp(y.data() + 48, oz.data(), 32) == 0, "ofset 48 manifest özeti");
        DOGRULA(d, std::memcmp(y.data() + 80, "/veri/sahne/manifest.json", 26) == 0, "ofset 80 manifest yolu");
        DOGRULA(d, oku_le<std::uint16_t>(y, 592) == 3 && oku_le<std::uint32_t>(y, 596) == 9, "ofset 592/596");
        DOGRULA(d, oku_le<std::uint32_t>(y, 600 + 16) == 101 && oku_le<std::uint16_t>(y, 600 + 16 + 8) == 1 &&
                       y[600 + 16 + 10] == 3,
                "GovdeBilgisi[1] ofsetleri");
    }
    // ---- dalga_kumesi (72 + 40·m) ----
    {
        std::array<DalgaBileseni, 5> bl{};
        for (std::size_t i = 0; i < bl.size(); ++i) {
            bl[i] = DalgaBileseni{0.1 * static_cast<double>(i), 1.0 + static_cast<double>(i), 0.2 + static_cast<double>(i),
                                  0.3 * static_cast<double>(i), -0.4 * static_cast<double>(i)};
        }
        DalgaKumesi dk{};
        dk.surum = 4;
        dk.derin_su = 0;
        dk.tohum = 0xDEADBEEFCAFEull;
        dk.g = 9.80665;
        dk.rho = 1025.0;
        dk.derinlik_m = 12.5;
        ic::Baytlar a;
        ic::kodla_dalga(a, 1, dk, bl);
        baslik_denetle(d, a, IletiTuru::dalga_kumesi, 1, 72 + 40 * 5, "dalga_kumesi");
        viz::DalgaIleti c;
        DOGRULA(d, viz::coz_dalga(yuk_gor(a), c) == viz::KodHata::yok, "dalga çözüldü (özet dahil)");
        DOGRULA(d, c.d.surum == 4 && c.d.bilesen_sayisi == 5 && c.bilesenler.size() == 5 && c.bilesenler[3].omega == 4.0 &&
                       c.d.derinlik_m == 12.5 && c.d.tohum == dk.tohum,
                "dalga alanları");
        viz::DalgaIleti v;
        v.d = dk;
        v.bilesenler.assign(bl.begin(), bl.end());
        viz::dalga_ozeti_hesapla(v);
        viz::Baytlar b;
        viz::kodla_dalga(b, 1, v);
        DOGRULA(d, a == b, "dalga: viz kodlayıcısıyla aynı baytlar (özet dahil)");
        const std::vector<std::uint8_t> y(a.begin() + 16, a.end());
        DOGRULA(d, oku_le<double>(y, 16) == 9.80665 && oku_le<double>(y, 24) == 1025.0, "ofset 16/24 g, rho");
        const ic::Ozet oz = ic::sha256(y.data() + 72, 200);
        DOGRULA(d, std::memcmp(y.data() + 40, oz.data(), 32) == 0, "ofset 40: bileşen baytlarının SHA-256'sı");
        // Bileşensiz küme de gönderilir.
        ic::Baytlar bos;
        ic::kodla_dalga(bos, 0, dk, {});
        baslik_denetle(d, bos, IletiTuru::dalga_kumesi, 0, 72, "bileşensiz dalga");
        DOGRULA(d, viz::coz_dalga(yuk_gor(bos), c) == viz::KodHata::yok && c.bilesenler.empty(), "bileşensiz dalga çözüldü");
    }
    // ---- ortam (208) ----
    {
        Yayinci y;
        const sahte::SahteDunya dn(y, ornek_ayar());
        Ortam o = dn.ortam();
        o.ortam_surumu = 6;
        ic::Baytlar a;
        ic::kodla_ortam(a, 2, o);
        baslik_denetle(d, a, IletiTuru::ortam, 2, 208, "ortam");
        Ortam c{};
        DOGRULA(d, viz::coz_ortam(yuk_gor(a), c) == viz::KodHata::yok && std::memcmp(&c, &o, sizeof o) == 0, "ortam çözüldü");
        viz::Baytlar b;
        viz::kodla_ortam(b, 2, o);
        DOGRULA(d, a == b, "ortam: viz kodlayıcısıyla aynı baytlar");
        const std::vector<std::uint8_t> yk(a.begin() + 16, a.end());
        DOGRULA(d, oku_le<double>(yk, 64) == 0.6 && oku_le<double>(yk, 64 + 16) == -0.8, "ofset 64 gunes_yonu_n");
        DOGRULA(d, oku_le<double>(yk, 120) == o.beta_d_m[0] && oku_le<std::uint32_t>(yk, 200) == 1, "ofset 120/200");
    }
    // ---- malzeme_optik (8 + 56·k) ----
    {
        std::array<MalzemeOptikGirdisi, 2> g{};
        g[0].malzeme_indeksi = 3;
        g[0].maske = 0x7;
        g[0].taban_rengi_dogrusal[0] = 0.25;
        g[0].taban_rengi_dogrusal[3] = 1.0;
        g[0].metaliklik = 0.5;
        g[0].puruzluluk = 0.75;
        g[1].malzeme_indeksi = 9;
        g[1].maske = 0x4;
        g[1].puruzluluk = 0.125;
        MalzemeOptik m{};
        m.surum = 2;
        ic::Baytlar a;
        ic::kodla_malzeme(a, 3, m, g);
        baslik_denetle(d, a, IletiTuru::malzeme_optik, 3, 8 + 56 * 2, "malzeme_optik");
        viz::MalzemeIleti c;
        DOGRULA(d, viz::coz_malzeme(yuk_gor(a), c) == viz::KodHata::yok && c.girdiler.size() == 2 && c.m.surum == 2 &&
                       c.girdiler[0].metaliklik == 0.5 && c.girdiler[1].malzeme_indeksi == 9,
                "malzeme çözüldü");
        viz::MalzemeIleti v;
        v.m = m;
        v.girdiler.assign(g.begin(), g.end());
        viz::Baytlar b;
        viz::kodla_malzeme(b, 3, v);
        DOGRULA(d, a == b, "malzeme: viz kodlayıcısıyla aynı baytlar");
        ic::Baytlar bos;
        ic::kodla_malzeme(bos, 0, m, {});
        baslik_denetle(d, bos, IletiTuru::malzeme_optik, 0, 8, "girdisiz malzeme");
    }
    // ---- durum (32 + 120·n) ----
    {
        std::array<GovdeDurumu, 2> g = {ornek_govde(5), ornek_govde(6)};
        Durum du{};
        du.t_ns = 1'250'000ll * 1013;
        du.adim_no = 1013;
        du.gonderim_duvar_ns = 123456789;
        du.dalga_kumesi_surumu = 4;
        du.calisma_durumu = 1;
        du.bayraklar = kDurumTakvimDisi;
        ic::Baytlar a;
        ic::kodla_durum(a, 44, du, g);
        baslik_denetle(d, a, IletiTuru::durum, 44, 32 + 120 * 2, "durum");
        viz::DurumIleti c;
        DOGRULA(d, viz::coz_durum(yuk_gor(a), kAdimNs, c) == viz::KodHata::yok, "durum çözüldü");
        DOGRULA(d, c.d.adim_no == 1013 && c.d.govde_sayisi == 2 && c.d.bayraklar == 1 && c.govdeler.size() == 2 &&
                       std::memcmp(&c.govdeler[1], &g[1], sizeof g[1]) == 0 && viz::govde_durumu_gecerli(c.govdeler[0]),
                "durum alanları ve gövde durumları");
        viz::DurumIleti v;
        v.d = du;
        v.govdeler.assign(g.begin(), g.end());
        viz::Baytlar b;
        viz::kodla_durum(b, 44, v);
        DOGRULA(d, a == b, "durum: viz kodlayıcısıyla aynı baytlar");
        // §3.4 ofsetleri: Durum 32 B; GovdeDurumu p_n 16, q_nb 40, v_n 72, w_b 96.
        const std::vector<std::uint8_t> y(a.begin() + 16, a.end());
        DOGRULA(d, oku_le<std::int64_t>(y, 0) == du.t_ns && oku_le<std::uint64_t>(y, 8) == 1013 &&
                       oku_le<std::int64_t>(y, 16) == 123456789 && oku_le<std::uint32_t>(y, 24) == 4 &&
                       oku_le<std::uint16_t>(y, 28) == 2 && y[30] == 1 && y[31] == 1,
                "Durum ofsetleri");
        const std::size_t g1 = 32 + 120;
        DOGRULA(d, oku_le<std::uint32_t>(y, g1) == 6 && oku_le<std::uint32_t>(y, g1 + 4) == 9 && y[g1 + 8] == 1 &&
                       y[g1 + 9] == 2 && oku_le<std::uint16_t>(y, g1 + 10) == 1,
                "GovdeDurumu baş alanları");
        DOGRULA(d, oku_le<double>(y, g1 + 16) == g[1].p_n[0] && oku_le<double>(y, g1 + 40) == 0.5 &&
                       oku_le<double>(y, g1 + 48) == -0.5 && oku_le<double>(y, g1 + 72) == g[1].v_n[0] &&
                       oku_le<double>(y, g1 + 96 + 16) == g[1].w_b[2],
                "GovdeDurumu p_n 16, q_nb 40 (w, x, …), v_n 72, w_b 96");
        ic::Baytlar bos;
        ic::kodla_durum(bos, 0, du, {});
        baslik_denetle(d, bos, IletiTuru::durum, 0, 32, "gövdesiz durum");
    }
    // ---- sahne_olayi (120) ----
    {
        SahneOlayi o{};
        o.t_ns = 1'250'000ll * 800;
        o.adim_no = 800;
        o.tur = static_cast<std::uint16_t>(SahneOlayiTuru::isabet);
        o.govde_kimligi = 100;
        o.diger_govde_kimligi = 2;
        o.nokta_n[0] = 1.0;
        o.normal_n[1] = -1.0;
        o.bagil_hiz_n[2] = 8.0;
        o.calisma_nesli = 3;
        ic::Baytlar a;
        ic::kodla_sahne_olayi(a, 5, o);
        baslik_denetle(d, a, IletiTuru::sahne_olayi, 5, 120, "sahne_olayi");
        SahneOlayi c{};
        DOGRULA(d, viz::coz_sahne_olayi(yuk_gor(a), kAdimNs, c) == viz::KodHata::yok && std::memcmp(&c, &o, sizeof o) == 0,
                "sahne_olayi çözüldü");
        viz::Baytlar b;
        viz::kodla_sahne_olayi(b, 5, o);
        DOGRULA(d, a == b, "sahne_olayi: viz kodlayıcısıyla aynı baytlar");
        const std::vector<std::uint8_t> y(a.begin() + 16, a.end());
        DOGRULA(d, oku_le<std::uint16_t>(y, 16) == 4 && oku_le<std::uint32_t>(y, 20) == 100 &&
                       oku_le<std::uint32_t>(y, 24) == 2 && oku_le<double>(y, 40) == 1.0 &&
                       oku_le<double>(y, 64 + 8) == -1.0 && oku_le<double>(y, 88 + 16) == 8.0 &&
                       oku_le<std::uint32_t>(y, 112) == 3,
                "SahneOlayi ofsetleri (tur 16, kimlik 20, nokta 40, normal 64, hız 88, nesil 112)");
    }
    return d.hata;
}

// Bir oturumun bütün çerçeveleri: yuk_bayt tür formülüne uyar; sıra 0'dan başlar ve seyreltme
// yokken her iletide tam 1 artar (A2-K07).
int test_tel_sira_ve_yuk()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 3;
    a.firlatma_adimi = 100;
    a.isabet_adimi = 300;
    sahte::SahteDunya dn(z.y, a);
    DOGRULA(d, dn.basla(), "çalıştırma başladı");
    DOGRULA(d, z.baglan(), "bağlandı");
    for (int i = 0; i < 50; ++i) {
        dn.adimla(16);
        z.ilerlet(20'000'000, 20'000'000);
    }
    z.ilerlet(1'500'000'000);  // nabız da görülsün
    DOGRULA(d, !z.k.cerceve_hatasi, "sunucu çıktısı viz çerçeveleyicisinden hatasız geçti");
    DOGRULA(d, z.k.cerceveler.size() > 70, "yeterli çerçeve");
    std::uint32_t beklenen_sira = 0;
    for (const Cerceve& c : z.k.cerceveler) {
        DOGRULA(d, c.b.sira == beklenen_sira, "sıra tam 1 artar (seyreltme yok)");
        beklenen_sira = c.b.sira + 1;
        std::size_t beklenen = 0;
        switch (c.tur()) {
            case IletiTuru::hosgeldin: beklenen = 600 + 16 * static_cast<std::size_t>(hosgeldin_coz(c).h.govde_sayisi); break;
            case IletiTuru::dalga_kumesi: beklenen = 72 + 40 * static_cast<std::size_t>(a.dalga_bileseni); break;
            case IletiTuru::ortam: beklenen = 208; break;
            case IletiTuru::malzeme_optik: beklenen = 8; break;
            case IletiTuru::durum: beklenen = 32 + 120 * static_cast<std::size_t>(durum_coz(c).d.govde_sayisi); break;
            case IletiTuru::sahne_olayi: beklenen = 120; break;
            case IletiTuru::sim_nabzi: beklenen = 32; break;
            default: DOGRULA(d, false, "beklenmeyen ileti türü"); break;
        }
        DOGRULA(d, c.b.yuk_bayt == beklenen && c.yuk.size() == beklenen, "yuk_bayt türün beklenen boyutu");
        DOGRULA(d, c.b.bayraklar == 0 && c.b.sihirli == kSihirliSayi, "başlık sabitleri");
    }
    DOGRULA(d, z.k.say(IletiTuru::sim_nabzi) >= 1 && z.k.say(IletiTuru::sahne_olayi) == 5, "nabız ve beş olay görüldü");
    DOGRULA(d, z.ist.sayaclar().sira_boslugu == 0, "istemci sıra boşluğu saymadı");
    DOGRULA(d, z.istemci_hatasiz(), "istemci protokol hatası bildirmedi: " + z.istemci_son_nedeni());
    return d.hata;
}

}  // namespace ida::a2::test
