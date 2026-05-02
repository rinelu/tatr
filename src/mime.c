#include "mime.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

typedef struct {
    const char *mime;
    size_t offset;
    const unsigned char *magic;
    size_t len;
} Mime_Sig;

static const unsigned char SIG_PNG[]    = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
static const unsigned char SIG_JPEG[]   = {0xFF,0xD8,0xFF};
static const unsigned char SIG_GIF87[]  = {'G','I','F','8','7','a'};
static const unsigned char SIG_GIF89[]  = {'G','I','F','8','9','a'};
static const unsigned char SIG_BMP[]    = {'B','M'};
static const unsigned char SIG_ICO[]    = {0x00,0x00,0x01,0x00};
static const unsigned char SIG_TIFF_LE[]= {0x49,0x49,0x2A,0x00};
static const unsigned char SIG_TIFF_BE[]= {0x4D,0x4D,0x00,0x2A};
static const unsigned char SIG_PDF[]    = {'%','P','D','F'};
static const unsigned char SIG_ZIP[]    = {'P','K',0x03,0x04};
static const unsigned char SIG_ZIP_E[]  = {'P','K',0x05,0x06};    // empty zip
static const unsigned char SIG_GZIP[]   = {0x1F,0x8B};
static const unsigned char SIG_ZSTD[]   = {0x28,0xB5,0x2F,0xFD};
static const unsigned char SIG_TAR[]    = {'u','s','t','a','r'};  // offset 257
static const unsigned char SIG_7Z[]     = {'7','z',0xBC,0xAF,0x27,0x1C};
static const unsigned char SIG_RAR[]    = {'R','a','r','!',0x1A,0x07};
static const unsigned char SIG_MP4_A[]  = {'f','t','y','p'};      // offset 4
static const unsigned char SIG_OGG[]    = {'O','g','g','S'};
static const unsigned char SIG_FLAC[]   = {'f','L','a','C'};
static const unsigned char SIG_MP3_ID[] = {'I','D','3'};
static const unsigned char SIG_MP3_FF[] = {0xFF,0xFB};
static const unsigned char SIG_WAV[]    = {'R','I','F','F'};      // + WAVE at 8
static const unsigned char SIG_ELF[]    = {0x7F,'E','L','F'};
static const unsigned char SIG_MACHO[]  = {0xCF,0xFA,0xED,0xFE};
static const unsigned char SIG_CLASS[]  = {0xCA,0xFE,0xBA,0xBE};
static const unsigned char SIG_WASM[]   = {0x00,'a','s','m'};
static const unsigned char SIG_SVG[]    = {'<','s','v','g'};
static const unsigned char SIG_SVG2[]   = {'<','S','V','G'};

static const Mime_Sig MIME_TABLE[] = {
    // Images
    { "image/png",              0,   SIG_PNG,     sizeof(SIG_PNG)     },
    { "image/jpeg",             0,   SIG_JPEG,    sizeof(SIG_JPEG)    },
    { "image/gif",              0,   SIG_GIF87,   sizeof(SIG_GIF87)   },
    { "image/gif",              0,   SIG_GIF89,   sizeof(SIG_GIF89)   },
    { "image/bmp",              0,   SIG_BMP,     sizeof(SIG_BMP)     },
    { "image/x-icon",           0,   SIG_ICO,     sizeof(SIG_ICO)     },
    { "image/tiff",             0,   SIG_TIFF_LE, sizeof(SIG_TIFF_LE) },
    { "image/tiff",             0,   SIG_TIFF_BE, sizeof(SIG_TIFF_BE) },
    { "image/svg+xml",          0,   SIG_SVG,     sizeof(SIG_SVG)     },
    { "image/svg+xml",          0,   SIG_SVG2,    sizeof(SIG_SVG2)    },

    // Documents
    { "application/pdf",        0,   SIG_PDF,     sizeof(SIG_PDF)     },

    // Archives
    { "application/zip",        0,   SIG_ZIP,     sizeof(SIG_ZIP)     },
    { "application/zip",        0,   SIG_ZIP_E,   sizeof(SIG_ZIP_E)   },
    { "application/gzip",       0,   SIG_GZIP,    sizeof(SIG_GZIP)    },
    { "application/zstd",       0,   SIG_ZSTD,    sizeof(SIG_ZSTD)    },
    { "application/x-tar",      257, SIG_TAR,     sizeof(SIG_TAR)     },
    { "application/x-7z-compressed",  0, SIG_7Z,  sizeof(SIG_7Z)      },
    { "application/x-rar-compressed", 0, SIG_RAR, sizeof(SIG_RAR)     },

    // Audio / Video (offset-based)
    { "video/mp4",              4,   SIG_MP4_A,   sizeof(SIG_MP4_A)   },
    { "audio/ogg",              0,   SIG_OGG,     sizeof(SIG_OGG)     },
    { "audio/flac",             0,   SIG_FLAC,    sizeof(SIG_FLAC)    },
    { "audio/mpeg",             0,   SIG_MP3_ID,  sizeof(SIG_MP3_ID)  },
    { "audio/mpeg",             0,   SIG_MP3_FF,  sizeof(SIG_MP3_FF)  },

    // Executables / bytecode
    { "application/x-elf",      0,   SIG_ELF,     sizeof(SIG_ELF)     },
    { "application/x-mach-binary", 0, SIG_MACHO,  sizeof(SIG_MACHO)   },
    { "application/java-vm",    0,   SIG_CLASS,   sizeof(SIG_CLASS)   },
    { "application/wasm",       0,   SIG_WASM,    sizeof(SIG_WASM)    },

    { NULL, 0, NULL, 0 }
};

typedef struct {
    const char *ext;
    const char *mime;
} Mime_Ext;

static const Mime_Ext EXT_TABLE[] = {
    { "md",    "text/markdown"      },
    { "txt",   "text/plain"         },
    { "csv",   "text/csv"           },
    { "html",  "text/html"          },
    { "htm",   "text/html"          },
    { "css",   "text/css"           },
    { "js",    "text/javascript"    },
    { "ts",    "text/typescript"    },
    { "json",  "application/json"   },
    { "xml",   "application/xml"    },
    { "yaml",  "application/yaml"   },
    { "yml",   "application/yaml"   },
    { "toml",  "application/toml"   },
    { "sh",    "application/x-sh"   },
    { "py",    "text/x-python"      },
    { "c",     "text/x-csrc"        },
    { "h",     "text/x-chdr"        },
    { "cpp",   "text/x-c++src"      },
    { "rs",    "text/x-rustsrc"     },
    { "go",    "text/x-go"          },
    { "java",  "text/x-java-source" },
    { "rb",    "text/x-ruby"        },
    { "log",   "text/plain"         },
    { "svg",   "image/svg+xml"      },
    { "mp4",   "video/mp4"          },
    { "mov",   "video/quicktime"    },
    { "mkv",   "video/x-matroska"   },
    { "wav",   "audio/wav"          },
    { "mp3",   "audio/mpeg"         },
    { "ogg",   "audio/ogg"          },
    { "flac",  "audio/flac"         },
    { "ttf",   "font/ttf"           },
    { "otf",   "font/otf"           },
    { "woff",  "font/woff"          },
    { "woff2", "font/woff2"         },
    { NULL, NULL }
};

static bool sig_match(const unsigned char *buf, size_t len, size_t off,
                      const unsigned char *sig, size_t siglen)
{
    if (len < off + siglen) return false;
    return memcmp(buf + off, sig, siglen) == 0;
}

static bool is_text_buf(const unsigned char *buf, size_t len)
{
    // NUL byte -> definitely binary.
    // >90% printable (including common control chars) -> treat as text.
    size_t printable = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c == 0x00) return false;
        if (c >= 0x20 || c == '\t' || c == '\n' || c == '\r')
            printable++;
    }
    return len > 0 && printable > (len * 9 / 10);
}

static const char *ext_from_path(const char *path)
{
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return NULL;
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (bslash > slash) slash = bslash;
#endif
    if (slash && dot < slash) return NULL;
    return dot + 1;
}

static const char *mime_from_ext(const char *ext)
{
    if (!ext) return NULL;
    for (size_t i = 0; EXT_TABLE[i].ext; i++)
        if (strcasecmp(EXT_TABLE[i].ext, ext) == 0)
            return EXT_TABLE[i].mime;
    return NULL;
}

const char *mime_from_buf(const unsigned char *buf, size_t len)
{
    if (!buf || len == 0)
        return "application/octet-stream";

    // WEBP: needs two checks at different offsets - handle before table
    if (sig_match(buf, len, 0, (const unsigned char *)"RIFF", 4) &&
        sig_match(buf, len, 8, (const unsigned char *)"WEBP", 4))
        return "image/webp";

    // WAV: RIFF + WAVE at offset 8
    if (sig_match(buf, len, 0, SIG_WAV, sizeof(SIG_WAV)) &&
        sig_match(buf, len, 8, (const unsigned char *)"WAVE", 4))
        return "audio/wav";

    // Table scan
    for (size_t i = 0; MIME_TABLE[i].mime; i++)
        if (sig_match(buf, len, MIME_TABLE[i].offset, MIME_TABLE[i].magic, MIME_TABLE[i].len))
            return MIME_TABLE[i].mime;

    if (is_text_buf(buf, len))
        return "text/plain";

    return "application/octet-stream";
}

const char *mime_from_file(const char *path)
{
    unsigned char buf[512];

    FILE *f = fopen(path, "rb");
    if (!f) return "application/octet-stream";

    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    const char *m = mime_from_buf(buf, n);

    // If magic only got us "text/plain"
    // Try to refine via extension
    if (strcmp(m, "text/plain") == 0 || strcmp(m, "application/octet-stream") == 0) {
        const char *ext_mime = mime_from_ext(ext_from_path(path));
        if (ext_mime) return ext_mime;
    }

    return m;
}

bool mime_is_text(const char *mime)
{
    if (!mime) return false;
    return strncmp(mime, "text/", 5) == 0
        || strcmp(mime, "application/json")  == 0
        || strcmp(mime, "application/xml")   == 0
        || strcmp(mime, "application/yaml")  == 0
        || strcmp(mime, "application/toml")  == 0
        || strcmp(mime, "image/svg+xml")     == 0;
}

bool mime_is_image(const char *mime)
{
    if (!mime) return false;
    return strncmp(mime, "image/", 6) == 0;
}
