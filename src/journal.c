/* journal.c — Idempotency journal: fixed-size ring of recent (key, ts) entries.
 * O(n) lookup with n typically 16..128; trivially fits in cache and RAM. */
#include "tinya2a.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char     key[TA2A_ID_LEN];
    uint64_t ts_ms;
    bool     used;
} journal_entry_t;

struct ta2a_journal {
    size_t           cap;
    size_t           count;
    size_t           next;       /* ring write index */
    journal_entry_t  *entries;
};

ta2a_journal_t *ta2a_journal_create(size_t capacity)
{
    if (capacity == 0) return NULL;
    ta2a_journal_t *j = (ta2a_journal_t *)calloc(1, sizeof(*j));
    if (!j) return NULL;
    j->entries = (journal_entry_t *)calloc(capacity, sizeof(journal_entry_t));
    if (!j->entries) { free(j); return NULL; }
    j->cap = capacity;
    return j;
}

void ta2a_journal_destroy(ta2a_journal_t *j)
{
    if (!j) return;
    free(j->entries);
    free(j);
}

bool ta2a_journal_seen(ta2a_journal_t *j, const char *key)
{
    if (!j || !key || !*key) return false;
    for (size_t i = 0; i < j->cap; i++) {
        if (j->entries[i].used && strcmp(j->entries[i].key, key) == 0) return true;
    }
    return false;
}

ta2a_result_t ta2a_journal_record(ta2a_journal_t *j, const char *key, uint64_t ts_ms)
{
    if (!j || !key || !*key) return TA2A_ERR_INVALID_ARG;
    /* Already there? refresh ts */
    for (size_t i = 0; i < j->cap; i++) {
        if (j->entries[i].used && strcmp(j->entries[i].key, key) == 0) {
            j->entries[i].ts_ms = ts_ms;
            return TA2A_OK;
        }
    }
    /* Insert at j->next, evict if needed */
    journal_entry_t *e = &j->entries[j->next];
    if (!e->used) j->count++;
    strncpy(e->key, key, TA2A_ID_LEN - 1);
    e->key[TA2A_ID_LEN - 1] = '\0';
    e->ts_ms = ts_ms;
    e->used  = true;
    j->next  = (j->next + 1) % j->cap;
    return TA2A_OK;
}

size_t ta2a_journal_size(const ta2a_journal_t *j)
{
    return j ? j->count : 0;
}

size_t ta2a_journal_capacity(const ta2a_journal_t *j)
{
    return j ? j->cap : 0;
}
