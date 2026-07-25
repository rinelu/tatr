#ifndef MIME_H_
#define MIME_H_

#include <stddef.h>
#include <stdbool.h>

const char *mime_from_buf(const unsigned char *buf, size_t len);
const char *mime_from_file(const char *path);

bool mime_is_text(const char *mime);
bool mime_is_image(const char *mime);

#endif // MIME_H_
