#include "line.h"
#include <stdint.h>
kg_line_result kg_line_feed(kg_line *l, unsigned char byte)
{
    if (byte == '\r' || byte == '\n') {
        kg_line_result result = l->rejected ? KG_LINE_REJECTED :
                               l->length ? KG_LINE_READY : KG_LINE_WAIT;
        l->text[l->length] = 0;
        l->length = 0;
        l->rejected = false;
        return result;
    }
    if (byte < 32 || byte > 126 || l->length == sizeof(l->text)-1) {
        l->rejected = true;
        return KG_LINE_WAIT;
    }
    if (!l->rejected) l->text[l->length++] = (char)byte;
    return KG_LINE_WAIT;
}
bool kg_button_press(kg_button *b, bool level, unsigned long time)
{
    uint32_t now = (uint32_t)time;
    if (level != b->observed) {
        b->observed = level;
        b->changed = now;
    }
    if (level != b->stable && (uint32_t)(now-(uint32_t)b->changed) >= 35) {
        b->stable = level;
        return level;
    }
    return false;
}
