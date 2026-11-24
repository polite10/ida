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
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <sys/qos.h>
#endif
#include <ctime>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include <algorithm>
#include <cmath>

namespace ida::sunucu {

#if defined(__APPLE__)
namespace {
std::uint64_t ns_mach(std::int64_t ns)
{
    static const mach_timebase_info_data_t tb = [] {
        mach_timebase_info_data_t t{};
        (void)mach_timebase_info(&t);
        return t;
    }();
    return static_cast<std::uint64_t>(ns) * tb.denom / tb.numer;
}
}  // namespace
#endif

void zamanlayici_onceligi()
{
#if defined(_WIN32)
    (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#elif defined(__APPLE__)
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    // Zaman kısıtlı (gerçek zamanlı) iş parçacığı politikası: fizik adımı periyodunda (1,25 ms)
    // en çok 0,5 ms hesap, 1,25 ms içinde bitmeli. Ses iş parçacıklarının kullandığı Mach
    // politikası; uyanma gecikmesini zamanlayıcı birleştirmesinden ve öncelik düşmesinden korur.
    thread_time_constraint_policy_data_t p{};
    p.period = static_cast<std::uint32_t>(ns_mach(1'250'000));
    p.computation = static_cast<std::uint32_t>(ns_mach(500'000));
    p.constraint = static_cast<std::uint32_t>(ns_mach(1'250'000));
    p.preemptible = 1;
    (void)thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_TIME_CONSTRAINT_POLICY,
                            reinterpret_cast<thread_policy_t>(&p), THREAD_TIME_CONSTRAINT_POLICY_COUNT);
#endif
}

void sure_uyu(std::int64_t ns)
{
    if (ns <= 0) {
        return;
    }
#if defined(_WIN32)
    // Yüksek çözünürlüklü beklenebilir zamanlayıcı (Windows 10 1803+). Ölçülmedi.
    static thread_local HANDLE z = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /* HIGH_RESOLUTION */,
                                                          TIMER_ALL_ACCESS);
    if (z != nullptr) {
        LARGE_INTEGER sure{};
        sure.QuadPart = -static_cast<LONGLONG>((ns + 99) / 100);   // göreli, 100 ns birim
        if (SetWaitableTimer(z, &sure, 0, nullptr, nullptr, FALSE) != 0) {
            (void)WaitForSingleObject(z, INFINITE);
            return;
        }
    }
    Sleep(0);
#elif defined(__APPLE__)
    (void)mach_wait_until(mach_absolute_time() + ns_mach(ns));
#else
    timespec ts{};
    ts.tv_sec = static_cast<decltype(ts.tv_sec)>(ns / 1'000'000'000);
    ts.tv_nsec = static_cast<decltype(ts.tv_nsec)>(ns % 1'000'000'000);
    (void)nanosleep(&ts, nullptr);
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

bool windows_komut_satiri(const std::vector<std::string>& argv, std::string& satir)
{
    satir.clear();
    for (std::size_t i = 0; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        if (i > 0) {
            satir += ' ';
        }
        if (i == 0) {
            if (a.find('"') != std::string::npos) {
                return false;
            }
            satir += '"' + a + '"';
            continue;
        }
        if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) {
            satir += a;
            continue;
        }
        satir += '"';
        std::size_t ters = 0;
        for (const char c : a) {
            if (c == '\\') {
                ++ters;
                continue;
            }
            if (c == '"') {
                satir.append(2 * ters + 1, '\\');
            } else {
                satir.append(ters, '\\');
            }
            ters = 0;
            satir += c;
        }
        satir.append(2 * ters, '\\');   // kapanış tırnağından önce
        satir += '"';
    }
    return true;
}

std::unique_ptr<AltSurec> surec_baslat(const std::vector<std::string>& argv, std::string& hata)
{
    if (argv.empty()) {
        hata = "boş komut";
        return nullptr;
    }
#if defined(_WIN32)
    std::string satir;
    if (!windows_komut_satiri(argv, satir)) {
        hata = "program adı tırnak içeriyor";
        return nullptr;
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
