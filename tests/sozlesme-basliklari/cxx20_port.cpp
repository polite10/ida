// Sözleşme başlıkları denetimi: A4 port.h, C++20 (sim sunucusu ve SIL koşucusu tüketir).
#include "ida/port.h"

#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<ida_in_t>, "ida_in_t standart düzen");
static_assert(std::is_trivially_copyable_v<ida_out_t>, "ida_out_t düz kopyalanabilir");

std::size_t ida_denetim_cxx20_port();
std::size_t ida_denetim_cxx20_port()
{
    return sizeof(ida_in_t) + sizeof(ida_out_t);
}
