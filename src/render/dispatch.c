#include "render.h"

const Renderer *renderer_for(Tatr_Format fmt)
{
    switch (fmt) {
        case TATR_FMT_JSON: return &RENDER_JSON;
        case TATR_FMT_HUMAN:
        default:            return &RENDER_HUMAN;
    }
}
