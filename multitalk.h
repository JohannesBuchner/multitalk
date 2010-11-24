/* multitalk.h - DMI - 29-Sept-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include "style.h"
#include "sdltools.h"

// From parse.cpp:
slidevector *parse_talk(linefile *talk_lf, stylevector *style_list);
void flatten_all(slidevector *talk);
void flatten(slide *sl, slidevector *talk);
void free_node(node *context);
slide *find_title(slidevector *talk, const char *title, int *card);
void peek_designsize(linefile *talk_lf);
void fold_all(slide *sl);
void unfold_all(slide *sl);
int is_foldable(slide *sl);

// From graph.cpp:
void save_slide_positions(Config *config, slidevector *talk);
void load_slide_positions(Config *config, slidevector *talk);

// From style.cpp for config.cpp:
void set_integer_property(dictionary *d, const char *name, int *dest);
void set_string_property(dictionary *d, const char *name, constCharPtr *dest);

// From multitalk.cpp
SDL_Surface *alloc_surface(int w, int h);
int to_screen_coords(int x);
int to_design_coords(int x);

// From multitalk.cpp for web.cpp
extern SDL_Surface *radar;
extern int RADAR_WIDTH, RADAR_HEIGHT, RADAR_MAG;
void clear_radar();
void render_radar(int cx, int cy, hotspotvector *hsv = NULL,
		int slide_num = -1);
void goto_card(slide *sl, int n);
void redraft(slide *sl);

#include "files.h"

// From config.cpp
Options *init_options(Config *config);

// From render.cpp
void render_slide(slide *sl);
void measure_slide(slide *sl);
void render_line(slide *sl, displayline *out, DrawMode *dm,
		SDL_Surface *surface);
void copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void mini_copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void micro_copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void pointer(int x, int y);
void reallocate_surfaces(slide *sl);
void scale(slide *sl);

// From latex.cpp
SDL_Surface *gen_latex(svector *tex, style *st);

// From web.cpp
void gen_html(slidevector *talk);
