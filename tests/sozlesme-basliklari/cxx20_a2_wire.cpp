// Sözleşme başlıkları denetimi: A2 a2_wire.h, C++20 (sim sunucusu ve görselleştirici).
#include "ida/a2_wire.h"

std::uint32_t ida_denetim_cxx20_a2();
std::uint32_t ida_denetim_cxx20_a2()
{
    return ida::a2::kAzamiDurumYuku + static_cast<std::uint32_t>(sizeof(ida::a2::YuvaBasligi));
}
