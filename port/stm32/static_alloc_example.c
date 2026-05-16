/* static_alloc_example.c — using TinyA2A without the heap.
 *
 * The library exposes opaque handles and uses malloc only inside
 * ta2a_journal_create() and ta2a_queue_create(). For projects that disable
 * malloc on Cortex-M, you can either:
 *   (1) Provide a small bump-allocator that backs malloc with a static
 *       arena (linker-script friendly), or
 *   (2) Replace the journal/queue with statically-sized wrappers that
 *       reuse the same struct definitions internally.
 *
 * This file shows option (1), which is usually the smallest patch.
 *
 * To use:
 *   - Add this file to your build alongside the rest of the library.
 *   - Define TA2A_STATIC_ARENA_BYTES to your budget (default 8 KB).
 *   - Don't link against newlib's malloc; this provides one.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef TA2A_STATIC_ARENA_BYTES
#define TA2A_STATIC_ARENA_BYTES (8 * 1024)
#endif

static uint8_t  s_arena[TA2A_STATIC_ARENA_BYTES] __attribute__((aligned(8)));
static size_t   s_used = 0;

void *malloc(size_t n) {
    /* round up to 8-byte alignment */
    size_t a = (n + 7u) & ~(size_t)7u;
    if (s_used + a > sizeof(s_arena)) return NULL;
    void *p = &s_arena[s_used];
    s_used += a;
    return p;
}

void *calloc(size_t nm, size_t n) {
    size_t total = nm * n;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void free(void *p) {
    /* bump-allocator: free is a no-op. The journal and queue are created
     * once at boot for the lifetime of the process — that's the supported
     * usage. If you destroy and re-create them, this allocator leaks.
     * For that case, swap in a real allocator (umm_malloc, FreeRTOS heap_4). */
    (void)p;
}
