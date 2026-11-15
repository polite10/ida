#include <stddef.h>

size_t my_malloc_count = 0;
int free_list[4] = {1, 2, 3, 4};

int toplam(void)
{
    ++my_malloc_count;
    return free_list[0] + free_list[1] + free_list[2] + free_list[3];
}
