#include "codec.h"

// ==================== BASE 64 ========================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(String_Builder *sb, const unsigned char *data, size_t len)
{
    size_t i = 0;

    for (; i + 2 < len; i += 3) {
        unsigned n = (unsigned int)(data[i] << 16) | (unsigned int)(data[i+1] << 8) | data[i+2];

        sb_append_char(sb, b64_table[(n >> 18) & 63]);
        sb_append_char(sb, b64_table[(n >> 12) & 63]);
        sb_append_char(sb, b64_table[(n >> 6)  & 63]);
        sb_append_char(sb, b64_table[n & 63]);
    }

    if (i < len) {
        unsigned n = (unsigned int)data[i] << 16;

        sb_append_char(sb, b64_table[(n >> 18) & 63]);

        if (i + 1 < len) {
            n |= (unsigned int)data[i+1] << 8;
            sb_append_char(sb, b64_table[(n >> 12) & 63]);
            sb_append_char(sb, b64_table[(n >> 6) & 63]);
            sb_append_char(sb, '=');
        } else {
            sb_append_char(sb, b64_table[(n >> 12) & 63]);
            sb_append_char(sb, '=');
            sb_append_char(sb, '=');
        }
    }
}

static int b64_value(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool b64_decode(String_Builder *sb, String_View input)
{
    int val = 0;
    int valb = -8;

    size_t start_count = sb->count;

    for (size_t i = 0; i < input.count; i++) {
        unsigned char c = (unsigned char)input.data[i];

        if (c == '=') break;

        int d = b64_value(c);
        if (d < 0) continue;

        val = (val << 6) | d;
        valb += 6;

        if (valb >= 0) {
            sb_append_char(sb, (char)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return sb->count > start_count;
}
