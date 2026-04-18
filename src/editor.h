#ifndef EDITOR_H_
#define EDITOR_H_

#include <stddef.h>
#include <stdbool.h>
#include "astring.h"

bool editor_edit(const char *content, size_t content_len, const char *suffix, String_Builder *out_sb);

#endif // EDITOR_H_
