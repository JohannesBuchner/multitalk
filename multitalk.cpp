/* multitalk.cpp - DMI - 28-9-2006

Copyright (C) 2006-8 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

slidevector *talk;
slidevector *render_list;

stylevector *style_list;

SDL_Surface *screen;
TTF_Font *osd_font, *help_font;
int viewx, viewy, zoom_level;
int refreshreq = 0;
int extra_button[3];

int fullscreen;
int slides_moved = 0;
int memory_display = 0;
int help_display = 0;
int radar_window = 0;
int watch_file = 1;
int gravity = 1;
int grid_mode = 1;
int gyro = 0;
int hover_mode = 1;
int reverse_mouse = 0;
int force_latex = 0;
int export_html = 0;
int pointer_on = 0, pointer_x, pointer_y, hide_pointer = 0;
char *proc_stat_buf;
int canvas_colour;
int SCREEN_WIDTH = 1024;
int SCREEN_HEIGHT = 768;
int CARD_EDGE = 16;
int display_width = -1, display_height = -1;
int design_width = -1, design_height = -1;
int scalep = 1, scaleq = 1;

Config *config;
Options *options;

int debug = 0;

time_t last_modified = -1;

SDL_Surface *radar;
int RADAR_WIDTH = SCREEN_WIDTH / 6;
int RADAR_HEIGHT = SCREEN_HEIGHT / 6;
int RADAR_MAG = 30;

slide *magnify;
SDL_Surface *mag_surface;

slide *pinned;
SDL_Surface *pin[4];

const int PROC_STAT_BUF_LEN = 1000;
const int SLIDE_GRID_STEP = 40;
const int IMAGE_GRID_STEP = 20;
const int WATCH_INTERVAL = 500;
// const int CLICK_THRESHOLD = 7; // normal
const int CLICK_THRESHOLD = 50; // gyromouse

static const char *OSD_FONT_FILE = "luxi/luxisr.ttf";
static const int OSD_FONT_SIZE = 36;

const int cursor_direction_bias = 3;
const int max_cursor_distance = 3 * SCREEN_WIDTH;

colour_definition *colour;

const char *VERSION = "1.4";

// Prototypes:
void cls();
slide *central_slide();
slide *slide_under_mouse(int mousex, int mousey, int *line_num = NULL,
		subimage **image = NULL);
slide *slide_under_click(int clickx, int clicky, int *line_num,
		subimage **image = NULL);
slide *slide_under_pointer();
void recentre(int *prefx, int *prefy);
void refresh(int flip = 1);
int check_memory();
void snap_to(int prefx, int prefy);
void viewloop();

linefile *open_talk(char *path)
{
	linefile *lf;
	
	lf = new linefile();
	if(lf->load(path) < 0)
		error("Can't open file %s", path);
	
	return lf;
}

void usage()
{
	printf("Usage: multitalk [options...] <talk-name>\n\n");
	printf("Options: -fs        full-screen\n");
	printf("         -win       window\n");
	printf("         -export    HTML export\n");
	printf("         -nowatch   don't watch talk file for changes\n");
//	printf("         -gyro      gyromouse mode (incomplete)\n");
	printf("         -reverse   reverse mouse direction\n");
	printf("         -force     must regenerate latex segments\n");
	printf("         -version   display version number\n");
	printf("         -displaysize=<width>x<height>   set window/screen size\n");
	exit(0);
}

void version()
{
	printf("Multitalk version %s\n", VERSION);
	exit(0);
}

const char *parse_args(int argc, char **argv)
{
	const char *talk_ref;
	const char *displayopt = "-displaysize=";
	
	if(argc < 2)
		usage();
	fullscreen = -1;
	for(int i = 1; i < argc - 1; i++)
	{
		if(!strcmp(argv[i], "-fs"))
			fullscreen = 1;
		else if(!strcmp(argv[i], "-win"))
			fullscreen = 0;
		else if(!strcmp(argv[i], "-nowatch"))
			watch_file = 0;
		else if(!strcmp(argv[i], "-gyro"))
			gyro = 1;
		else if(!strcmp(argv[i], "-reverse"))
			reverse_mouse = 1;
		else if(!strcmp(argv[i], "-force"))
			force_latex = 1;
		else if(!strcmp(argv[i], "-version"))
			version();
		else if(!strcmp(argv[i], "-export"))
			export_html = 1;
		else if(!strncmp(argv[i], displayopt, strlen(displayopt)))
		{
			// -displaysize=<width>x<height>
			const char *s = argv[i] + strlen(displayopt);
			svector *v = split_list(s, 'x');
			if(v->count() != 2)
				usage();
			display_width = atoi(v->item(0));
			display_height = atoi(v->item(1));
			if(display_width < 10 || display_height < 10 ||
					display_width > 99999 || display_height > 99999)
				error("Improbable displaysize");
			delete v;
		}
		else
			usage();
	}
	if(!strcmp(argv[argc - 1], "-version"))
		version();
	if(argv[argc - 1][0] == '-')
		usage();
	talk_ref = argv[argc - 1];
	return talk_ref;
}

int to_screen_coords(int x)
{
	if(scalep == 1)
		return x;
	x = (x * scalep) / scaleq;
	return x;
}

int to_design_coords(int x)
{
	if(scalep == 1)
		return x;
	x = (x * scaleq) / scalep;
	return x;
}

void set_resolution()
{
	/*
	printf("display_width = %d, display_height = %d\n",
			display_width, display_height);
	printf("design_width = %d, design_height = %d\n",
			design_width, design_height);
	*/
	if(display_width == -1 && design_width == -1)
		return; // Stick with default resolution
	if(design_width == -1)
	{
		// Talk doesn't specify, so change to user requested resolution:
		SCREEN_WIDTH = display_width;
		SCREEN_HEIGHT = display_height;
		return;
	}
	if(display_width == -1)
	{
		// User hasn't overridden the resolution, so try the talk's intended one:
		SCREEN_WIDTH = design_width;
		SCREEN_HEIGHT = design_height;
		return;
	}
	// Both display size and design size have been specified:
	if(display_width == design_width && display_height == design_height)
		return; // Happily, they are both the same
	if(display_width * design_height == display_height * design_width)
	{
		// Same aspect ratio:
		SCREEN_WIDTH = display_width;
		SCREEN_HEIGHT = display_height;
		scalep = display_height;
		scaleq = design_height;
		CARD_EDGE = to_screen_coords(CARD_EDGE);
		printf("Display size differs from design size; scaling by %f\n",
				(double)scalep / (double)scaleq);
	}
	else if(display_width * design_height > display_height * design_width)
	{
		// Screen is wider than expected (unused space at sides):
		scalep = display_height;
		scaleq = design_height;
		SCREEN_WIDTH = (design_width * scalep) / scaleq;
		SCREEN_HEIGHT = display_height;
		CARD_EDGE = to_screen_coords(CARD_EDGE);
		printf("Display aspect ratio is wider than the talk; scaling by %f\n",
				(double)scalep / (double)scaleq);
	}
	else
	{
		// Screen is narrower than expected (unused space at top and bottom):
		scalep = display_width;
		scaleq = design_width;
		SCREEN_WIDTH = display_width;
		SCREEN_HEIGHT = (design_height * scalep) / scaleq;
		CARD_EDGE = to_screen_coords(CARD_EDGE);
		printf("Display aspect ratio is narrower than the talk; scaling by %f\n",
				(double)scalep / (double)scaleq);
	}
}

void init_gui(const char *caption, int offscreen)
{
	init_sdl(caption, offscreen);
	
	osd_font = init_font(config, OSD_FONT_FILE, OSD_FONT_SIZE);
	help_font = init_font(config, OSD_FONT_FILE, OSD_FONT_SIZE / 2);
	init_colours();
	canvas_colour = colour->names->find("grey");
	viewx = UNKNOWN_POS;
	viewy = UNKNOWN_POS;
	zoom_level = 0;	
	for(int i = 0; i < 3; i++)
		extra_button[i] = 0;
	radar = alloc_surface(RADAR_WIDTH, RADAR_HEIGHT);
	SDL_SetAlpha(radar, SDL_SRCALPHA | SDL_RLEACCEL, 0xCC);
}

void fix_position(int *prefx, int *prefy)
{
	*prefx = viewx;
	*prefy = viewy;
}

int zoom_factor(int level)
{
	if(level == 2)
		return 9;
	else if(level == 1)
		return 3;
	return 1;
}

int zoom_offset(int level)
{
	if(level == 2)
		return 4;
	else if(level == 1)
		return 1;
	return 0;
}

void change_zoom_level(int level)
{
	int delta = zoom_offset(level) - zoom_offset(zoom_level);
	
	if(delta == 0)
		return;
	viewx -= delta * SCREEN_WIDTH;
	viewy -= delta * SCREEN_HEIGHT;
	zoom_level = level;
	
	if(zoom_level == 2)
	{
		magnify = slide_under_pointer();
		if(mag_surface != NULL)
		{
			SDL_FreeSurface(mag_surface);
			mag_surface = NULL;
		}
	}
}

void autoscroll(int *prefx, int *prefy)
{
	const int dist = 16; // This should be >= CARD_EDGE for smoothness
	
	if(viewx < *prefx)
	{
		viewx += dist;
		if(viewx > *prefx)
			viewx = *prefx;
	}
	else if(viewx > *prefx)
	{
		viewx -= dist;
		if(viewx < *prefx)
			viewx = *prefx;
	}
	if(viewy < *prefy)
	{
		viewy += dist;
		if(viewy > *prefy)
			viewy = *prefy;
	}
	else if(viewy > *prefy)
	{
		viewy -= dist;
		if(viewy < *prefy)
			viewy = *prefy;
	}
}

void warp_to(int x, int y)
{
	// From viewx, viewy; at end these should be set to x, y
	int dx, dy;
	const int warp_steps = 15;
	
	for(int i = 0; i < warp_steps; i++)
	{
		dx = x - viewx;
		dy = y - viewy;
		viewx += dx / (warp_steps - i);
		viewy += dy / (warp_steps - i);
		refresh();
		SDL_Delay(33);
	}
	// Flush events:
	SDL_Event event;
	while(SDL_PollEvent(&event) != 0) ;
	if(!pointer_on)
		SDL_ShowCursor(SDL_ENABLE);
	hide_pointer = 0;
}

void warp_to_slide(slide *sl)
{
	warp_to((sl->x + sl->scr_w / 2) - SCREEN_WIDTH / 2 -
			SCREEN_WIDTH * zoom_offset(zoom_level),
			(sl->y + sl->scr_h / 2) - SCREEN_HEIGHT / 2 -
			SCREEN_HEIGHT * zoom_offset(zoom_level));
}

void get_screen_centre(int *x, int *y)
{
	*x = viewx + SCREEN_WIDTH / 2 + SCREEN_WIDTH * zoom_offset(zoom_level);
	*y = viewy + SCREEN_HEIGHT / 2 + SCREEN_HEIGHT * zoom_offset(zoom_level);
}

void warp_right()
{
	slide *sl, *nearest = NULL;
	int centre_x, centre_y;
	int dist, min_dist = -UNKNOWN_POS;

	get_screen_centre(&centre_x, &centre_y);	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		dist = sl->x - centre_x;
		if(dist <= 0)
			continue;
		if(sl->y > centre_y)
			dist += cursor_direction_bias * (sl->y - centre_y);
		if(sl->y + sl->scr_h < centre_y)
			dist += cursor_direction_bias * (centre_y - sl->y - sl->scr_h);
		if(dist < min_dist)
		{
			nearest = sl;
			min_dist = dist;
		}
	}
	if(nearest != NULL && min_dist < max_cursor_distance)
		warp_to_slide(nearest);
}

void warp_left()
{
	slide *sl, *nearest = NULL;
	int centre_x, centre_y;
	int dist, min_dist = -UNKNOWN_POS;

	get_screen_centre(&centre_x, &centre_y);	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		dist = centre_x - (sl->x + sl->scr_w);
		if(dist <= 0)
			continue;
		if(sl->y > centre_y)
			dist += cursor_direction_bias * (sl->y - centre_y);
		if(sl->y + sl->scr_h < centre_y)
			dist += cursor_direction_bias * (centre_y - sl->y - sl->scr_h);
		if(dist < min_dist)
		{
			nearest = sl;
			min_dist = dist;
		}
	}
	if(nearest != NULL && min_dist < max_cursor_distance)
		warp_to_slide(nearest);
}

void warp_up()
{
	slide *sl, *nearest = NULL;
	int centre_x, centre_y;
	int dist, min_dist = -UNKNOWN_POS;

	get_screen_centre(&centre_x, &centre_y);	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		dist = centre_y - (sl->y + sl->scr_h);
		if(dist <= 0)
			continue;
		if(sl->x > centre_x)
			dist += cursor_direction_bias * (sl->x - centre_x);
		if(sl->x + sl->scr_w < centre_x)
			dist += cursor_direction_bias * (centre_x - sl->x - sl->scr_w);
		if(dist < min_dist)
		{
			nearest = sl;
			min_dist = dist;
		}
	}
	if(nearest != NULL && min_dist < max_cursor_distance)
		warp_to_slide(nearest);
}

void warp_down()
{
	slide *sl, *nearest = NULL;
	int centre_x, centre_y;
	int dist, min_dist = -UNKNOWN_POS;

	get_screen_centre(&centre_x, &centre_y);	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		dist = sl->y - centre_y;
		if(dist <= 0)
			continue;
		if(sl->x > centre_x)
			dist += cursor_direction_bias * (sl->x - centre_x);
		if(sl->x + sl->scr_w < centre_x)
			dist += cursor_direction_bias * (centre_x - sl->x - sl->scr_w);
		if(dist < min_dist)
		{
			nearest = sl;
			min_dist = dist;
		}
	}
	if(nearest != NULL && min_dist < max_cursor_distance)
		warp_to_slide(nearest);
}

int check_memory()
{
	int pid, fd, bytes, spaces;
	char proc_stat_file[100], *s;
	int mem;

	pid = getpid();
	sprintf(proc_stat_file, "/proc/%d/stat", pid);
	fd = open(proc_stat_file, O_RDONLY);
	if(fd == -1)
		error("Can't open /proc filesystem file to check memory usage.");
	bytes = read(fd, proc_stat_buf, PROC_STAT_BUF_LEN - 1);
	if(bytes < 1)
		error("Problem reading from /proc filesystem.");
	proc_stat_buf[bytes] = '\0';
	close(fd);
	
	// 23rd field is VM size in bytes
	spaces = 0;
	s = proc_stat_buf;
	while(spaces < 22)
	{
		if(*s == '\0')
			error("Incorrect /proc filesystem format.");
		if(*s == ' ')
			spaces++;
		s++;
	}
	sscanf(s, "%d", &mem);
	
	/*
	// Kludgey way, outputs to a file:
	char memcommand[100];
	sprintf(memcommand, "cat /proc/%d/status | grep VmSize > memusage.dat", pid);
	system(memcommand);
	*/

	/*
	// Unsupported by Linux:
	struct rusage usage;
	long bytes, bytes1, bytes2, bytes3;
	
	getrusage(RUSAGE_SELF, &usage);
	bytes = usage.ru_maxrss;
	bytes1 = usage.ru_ixrss;
	bytes2 = usage.ru_idrss;
	bytes3 = usage.ru_isrss;
	printf("Maximum rss = %ld, others %ld, %ld, %ld.\n", bytes,
			bytes1, bytes2, bytes3);
	*/
	
	return mem;
}

void copy_radar_to_screen()
{
	SDL_Rect dst;
	dst.x = SCREEN_WIDTH - RADAR_WIDTH;
	dst.y = SCREEN_HEIGHT - RADAR_HEIGHT;
	int ret = SDL_BlitSurface(radar, NULL, screen, &dst);
	if(ret != 0)
		error("SDL_BlitSurface returned %d\n", ret);
}

void render_radar(int cx, int cy, hotspotvector *hsv, int slide_num)
{
	slide *sl;
	SDL_Rect dst;
	int x1, y1;
	
	for(int i = 0; i < render_list->count(); i++)
	{
		sl = render_list->item(i);
		x1 = (sl->x - cx) / RADAR_MAG + RADAR_WIDTH / 2;
		y1 = (sl->y - cy) / RADAR_MAG + RADAR_HEIGHT / 2;
		dst.x = x1;
		dst.y = y1;
		dst.w = sl->scr_w / RADAR_MAG;
		dst.h = sl->scr_h / RADAR_MAG;
		if(hsv != NULL)
		{
			int x2, y2;
			hotspot *hs;
			
			x2 = dst.x + dst.w - 1;
			y2 = dst.y + dst.h - 1;
			
			if(!(x2 < 0 || y2 < 0 || x1 >= RADAR_WIDTH || y1 >= RADAR_HEIGHT))
			{
				// Some kind of overlap (now truncate if necessary):
				
				hs = new hotspot();
				hs->sl = sl;
				hs->card = 0;
				hs->unfold = 0;

				hs->x1 = (x1 < 0) ? 0 : x1;
				hs->y1 = (y1 < 0) ? 0 : y1;
				
				if(x2 >= RADAR_WIDTH) x2 = RADAR_WIDTH - 1;
				if(y2 >= RADAR_HEIGHT) y2 = RADAR_HEIGHT - 1;				
				hs->x2 = x2;
				hs->y2 = y2;
				
				hsv->add(hs);
			}
		}
		if(sl->image_file != NULL)
		{
			SDL_FillRect(radar, &dst, colour->white_fill);
			dst.x = x1;
			dst.y = y1;
			dst.w = sl->scr_w / RADAR_MAG;
			dst.h = sl->scr_h / RADAR_MAG;
			aaellipseColor(radar, dst.x + dst.w / 2, dst.y + dst.h / 2,
					(dst.w - 1) / 2, (dst.h - 1) / 2, colour->black_pen);
		}
		else
		{
			SDL_FillRect(radar, &dst, colour->fills->item(sl->st->barcolour));
		}
		dst.x = x1;
		dst.y = y1;
		dst.w = sl->scr_w / RADAR_MAG;
		dst.h = sl->scr_h / RADAR_MAG;
		rectangleColor(radar, dst.x, dst.y, dst.x + dst.w - 1,
				dst.y + dst.h - 1, colour->black_pen);
	}
	rectangleColor(radar, 0, 0, RADAR_WIDTH - 1, RADAR_HEIGHT - 1,
			colour->black_pen);

	if(hsv == NULL)
	{
		// Show displayed area:
		
		int xradius, yradius;
		xradius = SCREEN_WIDTH * zoom_factor(zoom_level) / 2;
		yradius = SCREEN_HEIGHT * zoom_factor(zoom_level) / 2;
		rectangleColor(radar, RADAR_WIDTH / 2 - xradius / RADAR_MAG,
				RADAR_HEIGHT / 2 - yradius / RADAR_MAG,
				RADAR_WIDTH / 2 + xradius / RADAR_MAG,
				RADAR_HEIGHT / 2 + yradius / RADAR_MAG, colour->black_pen);
	}
	else
	{
		// X marks the spot:
		int x, y, d = 5;

		if(slide_num == -1)
		{		
			x = RADAR_WIDTH / 2;
			y = RADAR_HEIGHT / 2;
		}
		else
		{
			sl = render_list->item(slide_num);
			x = (sl->x + sl->scr_w / 2 - cx) / RADAR_MAG + RADAR_WIDTH / 2;
			y = (sl->y + sl->scr_h / 2 - cy) / RADAR_MAG + RADAR_HEIGHT / 2;
		}
		
		lineColor(radar, x - d, y - d + 1, x + d - 1, y + d, colour->black_pen);
		lineColor(radar, x - d + 1, y - d, x + d, y + d - 1, colour->black_pen);
		
		lineColor(radar, x - d, y + d - 1, x + d - 1, y - d, colour->black_pen);
		lineColor(radar, x - d + 1, y + d, x + d, y - d + 1, colour->black_pen);
		
		lineColor(radar, x - d, y - d, x + d, y + d, colour->red_pen);
		lineColor(radar, x - d, y + d, x + d, y - d, colour->red_pen);
	}
}

void osd()
{
	if(memory_display)
	{
		char textstr[40];	
		int memuse = check_memory();
		sprintf(textstr, "%d", memuse);
		int w, h;
		TTF_SizeUTF8(osd_font, textstr, &w, &h);
		render_text(textstr, osd_font, &colour->red_text, screen,
				SCREEN_WIDTH - 15 - w, SCREEN_HEIGHT - 5 - h);
	}
	if(help_display)
	{
		const char helpstr[12][40] = {
			"TAB ... fullscreen mode",
			"Return ... go back to last hyperlink",
			". ... advance in stack",
			", ... previous in stack",
			"A ... align-to-grid mode",
			"E ... examine modes",
			"N ... navigation radar",
			"P ... giant arrow",
			"R ... re-read file",
			"Backspace ... pin a slide",
			"Insert ... single slide view",
			"Escape ... quit"
		};
		int w, h;
		int i;
		int bottom = 5;
		for (i = 0; i < 12; i++) {
			TTF_SizeUTF8(help_font, helpstr[i], &w, &h);
			render_text(helpstr[i], help_font, &colour->red_text, screen,
					10 /*SCREEN_WIDTH - 15 - w*/, SCREEN_HEIGHT - bottom - h);
			bottom += h;
		}
	}
	if(radar_window && zoom_level < 2)
	{
		int cx, cy;
		
		clear_radar();
		
		cx = viewx + SCREEN_WIDTH / 2;
		cy = viewy + SCREEN_HEIGHT / 2;
		if(zoom_level == 1)
		{
			cx += SCREEN_WIDTH;
			cy += SCREEN_HEIGHT;
		}
		
		render_radar(cx, cy);
	
		copy_radar_to_screen();
	}
	if(zoom_level == 2 && magnify != NULL && hover_mode > 0)
	{
		SDL_Rect src, dst;
		
		if(hover_mode == 1)
		{
			const int margin = 10;

			if(magnify->image_file == NULL)
			{
				if(mag_surface == NULL)
				{
					SDL_Surface *title_surface, *reduced_surface;
					double f = 0.5;
					
					src.x = 0;
					src.y = 0;
					src.w = magnify->scr_w;
					src.h = to_screen_coords(magnify->st->titlespacing - TITLE_EDGE);
				
					title_surface = alloc_surface(src.w, src.h);
					SDL_BlitSurface(magnify->scaled, &src, title_surface, NULL);
					
					reduced_surface = zoomSurface(title_surface, f, f, 1);
					SDL_FreeSurface(title_surface);
					
					mag_surface = alloc_surface(reduced_surface->w,
							reduced_surface->h);
					SDL_SetAlpha(mag_surface, SDL_SRCALPHA | SDL_RLEACCEL, 0xCC);						
					SDL_BlitSurface(reduced_surface, NULL, mag_surface, NULL);
					SDL_FreeSurface(reduced_surface);
				}

				dst.x = (SCREEN_WIDTH - mag_surface->w) / 2;
				dst.y = margin;
				// dst.y = SCREEN_HEIGHT - mag_surface->h - margin;
				SDL_BlitSurface(mag_surface, NULL, screen, &dst);
			}
		}
		else
		{
			if(mag_surface == NULL)
			{
				mag_surface = alloc_surface(magnify->mini->w, magnify->mini->h);
				SDL_SetAlpha(mag_surface, SDL_SRCALPHA | SDL_RLEACCEL, 0xCC);
				SDL_BlitSurface(magnify->mini, NULL, mag_surface, NULL);
			}
			
			dst.x = SCREEN_WIDTH - magnify->mini->w;
			dst.y = SCREEN_HEIGHT - magnify->mini->h;
			SDL_BlitSurface(mag_surface, NULL, screen, &dst);
		}
	}
	for(int i = 0; i < 4; i++)
	{
		if(pin[i] != NULL)
		{
			SDL_Rect dst;
			
			switch(i)
			{
				case 0:
					dst.x = 0;
					dst.y = 0;
					break;
				case 1:
					dst.x = SCREEN_WIDTH - pin[i]->w;
					dst.y = 0;
					break;
				case 2:
					dst.x = 0;
					dst.y = SCREEN_HEIGHT - pin[i]->h;
					break;
				case 3:
					dst.x = SCREEN_WIDTH - pin[i]->w;
					dst.y = SCREEN_HEIGHT - pin[i]->h;
					break;
				default: error("Impossible case"); break;
			}
			SDL_BlitSurface(pin[i], NULL, screen, &dst);
		}
	}
}

void refresh(int flip)
{
	cls();
	if(zoom_level == 2)
		micro_copy_all_to_screen(render_list, viewx, viewy);
	else if(zoom_level == 1)
		mini_copy_all_to_screen(render_list, viewx, viewy);
	else
		copy_all_to_screen(render_list, viewx, viewy);
	osd();
	if(pointer_on && !hide_pointer)
		pointer(pointer_x, pointer_y);
	if(flip)
		SDL_Flip(screen);
	refreshreq = 0;
}

void pop_to_front(slide *sl) // Note: Doesn't do a refresh
{
	int index = render_list->find(sl);
	if(index == -1)
		error("Can't find slide in render_list (impossible)");
	render_list->demote(index);
}

void redraft(slide *sl)
{
	flatten(sl, talk);
	measure_slide(sl);
	pop_to_front(sl);
	render_slide(sl);
}

void expand(slide *sl, displayline *out)
{
	node *source = out->source;
	source->folded = 0;
	redraft(sl);
	refreshreq = 1;
}

void collapse(slide *sl, displayline *out)
{
	node *source = out->source;
	source->folded = 1;
	flatten(sl, talk);
	measure_slide(sl);
	render_slide(sl);
	refreshreq = 1;
}

displayline *fold_possible(slide *sl, int line_num)
{
	displayline *out;
	int type;
	
	if(line_num < 1)
		return NULL;
	out = sl->repr->item(line_num - 1);
	type = out->type;
	if(type == TYPE_FOLDED || type == TYPE_EXPANDED)
		return out;
	return NULL;
}

void fold_request(slide *sl, int line_num)
{
	displayline *out;
	int type;
	
	if(line_num < 1)
		return;
	out = sl->repr->item(line_num - 1);
	type = out->type;
	if(type == TYPE_FOLDED)
		expand(sl, out);
	else if(type == TYPE_EXPANDED)
		collapse(sl, out);
}

slide *hyperlink_request(slide *sl_local, int line_num, displayline **ln)
{
	displayline *out;
	slide *sl_remote;

	if(line_num < 1)
		return NULL;
	if(sl_local->image_file != NULL)
		return NULL;
	
	out = sl_local->repr->item(line_num - 1);
	sl_remote = out->link;
	if(ln != NULL)
		*ln = out;
	return sl_remote;
}

int part_offscreen_x(slide *sl)
{
	int sx1, sx2; // Slide boundaries
	int dx1, dx2; // Display boundaries
	
	sx1 = sl->x;
	sx2 = sl->x + sl->scr_w;
	dx1 = viewx;
	dx2 = viewx + SCREEN_WIDTH * zoom_factor(zoom_level);
	
	if(sx2 > dx2 && sx1 < dx1)
		return 0; // Offscreen in both directions
	if(sx2 > dx2)
		return sx2 - dx2; // Offscreen to the right (+ve)
	if(sx1 < dx1)
		return sx1 - dx1; // Offscreen to the left (-ve)
	return 0; // Onscreen
}

int part_offscreen_y(slide *sl)
{
	int sy1, sy2; // Slide boundaries
	int dy1, dy2; // Display boundaries
	
	sy1 = sl->y;
	sy2 = sl->y + sl->scr_h;
	dy1 = viewy;
	dy2 = viewy + SCREEN_HEIGHT * zoom_factor(zoom_level);
	
	if(sy2 > dy2 && sy1 < dy1)
		return 0; // Offscreen in both directions
	if(sy2 > dy2)
		return sy2 - dy2; // Offscreen below (+ve)
	if(sy1 < dy1)
		return sy1 - dy1; // Offscreen above (-ve)
	return 0; // Onscreen
}

int talk_modified()
{
	struct stat st;
	int ret;
	time_t mod_time;
	
	ret = stat(config->talk_path, &st);
	if(ret != 0)
		error("Can't check talk file for modifications.");
	mod_time = st.st_mtime;
	if(last_modified == -1)
	{
		last_modified = mod_time;
	}
	else if(mod_time > last_modified)
	{
		last_modified = mod_time;
		return 1;
	}
	return 0;
}

Uint32 timer_callback_func(Uint32 interval, void *param)
{
	SDL_Event user_event;
	
	user_event.type = SDL_USEREVENT;
	user_event.user.code = 0;
	user_event.user.data1 = NULL;
	user_event.user.data2 = NULL;
	SDL_PushEvent(&user_event);
	/* param is ignored. */
	(void)param;

	return interval;
}

void button_release(int *scrollreqx, int *scrollreqy,
		int *prefx, int *prefy, int enable_grav)
{
	if(zoom_level == 0)
	{
		if(*scrollreqx != 0 || *scrollreqy != 0)
		{
			// Some unprocessed scrolling still to do:
			viewx += *scrollreqx;
			viewy += *scrollreqy;
			refreshreq = 1;
			*scrollreqx = *scrollreqy = 0;
		}
		if(gravity && enable_grav)
			recentre(prefx, prefy);
	}
}

void previous_card(slide *sl)
{
	if(sl->card > 1)
	{
		sl->card--;
		sl->x -= CARD_EDGE;
		sl->y += CARD_EDGE;
	}
	else
	{
		sl->card = sl->deck_size;
		sl->x += CARD_EDGE * (sl->deck_size - 1);
		sl->y -= CARD_EDGE * (sl->deck_size - 1);
	}
	redraft(sl);
}

void next_card(slide *sl)
{
	if(sl->card < sl->deck_size)
	{
		sl->card++;
		sl->x += CARD_EDGE;
		sl->y -= CARD_EDGE;
	}
	else
	{
		sl->card = 1;
		sl->x -= CARD_EDGE * (sl->deck_size - 1);
		sl->y += CARD_EDGE * (sl->deck_size - 1);
	}
	redraft(sl);
}

void goto_card(slide *sl, int n)
{
	if(n == sl->card)
		return;
	if(n < 1 || n > sl->deck_size)
		error("No such card number %d in slide %s", n, sl->content->line);
	sl->x += CARD_EDGE * (n - sl->card);
	sl->y -= CARD_EDGE * (n - sl->card);
	sl->card = n;
	redraft(sl);
}

void update_feedback(slide *sl)
{
	reallocate_surfaces(sl);
	scale(sl);
}

void unselect_all()
{
	slide *sl;
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		sl->selected = 0;
	}
}

void single_slide_view(slide *sl)
{
	SDL_Surface *full_surface;
	SDL_Rect dst;
	double f, g;

	f = (double)SCREEN_WIDTH / (double)sl->des_w;
	g = (double)SCREEN_HEIGHT / (double)sl->des_h;
	if(g < f)
		f = g;
	full_surface = zoomSurface(sl->render, f, f, 1);
	
	dst.x = (SCREEN_WIDTH - full_surface->w) / 2;
	dst.y = (SCREEN_HEIGHT - full_surface->h) / 2;
	
	cls();
	SDL_BlitSurface(full_surface, NULL, screen, &dst);
	SDL_Flip(screen);
	
	SDL_FreeSurface(full_surface);
	viewloop();
	refreshreq = 1;
}

void split_screen(slide *sl1, slide *sl2)
{
	double fv, fh;
	double f1, f2;
	int horiz;
	SDL_Surface *surface1, *surface2;
	SDL_Rect dst;
	
	// Try vertical dividing line:
	fv = (double)SCREEN_WIDTH / (double)(sl1->des_w + sl2->des_w);
	
	// Try horizontal dividing line:
	fh = (double)SCREEN_HEIGHT / (double)(sl1->des_h + sl2->des_h);
	
	if(fh > fv)
	{
		horiz = 1;
		f1 = f2 = fh;
		if(f1 * (double)(sl1->des_w) > (double)SCREEN_WIDTH)
			f1 = (double)SCREEN_WIDTH / (double)(sl1->des_w);
		if(f2 * (double)(sl2->des_w) > (double)SCREEN_WIDTH)
			f2 = (double)SCREEN_WIDTH / (double)(sl2->des_w);
	}
	else
	{
		horiz = 0;
		f1 = f2 = fv;
		if(f1 * (double)(sl1->des_h) > (double)SCREEN_HEIGHT)
			f1 = (double)SCREEN_HEIGHT / (double)(sl1->des_h);
		if(f2 * (double)(sl2->des_h) > (double)SCREEN_HEIGHT)
			f2 = (double)SCREEN_HEIGHT / (double)(sl2->des_h);
	}
	surface1 = zoomSurface(sl1->render, f1, f1, 1);
	surface2 = zoomSurface(sl2->render, f2, f2, 1);
	
	cls();
	if(horiz)
	{
		dst.x = (SCREEN_WIDTH - surface1->w) / 2;
		dst.y = 0;
	}
	else
	{
		dst.x = 0;
		dst.y = (SCREEN_HEIGHT - surface1->h) / 2;
	}
	SDL_BlitSurface(surface1, NULL, screen, &dst);
	if(horiz)
	{
		dst.x = (SCREEN_WIDTH - surface2->w) / 2;
		dst.y = SCREEN_HEIGHT - surface2->h;
	}
	else
	{
		dst.x = SCREEN_WIDTH - surface2->w;
		dst.y = (SCREEN_HEIGHT - surface2->h) / 2;
	}
	SDL_BlitSurface(surface2, NULL, screen, &dst);
	SDL_Flip(screen);
	
	SDL_FreeSurface(surface1);
	SDL_FreeSurface(surface2);
	viewloop();
	refreshreq = 1;
}

void viewloop()
{
	SDL_Event event;

	while(1)
	{
		SDL_WaitEvent(&event);
		switch(event.type)
		{
			case SDL_KEYDOWN:
				return;
				break;
			case SDL_MOUSEBUTTONDOWN:
				return;
				break;
			case SDL_QUIT:
				return;
				break;			
		}
	}
}

void maximise()
{
	slide *sl = NULL;
	
	if(zoom_level == 0)
		sl = central_slide();
	if(sl == NULL)
		sl = slide_under_pointer();
	if(sl == NULL)
		return;
	
	if(pin[1] != NULL)
		split_screen(sl, pinned);
	else
		single_slide_view(sl);
}

void do_pin()
{
	slide *sl = NULL;
	SDL_Surface *reduced;
	double f = 0.5;
	
	if(pin[1] != NULL)
	{
		SDL_FreeSurface(pin[1]);
		pin[1] = NULL;
		refreshreq = 1;
		return;
	}
	if(zoom_level == 0)
		sl = central_slide();
	if(sl == NULL)
		sl = slide_under_pointer();
	if(sl == NULL)
		return;
	pinned = sl;
	
	reduced = zoomSurface(sl->scaled, f, f, 1);

	pin[1] = alloc_surface(reduced->w, reduced->h);
	SDL_SetAlpha(pin[1], SDL_SRCALPHA | SDL_RLEACCEL, 0xDD);
	SDL_BlitSurface(reduced, NULL, pin[1], NULL);
	SDL_FreeSurface(reduced);
	
	refreshreq = 1;
}

void do_pin(int corner)
{
	slide *sl;

	if(pin[corner] != NULL)
	{
		SDL_FreeSurface(pin[corner]);
		pin[corner] = NULL;
	}
	sl = slide_under_pointer();
	if(sl != NULL)
	{
		SDL_Surface *reduced;
		
		double f = 0.5;
		reduced = zoomSurface(sl->scaled, f, f, 1);

		if(corner == 1)		
			pinned = sl;
		pin[corner] = alloc_surface(reduced->w, reduced->h);
		SDL_SetAlpha(pin[corner],
				SDL_SRCALPHA | SDL_RLEACCEL, 0xDD);
		SDL_BlitSurface(reduced, NULL, pin[corner], NULL);
		
		SDL_FreeSurface(reduced);
	}
	refreshreq = 1;
}

int mainloop()
{
	SDL_Event event;
	
	SDL_KeyboardEvent *key;
	SDLKey sym;
	SDLMod mod;
	Uint16 code;
	Uint8 raw;
	
	SDL_MouseButtonEvent *click;
	Uint8 button;
	int mouse_x, mouse_y, delta_x, delta_y;
	SDL_MouseMotionEvent *motion;
	Uint8 but_state;
	SDLMod modkeys;

	struct timeval tv_watched;
	
	int scrollreqx = 0;
	int scrollreqy = 0;
	int updatereq = 0;
	int prefx = viewx;
	int prefy = viewy;
	int restorex = 0, restorey = 0;
	int drag_dist = 0;
	int clickx = 0, clicky = 0;
	int control_key = 0;
	
	slide *grabbed = NULL;
	subimage *focus = NULL;
	displayline *lit = NULL;
	slide *sl_lit = NULL;
	slidevector *selected = new slidevector();
	int selections_added = 0;
	magnify = NULL;
	mag_surface = NULL;
	for(int i = 0; i < 4; i++)
		pin[i] = NULL;

	if(watch_file)
	{
		SDL_TimerID id;
		id = SDL_AddTimer(WATCH_INTERVAL, timer_callback_func, NULL);
		gettimeofday(&tv_watched, NULL);
	}
	while(1)
	{
		if(SDL_PollEvent(&event) == 0)
		{
			if(updatereq)
			{
				if(focus != NULL)
				{
					measure_slide(grabbed);
					render_slide(grabbed);
				}
				updatereq = 0;
				refreshreq = 1;
			}
			if(scrollreqx != 0 || scrollreqy != 0)
			{
				viewx += scrollreqx;
				viewy += scrollreqy;
				fix_position(&prefx, &prefy);
				scrollreqx = scrollreqy = 0;
				refreshreq = 1;
			}
			else if(prefx != viewx || prefy != viewy)
			{
				autoscroll(&prefx, &prefy);
				refresh();
				SDL_Delay(5);
				continue;
			}
			if(refreshreq)
				refresh();
			if(watch_file)
			{
				struct timeval tv_now;
				int ms;
				
				// Don't check too frequently:
				gettimeofday(&tv_now, NULL);
				ms = (tv_now.tv_sec - tv_watched.tv_sec) * 1000;
				ms += (tv_now.tv_usec - tv_watched.tv_usec) / 1000;
				if(ms > WATCH_INTERVAL)
				{
					tv_watched.tv_sec = tv_now.tv_sec;
					tv_watched.tv_usec = tv_now.tv_usec;
					if(talk_modified())
					{
						delete selected;
						unselect_all();
						return 0;
					}
				}
			}
			SDL_WaitEvent(&event);
		}
		switch(event.type)
		{
			case SDL_KEYDOWN:
				key = &event.key;
				sym = key->keysym.sym;
				code = key->keysym.unicode;
				mod = key->keysym.mod;
				raw = key->keysym.scancode;
				if(gyro)
				{
					if(raw == 93 && extra_button[0] == 0)
					{
						extra_button[0] = 1;
						SDL_ShowCursor(SDL_DISABLE);
						hide_pointer = 1;
					}
					else if(raw == 123 && extra_button[1] == 0)
					{
						extra_button[1] = 1;
						SDL_ShowCursor(SDL_DISABLE);
						hide_pointer = 1;
					}
					else if(raw == 127 && extra_button[2] == 0)
					{
						extra_button[2] = 1;
						SDL_ShowCursor(SDL_DISABLE);
						hide_pointer = 1;
						change_zoom_level(1);
						scrollreqx = scrollreqy = 0;
						fix_position(&prefx, &prefy);
						refreshreq = 1;
					}
				}
				switch(sym)
				{
					case SDLK_ESCAPE:
						delete selected;
						unselect_all();
						return 1;
						break;
					/*
					case SDLK_1: do_pin(0); break;
					case SDLK_2: do_pin(1); break;
					case SDLK_3: do_pin(2); break;
					case SDLK_4: do_pin(3); break;
					*/
					case SDLK_BACKSPACE:
						do_pin();
						break;
					case SDLK_SLASH:
						maximise();
						break;
					case SDLK_a:
						grid_mode = 1 - grid_mode;
						break;
					case SDLK_e:
						hover_mode++;
						if(hover_mode > 2)
							hover_mode = 1;
						if(mag_surface != NULL)
						{
							SDL_FreeSurface(mag_surface);
							mag_surface = NULL;
						}
						if(zoom_level == 2 && magnify != NULL)
							refreshreq = 1;
						break;
					case SDLK_h:
						help_display = 1 - help_display;
						refreshreq = 1;
						break;
					case SDLK_g:
						gravity = 1 - gravity;
						if(gravity)
							recentre(&prefx, &prefy);
						break;
					case SDLK_HOME:
						warp_to_slide(talk->item(0));
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_j:
						{
							slide * sl = slide_under_pointer();
							if(sl != NULL)
							{
								int i = talk->find(sl);
								if (i > 0) {
									warp_to_slide(talk->item(i - 1));
									fix_position(&prefx, &prefy);
									scrollreqx = scrollreqy = 0;
									refreshreq = 1;
								}
							}
						}
						break;
					case SDLK_k:
						{
							slide * sl = slide_under_pointer();
							if(sl != NULL) 
							{
								int i = talk->find(sl);
								if (i + 1 < talk->count()) {
									warp_to_slide(talk->item(i + 1));
									fix_position(&prefx, &prefy);
									scrollreqx = scrollreqy = 0;
									refreshreq = 1;
								}
							}
						}
						break;
					case SDLK_m:
						memory_display = 1 - memory_display;
						refreshreq = 1;
						break;
					case SDLK_n:
						radar_window = 1 - radar_window;
						refreshreq = 1;
						break;
					case SDLK_p:
						pointer_on = 1 - pointer_on;
						SDL_ShowCursor(pointer_on ? SDL_DISABLE : SDL_ENABLE);
						refreshreq = 1;
						break;
					case SDLK_q:
						if(mod & KMOD_CTRL)
						{
							delete selected;
							unselect_all();
							return 1;
						}
						break;
					case SDLK_r:
						// Reload talk file
						delete selected;
						unselect_all();
						return 0;
						break;
					case SDLK_SPACE:
						if(mod & KMOD_SHIFT)
						{
							if(zoom_level != 2)
								change_zoom_level(2);
							else
								change_zoom_level(0);
						}
						else
						{
							if(zoom_level == 2)
								change_zoom_level(1);
							else
								change_zoom_level(1 - zoom_level);
						}
						scrollreqx = scrollreqy = 0;
						fix_position(&prefx, &prefy);
						refreshreq = 1;
						break;
					case SDLK_RETURN:
						if(zoom_level == 2) break;
						warp_to(restorex - SCREEN_WIDTH * zoom_offset(zoom_level),
								restorey - SCREEN_HEIGHT * zoom_offset(zoom_level));
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_TAB:
						if(mod & KMOD_SHIFT)
						{
							SDL_WM_IconifyWindow();
							// SDL_WM_SetCaption("Multitalk", "Multitalk");
						}
						else
						{
							SDL_WM_ToggleFullScreen(screen);
							fullscreen = 1 - fullscreen;
						}
						break;
					case SDLK_HASH:
						{
							SDL_GrabMode grab;
							
							grab = SDL_WM_GrabInput(SDL_GRAB_QUERY);
							if(grab == SDL_GRAB_OFF)
								grab = SDL_GRAB_ON;
							else
								grab = SDL_GRAB_OFF;
							SDL_WM_GrabInput(grab);
						}
						break;
					case SDLK_COMMA:
						{
							if(zoom_level == 2) break;
							slide *sl = slide_under_pointer();
							if(sl != NULL && sl->deck_size > 1)
							{
								previous_card(sl);
								if(gravity)
								{
									recentre(&prefx, &prefy);
									snap_to(prefx, prefy);
								}
								refreshreq = 1;
							}
						}
						break;
					case SDLK_PERIOD:
						{
							if(zoom_level == 2) break;
							slide *sl = slide_under_pointer();
							if(sl != NULL && sl->deck_size > 1)
							{
								next_card(sl);
								if(gravity)
								{
									recentre(&prefx, &prefy);
									snap_to(prefx, prefy);
								}
								refreshreq = 1;
							}
						}
						break;
					case SDLK_LEFT:
						warp_left();
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_RIGHT:
						warp_right();
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_UP:
						warp_up();
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_DOWN:
						warp_down();
						fix_position(&prefx, &prefy);
						scrollreqx = scrollreqy = 0;
						refreshreq = 1;
						break;
					case SDLK_LCTRL:
					case SDLK_RCTRL:
						{
							slide *sl;
							
							if(control_key == 0)
								selections_added = 0;
							if(sym == SDLK_LCTRL)
								control_key = control_key | 1;
							else
								control_key = control_key | 2;
							
							sl = slide_under_pointer();
							if(sl != NULL && sl->selected == 0)
							{
								sl->selected = 1;
								refreshreq = 1;
								selected->add(sl);
								selections_added++;
							}
						}
						break;
					default:
						// Do nothing
						break;
				}
				break;
			case SDL_USEREVENT:
				break;
			case SDL_KEYUP:
				key = &event.key;
				sym = key->keysym.sym;
				code = key->keysym.unicode;
				mod = key->keysym.mod;
				raw = key->keysym.scancode;
				if(gyro)
				{
					if(raw == 93)
					{
						extra_button[0] = 0;
						if(!pointer_on)
							SDL_ShowCursor(SDL_ENABLE);
						hide_pointer = 0;
						button_release(&scrollreqx, &scrollreqy, &prefx, &prefy,
								(mod & KMOD_ALT) ? 0 : 1);
					}
					else if(raw == 123)
					{
						extra_button[1] = 0;
						if(!pointer_on)
							SDL_ShowCursor(SDL_ENABLE);
						hide_pointer = 0;
						button_release(&scrollreqx, &scrollreqy, &prefx, &prefy,
								(mod & KMOD_ALT) ? 0 : 1);
					}
					else if(raw == 127)
					{
						extra_button[2] = 0;
						if(!pointer_on)
							SDL_ShowCursor(SDL_ENABLE);
						hide_pointer = 0;
						change_zoom_level(0);
						updatereq = 0;
						scrollreqx = scrollreqy = 0;
						fix_position(&prefx, &prefy);
						refreshreq = 1;
						button_release(&scrollreqx, &scrollreqy, &prefx, &prefy,
								(mod & KMOD_ALT) ? 0 : 1);
					}
				}
				switch(sym)
				{
					case SDLK_LCTRL:
					case SDLK_RCTRL:
						if(sym == SDLK_LCTRL)
							control_key = control_key & 2; // Remove 1's
						else
							control_key = control_key & 1; // Remove 2's
						if(control_key == 0)
						{
							if(selections_added == 0)
							{
								selected->clear();
								unselect_all();
								refreshreq = 1;
							}
							selections_added = 0;
						}
						break;
					default:
						// Do nothing
						break;
				}
				break;
			case SDL_MOUSEMOTION:
				motion = &event.motion;
				mouse_x = motion->x;
				mouse_y = motion->y;
				delta_x = motion->xrel;
				delta_y = motion->yrel;
				but_state = motion->state;
				modkeys = SDL_GetModState();
				if(pointer_on)
				{
					pointer_x = mouse_x;
					pointer_y = mouse_y;
					refreshreq = 1;
				}
				drag_dist += abs(delta_x) + abs(delta_y);
				if(drag_dist >= CLICK_THRESHOLD && lit != NULL)
				{
					lit->highlighted = 0;
					render_line(sl_lit, lit, NULL, sl_lit->render);
					update_feedback(sl_lit);
					lit = NULL;
					refreshreq = 1;
				}
				if(((but_state & SDL_BUTTON(1)) || extra_button[0])
						&& grabbed != NULL)
				{
					// LMB drag while slide grabbed:
					if(focus == NULL)
					{
						// Drag slide:
						if(grabbed->selected)
						{
							slide *sl;
							
							for(int i = 0; i < selected->count(); i++)
							{
								sl = selected->item(i);
								sl->x += delta_x * zoom_factor(zoom_level);
								sl->y += delta_y * zoom_factor(zoom_level);
							}
						}
						else
						{
							grabbed->x += delta_x * zoom_factor(zoom_level);
							grabbed->y += delta_y * zoom_factor(zoom_level);
						}
						slides_moved = 1;
						updatereq = 1;

						// Scroll screen if window has moved off:
						scrollreqx = part_offscreen_x(grabbed);
						scrollreqy = part_offscreen_y(grabbed);
					}
					else
					{
						// Drag sub-image:
						focus->scr_x += delta_x * zoom_factor(zoom_level);
						focus->scr_y += delta_y * zoom_factor(zoom_level);
						focus->des_x = to_design_coords(focus->scr_x);
						focus->des_y = to_design_coords(focus->scr_y);
						slides_moved = 1;
						updatereq = 1;
					}
				}
				else if(but_state & SDL_BUTTON(1) || but_state & SDL_BUTTON(2) ||
						but_state & SDL_BUTTON(3) || extra_button[0] ||
						extra_button[1] || extra_button[2])
				{
					// Drag to scroll:
					if(delta_x < 100 && delta_y < 100 && delta_x > -100 &&
							delta_y > -100)
					{
						if(reverse_mouse)
						{
							delta_x = -delta_x;
							delta_y = -delta_y;
						}
						scrollreqx += 3 * zoom_factor(zoom_level) * delta_x;
						scrollreqy += 3 * zoom_factor(zoom_level) * delta_y;
					}
				}
				else if(control_key != 0)
				{
					// Mouse movement with control key pressed:					
					slide *sl;
					
					sl = slide_under_mouse(mouse_x, mouse_y, NULL, NULL);
					if(sl != NULL && sl->selected == 0)
					{
						sl->selected = 1;
						selected->add(sl);
						refreshreq = 1;
						selections_added++;
					}
				}
				else if(zoom_level == 2)
				{
					slide *sl;
					
					sl = slide_under_pointer();
					if(sl != magnify)
					{
						magnify = sl;
						if(mag_surface != NULL)
						{
							SDL_FreeSurface(mag_surface);
							mag_surface = NULL;
						}
						refreshreq = 1;
					}
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				click = &event.button;
				button = click->button; // SDL_BUTTON_LEFT/MIDDLE/RIGHT
				mouse_x = click->x;
				mouse_y = click->y;
				clickx = viewx + zoom_factor(zoom_level) * mouse_x;
				clicky = viewy + zoom_factor(zoom_level) * mouse_y;
				modkeys = SDL_GetModState();
				but_state = SDL_GetMouseState(NULL, NULL);
				SDL_ShowCursor(SDL_DISABLE);
				hide_pointer = 1;
				magnify = NULL;
				if(mag_surface != NULL)
				{
					SDL_FreeSurface(mag_surface);
					mag_surface = NULL;
				}
				if(lit != NULL)
				{
					lit->highlighted = 0;
					render_line(sl_lit, lit, NULL, sl_lit->render);
					update_feedback(sl_lit);
					lit = NULL;
					refreshreq = 1;
				}
				
				fix_position(&prefx, &prefy);
				scrollreqx = scrollreqy = 0;
				drag_dist = 0;
					
				if(button == SDL_BUTTON_RIGHT && (but_state & SDL_BUTTON(1)) &&
						zoom_level != 2)
				{
					// RMB press whilst holding down LMB:
					warp_to(restorex - zoom_offset(zoom_level) * SCREEN_WIDTH,
							restorey - zoom_offset(zoom_level) * SCREEN_HEIGHT);
					fix_position(&prefx, &prefy);
					scrollreqx = scrollreqy = 0;
					refreshreq = 1;
						
					// Prevent further actions when LMB released:
					drag_dist = CLICK_THRESHOLD + 1;
				}
				else if(button == SDL_BUTTON_LEFT && zoom_level == 2)
				{
					/* Just interested in slide clicked on, not sub-images or
						line number: */
					grabbed = slide_under_mouse(mouse_x, mouse_y, NULL, NULL);
					if(grabbed != NULL)
					{
						pop_to_front(grabbed);
						refreshreq = 1;
					}
				}
				else if(button == SDL_BUTTON_LEFT && (modkeys & KMOD_SHIFT))
				{
					// Shift-LMB:
					grabbed = slide_under_mouse(mouse_x, mouse_y, NULL, &focus);
					if(grabbed != NULL)
					{
						if(focus != NULL)
						{
							/* Save slide background for faster redraws whilst
								dragging embedded image: */
							;
						}
						pop_to_front(grabbed);
						refreshreq = 1;
					}
				}
				else if(button == SDL_BUTTON_LEFT)
				{
					// LMB:
					// Most of this section moved to button release...
					// Now mainly does visual feedback only here
					
					int line_num;
					slide *sl_local, *sl_remote;

					sl_local = slide_under_mouse(mouse_x, mouse_y, &line_num, NULL);
					if(sl_local != NULL)
					{
						sl_remote = hyperlink_request(sl_local, line_num, &lit);
						if(sl_remote != NULL) // Hyperlink under mouse
						{
							// Visual feedback for hyperlink click:
							lit->highlighted = 1;
							sl_lit = sl_local;
							render_line(sl_local, lit, NULL, sl_local->render);
							update_feedback(sl_local);
							refreshreq = 1;
						}
						else if(line_num == 0 && zoom_level == 1)
						{
							// Titlebar under mouse
							grabbed = sl_local;
							pop_to_front(grabbed);
							refreshreq = 1;
						}
						else if(sl_local->image_file == NULL && line_num > 0)
						{
							// Visual feedback for fold request:
							lit = fold_possible(sl_local, line_num);
							if(lit != NULL)
							{
								lit->highlighted = 1;
								sl_lit = sl_local;
								render_line(sl_local, lit, NULL, sl_local->render);
								update_feedback(sl_local);
								refreshreq = 1;
							}
						}
					}
				}
				else if(button == SDL_BUTTON_RIGHT)
				{
					// RMB:
					change_zoom_level(1);
					scrollreqx = scrollreqy = 0;
					fix_position(&prefx, &prefy);
					refreshreq = 1;
				}
				break;
			case SDL_MOUSEBUTTONUP:
				click = &event.button;
				button = click->button; // SDL_BUTTON_LEFT/MIDDLE/RIGHT
				mouse_x = click->x;
				mouse_y = click->y;
				modkeys = SDL_GetModState();
				if(!pointer_on)
					SDL_ShowCursor(SDL_ENABLE);
				hide_pointer = 0;
				if(lit != NULL)
				{
					lit->highlighted = 0;
					render_line(sl_lit, lit, NULL, sl_lit->render);
					update_feedback(sl_lit);
					lit = NULL;
					refreshreq = 1;
				}
				
				if(button == SDL_BUTTON_LEFT && grabbed != NULL)
				{
					if(focus == NULL)
					{
						if(grid_mode && (grabbed->selected == 0))
						{
							// Snap to slide grid:
							int extra;

							extra = grabbed->x % SLIDE_GRID_STEP;
							grabbed->x -= extra;
							if(extra >= SLIDE_GRID_STEP / 2)
								grabbed->x += SLIDE_GRID_STEP;

							extra = grabbed->y % SLIDE_GRID_STEP;
							grabbed->y -= extra;
							if(extra >= SLIDE_GRID_STEP / 2)
								grabbed->y += SLIDE_GRID_STEP;
						}

						slides_moved = 1;
						updatereq = 1;
					}
					else
					{
						if(grid_mode)
						{
							// Snap to image grid:
							int extra;

							extra = focus->des_x % IMAGE_GRID_STEP;
							focus->des_x -= extra;
							if(extra >= IMAGE_GRID_STEP / 2)
								focus->des_x += IMAGE_GRID_STEP;
							focus->scr_x = to_screen_coords(focus->des_x);

							extra = focus->des_y % IMAGE_GRID_STEP;
							focus->des_y -= extra;
							if(extra >= IMAGE_GRID_STEP / 2)
								focus->des_y += IMAGE_GRID_STEP;
							focus->scr_y = to_screen_coords(focus->des_y);
						}

						// Finalise sub-image position:
						render_slide(grabbed);
						updatereq = 1;
					}
					grabbed = NULL;
					focus = NULL;
				}
				else if(button == SDL_BUTTON_RIGHT && zoom_level == 1)
				{
					change_zoom_level(0);
					updatereq = 0;
					scrollreqx = scrollreqy = 0;
					fix_position(&prefx, &prefy);
					refreshreq = 1;
				}
				else if(drag_dist < CLICK_THRESHOLD && button == SDL_BUTTON_LEFT
					&& zoom_level < 2)
				{
					int line_num;
					slide *sl_local, *sl_remote;
					subimage *hyper_img;

					// sl_local = slide_under_mouse(mouse_x, mouse_y, &line_num, NULL);
					sl_local = slide_under_click(clickx, clicky, &line_num,
							&hyper_img);
					if(sl_local != NULL)
					{
						int link_card = 0;

						sl_remote = NULL;						
						if(hyper_img != NULL && hyper_img->hyperlink != NULL)
						{
							// Set link_card and sl_remote:
							sl_remote = find_title(talk, hyper_img->hyperlink,
									&link_card);
						}
						if(sl_remote == NULL)
						{
							displayline *hyper_line;
							
							sl_remote = hyperlink_request(sl_local, line_num,
									&hyper_line);
							if(sl_remote != NULL)
								link_card = hyper_line->link_card;
						}						
						if(sl_remote != NULL) // Hyperlink under mouse
						{
							if(link_card != 0)
								goto_card(sl_remote, link_card);
							restorex = viewx + zoom_offset(zoom_level) * SCREEN_WIDTH;
							restorey = viewy + zoom_offset(zoom_level) * SCREEN_HEIGHT;
							warp_to((sl_remote->x + sl_remote->scr_w / 2) -
									SCREEN_WIDTH / 2 -
									zoom_offset(zoom_level) * SCREEN_WIDTH,
									(sl_remote->y + sl_remote->scr_h / 2) -
									SCREEN_HEIGHT / 2 -
									zoom_offset(zoom_level) * SCREEN_HEIGHT);
							fix_position(&prefx, &prefy);
							scrollreqx = scrollreqy = 0;
							refreshreq = 1;
						}
						else if(line_num == 0 && sl_local->deck_size > 1)
						{
							// Titlebar of slide stack under mouse
							next_card(sl_local);
							recentre(&prefx, &prefy);
							snap_to(prefx, prefy);
							refreshreq = 1;
						}
						else if(sl_local->image_file == NULL && line_num > 0)
							fold_request(sl_local, line_num);
					}
				}
				button_release(&scrollreqx, &scrollreqy, &prefx, &prefy,
						(modkeys & KMOD_ALT) ? 0 : 1);
				break;
			case SDL_QUIT:
				delete selected;
				unselect_all();
				return 1;
				break;			
		}
	}
}

void splash_screen()
{
	SDL_Rect dst;

	// Clear screen to background colour:
	dst.x = dst.y = 0;
	dst.w = SCREEN_WIDTH;
	dst.h = SCREEN_HEIGHT;
	SDL_FillRect(screen, &dst, colour->grey_fill);	
	
	SDL_Flip(screen);
}

void clear_radar()
{
	SDL_Rect dst;

	// Clear radar to background colour:
	dst.x = dst.y = 0;
	dst.w = RADAR_WIDTH;
	dst.h = RADAR_HEIGHT;
	SDL_FillRect(radar, &dst, colour->grey_fill);	
}

void cls()
{
	clear_surface(screen, colour->fills->item(canvas_colour));
}

void measure_all()
{
	slide *sl;
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		measure_slide(sl);
	}
}

void snap_to(int prefx, int prefy)
{
	if(prefx == viewx && prefy == viewy)
		return;
	viewx = prefx;
	viewy = prefy;
}

void recentre(int *prefx, int *prefy)
{
	int centre_x, centre_y;
	
	*prefx = viewx;
	*prefy = viewy;
	slide *sl = central_slide();
	if(sl == NULL)
		return;
	if(sl->scr_w < SCREEN_WIDTH)
	{
		centre_x = sl->x + sl->scr_w / 2;
		*prefx = centre_x - SCREEN_WIDTH / 2;
	}
	if(sl->scr_h < SCREEN_HEIGHT)
	{
		centre_y = sl->y + sl->scr_h / 2;
		*prefy = centre_y - SCREEN_HEIGHT / 2;
	}
}

slide *slide_under_pointer()
{
	int x, y;
	slide *sl;
	
	SDL_GetMouseState(&x, &y);
	sl = slide_under_mouse(x, y);
	return sl;
}

slide *slide_under_click(int clickx, int clicky, int *line_num,
		subimage **image)
{
	int mousex = (clickx - viewx) / zoom_factor(zoom_level);
	int mousey = (clicky - viewy) / zoom_factor(zoom_level);
	return slide_under_mouse(mousex, mousey, line_num, image);
}

slide *slide_under_mouse(int mousex, int mousey, int *line_num,
		subimage **image)
{
	// Sets line_num to 0 for slide title, 1+ for lines of content, -1 for bg
	slide *sl;
	style *st;
	subimage *img;
	int x = viewx + zoom_factor(zoom_level) * mousex;
	int y = viewy + zoom_factor(zoom_level) * mousey;
	int rx, ry;
	int dx, dy;

	if(image != NULL)
		*image = NULL;	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		st = sl->st;
		rx = x - sl->x;
		ry = y - sl->y;
		if(rx > 0 && ry > 0 && rx < sl->scr_w && ry < sl->scr_h)
		{
			// This is the slide clicked on
			if(image != NULL)
			{
				for(int j = 0; j < sl->visible_images->count(); j++)
				{
					img = sl->visible_images->item(j);
					dx = rx - img->scr_x;
					dy = ry - img->scr_y;
					if(dx >= 0 && dy >= 0 && dx < img->scr_w && dy < img->scr_h)
					{
						// This sub-image has been clicked on
						*image = img;
						break;
					}
				}
			}
			if(line_num != NULL)
			{
				if(st->enablebar && ry < to_screen_coords(st->titlespacing))
					*line_num = 0;
				else if(sl->image_file != NULL)
					*line_num = -1;
				else
				{
					displayline *out;
					
					*line_num = -1;
					for(int i = 0; i < sl->repr->count(); i++)
					{
						out = sl->repr->item(i);
						if(ry >= to_screen_coords(out->y) &&
								ry < to_screen_coords(out->y + out->height))
						{
							*line_num = i + 1;
							break;
						}
					}
				}
			}
			return sl;
		}
	}
	return NULL;
}

slide *central_slide()
{
	slide *sl;
	int x = viewx + SCREEN_WIDTH / 2;
	int y = viewy + SCREEN_HEIGHT / 2;
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(sl->x < x && sl->y < y &&
				sl->x + sl->scr_w > x && sl->y + sl->scr_h > y)
		{
			return sl;
		}
	}
	return NULL;
}

void load_image(slide *sl)
{
	SDL_Surface *image;
	int w, h;
	SDL_Rect dst;
	style *st = sl->st;
	
	image = load_png(sl->image_file, 0);
	w = image->w;
	h = image->h;
	sl->render = alloc_surface(w + 2 * st->picturemargin,
			h + 2 * st->picturemargin);
	dst.x = 0;
	dst.y = 0;
	dst.w = sl->render->w;
	dst.h = sl->render->h;
	SDL_FillRect(sl->render, &dst, colour->white_fill);
	
	dst.x = st->picturemargin;
	dst.y = st->picturemargin;
	dst.w = w;
	dst.h = h;
	SDL_BlitSurface(image, NULL, sl->render, &dst);
	
	SDL_FreeSurface(image);
	
	for(int i = 0; i < st->pictureborder; i++)
	{
		rectangleColor(sl->render, i, i, w + 2 * st->picturemargin - 1 - i,
				h + 2 * st->picturemargin - 1 - i,
				colour->pens->item(st->bordercolour));
	}
	
	if(scalep == 1)
		sl->scaled = sl->render;
	else
	{
		if(sl->scaled != NULL)
			error("Paranoia: scaled surface not freed up");
		double g = (double)scalep / (double)scaleq;
		sl->scaled = zoomSurface(sl->render, g, g, 1);
		if(sl->scaled == NULL)
			error("zoomSurface returned NULL");
	}

	double f = 1.0 / 3.0;	
	// sl->mini = SDL_ResizeFactor(sl->scaled, (float)f, 1);
	sl->mini = zoomSurface(sl->scaled, f, f, 1);
	if(sl->mini == NULL)
		error("zoomSurface returned NULL");
	
	sl->micro = zoomSurface(sl->mini, f, f, 1);
	if(sl->micro == NULL)
		error("zoomSurface returned NULL");
}

void load_images()
{
	slide *sl;	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(sl->image_file != NULL)
			load_image(sl);
	}
}

void render_all()
{
	slide *sl;
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		render_slide(sl);
	}
}

void free_talk(slidevector *talk)
{
	slide *sl;
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		
		// Free rendered surfaces:
		if(sl->render != NULL)
			SDL_FreeSurface(sl->render);
		if(scalep != 1 && sl->scaled != NULL)
			SDL_FreeSurface(sl->scaled);
		if(sl->mini != NULL)
			SDL_FreeSurface(sl->mini);
		if(sl->micro != NULL)
			SDL_FreeSurface(sl->micro);
		
		// Free linearised representation:
		if(sl->repr != NULL)
		{
			displayline *out;

			for(int i = 0; i < sl->repr->count(); i++)
			{
				out = sl->repr->item(i);
				delete out;
			}
			delete sl->repr;
		}
		
		// Free content:
		free_node(sl->content);
		
		// Free images:
		delete sl->embedded_images;
		delete sl->visible_images;
		// TODO: Should delete the actual subimage surfaces - XXX
		if(sl->image_file != NULL)
			delete[] sl->image_file;
		
		// Free slide structure:
		delete sl;
	}
	
	delete talk;
}

void set_view_coords(slidevector *talk)
{
	slide *sl;
	
	if(viewx != UNKNOWN_POS || viewy != UNKNOWN_POS)
		return;
	if(talk->count() == 0)
		error("No slides in talk");
	sl = talk->item(0);
	viewx = sl->x + sl->scr_w / 2 - SCREEN_WIDTH / 2;
	viewy = sl->y + sl->scr_h / 2 - SCREEN_HEIGHT / 2;
}

slidevector *create_render_list(slidevector *talk)
{
	slidevector *render_list;
	slide *sl;
	
	render_list = new slidevector();
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		render_list->add(sl);
	}
	return render_list;
}

void free_render_list(slidevector *render_list)
{
	delete render_list;
}

int main(int argc, char **argv)
{
	int quit;
	const char *talk_ref;
	linefile *talk_lf;
	
	talk_ref = parse_args(argc, argv);
	config = init_paths(talk_ref);
	options = init_options(config);
	talk_lf = open_talk(config->talk_path);
	peek_designsize(talk_lf);
	set_resolution();
	init_gui(config->caption, export_html);
	if(!export_html)
		splash_screen();
	proc_stat_buf = new char[PROC_STAT_BUF_LEN];
	while(1)
	{
		if(export_html) printf("Loading styles...\n");
		style_list = load_styles(config);
		if(export_html) printf("Parsing talk...\n");
		talk = parse_talk(talk_lf, style_list);
		
		if(export_html) printf("Loading images...\n");
		load_images();
		if(export_html) printf("Measuring slides...\n");
		flatten_all(talk);
		load_slide_positions(config, talk);
		measure_all();
		render_list = create_render_list(talk);
		render_all();
		set_view_coords(talk);
		if(export_html)
		{
			gen_html(talk);
			break;
		}
		refresh();
		quit = mainloop();
		if(slides_moved)
		{
			save_slide_positions(config, talk);
			slides_moved = 0;
		}
		if(quit)
			break;
		
		// Reset:
		delete talk_lf;
		talk_lf = open_talk(config->talk_path);
		
		free_render_list(render_list);
		free_talk(talk);
	}
	return 0;
}

void error(const char *format, ...)
{
	static const int MAX_ERR_LEN = 200;
	va_list args;
	char *c = new char[MAX_ERR_LEN];

	va_start(args, format);
	vsnprintf(c, MAX_ERR_LEN, format, args);
	va_end(args);
	c[MAX_ERR_LEN - 1] = '\0';
	fprintf(stderr, "%s\n", c);
	
	exit(-1);
}
