// Sözleşme başlıkları denetimi: A8 algilama_kaydi.h tek başına, C++20.
#include "ida/algilama_kaydi.h"

#include <cstddef>

std::size_t ida_denetim_cxx20_algilama();
std::size_t ida_denetim_cxx20_algilama()
{
    return sizeof(ida_det_record_t) + sizeof(ida_det_batch_t);
}
