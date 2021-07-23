#ifndef GRAPHICS_ROTATE_H_
#define GRAPHICS_ROTATE_H_

#include "minui/minui.h"

void rotate_canvas_exit(void);
void rotate_canvas_init(GRSurface *gr_draw);
void rotate_surface(GRSurface *dst, GRSurface *src);
GRSurface *rotate_canvas_get(GRSurface *gr_draw);

#endif
