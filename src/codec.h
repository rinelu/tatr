#ifndef CODEC_H_
#define CODEC_H_

#include "astring.h"
#include <stddef.h>
#include <stdio.h>

void b64_encode(String_Builder *sb, const unsigned char *data, size_t len);
bool b64_decode(String_Builder *sb, String_View input);

#endif // CODEC_H_
