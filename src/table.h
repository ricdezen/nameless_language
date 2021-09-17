#ifndef NAMELESS_TABLE_H
#define NAMELESS_TABLE_H

#include "common.h"
#include "value.h"

/**
 * Key-value pair for a table.
 */
typedef struct {
    ObjString *key;
    Value value;
} Entry;

/**
 * Structure representing a hashtable. The implementation is the following:
 * - Items are inserted in the bucket having the index equal to the modulo between the hash of the item's key and the
 * table's capacity.
 * - If the bucket is already busy, find the next available one with linear probing.
 * - Lookup is the same, when you find an entry with NULL key and NIL value, you can be sure the key is not in the
 * table.
 * - Removing a key replaces its bucket with a tombstone, which is a {NULL, false} pair. Tombstones are skipped over
 * during lookup and overwritten during a write.
 * - When the total number of entries + tombstones is too high, the table is reallocated.
 *
 * It is vital for the hashtable to have capacity that is always a power of two. This way the modulo is the same as a
 * binary and, improving lookup speed by much.
 */
typedef struct {
    int size;
    int capacity;
    Entry *entries;
} Table;

/**
 * Initialize a table.
 *
 * @param table Pointer to the table to initialize.
 */
void initTable(Table *table);

/**
 * Free a table.
 *
 * @param table The table to free.
 */
void freeTable(Table *table);

/**
 * Get a value from the table.
 *
 * @param table The table.
 * @param key The key of the entry to get.
 * @param value Output parameter, will be the value if found.
 * @return true if the value was found, in which case `value` is going to be it, false otherwise (`value` will be left
 * unchanged).
 */
bool tableGet(Table *table, ObjString *key, Value *value);

/**
 * Set an entry in the table.
 *
 * @param table The target table.
 * @param key The key for the entry.
 * @param value The value for the entry.
 * @return true if the key was new, false if some old value was replaced.
 */
bool tableSet(Table *table, ObjString *key, Value value);

/**
 * Remove an entry from a table, replacing it with a tombstone.
 *
 * @param table The table.
 * @param key The key.
 * @return true if the key was found and removed, false if the key was not in the table (nothing was done).
 */
bool tableDelete(Table *table, ObjString *key);

/**
 * Copy content from one table to another.
 *
 * @param from Source table.
 * @param to Destination table.
 */
void tableAddAll(Table *from, Table *to);

/**
 * Find a String in the table's keys. Only needed for lookup during string interning. This implies the table uses
 * strings as keys. WILL cast objects to strings under such assumption. You have been warned.
 *
 * @param table The target table.
 * @param chars The raw c string to find.
 * @param length The length of the string.
 * @param hash The hash of the string.
 * @return The Object corresponding to the string or NULL if it is a new string.
 */
ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash);

/**
 * Mark the objects in the Table as reachable.
 *
 * @param table A Table.
 */
void markTable(Table *table);

#endif