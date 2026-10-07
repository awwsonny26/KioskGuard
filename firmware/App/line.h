#ifndef KG_LINE_H
#define KG_LINE_H
#include <stdbool.h>
typedef struct { char text[64]; unsigned length; bool rejected; } kg_line;
typedef enum { KG_LINE_WAIT, KG_LINE_READY, KG_LINE_REJECTED } kg_line_result;
kg_line_result kg_line_feed(kg_line *line, unsigned char byte);
typedef struct { bool observed, stable; unsigned long changed; } kg_button;
bool kg_button_press(kg_button *button, bool level, unsigned long time);
#endif
