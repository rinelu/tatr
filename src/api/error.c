#include "error.h"

const char *tatr_strerror(Tatr_Error err)
{
    switch (err) {
        case TATR_OK:              return "ok";
        case TATR_ERR_NOT_FOUND:   return "not found";
        case TATR_ERR_INVALID_ARG: return "invalid argument";
        case TATR_ERR_IO:          return "I/O error";
        case TATR_ERR_CONFLICT:    return "conflict";
        case TATR_ERR_STORAGE:     return "storage error";
        case TATR_ERR_UNKNOWN:     return "unknown error";
    }
    return "unknown error";
}
