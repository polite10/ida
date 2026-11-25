// test_destek.cpp — sim/kayit testlerinin ortak yardımcıları.
#include "test_destek.hpp"

#include "crc32.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;  // NOLINT
#endif

namespace ida::kayit::test {

std::uint64_t surec_kimligi()
{
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

GeciciDizin::GeciciDizin(std::string_view ad)
{
    yol_ = std::filesystem::temp_directory_path() /
           ("ida_kayit_test_" + std::to_string(surec_kimligi()) + "_" + std::string(ad));
    std::error_code ec;
    std::filesystem::remove_all(yol_, ec);
    std::filesystem::create_directories(yol_, ec);
}

GeciciDizin::~GeciciDizin()
{
    std::error_code ec;
    std::filesystem::remove_all(yol_, ec);
}

AcilisUstVerisi ornek_ust_veri(std::string_view run_id, std::string_view kip)
{
    AcilisUstVerisi u;
    u.a3_surumu = "1.0";
    u.a1_proto_surumu = "1.0";
    u.a1_dialect_hash = "1234567890123456789";
    u.a5_sema_surumu = "1.0";
    u.a7_manifest_ozeti = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    u.a7_kume = "ornek";
    u.a7_kume_surumu = "1";
    u.run_id = std::string(run_id);
    u.a1_run_id = "305419896";
    u.kip = std::string(kip);
    if (kip == "sil") {
        u.sil_n_bg = "2";
    }
    u.kademeler = "gorunum=sade,kamera=kapali,sonar=sade,kayit=telemetri";
    u.tohum = "42";
    u.senaryo_ozeti = "sha256:1111111111111111111111111111111111111111111111111111111111111111";
    u.senaryo_kaynak_ozetleri = "{}";
    u.sim_surumu = "sinama";
    u.platform = "macos-arm64";
    u.derleyici = "sinama";
    u.derleme_bayraklari = "-ffp-contract=off";
    u.bagimliliklar = "eigen=5.0.1";
    u.rng_surumu = "1";
    u.dalga_listesi_ozeti = "yok";
    u.kontrol_hizi_hz = "200";
    u.fizik_adimi_ns = "1250000";
    u.l_act_ns = "10000000";
    u.baslangic_utc = "2026-11-25T10:15:00Z";
    return u;
}

std::vector<Ek> OrnekEkler::ekler() const
{
    std::vector<Ek> e;
    for (const auto& [ad, veri] : icerik) {
        e.push_back(Ek{ad, std::span<const std::uint8_t>(veri.data(), veri.size())});
    }
    return e;
}

OrnekEkler ornek_ekler()
{
    OrnekEkler o;
    for (const EkTanimi& t : ekler()) {
        if (t.ad == kKatalogEki) {
            continue;
        }
        const std::string metin = "{\"ek\":\"" + std::string(t.ad) + "\",\"sinama\":true}";
        o.icerik.emplace_back(std::string(t.ad), std::vector<std::uint8_t>(metin.begin(), metin.end()));
    }
    return o;
}

KapanisAlanlari ornek_kapanis()
{
    KapanisAlanlari k;
    k.bitis_durumu = BitisDurumu::bitti;
    k.son_adim_no = 4800;
    k.son_sim_t_ns = 6'000'000'000;
    k.gecerli = true;
    k.gercek_zamani_kacirdi = false;
    k.sayaclar = "kayit_dusen=0";
    return k;
}

Ayar elle_ayar(const GeciciDizin& d, Kademe kademe, const SahteSaat& saat, std::size_t halka_bayt)
{
    Ayar a;
    a.dizin = d.yol();
    a.kademe = kademe;
    a.ureticiler.push_back(UreticiAyari{"dunya", halka_bayt});
    a.is_parcacigi = false;
    a.saat = saat.islev();
    return a;
}

Duzenek::Duzenek(std::string_view ad, Kademe kademe, std::string_view kip, const std::function<void(Ayar&)>& ayarla)
    : dizin(ad), ekler(ornek_ekler()), ayar(elle_ayar(dizin, kademe, saat))
{
    if (ayarla) {
        ayarla(ayar);
    }
    const std::vector<Ek> e = ekler.ekler();
    y = Yazici::ac(ayar, ornek_ust_veri(kOrnekRunId, kip), e, hata);
    if (!y) {
        std::printf("  düzenek açılamadı: %s\n", hata.c_str());
    }
}

std::unique_ptr<Okuyucu> Duzenek::oku()
{
    return Okuyucu::ac(dizin.yol() / (std::string(kOrnekRunId) + ".mcap"), acma);
}

std::vector<std::uint8_t> dosya_oku(const std::filesystem::path& yol)
{
    std::ifstream f(yol, std::ios::binary);
    std::vector<std::uint8_t> v;
    if (!f) {
        return v;
    }
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    v.resize(static_cast<std::size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(v.data()), n);
    }
    return v;
}

bool dosya_yaz(const std::filesystem::path& yol, const std::vector<std::uint8_t>& veri)
{
    std::ofstream f(yol, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }
    if (!veri.empty()) {
        f.write(reinterpret_cast<const char*>(veri.data()), static_cast<std::streamsize>(veri.size()));
    }
    return static_cast<bool>(f);
}

std::vector<OkunanIleti> hepsini_oku(Okuyucu& o)
{
    std::vector<OkunanIleti> v;
    o.basa_sar();
    Ileti i;
    while (o.sonraki(i)) {
        OkunanIleti k;
        k.kanal = std::string(i.kanal->ad);
        k.log_time = i.log_time;
        k.publish_time = i.publish_time;
        k.sequence = i.sequence;
        k.veri.assign(i.veri.begin(), i.veri.end());
        v.push_back(std::move(k));
    }
    return v;
}

std::vector<KayitKonumu> kayitlari_tara(const std::vector<std::uint8_t>& dosya)
{
    std::vector<KayitKonumu> v;
    std::uint64_t k = 8;
    while (k + 9 <= dosya.size()) {
        KayitKonumu r;
        r.op = dosya[static_cast<std::size_t>(k)];
        std::uint64_t n = 0;
        std::memcpy(&n, dosya.data() + k + 1, 8);  // testler küçük uçlu makinelerde koşar
        r.konum = k;
        r.govde = k + 9;
        r.boy = n;
        if (n > dosya.size() - r.govde) {
            break;
        }
        v.push_back(r);
        k = r.govde + n;
        if (r.op == 0x02) {
            break;  // Footer
        }
    }
    return v;
}

std::uint64_t veri_sonu_konumu(const std::vector<std::uint8_t>& dosya)
{
    std::uint64_t konum = 0;
    for (const KayitKonumu& k : kayitlari_tara(dosya)) {
        if (k.op == 0x0F && k.boy == 4) {
            konum = k.konum;
        }
    }
    return konum;
}

bool veri_crc_yenile(std::vector<std::uint8_t>& dosya)
{
    const std::uint64_t konum = veri_sonu_konumu(dosya);
    if (konum == 0) {
        return false;
    }
    const std::uint32_t crc = ic::crc32(dosya.data(), static_cast<std::size_t>(konum));
    std::memcpy(dosya.data() + konum + 9, &crc, 4);  // testler küçük uçlu makinelerde koşar
    return true;
}

// ---- elle MCAP kurucu ----
void ElleMcap::u8(std::uint8_t v)
{
    b_.push_back(v);
}
void ElleMcap::u16(std::uint16_t v)
{
    for (unsigned i = 0; i < 2; ++i) {
        b_.push_back(static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu));
    }
}
void ElleMcap::u32(std::uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) {
        b_.push_back(static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu));
    }
}
void ElleMcap::u64(std::uint64_t v)
{
    for (unsigned i = 0; i < 8; ++i) {
        b_.push_back(static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu));
    }
}
void ElleMcap::dize(std::string_view s)
{
    u32(static_cast<std::uint32_t>(s.size()));
    b_.insert(b_.end(), s.begin(), s.end());
}
void ElleMcap::kayit_basi(std::uint8_t op, std::uint64_t boy)
{
    u8(op);
    u64(boy);
}

ElleMcap& ElleMcap::sihir()
{
    static constexpr std::uint8_t kSihir[] = {0x89, 'M', 'C', 'A', 'P', 0x30, '\r', '\n'};
    b_.insert(b_.end(), std::begin(kSihir), std::end(kSihir));
    return *this;
}

ElleMcap& ElleMcap::baslik(std::string_view profil, std::string_view kitaplik)
{
    kayit_basi(0x01, 4 + profil.size() + 4 + kitaplik.size());
    dize(profil);
    dize(kitaplik);
    return *this;
}

ElleMcap& ElleMcap::sema(std::uint16_t id, std::string_view ad, std::string_view kodlama, std::string_view veri)
{
    kayit_basi(0x03, 2 + 4 + ad.size() + 4 + kodlama.size() + 4 + veri.size());
    u16(id);
    dize(ad);
    dize(kodlama);
    dize(veri);
    return *this;
}

namespace {
std::uint32_t eslem_boyu(const ElleMcap::Eslem& m)
{
    std::size_t n = 0;
    for (const auto& [k, v] : m) {
        n += 4 + k.size() + 4 + v.size();
    }
    return static_cast<std::uint32_t>(n);
}
}  // namespace

ElleMcap& ElleMcap::kanal(std::uint16_t id, std::uint16_t sema_id, std::string_view konu, std::string_view kodlama,
                          const Eslem& metadata)
{
    const std::uint32_t m = eslem_boyu(metadata);
    kayit_basi(0x04, 2 + 2 + 4 + konu.size() + 4 + kodlama.size() + 4 + m);
    u16(id);
    u16(sema_id);
    dize(konu);
    dize(kodlama);
    u32(m);
    for (const auto& [k, v] : metadata) {
        dize(k);
        dize(v);
    }
    return *this;
}

ElleMcap& ElleMcap::ileti(std::uint16_t kanal_id, std::uint32_t sira, std::uint64_t log_time, std::uint64_t publish_time,
                          const std::vector<std::uint8_t>& veri)
{
    kayit_basi(0x05, 2 + 4 + 8 + 8 + veri.size());
    u16(kanal_id);
    u32(sira);
    u64(log_time);
    u64(publish_time);
    b_.insert(b_.end(), veri.begin(), veri.end());
    return *this;
}

ElleMcap& ElleMcap::parca(const std::vector<std::uint8_t>& kayitlar)
{
    kayit_basi(0x06, 8 + 8 + 8 + 4 + 4 + 8 + kayitlar.size());
    u64(0);
    u64(0);
    u64(kayitlar.size());
    u32(ic::crc32(kayitlar.data(), kayitlar.size()));
    dize("");
    u64(kayitlar.size());
    b_.insert(b_.end(), kayitlar.begin(), kayitlar.end());
    return *this;
}

ElleMcap& ElleMcap::ust_veri(std::string_view ad, const Eslem& icerik)
{
    const std::uint32_t m = eslem_boyu(icerik);
    kayit_basi(0x0C, 4 + ad.size() + 4 + m);
    dize(ad);
    u32(m);
    for (const auto& [k, v] : icerik) {
        dize(k);
        dize(v);
    }
    return *this;
}

ElleMcap& ElleMcap::ek(std::string_view ad, std::string_view ortam_turu, const std::vector<std::uint8_t>& veri)
{
    kayit_basi(0x09, 8 + 8 + 4 + ad.size() + 4 + ortam_turu.size() + 8 + veri.size() + 4);
    const std::size_t bas = b_.size();
    u64(0);
    u64(0);
    dize(ad);
    dize(ortam_turu);
    u64(veri.size());
    b_.insert(b_.end(), veri.begin(), veri.end());
    u32(ic::crc32(b_.data() + bas, b_.size() - bas));
    return *this;
}

ElleMcap& ElleMcap::veri_sonu(std::uint32_t crc)
{
    kayit_basi(0x0F, 4);
    u32(crc);
    return *this;
}

ElleMcap& ElleMcap::veri_sonu_crcli()
{
    return veri_sonu(ic::crc32(b_.data(), b_.size()));
}

ElleMcap& ElleMcap::footer()
{
    kayit_basi(0x02, 20);
    u64(0);
    u64(0);
    u32(0);
    return sihir();
}

ElleMcap& ElleMcap::ham(const std::vector<std::uint8_t>& baytlar)
{
    b_.insert(b_.end(), baytlar.begin(), baytlar.end());
    return *this;
}

// ---- ayrı süreç ----
#if defined(_WIN32)

namespace {
std::string komut_satiri(const std::vector<std::string>& a)
{
    std::string k;
    for (const std::string& s : a) {
        if (!k.empty()) {
            k.push_back(' ');
        }
        k.push_back('"');
        k += s;
        k.push_back('"');
    }
    return k;
}

HANDLE baslat(const std::vector<std::string>& a)
{
    std::string k = komut_satiri(a);
    STARTUPINFOA si;
    std::memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof pi);
    if (!CreateProcessA(nullptr, k.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return nullptr;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}
}  // namespace

int surec_calistir(const std::vector<std::string>& argumanlar)
{
    const HANDLE h = baslat(argumanlar);
    if (h == nullptr) {
        return -1;
    }
    WaitForSingleObject(h, INFINITE);
    DWORD kod = 0;
    GetExitCodeProcess(h, &kod);
    CloseHandle(h);
    return static_cast<int>(kod);
}

std::uint64_t surec_baslat(const std::vector<std::string>& argumanlar)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(baslat(argumanlar)));
}

void surec_oldur(std::uint64_t tutamak)
{
    const HANDLE h = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(tutamak));
    if (h == nullptr) {
        return;
    }
    TerminateProcess(h, 9);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
}

#else

namespace {
pid_t baslat(const std::vector<std::string>& a)
{
    std::vector<std::string> kopya = a;
    std::vector<char*> argv;
    for (std::string& s : kopya) {
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0) {
        return 0;
    }
    return pid;
}
}  // namespace

int surec_calistir(const std::vector<std::string>& argumanlar)
{
    const pid_t pid = baslat(argumanlar);
    if (pid <= 0) {
        return -1;
    }
    int durum = 0;
    while (waitpid(pid, &durum, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(durum) ? WEXITSTATUS(durum) : -1;
}

std::uint64_t surec_baslat(const std::vector<std::string>& argumanlar)
{
    const pid_t pid = baslat(argumanlar);
    return pid <= 0 ? 0u : static_cast<std::uint64_t>(pid);
}

void surec_oldur(std::uint64_t tutamak)
{
    if (tutamak == 0) {
        return;
    }
    const auto pid = static_cast<pid_t>(tutamak);
    ::kill(pid, SIGKILL);
    int durum = 0;
    while (waitpid(pid, &durum, 0) < 0 && errno == EINTR) {
    }
}

#endif

}  // namespace ida::kayit::test
