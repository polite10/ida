// Sunucunun iç yardımcıları (yardimci.hpp).
#include "yardimci.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#endif
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include <algorithm>
#include <cmath>

namespace ida::sunucu {

SekilTersi::SekilTersi(const std::array<double, 5>& c)
{
    double enb = 0.0;
    for (int j = 0; j <= kN; ++j) {
        const double u = static_cast<double>(j) / kN;
        const double u2 = u * u;
        double t = c[4];
        t = t * u2 + c[3];
        t = t * u2 + c[2];
        t = t * u2 + c[1];
        t = t * u2 + c[0];
        enb = std::max(enb, t * u);
        tablo_[static_cast<std::size_t>(j)] = enb;
    }
}

double SekilTersi::sekil(double u) const
{
    const double x = std::clamp(u, 0.0, 1.0) * kN;
    const auto i = static_cast<std::size_t>(std::min(std::floor(x), static_cast<double>(kN - 1)));
    const double f = x - static_cast<double>(i);
    return tablo_[i] + f * (tablo_[i + 1U] - tablo_[i]);
}

double SekilTersi::ters(double hedef) const
{
    double lo = 0.0;
    double hi = 1.0;
    for (int k = 0; k < 60; ++k) {
        const double orta = 0.5 * (lo + hi);
        if (sekil(orta) < hedef) {
            lo = orta;
        } else {
            hi = orta;
        }
    }
    return hi;
}

double SekilTersi::olcekle(double pwm, double carpan, double notr, double kazanc, double olu_bant) const
{
    const double d = pwm - notr;
    if (std::abs(d) <= olu_bant) {
        return pwm;
    }
    const double u = std::min(std::abs(d) / kazanc, 1.0);
    const double u2 = ters(carpan * sekil(u));
    const double d2 = std::round(u2 * kazanc);
    return d > 0.0 ? notr + d2 : notr - d2;
}

void zamanlayici_onceligi()
{
#if defined(_WIN32)
    (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#elif defined(__APPLE__)
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

namespace {

#if defined(_WIN32)
class WinSurec final : public AltSurec {
public:
    explicit WinSurec(PROCESS_INFORMATION pi) : pi_(pi) {}
    ~WinSurec() override
    {
        sonlandir();
        CloseHandle(pi_.hThread);
        CloseHandle(pi_.hProcess);
    }
    bool calisiyor() override { return WaitForSingleObject(pi_.hProcess, 0) == WAIT_TIMEOUT; }
    void sonlandir() override
    {
        if (calisiyor()) {
            if (WaitForSingleObject(pi_.hProcess, 2000) == WAIT_TIMEOUT) {
                TerminateProcess(pi_.hProcess, 1);
            }
        }
    }

private:
    PROCESS_INFORMATION pi_;
};
#else
class PosixSurec final : public AltSurec {
public:
    explicit PosixSurec(pid_t p) : pid_(p) {}
    ~PosixSurec() override { sonlandir(); }
    bool calisiyor() override
    {
        if (pid_ <= 0) {
            return false;
        }
        int durum = 0;
        if (waitpid(pid_, &durum, WNOHANG) == pid_) {
            pid_ = -1;
            return false;
        }
        return true;
    }
    void sonlandir() override
    {
        if (pid_ <= 0) {
            return;
        }
        for (int i = 0; i < 200 && calisiyor(); ++i) {
            usleep(10'000);   // koşucu bağlantı kapanınca kendisi çıkar (bekleme süresi)
        }
        if (pid_ > 0) {
            (void)kill(pid_, SIGTERM);
            int durum = 0;
            (void)waitpid(pid_, &durum, 0);
            pid_ = -1;
        }
    }

private:
    pid_t pid_;
};
#endif

}  // namespace

std::unique_ptr<AltSurec> surec_baslat(const std::vector<std::string>& argv, std::string& hata)
{
    if (argv.empty()) {
        hata = "boş komut";
        return nullptr;
    }
#if defined(_WIN32)
    std::string satir;
    for (const std::string& a : argv) {
        if (!satir.empty()) {
            satir += ' ';
        }
        satir += '"' + a + '"';
    }
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::vector<char> tampon(satir.begin(), satir.end());
    tampon.push_back('\0');
    if (CreateProcessA(nullptr, tampon.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) == 0) {
        hata = "CreateProcess: hata " + std::to_string(GetLastError());
        return nullptr;
    }
    return std::make_unique<WinSurec>(pi);
#else
    std::vector<char*> av;
    std::vector<std::string> kopya = argv;
    for (std::string& a : kopya) {
        av.push_back(a.data());
    }
    av.push_back(nullptr);
    pid_t pid = 0;
    const int r = posix_spawn(&pid, av[0], nullptr, nullptr, av.data(), environ);
    if (r != 0) {
        hata = "posix_spawn: hata " + std::to_string(r);
        return nullptr;
    }
    return std::make_unique<PosixSurec>(pid);
#endif
}

}  // namespace ida::sunucu
