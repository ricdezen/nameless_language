#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

/**
 * Load limit for an hashtable. Needs to be between 0 and 1.
 */
#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
    table->size = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

/**
 * Find the entry with the given key if it exists or find the first available bucket for an insertion.
 *
 * @param entries The entries of the table.
 * @param capacity The capacity of the table.
 * @param key The key of the entry to find.
 * @return The entry or the first empty one.
 */
static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
    // Hash the key.
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    // Linear probe until an entry is found.
    for (;;) {
        Entry *entry = &entries[index];
        // Either empty or tombstone.
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry -> no key ->
                // if a tombstone came first, return that,
                // otherwise return this empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone -> skip it.
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

/**
 * Increase a table's capacity and migrate its contents to the new array.
 *
 * @param table The table.
 * @param capacity The new capacity.
 */
static void adjustCapacity(Table *table, int capacity) {
    Entry *entries = ALLOCATE(Entry, capacity);

    // Ensure entries are empty.
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // Size might change since I don't transfer tombstones.
    table->size = 0;
    // Reallocate the entries that were in the table in the new array.
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        // Ignore empty buckets (or tombstones).
        if (entry->key == NULL)
            continue;

        Entry *dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->size++;
    }

    // Free old array and point to new one.
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableGet(Table *table, ObjString *key, Value *value) {
    // Empty table -> no entry.
    if (table->size == 0)
        return false;

    // Find the entry.
    Entry *entry = findEntry(table->entries, table->capacity, key);

    // Empty bucket -> no entry.
    if (entry->key == NULL)
        return false;

    // Got em -> Got em.
    *value = entry->value;
    return true;
}

bool tableSet(Table *table, ObjString *key, Value value) {
    // Resize the table if necessary.
    if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    // New key AND not a tombstone (cause tombstones count towards load).
    if (isNewKey && IS_NIL(entry->value))
        table->size++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table *table, ObjString *key) {
    // Empty table -> no entry.
    if (table->size == 0)
        return false;

    // Find the entry.
    Entry *entry = findEntry(table->entries, table->capacity, key);

    // Empty bucket -> no entry.
    if (entry->key == NULL)
        return false;

    // Delet dis and place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash) {
    // Similar to a normal lookup.
    if (table->size == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
        } else if (// First look at length.
                entry->key->length == length &&
                // Then hashes.
                entry->key->hash == hash &&
                // Only on hash conflict compare full string.
                memcmp(entry->key->chars, chars, length) == 0
                ) {
            // We found the string.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void markTable(Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        markObject((Obj *) entry->key);
        markValue(entry->value);
    }
}