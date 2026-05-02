#ifndef GLOBAL_H_
#define GLOBAL_H_

#define TATR_VERSION "2.0.0"
#define TATRLOG_PATH ".tatr/log"
#define TATRLOG_SEPARATOR "--- entry ---"
#define CONFIG_LOCAL_PATH  ".tatr/config"

#ifdef _WIN32
#define USERNAME_ENV getenv("USERNAME")
#else
#define USERNAME_ENV getenv("USER")
#endif

#endif // GLOBAL_H_
