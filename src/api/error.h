#ifndef TATR_API_ERROR_H_
#define TATR_API_ERROR_H_

typedef enum {
    TATR_OK = 0,
    TATR_ERR_NOT_FOUND,    // e.g. issue id / config key does not exist
    TATR_ERR_INVALID_ARG,  // malformed input (bad id, bad enum value...)
    TATR_ERR_IO,           // filesystem / storage read-write failure
    TATR_ERR_CONFLICT,     // duplicate id, invalid state transition
    TATR_ERR_STORAGE,      // storage backend is broken/unavailable
    TATR_ERR_UNKNOWN
} Tatr_Error;

const char *tatr_strerror(Tatr_Error err);

#endif // TATR_API_ERROR_H_
