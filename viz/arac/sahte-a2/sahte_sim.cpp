#include "sahte_sim.hpp"

#include <cstring>

namespace ida::sahte {

namespace {
constexpr std::int64_t kAdimNs = 1'250'000;  // iskelet §4
}

SahteSim::SahteSim(SenaryoAyarlari a) : senaryo_(std::move(a)) {}

std::uint64_t SahteSim::takvim_adimi(std::uint64_t k) {
  // A2-K20: 60 Hz takvim en yakın adım sınırına: round(k · 800 / 60) = ⌊(80k + 3) / 6⌋.
  return (80u * k + 3u) / 6u;
}

Baytlar SahteSim::giden_al() {
  Baytlar b;
  b.swap(giden_);
  return b;
}

void SahteSim::baglanti_acildi(std::int64_t duvar_ns, std::uint64_t oturum_kimligi) {
  asama_ = Asama::merhaba_bekleniyor;
  okuyucu_.temizle();
  giden_.clear();
  sira_ = 0;
  ilk_alim_ = true;
  kapatilmali_ = false;
  oturum_kimligi_ = oturum_kimligi;
  baglanti_duvar_ = duvar_ns;
  duvar_ = duvar_ns > 0 ? duvar_ns : 1;
  son_alim_duvar_ = duvar_ns;
  son_nabiz_duvar_ = duvar_ns;
  hazir_alindi_ = false;
  son_olay_ = "bağlantı açıldı";
}

void SahteSim::baglanti_kapandi() {
  asama_ = Asama::yok;
  kapatilmali_ = false;
  son_olay_ = "bağlantı kapandı";
}

void SahteSim::kapat(const std::string& neden) {
  asama_ = Asama::kapaniyor;
  kapatilmali_ = true;
  son_olay_ = neden;
}

void SahteSim::hata(KodHata h, const char* baglam) {
  ++sayac_.protokol_hatasi;
  kodla_veda(giden_, sira(), a2::VedaNedeni::protokol_hatasi);
  kapat(std::string("protokol hatası: ") + baglam + ": " + hata_adi(h));
}

void SahteSim::kapaniyor() {
  if (asama_ == Asama::akis || asama_ == Asama::merhaba_bekleniyor) {
    kodla_veda(giden_, sira(), a2::VedaNedeni::sim_kapaniyor);
    kapat("sim kapanıyor");
  }
}

void SahteSim::gonder_kurulum() {
  // A2-K12: hosgeldin, ardından dalga_kumesi, ortam, malzeme_optik.
  HosgeldinIleti h;
  h.h.oturum_kimligi = oturum_kimligi_;
  h.h.surum_ana = a2::kSurumAna;
  h.h.surum_alt = a2::kSurumAlt;
  h.h.sim_kipi = static_cast<std::uint8_t>(a2::SimKipi::sil);
  h.h.kademe_3b = senaryo_.ayarlar().kademe_3b;
  h.h.kademe_kamera = 0;
  h.h.t_ns = t_ns_;
  h.h.adim_no = adim_no_;
  h.h.adim_ns = kAdimNs;
  h.h.durum_hiz_pay = a2::kDurumHizPay;
  h.h.durum_hiz_payda = a2::kDurumHizPayda;
  std::memcpy(h.h.manifest_sha256, senaryo_.ayarlar().manifest_ozeti.data(), 32);
  metin_yaz(h.h.manifest_yolu, sizeof h.h.manifest_yolu, senaryo_.ayarlar().manifest_yolu);
  h.h.calisma_nesli = senaryo_.nesil();
  h.govdeler = senaryo_.govde_bilgileri();
  kodla_hosgeldin(giden_, sira(), h);
  kodla_dalga(giden_, sira(), senaryo_.dalga());
  kodla_ortam(giden_, sira(), senaryo_.ortam());
  MalzemeIleti m;
  m.m.surum = 1;
  kodla_malzeme(giden_, sira(), m);
  sayac_.gonderilen_ileti += 4;
  asama_ = Asama::akis;
  // İlk durum hemen (takvim dışı): görselleştirici sahneyi beklemeden poz alabilsin.
  durum_gonder(t_ns_, adim_no_, true);
}

void SahteSim::durum_gonder(std::int64_t t, std::uint64_t adim, bool takvim_disi) {
  if (asama_ != Asama::akis) return;
  DurumIleti d;
  d.d.t_ns = t;
  d.d.adim_no = adim;
  d.d.gonderim_duvar_ns = duvar_;
  d.d.dalga_kumesi_surumu = senaryo_.dalga().d.surum;
  d.d.calisma_durumu = static_cast<std::uint8_t>(a2::CalismaDurumu::calisiyor);
  d.d.bayraklar = takvim_disi ? a2::kDurumTakvimDisi : 0;
  d.govdeler = senaryo_.govde_durumlari(t);
  kodla_durum(giden_, sira(), d);
  ++sayac_.durum;
  ++sayac_.gonderilen_ileti;
}

void SahteSim::sim_ilerle(std::int64_t t_toplam_ns) {
  std::int64_t t_hedef_ns = t_toplam_ns - kayma_ns_;
  for (;;) {
    const std::uint64_t adim = takvim_adimi(takvim_k_);
    const auto t = static_cast<std::int64_t>(adim) * kAdimNs;
    const std::int64_t sifirlama = senaryo_.ayarlar().sifirlama_ns;
    if (sifirlama > 0 && t >= sifirlama) {
      // A2-K29, K80: sıfırlama; nesil +1, zaman 0'dan; yeni gövdeler yeni kimlikle doğar.
      senaryo_.sifirla();
      t_ns_ = 0;
      adim_no_ = 0;
      takvim_k_ = 0;
      t_hedef_ns -= t;
      kayma_ns_ += t;
      ++sayac_.sifirlama;
      if (asama_ == Asama::akis) {
        a2::SahneOlayi o{};
        o.t_ns = 0;
        o.adim_no = 0;
        o.tur = static_cast<std::uint16_t>(a2::SahneOlayiTuru::sifirlama);
        o.calisma_nesli = senaryo_.nesil();
        kodla_sahne_olayi(giden_, sira(), o);
        for (const auto& g : senaryo_.govde_bilgileri()) {
          a2::SahneOlayi d{};
          d.t_ns = 0;
          d.adim_no = 0;
          d.tur = static_cast<std::uint16_t>(a2::SahneOlayiTuru::govde_dogdu);
          d.govde_kimligi = g.govde_kimligi;
          d.sureklilik_no = 0;
          d.varlik_indeksi = g.varlik_indeksi;
          d.govde_turu = g.govde_turu;
          d.calisma_nesli = senaryo_.nesil();
          kodla_sahne_olayi(giden_, sira(), d);
        }
        sayac_.gonderilen_ileti += 3;
      }
      continue;
    }
    if (t > t_hedef_ns) break;
    t_ns_ = t;
    adim_no_ = adim;
    ++takvim_k_;
    durum_gonder(t, adim, false);
  }
}

void SahteSim::zaman(std::int64_t duvar_ns) {
  duvar_ = duvar_ns > 0 ? duvar_ns : 1;
  if (asama_ == Asama::yok || asama_ == Asama::kapaniyor) return;
  if (asama_ == Asama::merhaba_bekleniyor && duvar_ns - baglanti_duvar_ > a2::kMerhabaZamanAsimiNs) {
    kapat("merhaba 5 s içinde gelmedi (A2-K10)");
    return;
  }
  if (asama_ == Asama::akis && !hazir_alindi_ && duvar_ns - baglanti_duvar_ > a2::kHazirZamanAsimiNs) {
    kodla_ret(giden_, sira(), a2::RetNedeni::zaman_asimi, "hazir 120 s içinde gelmedi");
    kapat("hazir zaman aşımı (A2-K15)");
    return;
  }
  if (duvar_ns - son_alim_duvar_ > a2::kNabizZamanAsimiNs) {
    kodla_veda(giden_, sira(), a2::VedaNedeni::nabiz_zaman_asimi);
    kapat("görselleştirici 3 s sessiz (A2-K62)");
    return;
  }
  if (asama_ == Asama::akis && duvar_ns - son_nabiz_duvar_ >= a2::kNabizAraligiNs) {
    son_nabiz_duvar_ = duvar_ns;
    a2::SimNabzi n{};
    n.t_ns = t_ns_;
    n.adim_no = adim_no_;
    n.gonderim_duvar_ns = duvar_ns;
    n.calisma_durumu = static_cast<std::uint8_t>(a2::CalismaDurumu::calisiyor);
    n.sim_kipi = static_cast<std::uint8_t>(a2::SimKipi::sil);
    kodla_sim_nabzi(giden_, sira(), n);
    ++sayac_.gonderilen_ileti;
  }
}

void SahteSim::veri_geldi(const Bayt* veri, std::size_t n, std::int64_t duvar_ns) {
  for (std::size_t i = 0; i < n && (asama_ == Asama::merhaba_bekleniyor || asama_ == Asama::akis);
       i += kEklemeParcasi) {
    parca_isle(veri + i, (n - i < kEklemeParcasi) ? n - i : kEklemeParcasi, duvar_ns);
  }
}

void SahteSim::parca_isle(const Bayt* veri, std::size_t n, std::int64_t duvar_ns) {
  if (asama_ == Asama::yok || asama_ == Asama::kapaniyor) return;
  if (!okuyucu_.ekle(veri, n)) return hata(KodHata::yuk_buyuk, "bekleyen tampon");
  a2::Baslik b{};
  Baytlar yuk;
  KodHata h = KodHata::yok;
  while (asama_ == Asama::merhaba_bekleniyor || asama_ == Asama::akis) {
    if (!okuyucu_.sonraki(b, yuk, h)) {
      if (h != KodHata::yok) hata(h, "başlık");
      return;
    }
    son_alim_duvar_ = duvar_ns;
    // A2-K07: görselleştirici → sim yönünde boşluk da protokol hatasıdır.
    if (ilk_alim_ ? b.sira != 0 : b.sira != son_alinan_sira_ + 1u) return hata(KodHata::alan, "sıra");
    ilk_alim_ = false;
    son_alinan_sira_ = b.sira;
    const BaytGorunumu g(yuk);
    const auto tur = static_cast<a2::IletiTuru>(b.tur);
    if (tur == a2::IletiTuru::veda) {
      a2::Veda v{};
      if (coz_veda(g, v) != KodHata::yok) return hata(KodHata::alan, "veda");
      ++sayac_.veda_alinan;
      kapat("görselleştirici veda gönderdi (neden " + std::to_string(v.neden) + ")");
      return;
    }
    if (asama_ == Asama::merhaba_bekleniyor) {
      if (tur != a2::IletiTuru::merhaba) return hata(KodHata::alan, "ilk ileti merhaba değil");
      a2::Merhaba m{};
      const KodHata e = coz_merhaba(g, m);
      if (e == KodHata::alan) {
        kodla_ret(giden_, sira(), a2::RetNedeni::protokol, "platform ya da çizici geçersiz");
        kapat("merhaba alanı geçersiz");
        return;
      }
      if (e != KodHata::yok) return hata(e, "merhaba");
      if (m.surum_ana != a2::kSurumAna) {  // A2-K11
        kodla_ret(giden_, sira(), a2::RetNedeni::surum, "A2 ana sürümü 1 değil");
        kapat("sürüm uyuşmazlığı");
        return;
      }
      son_olay_ = "merhaba alındı";
      gonder_kurulum();
      continue;
    }
    switch (tur) {
      case a2::IletiTuru::hazir: {
        a2::Hazir hz{};
        const KodHata e = coz_hazir(g, hz);
        if (e != KodHata::yok) return hata(e, "hazir");
        if (hazir_alindi_) return hata(KodHata::alan, "ikinci hazir");
        hazir_alindi_ = true;
        ++sayac_.hazir;
        sayac_.son_hazir_durumu = hz.durum;
        sayac_.son_hazir_ozet_tuttu =
            std::memcmp(hz.manifest_sha256, senaryo_.ayarlar().manifest_ozeti.data(), 32) == 0;
        // A2-K14
        if (!sayac_.son_hazir_ozet_tuttu || hz.durum != 0) {
          const bool halka = hz.durum == 6 || hz.durum == 7;
          kodla_ret(giden_, sira(), halka ? a2::RetNedeni::halka : a2::RetNedeni::manifest,
                    sayac_.son_hazir_ozet_tuttu ? "hazir.durum başarısız" : "manifest özeti uyuşmuyor");
          kapat("hazir reddedildi (durum " + std::to_string(hz.durum) + ")");
          return;
        }
        son_olay_ = "hazir alındı";
        break;
      }
      case a2::IletiTuru::goruntuleyici_nabzi: {
        a2::GoruntuleyiciNabzi gn{};
        const KodHata e = coz_goruntuleyici_nabzi(g, gn);
        if (e != KodHata::yok) return hata(e, "goruntuleyici_nabzi");
        ++sayac_.gor_nabzi;
        break;
      }
      case a2::IletiTuru::kare_dustu:
      case a2::IletiTuru::kare_hazir:
      case a2::IletiTuru::halka_eslendi:
        return hata(KodHata::alan, "kamera yokken kamera iletisi");
      default:
        return hata(KodHata::alan, "beklenmeyen ileti");
    }
  }
}

}  // namespace ida::sahte
