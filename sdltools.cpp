/* sdltools.cpp - DMI - 6-Oct-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

typedef const char *ConstCharPtr;
typedef Uint32 *Uint32Ptr;

extern Config *config;

Uint32 surface_flags = SDL_HWSURFACE;

// Prototypes:
TTF_Font *load_font(const char *font_path, int size);
SDL_Surface *search_png(const char *dir, const char *filename, int alpha);
SDL_Surface *do_load_png(const char *filename, int alpha);
int hex_to_byte(const char *hex);

SDL_Surface *load_local_png(const char *filename, int alpha)
{
	// Suitable for loading images in current directory
	return do_load_png(filename, alpha);
}

SDL_Surface *load_png(const char *filename, int alpha)
{
	SDL_Surface *img;
	char *t;

	if(filename[0] == '/')
		return do_load_png(filename, alpha);
	t = expand_tilde(filename);
	if(t != NULL)
	{
		img = do_load_png(t, alpha);
		delete[] t;
		return img;
	}
	
	img = search_png(config->project_dir, filename, alpha);	
	if(img != NULL) return img;
	img = search_png(config->home_image_dir, filename, alpha);	
	if(img != NULL) return img;
	img = search_png(config->env_image_dir, filename, alpha);	
	if(img != NULL) return img;
	img = search_png(config->sys_image_dir, filename, alpha);	
	if(img != NULL) return img;
	
	error("Cannot locate image <%s>", filename);
	return NULL;
}

SDL_Surface *search_png(const char *dir, const char *filename, int alpha)
{
	char *image_file;
	SDL_Surface *img;

	if(dir == NULL) // Possible if environment variable not set
		return NULL;	
	image_file = combine_path(dir, filename);
	if(fexists(image_file))
	{
		img = do_load_png(image_file, alpha);
		delete[] image_file;
		return img;
	}
	delete[] image_file;
	return NULL;
}

SDL_Surface *do_load_png(const char *filename, int alpha)
{
	/* Load an image and convert it to the display's native format,
		so that SDL_BlitSurface will perform better later: */

	SDL_Surface *temp;
	SDL_Surface *final;
		
	temp = IMG_Load(filename);
	if(temp == NULL)
		error("Unable to load image from %s.", filename);
	
	if(alpha)
		final = SDL_DisplayFormatAlpha(temp);
	else
		final = SDL_DisplayFormat(temp);
	if(final == NULL)
		error("Unable to convert image to display format.");
	SDL_FreeSurface(temp);
	
	return final;
}

int highest_resolution()
{
	SDL_Rect **modes;
	int max_width = 0;

	// FIXME - Add depth 16 to pixel format
	modes = SDL_ListModes(NULL, SDL_DOUBLEBUF | SDL_FULLSCREEN | SDL_HWSURFACE);

	if(modes == NULL)
		error("No video modes available.");

	if(modes == (SDL_Rect **)-1)
		return -1; // Any resolution permitted

	for(int i = 0; modes[i] != NULL; i++)
	{
		if(modes[i]->w > max_width)
			max_width = modes[i]->w;
		// printf("  %d x %d\n", modes[i]->w, modes[i]->h);
	}
	return max_width;
}

void init_sdl(const char *caption, int offscreen)
{
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
		error("Unable to initialize SDL: %s\n", SDL_GetError());

	/* Make sure SDL_Quit gets called when the program exits! */
	atexit(SDL_Quit);

	SDL_WM_SetCaption(caption, caption);

	if(fullscreen == -1)
	{
		// Autodetect:
		if(highest_resolution() == SCREEN_WIDTH)
			fullscreen = 1;
		else
			fullscreen = 0;
	}
	
	if(offscreen)
	{
		screen = SDL_SetVideoMode(1, 1,
			16, SDL_DOUBLEBUF | surface_flags);
	}
	else if(fullscreen)
	{
		screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT,
			16, SDL_DOUBLEBUF | surface_flags | SDL_FULLSCREEN);
	}
	else
	{
		screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT,
			16, SDL_DOUBLEBUF | surface_flags);
	}
	
	if(screen == NULL)
		error("Unable to set video mode: %s\n", SDL_GetError());

	if(offscreen)
		SDL_WM_IconifyWindow();
	
	/*
	printf("Repeat delay = %d\n", SDL_DEFAULT_REPEAT_DELAY);
	printf("Repeat interval = %d\n", SDL_DEFAULT_REPEAT_INTERVAL);
	*/
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			(SDL_DEFAULT_REPEAT_INTERVAL * 7) / 10);
	// SDL_EnableUNICODE(1);
	
	// Initialise SDL_ttf:
	if(TTF_Init() < 0)
		error("Couldn't initialize Truetype font library: %s\n", SDL_GetError());
	atexit(TTF_Quit);
}

TTF_Font *try_font(const char *dir, const char *file, int size)
{
	char *font_path;
	struct stat buf;
	TTF_Font *font;

	if(dir == NULL) // Likely if env_dir not set
		return NULL;	
	font_path = combine_path(dir, file);
	if(stat(font_path, &buf) == 0)
		font = load_font(font_path, size);
	else
		font = NULL;
	delete[] font_path;
	return font;
}

TTF_Font *init_font(Config *config, const char *font_file, int size)
{
	TTF_Font *font;

	font = try_font(config->proj_font_dir, font_file, size);
	if(font != NULL)
		return font;
	font = try_font(config->home_font_dir, font_file, size);
	if(font != NULL)
		return font;
	font = try_font(config->env_font_dir, font_file, size);
	if(font != NULL)
		return font;
	font = try_font(config->sys_font_dir, font_file, size);
	if(font != NULL)
		return font;
	error("Can't locate font <%s>", font_file);
	return NULL;
}

/*		
TTF_Font *init_font(const char *resource_path, const char *font_file, int size)
{
	char *font_path;
	struct stat buf;
	TTF_Font *font;
	
	font_path = new char[strlen(resource_path) + strlen(font_file) + 20];
	sprintf(font_path, "%s/fonts/%s", resource_path, font_file);

	// Poorly written library: TTF_OpenFont crashes if "font_path" doesn't exist:
	if(stat(font_path, &buf) != 0)
		error("Couldn't load font from %s\n", font_path);
	
	font = load_font(font_path, size);	
	delete[] font_path;
	return font;
}
*/

TTF_Font *load_font(const char *font_path, int size)
{
	TTF_Font *font;
	
	font = TTF_OpenFont(font_path, size); // Point size
	if(font == NULL)
		error("Couldn't load font from %s: %s\n", font_path, SDL_GetError());
	TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
	
	return font;
}

void init_colours()
{
	colour = new colour_definition();
}

colour_definition::colour_definition()
{
	black_text.r = 0x00;
	black_text.g = 0x00;
	black_text.b = 0x00;
	red_text.r = 0xFF;
	red_text.g = 0x00;
	red_text.b = 0x00;
	purple_text.r = 0x88;
	purple_text.g = 0x00;
	purple_text.b = 0xAA;
	
	white_fill = SDL_MapRGB(screen->format, 0xFF, 0xFF, 0xFF);
	light_grey_fill = SDL_MapRGB(screen->format, 0xC4, 0xC4, 0xC4);
	grey_fill = SDL_MapRGB(screen->format, 0x88, 0x88, 0x88);
	dark_grey_fill = SDL_MapRGB(screen->format, 0x44, 0x44, 0x44);
	black_fill = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	light_green_fill = SDL_MapRGB(screen->format, 0x66, 0xFF, 0x66);
	cyan_fill = SDL_MapRGB(screen->format, 0x00, 0xFF, 0xFF);
	yellow_fill = SDL_MapRGB(screen->format, 0xFF, 0xFF, 0x00);
	red_fill = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	orange_fill = SDL_MapRGB(screen->format, 0xFF, 0x88, 0x00);
	purple_fill = SDL_MapRGB(screen->format, 0x88, 0x00, 0xAA);
	
	white_pen = (0xFF << 24) + (0xFF << 16) + (0xFF << 8) + 255;
	black_pen = (0x00 << 24) + (0x00 << 16) + (0x00 << 8) + 255;
	red_pen = (0xFF << 24) + (0x6d << 16) + (0x66 << 8) + 255;
	orange_pen = (0xFF << 24) + (0xa0 << 16) + (0x00 << 8) + 255;
	yellow_pen = (0xFF << 24) + (0xFF << 16) + (0x00 << 8) + 255;
	green_pen = (0x28 << 24) + (0x94 << 16) + (0x07 << 8) + 255;
	light_green_pen = (0xb8 << 24) + (0xF6 << 16) + (0x36 << 8) + 255;
	sky_pen = (0x5c << 24) + (0xFF << 16) + (0xF4 << 8) + 255;
	cyan_pen = (0x00 << 24) + (0xFF << 16) + (0xFF << 8) + 255;
	blue_pen = (0x07 << 24) + (0x28 << 16) + (0x94 << 8) + 255;
	grey_pen = (0x88 << 24) + (0x88 << 16) + (0x88 << 8) + 255;
	dark_grey_pen = (0x44 << 24) + (0x44 << 16) + (0x44 << 8) + 255;
	light_grey_pen = (0xC4 << 24) + (0xC4 << 16) + (0xC4 << 8) + 255;
	brown_pen = (0x88 << 24) + (0x00 << 16) + (0x00 << 8) + 255;
	purple_pen = (0x8a << 24) + (0x46 << 16) + (0xc8 << 8) + 255;
	pink_pen = (0xFF << 24) + (0x99 << 16) + (0x99 << 8) + 255;
	magenta_pen = (0xFF << 24) + (0x00 << 16) + (0xFF << 8) + 255;

	names = new svector();
	pens = new Uint32vector();
	
	names->add("white");			pens->add(white_pen);
	names->add("black");			pens->add(black_pen);
	names->add("red");			pens->add(red_pen);
	names->add("orange");		pens->add(orange_pen);
	names->add("yellow");		pens->add(yellow_pen);
	names->add("green");			pens->add(green_pen);
	names->add("lightgreen");	pens->add(light_green_pen);
	names->add("sky");			pens->add(sky_pen);
	names->add("cyan");			pens->add(cyan_pen);
	names->add("blue");			pens->add(blue_pen);
	names->add("grey");			pens->add(grey_pen);
	names->add("darkgrey");		pens->add(dark_grey_pen);
	names->add("lightgrey");	pens->add(light_grey_pen);
	names->add("brown");			pens->add(brown_pen);
	names->add("purple");		pens->add(purple_pen);
	names->add("pink");			pens->add(pink_pen);
	names->add("magenta");		pens->add(magenta_pen);

	SDL_Color *sdlcol;	
	inks = new SDL_Color_pvector();
	fills = new Uint32vector();
	for(int i = 0; i < pens->count(); i++)
	{
		sdlcol = new SDL_Color;
		sdlcol->r = pens->item(i) >> 24;
		sdlcol->g = (pens->item(i) >> 16) & 0xFF;
		sdlcol->b = (pens->item(i) >> 8) & 0xFF;
		inks->add(sdlcol);
		fills->add(SDL_MapRGB(screen->format, sdlcol->r, sdlcol->g, sdlcol->b));
	}
}

int colour_definition::search_add(const char *hex)
{
	// Note: '#' character already stripped
	int r, g, b;
	
	if(strlen(hex) < 6)
		return -1;
	r = hex_to_byte(hex);
	g = hex_to_byte(hex + 2);
	b = hex_to_byte(hex + 4);
	if(r == -1 || g == -1 || b == -1)
		return -1;
	
	int num_colours = pens->count();
	int colour_index = -1;
	SDL_Color *sdlcol;
	for(int i = 0; i < num_colours; i++)
	{
		sdlcol = inks->item(i);
		if(sdlcol->r == r && sdlcol->g == g && sdlcol->b == b)
		{
			colour_index = i;
			break;
		}
	}
	if(colour_index == -1)
	{
		colour_index = num_colours;
		sdlcol = new SDL_Color;
		sdlcol->r = r; sdlcol->g = g; sdlcol->b = b;
		names->add(hex);
		pens->add((r << 24) + (g << 16) + (b << 8) + 255);
		inks->add(sdlcol);
		fills->add(SDL_MapRGB(screen->format, r, g, b));
	}
	return colour_index;
}

int hex_to_byte(const char *hex)
{
	int n = 0;
	
	for(int i = 0; i < 2; i++)
	{
		n *= 16;
		if(*hex >= '0' && *hex <= '9')
			n += *hex - '0';
		else if(*hex >= 'a' && *hex <= 'f')
			n += *hex - 'a' + 10;
		else if(*hex >= 'A' && *hex <= 'F')
			n += *hex - 'A' + 10;
		else
			return -1;
		hex++;
	}
	return n;
}

int render_text(const char *s, TTF_Font *font, int colour_index,
		SDL_Surface *surface, int x, int y, int underlined)
{
	SDL_Rect dst;
	SDL_Surface *text;
	int new_pos;
	SDL_Color *col = colour->inks->item(colour_index);
	Uint32 pen = colour->pens->item(colour_index);

	text = TTF_RenderUTF8_Blended(font, s, *col);
	if(text == NULL)
		error("Couldn't render text: %s\n", SDL_GetError());
	dst.x = x;
	dst.y = y;
	// dst.w = text->w;
	// dst.h = text->h;
	int ret = SDL_BlitSurface(text, NULL, surface, &dst);
	if(ret != 0)
		error("SDL_BlitSurface return %d in render_text (%d, %d)\n", ret, x, y);
	new_pos = text->w + x;
	SDL_FreeSurface(text);
	// If non-zero, request a line drawn "underlined" pixels below:
	if(underlined > 0)
	{
		hlineColor(surface, x, new_pos, y + underlined, pen);
		hlineColor(surface, x, new_pos, y + underlined + 1, pen);
	}
	return new_pos;
}

int render_text(const char *s, TTF_Font *font, SDL_Color *col,
		SDL_Surface *surface, int x, int y)
{
	SDL_Rect dst;
	SDL_Surface *text;
	int new_pos;

	text = TTF_RenderUTF8_Blended(font, s, *col);
	if(text == NULL)
		error("Couldn't render text: %s\n", SDL_GetError());
	dst.x = x;
	dst.y = y;
	// dst.w = text->w;
	// dst.h = text->h;
	int ret = SDL_BlitSurface(text, NULL, surface, &dst);
	if(ret != 0)
		error("SDL_BlitSurface return %d in render_text (%d, %d)\n", ret, x, y);
	new_pos = text->w + x;
	SDL_FreeSurface(text);
	return new_pos;
}

SDL_Surface *alloc_surface(int w, int h)
{
	SDL_Surface *temp_surface, *final_surface;
	Uint32 rmask, gmask, bmask, amask;
	
	rmask = 0x000000FF;
	gmask = 0x0000FF00;
	bmask = 0x00FF0000;
	amask = 0xFF000000;
	temp_surface = SDL_CreateRGBSurface(surface_flags, w, h, 24,
			rmask, gmask, bmask, amask);
	if(temp_surface == NULL)
		error("SDL_CreateRGBSurface failed on (%d, %d)\n", w, h);
	
	final_surface = SDL_DisplayFormat(temp_surface);
	if(final_surface == NULL)
		error("SDL_DisplayFormat failed on (%d, %d)\n", w, h);
	SDL_FreeSurface(temp_surface);
	
	return final_surface;
}

void clear_surface(SDL_Surface *surface, Uint32 co)
{
	SDL_Rect dst;

	dst.x = dst.y = 0;
	dst.w = surface->w;
	dst.h = surface->h;
	SDL_FillRect(surface, &dst, co);	
}

