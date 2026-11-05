#include "lvgl.h"
#include "esp_heap_caps.h"
#include <string.h>

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
void *lv_malloc_core(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ptr ? ptr : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}
void *lv_realloc_core(void *ptr, size_t size)
{
    // A failed realloc keeps ptr valid. malloc is allowed to use PSRAM by sdkconfig.
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
}
void lv_free_core(void *ptr) { heap_caps_free(ptr); }
void lv_mem_monitor_core(lv_mem_monitor_t *monitor)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    memset(monitor, 0, sizeof(*monitor));
    monitor->total_size = info.total_free_bytes + info.total_allocated_bytes;
    monitor->free_size = info.total_free_bytes;
    monitor->free_biggest_size = info.largest_free_block;
}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
