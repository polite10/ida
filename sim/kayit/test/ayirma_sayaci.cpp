// ayirma_sayaci.cpp — genel `operator new`/`delete` yerine geçenler: bir iş parçacığında, sayım
// açıkken yapılan bellek ayırmalarını sayar (A6-K06: üretici çağrısı bellek ayırmaz).
//
// Sanitizer'lı derlemede yerine geçme yapılmaz (ASan kendi ayırıcısını izler); `ayirma_sayimi_var`
// false döner ve test ölçmeden geçer.
#include "test_destek.hpp"

#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define IDA_KAYIT_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define IDA_KAYIT_SANITIZER 1
#endif
#endif

namespace {
thread_local bool t_say = false;
thread_local std::uint64_t t_sayi = 0;
}  // namespace

namespace ida::kayit::test {

bool ayirma_sayimi_var() noexcept
{
#if defined(IDA_KAYIT_SANITIZER)
    return false;
#else
    return true;
#endif
}

void ayirma_sayimini_baslat() noexcept
{
    t_sayi = 0;
    t_say = true;
}

std::uint64_t ayirma_sayimini_bitir() noexcept
{
    t_say = false;
    return t_sayi;
}

}  // namespace ida::kayit::test

#if !defined(IDA_KAYIT_SANITIZER)

namespace {

void* ayir(std::size_t n) noexcept
{
    if (t_say) {
        ++t_sayi;
    }
    return std::malloc(n == 0 ? 1 : n);
}

void* hizali_ayir(std::size_t n, std::size_t hiza) noexcept
{
    if (t_say) {
        ++t_sayi;
    }
#if defined(_WIN32)
    return _aligned_malloc(n == 0 ? 1 : n, hiza);
#else
    void* p = nullptr;
    if (posix_memalign(&p, hiza < sizeof(void*) ? sizeof(void*) : hiza, n == 0 ? 1 : n) != 0) {
        return nullptr;
    }
    return p;
#endif
}

void hizali_birak(void* p) noexcept
{
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

}  // namespace

void* operator new(std::size_t n)
{
    void* p = ayir(n);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new[](std::size_t n)
{
    void* p = ayir(n);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
    return ayir(n);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
    return ayir(n);
}
void* operator new(std::size_t n, std::align_val_t a)
{
    void* p = hizali_ayir(n, static_cast<std::size_t>(a));
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new[](std::size_t n, std::align_val_t a)
{
    void* p = hizali_ayir(n, static_cast<std::size_t>(a));
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return hizali_ayir(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return hizali_ayir(n, static_cast<std::size_t>(a));
}

void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept
{
    std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    hizali_birak(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    hizali_birak(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{
    hizali_birak(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{
    hizali_birak(p);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
    hizali_birak(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
    hizali_birak(p);
}

#endif  // !IDA_KAYIT_SANITIZER
