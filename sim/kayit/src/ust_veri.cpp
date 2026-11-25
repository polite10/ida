// ust_veri.cpp — üst veri denetimi ve tekrar güvencesi (A6 §3.5, A6-K33).
#include "ida/kayit/ust_veri.hpp"

#include "mcap_yazim.hpp"

#include <algorithm>

namespace ida::kayit {

namespace {

struct BitisAdi {
    BitisDurumu b;
    std::string_view ad;
};
constexpr BitisAdi kBitisAdlari[] = {
    {BitisDurumu::bitti, "bitti"},
    {BitisDurumu::durduruldu, "durduruldu"},
    {BitisDurumu::on_denetim_kaldi, "on_denetim_kaldi"},
    {BitisDurumu::hata, "hata"},
    {BitisDurumu::disk_dolu, "disk_dolu"},
    {BitisDurumu::yazma_hatasi, "yazma_hatasi"},
    {BitisDurumu::sinira_ulasti, "sinira_ulasti"},
    {BitisDurumu::sim_kapanisi, "sim_kapanisi"},
};

struct GuvenceAdi {
    Guvence g;
    std::string_view ad;
};
constexpr GuvenceAdi kGuvenceAdlari[] = {
    {Guvence::yok, "yok"},
    {Guvence::izleme, "izleme"},
    {Guvence::fizik, "fizik"},
    {Guvence::girdi, "girdi"},
};

bool onluk_u32(std::string_view s) noexcept
{
    // $defs deseni: 0 ya da başında sıfır olmayan, en çok 4294967295.
    if (s.empty() || s.size() > 10 || (s.size() > 1 && s[0] == '0')) {
        return false;
    }
    std::uint64_t v = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10u + static_cast<std::uint64_t>(c - '0');
    }
    return v <= 0xFFFFFFFFull;
}

void neden_ekle(std::string& liste, std::string_view neden)
{
    if (neden.empty()) {
        return;
    }
    std::size_t bas = 0;
    while (bas <= liste.size()) {
        const std::size_t son = std::min(liste.find(',', bas), liste.size());
        if (std::string_view(liste).substr(bas, son - bas) == neden) {
            return;
        }
        bas = son + 1;
    }
    if (!liste.empty()) {
        liste.push_back(',');
    }
    liste.append(neden);
}

}  // namespace

std::string_view bitis_durumu_adi(BitisDurumu b) noexcept
{
    for (const auto& a : kBitisAdlari) {
        if (a.b == b) {
            return a.ad;
        }
    }
    return {};
}

bool bitis_durumu_coz(std::string_view ad, BitisDurumu& b) noexcept
{
    for (const auto& a : kBitisAdlari) {
        if (a.ad == ad) {
            b = a.b;
            return true;
        }
    }
    return false;
}

std::string_view guvence_adi(Guvence g) noexcept
{
    for (const auto& a : kGuvenceAdlari) {
        if (a.g == g) {
            return a.ad;
        }
    }
    return {};
}

bool guvence_coz(std::string_view ad, Guvence& g) noexcept
{
    for (const auto& a : kGuvenceAdlari) {
        if (a.ad == ad) {
            g = a.g;
            return true;
        }
    }
    return false;
}

bool eksik_duzeyi_etkiler(const KanalTanimi& kanal, Guvence duzey) noexcept
{
    switch (duzey) {
    case Guvence::yok:
        return false;
    case Guvence::izleme:
        return (kanal.tekrar_duzeyleri & tekrar::izleme) != 0;
    case Guvence::fizik:
        return (kanal.tekrar_duzeyleri & tekrar::fizik) != 0;
    case Guvence::girdi:
        return (kanal.tekrar_duzeyleri & tekrar::girdi) != 0;
    }
    return false;
}

Guvence beklenen_duzey(Kademe kademe, std::string_view kip) noexcept
{
    // Tablo 4.6'nın kısıtlayan okuması (A6-K33 "kademe, kip ve kanal gereği"):
    //   (b) `fizik`: en düşük kademe `telemetri`; gereken kanallar `fizik/durum`, `fizik/adim_sonucu`:
    //       yalnız fiziğin koştuğu kiplerde üretilir (`sil`, `hil_1x`, `hil_bariyer`, `tekrar_fizik`).
    //       `tekrar_girdi_sil` ve `tekrar_yeniden_kurma` koşularında fizik koşmaz (Tablo 4.6 (c), (d)):
    //       o kayıtlar `fizik` güvencesi iddia edemez.
    //   (c) `girdi`: `tam` kademesi ve yalnız `sil` kipindeki kayıt; HIL kaydı en çok `fizik` (A6-K34).
    //   (d) `yeniden_kurma` bir güvence düzeyi değildir.
    // Bilinmeyen kip (bu sürümün tanımadığı): en kısıtlayan düzey.
    const bool fizik_kosar = kip == "sil" || kip == "hil_1x" || kip == "hil_bariyer" || kip == "tekrar_fizik";
    if (kademe == Kademe::olaylar || !fizik_kosar) {
        return Guvence::izleme;
    }
    if (kademe == Kademe::tam && kip == "sil") {
        return Guvence::girdi;
    }
    return Guvence::fizik;
}

GuvenceSonucu guvence_belirle(const GuvenceGirdisi& g)
{
    GuvenceSonucu s;
    s.beklenen = beklenen_duzey(g.kademe, g.kip);
    if (g.guvencesiz_baslatma || g.canli_mudahale) {
        s.duzey = Guvence::yok;
        if (g.guvencesiz_baslatma) {
            neden_ekle(s.nedenler, "guvencesiz_baslatma");
        }
        if (g.canli_mudahale) {
            neden_ekle(s.nedenler, "canli_mudahale");
        }
        return s;
    }
    const bool erken = !g.erken_neden.empty();
    std::string nedenler;
    Guvence duzey = s.beklenen;
    while (duzey > Guvence::izleme) {
        const std::optional<std::uint64_t>& ilk = duzey == Guvence::girdi ? g.ilk_eksik_girdi : g.ilk_eksik_fizik;
        const bool eksik_basta = ilk.has_value() && *ilk == 0;
        const bool erken_basta = erken && !g.erken_son_adim.has_value();
        if (eksik_basta || erken_basta) {
            // İlk eksik adım 0: o düzey verilmez, bir alt düzeye inilir.
            if (eksik_basta) {
                neden_ekle(nedenler, "kayit_eksik");
            }
            if (erken_basta) {
                neden_ekle(nedenler, g.erken_neden);
            }
            duzey = static_cast<Guvence>(static_cast<std::uint8_t>(duzey) - 1u);
            continue;
        }
        if (ilk.has_value()) {
            s.on_eki_son_adim = *ilk - 1u;
        }
        if (erken) {
            s.on_eki_son_adim = s.on_eki_son_adim.has_value() ? std::min(*s.on_eki_son_adim, *g.erken_son_adim)
                                                              : *g.erken_son_adim;
        }
        break;
    }
    s.duzey = duzey;
    if (duzey == Guvence::izleme) {
        s.on_eki_son_adim.reset();
    }
    if (static_cast<std::uint8_t>(duzey) < static_cast<std::uint8_t>(s.beklenen)) {
        s.nedenler = std::move(nedenler);
    }
    return s;
}

bool ust_veri_denetle(std::string_view kayit, const UstVeri& u, std::string& hata)
{
    std::span<const UstVeriAnahtari> tablo;
    if (kayit == kUstVeriAcilis) {
        tablo = acilis_anahtarlari();
    } else if (kayit == kUstVeriGuncelleme) {
        tablo = guncelleme_anahtarlari();
    } else if (kayit == kUstVeriKapanis) {
        tablo = kapanis_anahtarlari();
    } else {
        hata = "bilinmeyen üst veri kaydı: " + std::string(kayit);
        return false;
    }
    for (const auto& [anahtar, deger] : u) {
        const auto it = std::find_if(tablo.begin(), tablo.end(), [&](const UstVeriAnahtari& a) { return a.ad == anahtar; });
        if (it == tablo.end()) {
            hata = std::string(kayit) + ": bilinmeyen anahtar `" + anahtar + "`";
            return false;
        }
        if (deger.size() < it->en_az_uzunluk) {
            hata = std::string(kayit) + ": `" + anahtar + "` boş olamaz";
            return false;
        }
        // Koşullu anahtar açılışta boş metinle yazılabilir (A6-K46); biçim yalnız dolu değerde aranır.
        if (it->bicim == UstVeriBicimi::onluk_u32 && !deger.empty() && !onluk_u32(deger)) {
            hata = std::string(kayit) + ": `" + anahtar + "` onluk u32 değil";
            return false;
        }
    }
    for (const auto& a : tablo) {
        if (!a.zorunlu) {
            continue;
        }
        const auto it = u.find(a.ad);
        if (it == u.end()) {
            hata = std::string(kayit) + ": zorunlu anahtar `" + std::string(a.ad) + "` yok";
            return false;
        }
    }
    if (kayit == kUstVeriGuncelleme && u.empty()) {
        hata = std::string(kayit) + ": boş güncelleme";
        return false;
    }
    if (kayit == kUstVeriAcilis) {
        // $defs/ust_veri_ida_kayit allOf: `kip` = `sil` iken `sil_n_bg` zorunlu (W1-uyum U10).
        const auto kip = u.find("kip");
        if (kip != u.end() && kip->second == "sil") {
            const auto n = u.find("sil_n_bg");
            if (n == u.end()) {
                hata = "ida_kayit: `kip` = `sil` iken `sil_n_bg` zorunlu";
                return false;
            }
            if (!onluk_u32(n->second)) {
                hata = "ida_kayit: `sil_n_bg` onluk u32 değil";
                return false;
            }
        }
    }
    return true;
}

bool run_id_gecerli(std::string_view r) noexcept
{
    // A3 $defs/run_id: ^[0-9]{8}-[0-9]{6}-[0-9a-f]{6}$
    if (r.size() != 22 || r[8] != '-' || r[15] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (i == 8 || i == 15) {
            continue;
        }
        const char c = r[i];
        const bool rakam = c >= '0' && c <= '9';
        const bool onaltilik = rakam || (c >= 'a' && c <= 'f');
        if (i < 15 ? !rakam : !onaltilik) {
            return false;
        }
    }
    return true;
}

std::string bagimlilik_surumleri()
{
    return "mcap=" + ic::McapYazim::mcap_surumu() + ",zstd=" + ic::McapYazim::zstd_surumu();
}

}  // namespace ida::kayit
