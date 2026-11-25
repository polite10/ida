// test_dayaniklilik.cpp — kabul 9: sabit tohumlu rastgele ve bozuk girdi. Çökme, sınırsız bellek ya
// da kilitlenme olmamalı; sunucunun ürettiği her bayt düzgün çerçeve olmalı.
#include "test_destek.hpp"

#include <algorithm>
#include <cstring>
#include <random>

namespace ida::a2::test {

namespace {

viz::Baytlar gecerli_merhaba(std::uint32_t sira)
{
    Merhaba m{};
    m.surum_ana = 1;
    m.platform = 1;
    m.cizici = 1;
    viz::Baytlar b;
    viz::kodla_merhaba(b, sira, m);
    return b;
}

viz::Baytlar gecerli_hazir(std::uint32_t sira)
{
    Hazir h{};
    const auto o = ornek_ozet();
    std::memcpy(h.manifest_sha256, o.data(), 32);
    viz::Baytlar b;
    viz::kodla_hazir(b, sira, h);
    return b;
}

// Rastgele ama biçimce geçerli bir görselleştirici → sim iletisi.
viz::Baytlar rastgele_ileti(std::mt19937& rng, std::uint32_t sira)
{
    viz::Baytlar b;
    switch (rng() % 7u) {
        case 0: {
            GoruntuleyiciNabzi n{};
            n.gonderim_duvar_ns = static_cast<std::int64_t>(rng());
            n.cizim_hizi_mhz = rng();
            viz::kodla_goruntuleyici_nabzi(b, sira, n);
            break;
        }
        case 1: b = gecerli_hazir(sira); break;
        case 2: b = gecerli_merhaba(sira); break;
        case 3: viz::kodla_kare_dustu(b, sira, KareDustu{}); break;
        case 4: viz::kodla_kare_hazir(b, sira, KareHazir{}); break;
        case 5: viz::kodla_halka_eslendi(b, sira, HalkaEslendi{}); break;
        default: viz::kodla_veda(b, sira, static_cast<VedaNedeni>(rng() % 8u)); break;
    }
    return b;
}

void boz(std::mt19937& rng, viz::Baytlar& b)
{
    if (b.empty()) {
        return;
    }
    switch (rng() % 6u) {
        case 0: b[rng() % b.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8u)); break;       // bit çevir
        case 1: b.resize(rng() % b.size()); break;                                                   // kes
        case 2: b[rng() % (b.size() < 16 ? b.size() : 16)] = static_cast<std::uint8_t>(rng()); break;  // başlık baytı
        case 3: b.insert(b.begin() + static_cast<std::ptrdiff_t>(rng() % b.size()), static_cast<std::uint8_t>(rng())); break;
        case 4:
            for (std::size_t i = 8; i < 12 && i < b.size(); ++i) {
                b[i] = static_cast<std::uint8_t>(rng());  // yuk_bayt
            }
            break;
        default:
            for (std::size_t i = 12; i < 16 && i < b.size(); ++i) {
                b[i] = static_cast<std::uint8_t>(rng());  // sira
            }
            break;
    }
}

}  // namespace

// ≥ 100 000 vaka: rastgele baytlar, bozulmuş geçerli iletiler, rastgele parçalama ve rastgele zaman.
int test_rastgele_girdi()
{
    Denetci d;
    std::mt19937 rng(0x1DA2A2u);  // sabit tohum
    const std::size_t kSinir = std::size_t{128} << 10u;
    Duzenek z(kSinir, std::size_t{1} << 20u);
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 4;
    a.sim_kipi = SimKipi::sil;
    sahte::SahteDunya dn(z.y, a);
    dn.basla();
    z.k.sakla = false;

    const int kVaka = 120'000;
    std::uint64_t toplam_bayt = 0;
    std::uint64_t akisa_ulasan = 0;
    for (int v = 0; v < kVaka; ++v) {
        if (!z.ham_baglan()) {
            DOGRULA(d, false, "vaka " + std::to_string(v) + ": yeni bağlantı kabul edilmedi (çekirdek takıldı)");
            break;
        }
        // Vaka: 0…3 geçerli önek, sonra rastgele/bozuk kuyruk.
        viz::Baytlar akis;
        std::uint32_t sira = 0;
        const std::uint32_t tur = rng() % 5u;
        if (tur >= 1) {
            const viz::Baytlar m = gecerli_merhaba(sira++);
            akis.insert(akis.end(), m.begin(), m.end());
        }
        if (tur >= 2) {
            const viz::Baytlar h = gecerli_hazir(sira++);
            akis.insert(akis.end(), h.begin(), h.end());
        }
        const std::uint32_t ek = rng() % 6u;
        for (std::uint32_t i = 0; i < ek; ++i) {
            viz::Baytlar m = rastgele_ileti(rng, sira++);
            if (rng() % 3u == 0) {
                boz(rng, m);
            }
            akis.insert(akis.end(), m.begin(), m.end());
        }
        if (tur == 0 || rng() % 4u == 0) {
            const std::size_t n = rng() % 300u;
            for (std::size_t i = 0; i < n; ++i) {
                akis.push_back(static_cast<std::uint8_t>(rng()));
            }
        }
        toplam_bayt += akis.size();
        // Rastgele parçalarla ver; arada dünya ilerler ve saat rastgele atlar.
        std::size_t i = 0;
        int tur_sayisi = 0;
        while (i < akis.size() && z.c.oturum_var() && tur_sayisi < 1000) {
            const std::size_t parca = 1 + rng() % 200u;
            const std::size_t n = akis.size() - i < parca ? akis.size() - i : parca;
            z.c.veri_geldi(akis.data() + i, n, z.simdi);
            i += n;
            if (rng() % 4u == 0) {
                dn.adimla(rng() % 30u);
            }
            const std::uint32_t zar = rng() % 100u;
            z.simdi += zar < 90 ? static_cast<std::int64_t>(rng() % 20'000'000u)
                                : (zar < 98 ? 1'100'000'000 : 6'000'000'000);
            z.pompala();
            ++tur_sayisi;
            DOGRULA(d, z.c.kuyruk().bayt() <= kSinir + 4096, "kuyruk sınırlı");
        }
        if (z.c.asama() == ic::Asama::akis) {
            ++akisa_ulasan;
        }
        if (z.c.oturum_var()) {
            if (v % 2 == 0) {
                z.c.baglanti_koptu();  // istemci öldü
            } else {
                z.simdi += 200'000'000'000;  // her zaman aşımını geçer
                z.pompala();
                z.pompala();
            }
        }
        z.pompala();
        DOGRULA(d, !z.c.oturum_var() && !z.c.kapanis_var(), "vaka " + std::to_string(v) + ": oturum kapandı");
        DOGRULA(d, !z.k.cerceve_hatasi, "vaka " + std::to_string(v) + ": sunucu çıktısı düzgün çerçeve");
        if (d.hata != 0) {
            break;
        }
    }
    const SunucuSayaclari s = z.c.sayaclar();
    std::printf("  %d vaka, %llu girdi baytı; protokol hatası %llu, biten oturum %llu, akışa ulaşan %llu, sunucu çıktısı %llu ileti\n",
                kVaka, static_cast<unsigned long long>(toplam_bayt), static_cast<unsigned long long>(s.protokol_hatasi),
                static_cast<unsigned long long>(s.biten_oturum), static_cast<unsigned long long>(akisa_ulasan),
                static_cast<unsigned long long>(z.k.sayi));
    DOGRULA(d, s.baglanti_sayisi == static_cast<std::uint64_t>(kVaka) && s.biten_oturum == s.baglanti_sayisi, "her vaka açıldı ve kapandı");
    DOGRULA(d, s.protokol_hatasi > static_cast<std::uint64_t>(kVaka) / 4, "bozuk girdilerin çoğu protokol hatası olarak yakalandı");
    DOGRULA(d, akisa_ulasan > 1000, "geçerli öneklerle akışa ulaşıldı (sınama boş koşmadı)");
    DOGRULA(d, s.azami_kuyruk_bayt <= kSinir, "bellek tavanı: gönderme kuyruğu sınırı aşmadı");
    DOGRULA(d, z.y.sayaclar().gecersiz_cagri == 0, "yayıncı etkilenmedi");
    return d.hata;
}

// Veri kaynağı ne verirse versin (geçersiz ve sırasız çağrılar dahil) gerçek istemci hiçbir zaman
// protokol hatası görmez: sunucu sözleşmeye uymayan akış üretmez.
int test_rastgele_kaynak()
{
    Denetci d;
    std::mt19937 rng(0xA2D0u);
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 0;
    sahte::SahteDunya tanim(z.y, a);
    tanim.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    z.k.sakla = false;

    std::uint32_t arac = tanim.arac_kimligi();  // araç hiç ölmez (yaşayan listesi boşalmaz)
    std::vector<std::uint32_t> yasayan = {arac};
    std::uint32_t sonraki = 100;
    std::uint64_t adim = 0;
    bool durakli = false;
    std::array<GovdeAni, kAzamiGovde> an{};
    std::uint64_t kabul = 0;
    std::uint64_t oturum = 1;
    for (int i = 0; i < 300'000; ++i) {
        const std::uint32_t zar = rng() % 1000u;
        const std::uint64_t hedef_adim = adim + (zar % 7u == 0 ? 0u : 1u);  // ara sıra geçmiş sınır (geçersiz)
        if (zar < 60) {
            const GovdeKaydi g{rng() % 8u == 0 ? yasayan[rng() % yasayan.size()] : sonraki++, static_cast<std::uint16_t>(rng() % 4u),
                               static_cast<GovdeTuru>(rng() % 5u), static_cast<std::uint8_t>(rng() % 4u)};
            if (z.y.govde_dogdu(hedef_adim, g, static_cast<std::uint8_t>(rng() % 4u))) {
                yasayan.push_back(g.kimlik);
                ++kabul;
            }
        } else if (zar < 110) {
            const std::uint32_t kimlik = rng() % 4u == 0 ? 999'999u : yasayan[rng() % yasayan.size()];
            if (kimlik != arac && z.y.govde_oldu(hedef_adim, kimlik, static_cast<OlumNedeni>(rng() % 6u))) {
                yasayan.erase(std::find(yasayan.begin(), yasayan.end(), kimlik));
                ++kabul;
            }
        } else if (zar < 150) {
            kabul += z.y.kutle_durumu(hedef_adim, yasayan[rng() % yasayan.size()], static_cast<std::uint8_t>(rng() % 4u)) ? 1u : 0u;
        } else if (zar < 180) {
            kabul += z.y.sureklilik_kir(hedef_adim, rng() % 3u == 0 ? 424'242u : yasayan[rng() % yasayan.size()]) ? 1u : 0u;
        } else if (zar < 200) {
            kabul += z.y.isabet(hedef_adim, 1, 2, {1, 2, 3}, {0, 0, 1}, {4, 5, 6}) ? 1u : 0u;
        } else if (zar < 206) {
            if (durakli ? z.y.surdur() : z.y.duraklat()) {
                durakli = !durakli;
            }
        } else if (zar < 209) {
            // Sıfırlama: kimlikler yeniden kullanılmaz (çağıranın yükümlülüğü, A2-K23).
            if (z.y.sifirla(0)) {
                adim = 0;
                yasayan.clear();
                const GovdeKaydi g{sonraki++, 0, GovdeTuru::arac, 3};
                z.y.govde_dogdu(0, g);
                yasayan.push_back(g.kimlik);
                arac = g.kimlik;
                GovdeAni ilk;
                ilk.kimlik = g.kimlik;
                z.y.adim_siniri(0, std::span<const GovdeAni>(&ilk, 1));
            }
        } else if (zar < 211) {
            DalgaGirdisi dg = tanim.dalga();
            dg.g = rng() % 3u == 0 ? -1.0 : 9.81;  // ara sıra geçersiz
            z.y.dalga_kumesi(dg);
        } else if (zar < 213) {
            Ortam o = tanim.ortam();
            if (rng() % 3u == 0) {
                o.statik_q_nb[1] = 0.3;  // birim değil: reddedilir
            }
            z.y.ortam(o);
        } else {
            // Adım sınırı: bütün yaşayanların durumu (ara sıra eksik ya da fazla gövdeyle).
            const std::uint64_t yeni = adim + 1 + (zar % 97u == 0 ? 5u : 0u);
            std::size_t m = 0;
            for (std::uint32_t kimlik : yasayan) {
                if (m >= kAzamiGovde) {
                    break;
                }
                if (rng() % 200u == 0) {
                    continue;  // eksik gövde
                }
                GovdeAni g;
                g.kimlik = kimlik;
                g.p_n = {static_cast<double>(kimlik), static_cast<double>(yeni), 0.5};
                g.gecerli = rng() % 100u != 0;
                an[m++] = g;
            }
            if (z.y.adim_siniri(yeni, std::span<const GovdeAni>(an.data(), m))) {
                adim = yeni;
            }
        }
        if (i % 16 == 0) {
            z.ilerlet(20'000'000, 20'000'000);
            if (!z.c.oturum_var()) {
                // Yalnız yayın taşması (sabit olay tamponu) oturumu kapatabilir; yeniden bağlanılır.
                DOGRULA(d, z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::yayin_tasmasi,
                        std::string("beklenmeyen kopma: ") + kopma_nedeni_adi(z.c.sayaclar().son_kopma_nedeni));
                DOGRULA(d, z.baglan(), "yeniden bağlandı");
                ++oturum;
            }
            if (!z.istemci_hatasiz() || z.k.cerceve_hatasi) {
                break;
            }
        }
    }
    DOGRULA(d, z.istemci_hatasiz(), "istemci hiçbir protokol hatası bildirmedi: " + z.istemci_son_nedeni());
    DOGRULA(d, !z.k.cerceve_hatasi && z.c.sayaclar().protokol_hatasi == 0, "sunucu çıktısı düzgün");
    const YayinSayaclari ys = z.y.sayaclar();
    std::printf("  300 000 çağrı: kabul edilen olay %llu, reddedilen çağrı %llu, eksik gövde %llu, oturum %llu, sunucu çıktısı %llu ileti\n",
                static_cast<unsigned long long>(kabul), static_cast<unsigned long long>(ys.gecersiz_cagri),
                static_cast<unsigned long long>(ys.eksik_govde), static_cast<unsigned long long>(oturum),
                static_cast<unsigned long long>(z.k.sayi));
    DOGRULA(d, kabul > 10'000 && ys.gecersiz_cagri > 1000 && z.k.sayi > 20'000, "sınama boş koşmadı");
    return d.hata;
}

}  // namespace ida::a2::test
