#ifndef CONFIG_H_
#define CONFIG_H_

#include "astring.h"
#include <stdbool.h>
#include <stddef.h>
#include "global.h"

// ~/.config/tatr/config  (resolved at runtime via config_global_path())
const char *config_global_path(void);

typedef struct {
    const char *key;
    const char *desc;
    const char *default_val; // NULL = no default
} Config_Key_Def;

extern const Config_Key_Def CONFIG_KEYS[];
extern const size_t         CONFIG_KEYS_COUNT;

// Returns the definition for `key`, or NULL if unknown.
const Config_Key_Def *config_key_def(const char *key);

typedef struct {
    String_View key;
    String_View val;
} Config_Entry;

typedef struct {
    struct {
        Config_Entry *items;
        size_t count;
        size_t capacity;
    } entries;

    char  *_buf; // owns string data
    char  *path; // heap-allocated path string
} Config_Store;

typedef struct {
    Config_Store local;
    Config_Store global;
} Config;

// Load both scopes.
// Missing files are silently skipped (not an error).
void config_load(Config *c);

// Get the resolved value for `key` (local wins over global).
// Returns NULL if not set in either scope.
const char *config_get(const Config *c, const char *key);

// Get with fallback to default_val if key is unset.
// Never returns NULL.
const char *config_get_or_default(const Config *c, const char *key);

const char *store_get_source(const Config *c, const char *key);

// Set a key in the given scope file.
// Returns false on I/O error or unknown key.
bool config_set(const char *path, const char *key, const char *val);

// Remove a key from the given scope file.
// Returns false if the key was not present or on I/O error.
bool config_unset(const char *path, const char *key);

// Free all heap memory owned by a Config.
void config_free(Config *c);

bool config_write_global_default(void);

#endif // CONFIG_H_
