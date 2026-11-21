#include "ida/viz/oturum.hpp"

#include <algorithm>
#include <cstring>
#include <set>

namespace ida::viz {

namespace {
using a2::IletiTuru;
}

const char* asama_adi(Asama a) {
  switch (a) {
    case Asama::kapali: return "kapali";
    case Asama::merhaba_gonderildi: return "merhaba_gonderildi";
    case Asama::kurulum: return "kurulum";
    case Asama::uygulama_bekleniyor: return "uygulama_bekleniyor";
    case Asama::akis: return "akis";
  }
  return "?";
}

IstemciOturumu::IstemciOturumu(Ayarlar ayarlar) : ayar_(std::move(ayarlar)), saat_(ayar_.d_op_ns) {}

void IstemciOturumu::baglandi(std::int64_t simdi_ns, bool pencere_cizilebilir) {
  // Yeni oturum: önceki oturumun bütün durumu geçersiz (A2-K65). Sahne, yeni hosgeldin
  // işlenene kadar uygulamada donmuş kalır (A2-K63); burada yalnız protokol durumu sıfırlanır.
  asama_ = Asama::merhaba_gonderildi;
  kurulum_adimi_ = 0;
  okuyucu_.temizle();
  giden_.clear();
  giden_sira_ = 0;
  ilk_ileti_ = true;
  son_sira_ = 0;
  son_alim_ns_ = simdi_ns;
  son_nabiz_ns_ = simdi_ns;
  kapatilmali_ = false;
  yeniden_baglanabilir_ = true;
  tutarsiz_ = false;
  durum_var_ = false;
  atlama_bekleniyor_ = false;
  tablo_sonrasi_ilk_durum_ = false;
  kimlikler_.clear();
  istek_var_ = false;
  sayac_ = {};

  // A2-K10: bağlantı kurulunca ilk ileti merhaba.
  a2::Merhaba m{};
  m.surum_ana = a2::kSurumAna;
  m.surum_alt = a2::kSurumAlt;
  m.yetenekler = 0;  // kamera çizim yolu bu dilimde yok (bit 0 = 0)
  m.platform = ayar_.platform;
  m.cizici = static_cast<std::uint8_t>(a2::Cizici::forward_plus);
  m.pencere_cizilebilir = pencere_cizilebilir ? 1 : 0;
  m.surec_kimligi = ayar_.surec_kimligi;
  metin_yaz(m.godot_surumu, sizeof m.godot_surumu, ayar_.godot_surumu);
  metin_yaz(m.uzanti_surumu, sizeof m.uzanti_surumu, ayar_.uzanti_surumu);
  metin_yaz(m.gpu_adi, sizeof m.gpu_adi, ayar_.gpu_adi);
  kodla_merhaba(giden_, sonraki_sira(), m);
}

Baytlar IstemciOturumu::giden_al() {
  Baytlar b;
  b.swap(giden_);
  return b;
}

std::vector<Olay> IstemciOturumu::olaylari_al() {
  std::vector<Olay> o;
  o.swap(olaylar_);
  return o;
}

void IstemciOturumu::kapat(const std::string& neden, bool yeniden) {
  if (asama_ == Asama::kapali) return;
  asama_ = Asama::kapali;
  kapatilmali_ = true;
  yeniden_baglanabilir_ = yeniden;
  son_neden_ = neden;
  okuyucu_.temizle();
  olaylar_.push_back({OlayTuru::kapandi, neden});
}

void IstemciOturumu::hata(KodHata h, const char* baglam) {
  // A2-K06 / H01: veda(protokol_hatasi), kapat; A2-K63 ile yeniden bağlan.
  kodla_veda(giden_, sonraki_sira(), a2::VedaNedeni::protokol_hatasi);
  kapat(std::string("protokol hatası: ") + baglam + ": " + hata_adi(h), true);
}

void IstemciOturumu::protokol_hatasi(const std::string& neden) {
  if (asama_ == Asama::kapali) return;
  kodla_veda(giden_, sonraki_sira(), a2::VedaNedeni::protokol_hatasi);
  kapat("protokol hatası: " + neden, true);
}

void IstemciOturumu::baglanti_koptu(const std::string& neden) {
  if (asama_ == Asama::kapali) {
    kapatilmali_ = false;
    return;
  }
  kapat("bağlantı koptu: " + neden, true);
  kapatilmali_ = false;  // soket zaten kapandı
}

void IstemciOturumu::olagan_kapat() {
  if (asama_ == Asama::kapali) return;
  kodla_veda(giden_, sonraki_sira(), a2::VedaNedeni::olagan);
  kapat("olağan kapanış", false);
}

void IstemciOturumu::zaman(std::int64_t simdi_ns, std::uint32_t cizim_hizi_mhz, bool pencere_cizilebilir) {
  if (asama_ == Asama::kapali) return;
  // A2-K62: 3 s boyunca hiçbir ileti gelmezse karşı ölü sayılır.
  if (simdi_ns - son_alim_ns_ > a2::kNabizZamanAsimiNs) {
    kodla_veda(giden_, sonraki_sira(), a2::VedaNedeni::nabiz_zaman_asimi);
    kapat("nabız zaman aşımı (3 s ileti yok)", true);
    return;
  }
  // A2-K62: 1 s aralıkla nabız. Okuma: merhaba'dan itibaren gönderilir (hazir'dan önce de);
  // aksi hâlde A2-K15'in 120 s hazır penceresi A2-K62'nin 3 s'lik ölçütüyle çelişirdi.
  if (simdi_ns - son_nabiz_ns_ >= a2::kNabizAraligiNs) {
    son_nabiz_ns_ = simdi_ns;
    a2::GoruntuleyiciNabzi n{};
    n.gonderim_duvar_ns = simdi_ns;
    n.son_uygulanan_t_ns = saat_.gecerli() ? saat_.t_op() : 0;
    n.cizim_hizi_mhz = cizim_hizi_mhz;
    n.elle_cizim_sayaci = 0;
    n.operator_bekleme_sayaci = saat_.bekleme_sayaci();
    n.uretilen_kare = 0;
    n.dusen_kare = static_cast<std::uint32_t>(sayac_.dusen_kare);
    n.pencere_cizilebilir = pencere_cizilebilir ? 1 : 0;
    n.gpu_bellek_bayt = 0;
    kodla_goruntuleyici_nabzi(giden_, sonraki_sira(), n);
  }
}

void IstemciOturumu::veri_geldi(const Bayt* veri, std::size_t n, std::int64_t simdi_ns) {
  // sol #1: veri parça parça eklenir, her parçadan sonra çerçeveler çekilir; bekleyen bellek
  // kAzamiBekleyenBayt ile sınırlı.
  for (std::size_t i = 0; i < n && asama_ != Asama::kapali; i += kEklemeParcasi) {
    const std::size_t m = (n - i < kEklemeParcasi) ? n - i : kEklemeParcasi;
    parca_isle(veri + i, m, simdi_ns);
  }
}

void IstemciOturumu::parca_isle(const Bayt* veri, std::size_t n, std::int64_t simdi_ns) {
  if (asama_ == Asama::kapali) return;  // A2-K67: veda'dan sonrakiler yok sayılır
  if (!okuyucu_.ekle(veri, n)) return hata(KodHata::yuk_buyuk, "bekleyen tampon");
  a2::Baslik b{};
  Baytlar yuk;
  KodHata h = KodHata::yok;
  while (asama_ != Asama::kapali) {
    if (!okuyucu_.sonraki(b, yuk, h)) {
      if (h != KodHata::yok) hata(h, "başlık");
      return;
    }
    son_alim_ns_ = simdi_ns;
    ++sayac_.alinan_ileti;
    // A2-K07: ilk ileti 0; kesin artış; sim → görselleştirici yönünde boşluk sayılır.
    if (ilk_ileti_) {
      if (b.sira != 0) {
        hata(KodHata::alan, "ilk iletinin sırası 0 değil");
        return;
      }
      ilk_ileti_ = false;
    } else {
      if (!sira_yeni(b.sira, son_sira_)) {
        hata(KodHata::alan, "sıra azaldı ya da yinelendi");
        return;
      }
      sayac_.sira_boslugu += static_cast<std::uint32_t>(b.sira - son_sira_ - 1u);
    }
    son_sira_ = b.sira;
    isle(b, yuk, simdi_ns);
  }
}

void IstemciOturumu::isle(const a2::Baslik& b, const Baytlar& yuk, std::int64_t simdi_ns) {
  const auto tur = static_cast<IletiTuru>(b.tur);
  const BaytGorunumu g(yuk);

  // Her aşamada geçerli olanlar.
  if (tur == IletiTuru::veda) {
    a2::Veda v{};
    const KodHata e = coz_veda(g, v);
    if (e != KodHata::yok) return hata(e, "veda");
    kapat("sim veda gönderdi (neden " + std::to_string(v.neden) + ")", true);
    return;
  }
  if (tur == IletiTuru::ret) {
    a2::Ret r{};
    const KodHata e = coz_ret(g, r);
    if (e != KodHata::yok) return hata(e, "ret");
    const bool yeniden = !(r.neden == static_cast<std::uint32_t>(a2::RetNedeni::surum) ||
                           r.neden == static_cast<std::uint32_t>(a2::RetNedeni::manifest));
    kapat("sim reddetti (neden " + std::to_string(r.neden) + "): " + metin_oku(r.ayrinti, sizeof r.ayrinti),
          yeniden);
    return;
  }

  switch (asama_) {
    case Asama::merhaba_gonderildi: {
      if (tur != IletiTuru::hosgeldin) return hata(KodHata::alan, "hosgeldin beklenirken başka ileti");
      HosgeldinIleti h;
      const KodHata e = coz_hosgeldin(g, h);
      if (e != KodHata::yok) return hata(e, "hosgeldin");
      // A2-K11: oturum alt sürümü min(sim, görselleştirici) olmalı.
      if (h.h.surum_alt > a2::kSurumAlt) return hata(KodHata::alan, "hosgeldin alt sürümü bizimkinden büyük");
      hosgeldin_ = std::move(h);
      nesil_ = hosgeldin_.h.calisma_nesli;
      sahne_.temizle();
      saat_.sifirla();
      for (const auto& gb : hosgeldin_.govdeler) {
        kimlikler_[gb.govde_kimligi] = {gb.varlik_indeksi, gb.govde_turu};
        GovdeKaydi& k = sahne_.ekle(gb.govde_kimligi, gb.varlik_indeksi, gb.govde_turu);
        k.son_sureklilik = gb.sureklilik_no;
        k.dolu_tupler = gb.dolu_tupler;
      }
      asama_ = Asama::kurulum;
      kurulum_adimi_ = 0;
      return;
    }
    case Asama::kurulum: {
      // A2-K12: hosgeldin'den sonra sırasıyla dalga_kumesi, ortam, malzeme_optik.
      if (kurulum_adimi_ == 0) {
        if (tur != IletiTuru::dalga_kumesi) return hata(KodHata::alan, "dalga_kumesi beklenirken başka ileti");
        DalgaIleti d;
        const KodHata e = coz_dalga(g, d);
        if (e != KodHata::yok) return hata(e, "dalga_kumesi");
        dalga_ = std::move(d);
        kurulum_adimi_ = 1;
      } else if (kurulum_adimi_ == 1) {
        if (tur != IletiTuru::ortam) return hata(KodHata::alan, "ortam beklenirken başka ileti");
        a2::Ortam o{};
        const KodHata e = coz_ortam(g, o);
        if (e != KodHata::yok) return hata(e, "ortam");
        ortam_ = o;
        kurulum_adimi_ = 2;
      } else {
        if (tur != IletiTuru::malzeme_optik) return hata(KodHata::alan, "malzeme_optik beklenirken başka ileti");
        MalzemeIleti m;
        const KodHata e = coz_malzeme(g, m);
        if (e != KodHata::yok) return hata(e, "malzeme_optik");
        malzeme_ = std::move(m);
        asama_ = Asama::uygulama_bekleniyor;
        olaylar_.push_back({OlayTuru::sahne_kur, metin_oku(hosgeldin_.h.manifest_yolu,
                                                           sizeof hosgeldin_.h.manifest_yolu)});
      }
      return;
    }
    case Asama::uygulama_bekleniyor:
    case Asama::akis:
      break;
    case Asama::kapali:
      return;
  }

  const std::int64_t adim_ns = hosgeldin_.h.adim_ns;
  switch (tur) {
    case IletiTuru::durum: {
      DurumIleti d;
      const KodHata e = coz_durum(g, adim_ns, d);
      if (e != KodHata::yok) return hata(e, "durum");
      durum_isle(d, simdi_ns);
      return;
    }
    case IletiTuru::sahne_olayi: {
      a2::SahneOlayi o{};
      const KodHata e = coz_sahne_olayi(g, adim_ns, o);
      if (e != KodHata::yok) return hata(e, "sahne_olayi");
      olay_isle(o);
      return;
    }
    case IletiTuru::govde_tablosu: {
      GovdeTablosuIleti t;
      const KodHata e = coz_govde_tablosu(g, adim_ns, t);
      if (e != KodHata::yok) return hata(e, "govde_tablosu");
      tablo_isle(t);
      return;
    }
    case IletiTuru::sim_nabzi: {
      a2::SimNabzi n{};
      const KodHata e = coz_sim_nabzi(g, n);
      if (e != KodHata::yok) return hata(e, "sim_nabzi");
      calisma_durumu_ = n.calisma_durumu;
      return;
    }
    case IletiTuru::dalga_kumesi: {
      DalgaIleti d;
      const KodHata e = coz_dalga(g, d);
      if (e != KodHata::yok) return hata(e, "dalga_kumesi");
      if (d.d.surum <= dalga_.d.surum) return hata(KodHata::alan, "dalga_kumesi sürümü artmadı");
      dalga_ = std::move(d);
      olaylar_.push_back({OlayTuru::dalga_degisti, {}});
      return;
    }
    case IletiTuru::ortam: {
      a2::Ortam o{};
      const KodHata e = coz_ortam(g, o);
      if (e != KodHata::yok) return hata(e, "ortam");
      if (o.ortam_surumu <= ortam_.ortam_surumu) return hata(KodHata::alan, "ortam sürümü artmadı");
      ortam_ = o;
      olaylar_.push_back({OlayTuru::ortam_degisti, {}});
      return;
    }
    case IletiTuru::malzeme_optik: {
      MalzemeIleti m;
      const KodHata e = coz_malzeme(g, m);
      if (e != KodHata::yok) return hata(e, "malzeme_optik");
      malzeme_ = std::move(m);
      olaylar_.push_back({OlayTuru::malzeme_degisti, {}});
      return;
    }
    case IletiTuru::kamera_istegi: {
      KameraIstegiIleti k;
      const KodHata e = coz_kamera_istegi(g, k);
      if (e != KodHata::yok) return hata(e, "kamera_istegi");
      kamera_istegi_isle(k);
      return;
    }
    case IletiTuru::kamera_iptal: {
      a2::KameraIptal k{};
      const KodHata e = coz_kamera_iptal(g, k);
      if (e != KodHata::yok) return hata(e, "kamera_iptal");
      return;  // her isteğe zaten yanıt verildi
    }
    case IletiTuru::halka_yenile: {
      a2::HalkaBilgisi hb{};
      const KodHata e = coz_halka_bilgisi(g, hb);
      if (e != KodHata::yok) return hata(e, "halka_yenile");
      a2::HalkaEslendi he{};
      he.kamera_kimligi = hb.kamera_kimligi;
      he.halka_nesli = hb.halka_nesli;
      he.durum = static_cast<std::uint32_t>(a2::HazirDurumu::halka_eslenemedi);
      kodla_halka_eslendi(giden_, sonraki_sira(), he);
      return;
    }
    default:
      return hata(KodHata::alan, "bu aşamada beklenmeyen ileti");
  }
}

void IstemciOturumu::durum_isle(const DurumIleti& d, std::int64_t simdi_ns) {
  if (atlama_bekleniyor_) return hata(KodHata::alan, "atlamadan sonra govde_tablosu'ndan önce durum (A2-K91)");
  if (tablo_sonrasi_ilk_durum_) {
    // A2-K91: tablodan sonraki ilk durum tablonun zamanında ve aynı kimliklerle.
    if (d.d.t_ns != tablo_t_) return hata(KodHata::alan, "tablodan sonraki ilk durumun zamanı tutmuyor");
    std::set<std::uint32_t> a, b;
    for (const auto& gd : d.govdeler) a.insert(gd.govde_kimligi);
    for (const auto& [k, v] : sahne_.govdeler()) b.insert(k);
    if (a != b) return hata(KodHata::alan, "tablodan sonraki ilk durumun gövdeleri tabloyla aynı değil");
    tablo_sonrasi_ilk_durum_ = false;
  }
  // §3.4: t_ns önceki durum.t_ns'den küçük olamaz (sıfırlama ve atlama dışında).
  if (durum_var_ && d.d.t_ns < son_durum_t_) return hata(KodHata::alan, "durum zamanı geriledi");
  durum_var_ = true;
  son_durum_t_ = d.d.t_ns;
  calisma_durumu_ = d.d.calisma_durumu;
  // A2-K28
  tutarsiz_ = d.d.dalga_kumesi_surumu != dalga_.d.surum;
  if (tutarsiz_) ++sayac_.tutarsiz_durum;

  std::size_t yasayan_bulunan = 0;
  for (const auto& gd : d.govdeler) {
    GovdeKaydi* k = sahne_.bul(gd.govde_kimligi);
    // §3.4: yaşayan bir gövde olmalı.
    if (!k || d.d.t_ns >= k->t_olum || d.d.t_ns < k->t_dogum)
      return hata(KodHata::alan, "durumda yaşamayan gövde");
    if (gd.govde_turu != k->tur) return hata(KodHata::alan, "gövde türü doğumdakinden farklı");
    if (gd.sureklilik_no < k->son_sureklilik) return hata(KodHata::alan, "süreklilik numarası geriledi");
    k->son_sureklilik = gd.sureklilik_no;
    k->dolu_tupler = gd.dolu_tupler;
    ++yasayan_bulunan;
    if (!govde_durumu_gecerli(gd)) {  // A2-K27: son geçerli pozunda kalır, sayılır
      ++k->gecersiz_sayisi;
      ++sayac_.gecersiz_govde;
      Sahne::gecersiz_ekle(*k, d.d.t_ns, gd.sureklilik_no, gd.dolu_tupler);
      continue;
    }
    Sahne::ornek_ekle(*k, d.d.t_ns, gd);
  }
  std::size_t yasayan = 0;
  for (const auto& [kim, k] : sahne_.govdeler()) {
    if (d.d.t_ns >= k.t_dogum && d.d.t_ns < k.t_olum) ++yasayan;
  }
  if (yasayan > yasayan_bulunan) sayac_.eksik_govde += yasayan - yasayan_bulunan;
  saat_.durum_alindi(d.d.t_ns, simdi_ns);
  saat_.duraklat(d.d.calisma_durumu == static_cast<std::uint8_t>(a2::CalismaDurumu::duraklatildi));
}

bool IstemciOturumu::kimlik_esle(std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur) {
  auto it = kimlikler_.find(kimlik);
  if (it == kimlikler_.end()) {
    kimlikler_[kimlik] = {varlik, tur};
    return true;
  }
  return it->second.first == varlik && it->second.second == tur;
}

void IstemciOturumu::olay_isle(const a2::SahneOlayi& o) {
  if (atlama_bekleniyor_) return hata(KodHata::alan, "atlama bloğunda sahne olayı (A2-K90)");
  const auto tur = static_cast<a2::SahneOlayiTuru>(o.tur);
  const bool nesil_degisir = tur == a2::SahneOlayiTuru::sifirlama || tur == a2::SahneOlayiTuru::atlama;
  // A2-K80: nesil sıfırlama ve atlamada tam 1 artar; diğer olaylarda geçerli nesildir.
  if (nesil_degisir ? o.calisma_nesli != nesil_ + 1 : o.calisma_nesli != nesil_)
    return hata(KodHata::alan, "olayın çalıştırma nesli tutmuyor");

  switch (tur) {
    case a2::SahneOlayiTuru::govde_dogdu: {
      // A2-K92: kimlik oturum boyunca tek gövdedir. Tekrar izlemede geri atlamadan sonra aynı
      // gövde (aynı kimlik, varlık, tür) yeniden doğabilir (sol #3); başka varlık ya da türle
      // gelmesi, yaşayan bir gövdenin yinelenen doğumu ve tekrar izleme dışında yeniden
      // kullanım (sıfırlama kimlikleri yeniden kullanmaz) protokol hatasıdır.
      auto onceki = kimlikler_.find(o.govde_kimligi);
      if (onceki != kimlikler_.end()) {
        if (hosgeldin_.h.sim_kipi != static_cast<std::uint8_t>(a2::SimKipi::tekrar_izleme))
          return hata(KodHata::alan, "doğan gövdenin kimliği daha önce kullanıldı (A2-K92)");
        if (onceki->second.first != o.varlik_indeksi || onceki->second.second != o.govde_turu)
          return hata(KodHata::alan, "kimlik başka varlık ya da türle doğdu (A2-K92)");
        const GovdeKaydi* yasayan = sahne_.bul(o.govde_kimligi);
        if (yasayan && o.t_ns < yasayan->t_olum)
          return hata(KodHata::alan, "yaşayan gövde yeniden doğdu");
      }
      kimlikler_[o.govde_kimligi] = {o.varlik_indeksi, o.govde_turu};
      GovdeKaydi& k = sahne_.ekle(o.govde_kimligi, o.varlik_indeksi, o.govde_turu);
      k.t_dogum = o.t_ns;
      k.son_sureklilik = o.sureklilik_no;
      return;
    }
    case a2::SahneOlayiTuru::govde_oldu: {
      GovdeKaydi* k = sahne_.bul(o.govde_kimligi);
      if (!k || k->t_olum != kZamanArtiSonsuz) return hata(KodHata::alan, "ölen gövde yaşamıyor");
      k->t_olum = o.t_ns;
      return;
    }
    case a2::SahneOlayiTuru::kutle_durumu: {
      GovdeKaydi* k = sahne_.bul(o.govde_kimligi);
      if (!k || k->tur != static_cast<std::uint8_t>(a2::GovdeTuru::arac))
        return hata(KodHata::alan, "kütle durumu araç olmayan gövdede");
      if (o.sureklilik_no < k->son_sureklilik) return hata(KodHata::alan, "kütle durumunda süreklilik geriledi");
      k->son_sureklilik = o.sureklilik_no;
      k->dolu_tupler = o.yeni_tupler;
      return;
    }
    case a2::SahneOlayiTuru::isabet:
      return;  // yalnız görselleştirme işareti; bu dilimde çizilmez
    case a2::SahneOlayiTuru::sifirlama: {
      // A2-K29: bütün gövdeler ölmüş sayılır, geçmiş silinir, nesil artar.
      nesil_ = o.calisma_nesli;
      sahne_.temizle();
      saat_.sifirla();
      durum_var_ = false;
      olaylar_.push_back({OlayTuru::sifirlandi, "sıfırlama"});
      return;
    }
    case a2::SahneOlayiTuru::duraklat:
      saat_.duraklat(true);
      return;
    case a2::SahneOlayiTuru::devam:
      saat_.duraklat(false);
      return;
    case a2::SahneOlayiTuru::atlama: {
      // A2-K91: geçmiş silinir, gövde tablosu gelene kadar sahne değişmez.
      nesil_ = o.calisma_nesli;
      atlama_bekleniyor_ = true;
      atlama_t_ = o.t_ns;
      atlama_nesli_ = o.calisma_nesli;
      for (auto& [kim, k] : sahne_.govdeler()) k.gecmis.clear();
      saat_.sifirla();
      durum_var_ = false;
      olaylar_.push_back({OlayTuru::sifirlandi, "atlama"});
      return;
    }
  }
}

void IstemciOturumu::tablo_isle(const GovdeTablosuIleti& t) {
  if (!atlama_bekleniyor_) return hata(KodHata::alan, "atlama olmadan govde_tablosu");
  if (t.t.t_ns != atlama_t_ || t.t.calisma_nesli != atlama_nesli_)
    return hata(KodHata::alan, "govde_tablosu zamanı ya da nesli atlamayla tutmuyor (A2-K91)");
  for (const auto& gb : t.govdeler) {
    if (!kimlik_esle(gb.govde_kimligi, gb.varlik_indeksi, gb.govde_turu))
      return hata(KodHata::alan, "kimlik başka varlık ya da türle geldi (A2-K92)");
  }
  std::set<std::uint32_t> tablodaki;
  for (const auto& gb : t.govdeler) tablodaki.insert(gb.govde_kimligi);
  std::vector<std::uint32_t> sil;
  for (const auto& [kim, k] : sahne_.govdeler()) {
    if (tablodaki.count(kim) == 0) sil.push_back(kim);
  }
  for (std::uint32_t kim : sil) sahne_.sil(kim);
  for (const auto& gb : t.govdeler) {
    GovdeKaydi* k = sahne_.bul(gb.govde_kimligi);
    if (!k) k = &sahne_.ekle(gb.govde_kimligi, gb.varlik_indeksi, gb.govde_turu);
    k->t_dogum = kZamanEksiSonsuz;
    k->t_olum = kZamanArtiSonsuz;
    k->son_sureklilik = gb.sureklilik_no;
    k->dolu_tupler = gb.dolu_tupler;
    k->gecmis.clear();
  }
  atlama_bekleniyor_ = false;
  tablo_sonrasi_ilk_durum_ = true;
  tablo_t_ = t.t.t_ns;
}

void IstemciOturumu::kamera_istegi_isle(const KameraIstegiIleti& k) {
  if (atlama_bekleniyor_) return hata(KodHata::alan, "atlama bloğunda kamera isteği (A2-K91)");
  // §3.5: istek kimliği kesin artan; kare sırası > 0 ve kamera başına kesin artan.
  if (istek_var_ && k.k.istek_kimligi <= son_istek_) return hata(KodHata::alan, "istek kimliği artmadı");
  if (k.k.kare_sirasi == 0 || (istek_var_ && !sira_yeni(k.k.kare_sirasi, son_kare_sirasi_)))
    return hata(KodHata::alan, "kare sırası artmadı");
  istek_var_ = true;
  son_istek_ = k.k.istek_kimligi;
  son_kare_sirasi_ = k.k.kare_sirasi;
  ++sayac_.kamera_istegi;
  a2::KareDustu kd{};
  kd.istek_kimligi = k.k.istek_kimligi;
  kd.halka_nesli = k.k.halka_nesli;
  kd.yuva = k.k.yuva;
  kd.kamera_kimligi = k.k.kamera_kimligi;
  kd.kare_sirasi = k.k.kare_sirasi;
  kd.calisma_nesli = k.k.calisma_nesli;
  // Kamera yapılandırılmamışsa istek geçersizdir; yapılandırılmışsa bu dilimde kamera
  // çizilmediği için "sahne hazır değil" (Okuma).
  kd.neden = static_cast<std::uint8_t>(hosgeldin_.h.kamera_sayisi == 0 ? a2::KareDusmeNedeni::gecersiz_istek
                                                                         : a2::KareDusmeNedeni::sahne_hazir_degil);
  kodla_kare_dustu(giden_, sonraki_sira(), kd);
  ++sayac_.dusen_kare;
}

void IstemciOturumu::sahne_sonucu(a2::HazirDurumu durum, const Ozet& manifest_ozeti) {
  if (asama_ != Asama::uygulama_bekleniyor) return;
  a2::Hazir h{};
  std::memcpy(h.manifest_sha256, manifest_ozeti.data(), 32);
  h.dalga_kumesi_surumu = dalga_.d.surum;
  h.ortam_surumu = ortam_.ortam_surumu;
  h.halka_eslendi_maskesi = 0;
  // Bu dilimde halka eşlenmez: kamera yapılandırılmışsa sahne kurulsa da hazır değildir
  // (A2-K13: herhangi bir adım başarısızsa ilgili kod).
  if (durum == a2::HazirDurumu::basarili && hosgeldin_.h.kamera_sayisi > 0) durum = a2::HazirDurumu::halka_eslenemedi;
  h.durum = static_cast<std::uint32_t>(durum);
  kodla_hazir(giden_, sonraki_sira(), h);
  asama_ = Asama::akis;
}

}  // namespace ida::viz
