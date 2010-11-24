/* render.cpp - DMI - 25-11-2006

Copyright (C) 2006 David Ingram

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

const int TITLE_EDGE = 8;

// Prototypes:
void draw_subimage(SDL_Surface *target, subimage *img);
void load_subimage(subimage *img);
svector *split_string(const char *line, svector **codes);
void render_decorations(slide *sl);

void foldicon(int folded, int exposure_level, int x,
		int top, int height, int y,
		SDL_Surface *surface, style *st, int highlighted)
{
	// x is a left edge, y is a centre
	SDL_Surface *icon;
	int colour_index;
	
	if(folded)
		icon = st->collapsedicon;
	else
		icon = st->expandedicon;
	
	colour_index = folded ? st->foldcollapsedcolour : st->foldexpandedcolour;
	if(highlighted)
		colour_index = st->highlightcolour;
	
	if(!folded)
	{
		Uint32 pen;

		exposure_level++;
		if(exposure_level == 1)
			pen = colour->pens->item(st->foldexposed1colour);
		else if(exposure_level == 2)
			pen = colour->pens->item(st->foldexposed2colour);
		else
			pen = colour->pens->item(st->foldexposed3colour);
		/* boxColor(surface, x + 7, y, x + 13,
				y + (st->text_ascent - st->text_descent), pen); */
		boxColor(surface, x + 7, y, x + 13, top + height - 1, pen);
	}
	if(icon == NULL)
	{
		boxColor(surface, x, y - 10, x + 20, y + 10,
				colour->pens->item(colour_index));
		rectangleColor(surface, x, y - 10, x + 20, y + 10, colour->black_pen);
		filledTrigonColor(surface, x + 3, y - 6, x + 17, y - 6,
				x + 10, y + 6, colour->white_pen);
		aatrigonColor(surface, x + 3, y - 6, x + 17, y - 6,
				x + 10, y + 6, colour->black_pen);
	}
	else
	{
		SDL_Rect dst;
		dst.x = x + 10 - (icon->w / 2);
		dst.y = y - (icon->h / 2);
		SDL_BlitSurface(icon, NULL, surface, &dst);
	}
}

void exposeicon(int level, int x, int y, int height,
		SDL_Surface *surface, style *st)
{
	Uint32 pen;
	
	if(level == 1)
		pen = colour->pens->item(st->foldexposed1colour);
	else if(level == 2)
		pen = colour->pens->item(st->foldexposed2colour);
	else
		pen = colour->pens->item(st->foldexposed3colour);
	boxColor(surface, x + 7, y, x + 13, y + height - 1, pen);
}

void bigbullet(int x, int y, SDL_Surface *surface, style *st)
{
	if(st->bullet1 != NULL)
	{
		SDL_Rect dst;
		dst.x = x - (st->bullet1->w / 2);
		dst.y = y - (st->bullet1->h / 2);
		SDL_BlitSurface(st->bullet1, NULL, surface, &dst);
	}
	else
	{
		Uint32 pen = colour->pens->item(st->bullet1colour);
		filledCircleColor(surface, x, y, st->bullet1size, colour->light_grey_pen);
		filledCircleColor(surface, x, y, st->bullet1size - 1, pen);
	}
}

void mediumbullet(int x, int y, SDL_Surface *surface, style *st)
{
	if(st->bullet2 != NULL)
	{
		SDL_Rect dst;
		dst.x = x - (st->bullet2->w / 2);
		dst.y = y - (st->bullet2->h / 2);
		SDL_BlitSurface(st->bullet2, NULL, surface, &dst);
	}
	else
	{
		Uint32 pen = colour->pens->item(st->bullet2colour);
		filledCircleColor(surface, x, y, st->bullet2size, colour->light_grey_pen);
		filledCircleColor(surface, x, y, st->bullet2size - 1, pen);
	}
}

void smallbullet(int x, int y, SDL_Surface *surface, style *st)
{	
	if(st->bullet3 != NULL)
	{
		SDL_Rect dst;
		dst.x = x - (st->bullet3->w / 2);
		dst.y = y - (st->bullet3->h / 2);
		SDL_BlitSurface(st->bullet3, NULL, surface, &dst);
	}
	else
	{
		Uint32 pen = colour->pens->item(st->bullet3colour);
		filledEllipseColor(surface, x, y, st->bullet3size,
				(st->bullet3size * 2) / 3, colour->light_grey_pen);
		filledEllipseColor(surface, x, y, st->bullet3size - 1,
				(st->bullet3size * 2) / 3 - 1, pen);
	}
}

void bullet(int level, int x, int y, SDL_Surface *surface, style *st)
{
	switch(level)
	{
		case 1: bigbullet(x, y, surface, st); break;
		case 2: mediumbullet(x, y, surface, st); break;
		case 3: smallbullet(x, y, surface, st); break;
		default: error("Impossible: unknown bullet level");
	}
}

svector *split_string(const char *line, svector **codes_out)
{
	svector *v = NULL;
	svector *codes;
	char code[2], colour[40];
	const char *start = line;
	const char *pos = line;
	const char *arg;
	int literal = 0;

	code[1] = '\0';
	v = new svector();
	codes = new svector();	
	while(*pos != '\0')
	{
		if(literal == 1)
		{
			literal = 0;
		}
		else if(*pos == '\\')
		{
			v->add(start, pos - start);
			code[0] = '\\';
			codes->add(code);
			start = pos + 1;
			literal = 1;
		}
		else if(*pos == '%')
		{
			arg = pos + 1;
			while(*arg != '.')
			{
				if(*arg == '\0')
					error("Unterminated %% expression in line <%s>", line);
				arg++;
			}
			int len = arg - pos; // Includes %, excludes "."
			if(len + 1 > 40)
				error("%% expression too long on line <%s>", line);
			strncpy(colour, pos, len);
			colour[len] = '\0';
			
			v->add(start, pos - start);
			codes->add(colour);
			start = arg + 1;
		}
		else if(*pos == '$' || *pos == '*' || *pos == '/')
		{
			v->add(start, pos - start);
			code[0] = *pos;
			codes->add(code);
			start = pos + 1;
		}
		pos++;
	}
	// Finish last string:
	v->add(start, pos - start);
	code[0] = '\n';
	codes->add(code);
	
	*codes_out = codes;
	return v;
}

int max_embedded_height(slide *sl)
{
	subimage *img;
	int height = 0;
	
	for(int i = 0; i < sl->visible_images->count(); i++)
	{
		img = sl->visible_images->item(i);
		if(img->surface == NULL)
			load_subimage(img);
		if(img->des_y + img->des_h > height)
			height = img->des_y + img->des_h;
	}	
	return height;
}

int max_embedded_width(slide *sl)
{
	subimage *img;
	int width = 0;
	
	for(int i = 0; i < sl->visible_images->count(); i++)
	{
		img = sl->visible_images->item(i);
		if(img->surface == NULL)
			load_subimage(img);
		if(img->des_x + img->des_w > width)
			width = img->des_x + img->des_w;
	}
	return width;
}

int calc_height(slide *sl)
{
	displaylinevector *repr = sl->repr;
	displayline *out;
	int height;
	style *st = sl->st;
	
	height = st->linespacing / 2;
	if(st->enablebar)
		height += st->titlespacing;
	for(int i = 0; i < repr->count(); i++)
	{
		out = repr->item(i);
		out->y = height;
		height += out->height;
	}

	// Check embedded images also fit:
	int h_img = max_embedded_height(sl);
	if(h_img > height)
		height = h_img;
	
	height += st->bottommargin;
	return height;
}

int calc_width(slide *sl)
{
	style *st = sl->st;
	displaylinevector *repr = sl->repr;
	displayline *out;
	
	int w = 0, wpart, h, max_width;
	
	int ttmode = 0, boldmode = 0, italicmode = 0;
	int lastshift = 0;
	
	if(st->enablebar)
	{
		char *title = sl->content->line;
		TTF_SizeUTF8(st->title_font, title, &w, &h);
	}
	max_width = w;
	
	for(int i = 0; i < repr->count(); i++)
	{
		int shift;
		TTF_Font *font;
		
		out = repr->item(i);
		shift = 0;
		if(out->bullet == -1)
		{
			shift = lastshift;
		}
		else if(out->bullet > 0)
		{
			shift = 10 + 30 * out->bullet;
		}
		if(out->bullet == 0 && out->prespace > 0)
		{
			shift += out->prespace *
					(ttmode ? st->fixed_space_width : st->text_space_width);
		}
		if(out->import != NULL)
		{
			w = out->import->w + shift;
			out->width = w;
			if(w > max_width)
				max_width = w;
			lastshift = shift;
		}
		else if(out->line == NULL)
			error("Impossible out->line in render.cpp");
		else if(out->heading && strlen(out->line) > 0)
		{
			TTF_SizeUTF8(st->title_font, out->line, &w, &h);
			w += shift;
			out->width = w;
			if(w > max_width)
				max_width = w;
			lastshift = shift;
		}
		else if(strlen(out->line) > 0)
		{
			svector *codes;
			char code;
			svector *v = split_string(out->line, &codes);
			const char *s;
			
			w = 0;
			for(int j = 0; j < v->count(); j++)
			{
				s = v->item(j);
				code = codes->item(j)[0];
				if(strlen(s) > 0)
				{
					if(italicmode)
						font = st->italic_font;
					else if(boldmode)
						font = st->bold_font;
					else if(ttmode)
						font = st->fixed_font;
					else
						font = st->text_font;
					TTF_SizeUTF8(font, s, &wpart, &h);
					w += wpart;
				}
				if(code == '$')
					ttmode = 1 - ttmode;
				else if(code == '*')
				{
					if(ttmode == 0)
						boldmode = 1 - boldmode;
					else
					{
						TTF_SizeUTF8(st->fixed_font, "*", &wpart, &h);
						w += wpart;
					}
				}
				else if(code == '/')
				{
					if(ttmode == 0)
						italicmode = 1 - italicmode;
					else
					{
						TTF_SizeUTF8(st->fixed_font, "/", &wpart, &h);
						w += wpart;
					}
				}
			}
			delete v;
			delete codes;
			
			w += shift;
			out->width = w;
			if(w > max_width)
				max_width = w;
			lastshift = shift;
		}
		else
		{
			out->width = 0;
		}
	}
	max_width += st->leftmargin + st->foldmargin;
	
	// Check embedded images also fit:
	int w_img = max_embedded_width(sl);
	if(w_img > max_width)
		max_width = w_img;
	
	max_width += st->rightmargin;
	return max_width;
}

void measure_slide(slide *sl)
{
	if(sl->image_file != NULL)
	{
		sl->des_w = sl->render->w;
		sl->des_h = sl->render->h;
	}
	else
	{
		sl->des_w = calc_width(sl);
		sl->des_h = calc_height(sl);
	}
	sl->scr_w = to_screen_coords(sl->des_w);
	sl->scr_h = to_screen_coords(sl->des_h);
}

void render_background(slide *sl, SDL_Surface *surface)
{
	SDL_Rect dst;
	style *st = sl->st;
	
	// Set slide area to the background colour:
	dst.x = 0;
	dst.y = 0;
	dst.w = sl->des_w;
	dst.h = sl->des_h;
	SDL_FillRect(surface, &dst, colour->fills->item(st->bgcolour));

	// Fill in the	titlebar area:
	if(st->enablebar)
	{
		dst.h = st->titlespacing - TITLE_EDGE;
		SDL_FillRect(surface, &dst, colour->fills->item(st->barcolour));
	}

	// Draw background image or texture:
	if(st->background != NULL)
	{
		int src_w, src_h, dst_w, dst_h;
		SDL_Surface *scaled;
		SDL_Rect dst;
		
		// Scale background image to fit this slide:
		src_w = st->background->w;
		src_h = st->background->h;
		dst_w = sl->des_w;
		if(st->bgbar || !st->enablebar)
		{
			dst_h = sl->des_h;
			dst.y = 0;
		}
		else
		{
			dst_h = sl->des_h - (st->titlespacing - TITLE_EDGE);
			dst.y = st->titlespacing - TITLE_EDGE;
		}
		scaled = zoomSurface(st->background, (double)dst_w / (double)src_w,
				(double)dst_h / (double)src_h, 1);
		
		// Blit:
		dst.x = 0;
		SDL_BlitSurface(scaled, NULL, surface, &dst);
	
		SDL_FreeSurface(scaled);
	}
	else if(st->texture != NULL)
	{
		SDL_Rect dst;
		int w, h;
		int xrepeats, yrepeats;
		int vert_offset;
		
		w = st->texture->w;
		h = st->texture->h;
		if(st->bgbar || !st->enablebar)
			vert_offset = 0;
		else
			vert_offset = st->titlespacing - TITLE_EDGE;
		xrepeats = SCREEN_WIDTH / w;
		if(SCREEN_WIDTH % w > 0)
			xrepeats++;
		yrepeats = (SCREEN_HEIGHT - vert_offset) / h;
		if((SCREEN_HEIGHT - vert_offset) % h > 0)
			yrepeats++;
		for(int x = 0; x < xrepeats; x++)
		{
			for(int y = 0; y < yrepeats; y++)
			{
				dst.x = x * w;
				dst.y = vert_offset + y * h;
				SDL_BlitSurface(st->texture, NULL, surface, &dst);
			}
		}
	}
	
	// Draw the slide border:
	for(int i = 0; i < st->slideborder; i++)
	{
		rectangleColor(surface, i, i, sl->des_w - 1 - i, sl->des_h - 1 - i,
				colour->pens->item(st->bordercolour));
	}
	
	// Draw dividing line between titlebar and slide body, and the slide title:
	if(st->enablebar)
	{
		char *title = sl->content->line;
		int xbase = st->leftmargin + st->foldmargin;
		
		for(int i = 0; i < st->barborder; i++)
		{
			hlineColor(surface, 1, sl->des_w - 2,
					st->titlespacing - TITLE_EDGE - 1 + i,
					colour->pens->item(st->bordercolour));
		}

		render_text(title, st->title_font, colour->inks->item(st->titlecolour),
				surface, xbase, (st->titlespacing - TITLE_EDGE -
				(st->title_ascent - st->title_descent)) / 2 - 1 + st->topmargin);
	}
}

void render_line(slide *sl, displayline *out, DrawMode *dm,
		SDL_Surface *surface)
{
	style *st = sl->st;
	int single_line;
	
	int textshift, x, shift;
	int xmargin = st->leftmargin;
	int xbase = st->leftmargin + st->foldmargin;
	TTF_Font *font;

	if(dm != NULL)
	{
		// Store initial draw mode to make future individual line redraws easy:
		out->initial_dm = *dm;
		single_line = 0;
	}
	else
	{
		// Just drawing this line (not whole slide), so use saved draw mode:
		dm = new DrawMode;
		*dm = out->initial_dm;
		single_line = 1;
	}
	
	shift = 0;
	if(out->bullet == -1)
	{
		shift = dm->lastshift;
	}
	else if(out->bullet > 0)
	{
		shift = 10 + 30 * out->bullet;
		bullet(out->bullet, xbase + shift - 20,
				out->y + (st->text_ascent - st->text_descent) / 2,
				surface, st);
	}
	if(out->centred)
	{
		int text_area = sl->des_w - st->leftmargin - st->foldmargin -
				st->rightmargin;
		shift = (sl->des_w - out->width) / 2 - st->leftmargin - st->foldmargin;

		/* Force long lines into the text area, even if that means
			they end up off-centre within the slide itself due to
			different left/fold and right margins: */
		if(shift + out->width > text_area) // Too far right
			shift = text_area - out->width;
		if(shift < 0) // Too far left
			shift = 0;
	}
	
	if(out->exposed > 0)
		exposeicon(out->exposed, xmargin, out->y, out->height, surface, st);
	if(out->type == TYPE_FOLDED)
	{
		textshift = 0;
		if(dm->ttmode)
			textshift += (st->text_ascent - st->fixed_ascent) / 2;
		else if(dm->italicmode)
			textshift += (st->text_ascent - st->italic_ascent) / 2;
		else if(dm->boldmode)
			textshift += (st->text_ascent - st->bold_ascent) / 2;
		foldicon(1, out->exposed, xmargin, out->y, out->height,
				out->y + (st->text_ascent - st->text_descent) / 2 + textshift,
				surface, st, out->highlighted);
	}
	else if(out->type == TYPE_EXPANDED)
	{
		textshift = 0;
		if(dm->ttmode)
			textshift += (st->text_ascent - st->fixed_ascent) / 2;
		else if(dm->italicmode)
			textshift += (st->text_ascent - st->italic_ascent) / 2;
		else if(dm->boldmode)
			textshift += (st->text_ascent - st->bold_ascent) / 2;
		foldicon(0, out->exposed, xmargin, out->y, out->height,
				out->y + (st->text_ascent - st->text_descent) / 2 + textshift,
				surface, st, out->highlighted);
	}
	
	x = xbase + shift;
	if(out->bullet == 0 && out->prespace > 0)
	{
		x += out->prespace * (dm->ttmode ? st->fixed_space_width :
				st->text_space_width);
	}

	if(out->import != NULL)
	{
		SDL_Rect dst;

		dst.x = x;
		dst.y = out->y + st->latexspaceabove;
		SDL_BlitSurface(out->import, NULL, surface, &dst);
	}
	else if(out->rule)
	{
		int width;
		SDL_Rect dst;
		
		width = sl->des_w * st->rulewidth / 100;
		
		dst.x = (sl->des_w - width) / 2;
		dst.y = out->y + st->rulespaceabove;
		dst.w = width;
		dst.h = st->ruleheight;
		SDL_FillRect(surface, &dst, colour->fills->item(st->rulecolour));
	}
	else if(out->line == NULL)
		error("Impossible out->line in render.cpp");
	else if(out->heading && strlen(out->line) > 0)
	{
		x = render_text(out->line, st->heading_font,
				st->headingcolour, surface,
				x, out->y + st->headspaceabove, 0);
	}
	else if(strlen(out->line) > 0)
	{
		svector *codes;
		char code;
		svector *v = split_string(out->line, &codes);
		const char *s;
		int link_text_colour_index;

		if(out->highlighted)
			link_text_colour_index = st->highlightcolour;
		else
			link_text_colour_index = st->linkcolour;

		for(int j = 0; j < v->count(); j++)
		{
			s = v->item(j);
			code = codes->item(j)[0];
			if(strlen(s) > 0)
			{
				textshift = 0;
				if(dm->italicmode)
				{
					font = st->italic_font;
					textshift += st->text_ascent - st->italic_ascent;
				}
				else if(dm->boldmode)
				{
					font = st->bold_font;
					textshift += st->text_ascent - st->bold_ascent;
				}
				else if(dm->ttmode)
				{
					font = st->fixed_font;
					textshift += st->text_ascent - st->fixed_ascent;
				}
				else
				{
					font = st->text_font;
				}
				x = render_text(s, font,
						out->link == NULL ? dm->current_text_colour_index :
						link_text_colour_index, surface, x,
						out->y + textshift,
						out->link == NULL || !st->underlinelinks ? 0 :
						st->textsize);
			}
			if(code == '$')
				dm->ttmode = 1 - dm->ttmode;
			else if(code == '%')
			{
				if(codes->item(j)[1] == '\0')
				{
					dm->current_text_colour_index = st->textcolour; // Default
				}
				else
				{
					int colour_index;
					
					if(codes->item(j)[1] == '#')
					{
						colour_index = colour->search_add(codes->item(j) + 2);
						if(colour_index == -1)
							error("Incorrect hex colour %s", codes->item(j) + 1);
					}
					else
					{
						colour_index = colour->names->find(codes->item(j) + 1);
						if(colour_index == -1)
							error("Unknown colour <%s>", codes->item(j) + 1);
					}
					dm->current_text_colour_index = colour_index;
				}
			}
			else if(code == '*')
			{
				if(dm->ttmode == 0)
					dm->boldmode = 1 - dm->boldmode;
				else
					x = render_text("*", st->fixed_font,
						out->link == NULL ? dm->current_text_colour_index :
						link_text_colour_index, surface, x,
						out->y + st->text_ascent - st->fixed_ascent,
						out->link == NULL || !st->underlinelinks ? 0 :
						st->textsize);
			}
			else if(code == '/')
			{
				if(dm->ttmode == 0)
					dm->italicmode = 1 - dm->italicmode;
				else
					x = render_text("/", st->fixed_font,
						out->link == NULL ? dm->current_text_colour_index :
						link_text_colour_index, surface, x,
						out->y + st->text_ascent - st->fixed_ascent,
						out->link == NULL || !st->underlinelinks ? 0 :
						st->textsize);
			}
		}
		delete v;
		delete codes;
	}

	if(debug & DEBUG_BASELINES)
	{		
		pixelColor(surface, xbase, out->y, colour->red_pen);
		pixelColor(surface, xbase - 1, out->y - 1, colour->red_pen);
		pixelColor(surface, xbase + 1, out->y + 1, colour->red_pen);
		pixelColor(surface, xbase - 1, out->y + 1, colour->red_pen);
		pixelColor(surface, xbase + 1, out->y - 1, colour->red_pen);
	}

	if(single_line)
		delete dm;
	else
		dm->lastshift = shift;
}

void draw_logos(slide *sl, style *st, SDL_Surface *surface)
{
	SDL_Rect dst;
	logo *lo;

	for(int i = 0; i < st->logos->count(); i++)
	{
		lo = st->logos->item(i);
		if(lo->x < 0)
			dst.x = sl->des_w + lo->x - lo->image->w;
		else
			dst.x = lo->x;
		if(lo->y < 0)
			dst.y = sl->des_h + lo->y - lo->image->h;
		else
			dst.y = lo->y;
		SDL_BlitSurface(lo->image, NULL, surface, &dst);
	}
}

void reallocate_surfaces(slide *sl)
{
	if(scalep != 1)
	{
		if(sl->scaled != NULL)
			SDL_FreeSurface(sl->scaled);
		sl->scaled = NULL; // Paranoia
	}	
	if(sl->mini != NULL)
	{
		SDL_FreeSurface(sl->mini);
		sl->mini = NULL; // Paranoia
	}
	if(sl->micro != NULL)
	{
		SDL_FreeSurface(sl->micro);
		sl->micro = NULL; // Paranoia
	}
	// Should we delete the mini decor here??? - XXX
}

void scale(slide *sl)
{
	double f;
	
	if(scalep == 1)
		sl->scaled = sl->render;
	else
	{
		if(sl->scaled != NULL)
			error("Paranoia: scaled surface not freed up");
		f = (double)scalep / (double)scaleq;
		sl->scaled = zoomSurface(sl->render, f, f, 1);
		if(sl->scaled == NULL)
			error("zoomSurface returned NULL");
	}
	
	// Create zoomed out version of everything we've just drawn:
	f = 1.0 / 3.0;
	if(sl->mini != NULL)
		error("Paranoia: mini surface not freed up");
	// sl->mini = SDL_ResizeFactor(sl->scaled, (float)f, 1);
	sl->mini = zoomSurface(sl->scaled, f, f, 1);
	if(sl->mini == NULL)
		error("zoomSurface returned NULL");
	
	if(sl->deck_size > 1)
	{
		if(sl->decor.top != NULL)
			sl->mini_decor.top = zoomSurface(sl->decor.top, f, f, 1);
		if(sl->decor.bottom != NULL)
			sl->mini_decor.bottom = zoomSurface(sl->decor.bottom, f, f, 1);
		if(sl->decor.left != NULL)
			sl->mini_decor.left = zoomSurface(sl->decor.left, f, f, 1);
		if(sl->decor.right != NULL)
			sl->mini_decor.right = zoomSurface(sl->decor.right, f, f, 1);
	}
	
	if(sl->micro != NULL)
		error("Paranoia: micro surface not freed up");
	sl->micro = zoomSurface(sl->mini, f, f, 1);
	if(sl->micro == NULL)
		error("zoomSurface returned NULL");
}

void render_slide(slide *sl)
{
	/* Returns a specially allocated surface only if the "except"
		subimage != NULL; otherwise uses the slide's built-in render surface. */
	
	style *st = sl->st;
	displaylinevector *repr = sl->repr;
	displayline *out;
	DrawMode *dm;
	SDL_Surface *surface;

	if(export_html == 1)
		printf("Rendering %s\n", sl->content->line);	
	if(sl->image_file != NULL)
		return;
	
	if(sl->render != NULL)
		SDL_FreeSurface(sl->render);
	sl->render = alloc_surface(sl->des_w, sl->des_h);
	
	reallocate_surfaces(sl);
	surface = sl->render;
		
	render_background(sl, surface);
	
	// Render the text lines:	
	dm = new DrawMode;
	dm->ttmode = dm->boldmode = dm->italicmode = 0;
	dm->current_text_colour_index = st->textcolour;
	dm->lastshift = 0;
	for(int i = 0; i < repr->count(); i++)
	{
		out = repr->item(i);
		render_line(sl, out, dm, surface);
	}
	delete dm;
	
	draw_logos(sl, st, surface);

	// Draw embedded images:
	subimage *img;
	for(int i = 0; i < sl->visible_images->count(); i++)
	{
		img = sl->visible_images->item(i);
		if(img->surface == NULL)
			load_subimage(img);
		draw_subimage(surface, img);
	}
	if(sl->deck_size > 1)
		render_decorations(sl);

	scale(sl);
}

void load_subimage(subimage *img)
{
	img->surface = load_png(img->path_name, 1);
	if(img->surface == NULL)
		error("Can't load image %s", img->path_name);
	img->des_w = img->surface->w;
	img->des_h = img->surface->h;
	img->scr_w = to_screen_coords(img->des_w);
	img->scr_h = to_screen_coords(img->des_h);
}

void draw_subimage(SDL_Surface *target, subimage *img)
{
	SDL_Rect dst;
	dst.x = img->des_x;
	dst.y = img->des_y;
	if(img->surface == NULL || target == NULL)
		error("Tried to render a NULL surface in draw_subimage");
	int ret = SDL_BlitSurface(img->surface, NULL, target, &dst);
	if(ret != 0)
		error("SDL_BlitSurface failed in draw_subimage()");
}

void free_decorations(slide *sl)
{
	if(sl->decor.top != NULL)
		SDL_FreeSurface(sl->decor.top);
	if(sl->decor.bottom != NULL)
		SDL_FreeSurface(sl->decor.bottom);
	if(sl->decor.left != NULL)
		SDL_FreeSurface(sl->decor.left);
	if(sl->decor.right != NULL)
		SDL_FreeSurface(sl->decor.right);
	if(sl->mini_decor.top != NULL)
		SDL_FreeSurface(sl->mini_decor.top);
	if(sl->mini_decor.bottom != NULL)
		SDL_FreeSurface(sl->mini_decor.bottom);
	if(sl->mini_decor.left != NULL)
		SDL_FreeSurface(sl->mini_decor.left);
	if(sl->mini_decor.right != NULL)
		SDL_FreeSurface(sl->mini_decor.right);
	
	sl->decor.top = sl->decor.bottom = NULL;
	sl->decor.left = sl->decor.right = NULL;
	sl->mini_decor.top = sl->mini_decor.bottom = NULL;
	sl->mini_decor.left = sl->mini_decor.right = NULL;
}

void render_decorations(slide *sl)
{
	int x1, y1;
	int below, above;
	SDL_Rect dst;
	style *st = sl->st;
	Uint32 border_pen = colour->pens->item(st->bordercolour);

	free_decorations(sl);
		
	below = sl->deck_size - sl->card;
	above = sl->card - 1;	
	
	if(below > 0)
	{
		sl->decor.top = alloc_surface(sl->scr_w, CARD_EDGE * below);
		sl->decor.right = alloc_surface(CARD_EDGE * below,
				sl->scr_h + CARD_EDGE * below);
		clear_surface(sl->decor.top, colour->grey_fill);
		clear_surface(sl->decor.right, colour->grey_fill);
		for(int i = 0; i < below; i++)
		{
			// Top edge:
			x1 = CARD_EDGE * (i + 1);
			y1 = CARD_EDGE * (below - i - 1);
			dst.x = x1;
			dst.y = y1;
			dst.w = sl->scr_w - x1;
			dst.h = CARD_EDGE;
			SDL_FillRect(sl->decor.top, &dst,
					st->enablebar ? colour->fills->item(st->barcolour)
					: colour->fills->item(st->bgcolour));
			for(int j = 0; j < st->slideborder; j++)
			{
				hlineColor(sl->decor.top, x1 + j, sl->scr_w - 1, y1 + j,
						border_pen);
				vlineColor(sl->decor.top, x1 + j, y1 + j, y1 + CARD_EDGE - 1,
						border_pen);
			}

			// Top-right corner:
			x1 = 0;
			y1 = CARD_EDGE * (below - i - 1);
			dst.x = x1;
			dst.y = y1;
			dst.w = CARD_EDGE * i;
			dst.h = CARD_EDGE;
			SDL_FillRect(sl->decor.right, &dst,
					st->enablebar ? colour->fills->item(st->barcolour)
					: colour->fills->item(st->bgcolour));
			for(int j = 0; j < st->slideborder; j++)
			{
				hlineColor(sl->decor.right, 0, CARD_EDGE * (i + 1) - 1,
						y1 + j, border_pen);
			}
			
			// Right edge:
			x1 = CARD_EDGE * i;
			y1 = CARD_EDGE * (below - i - 1);
			dst.x = x1;
			dst.y = y1 + st->slideborder;
			dst.w = CARD_EDGE;
			dst.h = sl->scr_h - st->slideborder;
			SDL_FillRect(sl->decor.right, &dst, colour->fills->item(st->bgcolour));
			for(int j = 0; j < st->slideborder; j++)
			{
				vlineColor(sl->decor.right, x1 + CARD_EDGE - 1 - j,
						y1, y1 + sl->scr_h - 1, border_pen);
				hlineColor(sl->decor.right, x1, x1 + CARD_EDGE - 1,
						y1 + sl->scr_h - 1 - j, border_pen);
			}
			
			// Titlebar area:
			if(st->enablebar)
			{
				x1 = CARD_EDGE * i;
				y1 = CARD_EDGE * (below - i - 1) + st->slideborder;
				dst.x = x1;
				dst.y = y1;
				dst.w = CARD_EDGE - st->slideborder;
				dst.h = to_screen_coords(st->titlespacing - TITLE_EDGE) -
						st->slideborder;
				SDL_FillRect(sl->decor.right, &dst,
						colour->fills->item(st->barcolour));
			}
			
			// Dividing line below bar:
			x1 = CARD_EDGE * i;
			y1 = CARD_EDGE * (below - i - 1);
			if(st->enablebar)
			{
				for(int j = 0; j < st->barborder; j++)
				{
					hlineColor(sl->decor.right, x1, x1 + CARD_EDGE - 1,
							y1 +
							to_screen_coords(st->titlespacing - TITLE_EDGE - 1) + j,
							colour->pens->item(st->bordercolour));
				}
			}
		}
	}
	if(above > 0)
	{
		sl->decor.left = alloc_surface(CARD_EDGE * above,
				sl->scr_h + CARD_EDGE * above);
		sl->decor.bottom = alloc_surface(sl->scr_w, CARD_EDGE * above);
		clear_surface(sl->decor.left, colour->grey_fill);
		clear_surface(sl->decor.bottom, colour->grey_fill);
		for(int i = 0; i < above; i++)
		{
			// Bottom edge:
			x1 = 0;
			y1 = CARD_EDGE * i;
			dst.x = x1;
			dst.y = y1;
			dst.w = sl->scr_w - CARD_EDGE * (i + 1);
			dst.h = CARD_EDGE;
			SDL_FillRect(sl->decor.bottom, &dst, colour->light_grey_fill);
			for(int j = 0; j < st->slideborder; j++)
			{
				hlineColor(sl->decor.bottom, 0,
						sl->scr_w - CARD_EDGE * (i + 1) - 1,
						y1 + CARD_EDGE - 1 - j, border_pen);
				vlineColor(sl->decor.bottom,
						sl->scr_w - CARD_EDGE * (i + 1) - 1 - j,
						y1, y1 + CARD_EDGE - 1, border_pen);
			}
			
			// Bottom-left corner:
			x1 = CARD_EDGE * (above - i - 1);
			y1 = sl->scr_h + CARD_EDGE * i;
			dst.x = x1;
			dst.y = y1;
			dst.w = (i + 1) * CARD_EDGE;
			dst.h = CARD_EDGE;
			SDL_FillRect(sl->decor.left, &dst, colour->light_grey_fill);
			
			// Left edge:
			x1 = CARD_EDGE * (above - i - 1);
			y1 = CARD_EDGE * (i + 1);
			dst.x = x1;
			dst.y = y1;
			dst.w = CARD_EDGE;
			dst.h = sl->scr_h;
			SDL_FillRect(sl->decor.left, &dst, colour->light_grey_fill);
			for(int j = 0; j < st->slideborder; j++)
			{
				hlineColor(sl->decor.left, x1, x1 + CARD_EDGE - 1,
						y1 + j, border_pen);
				hlineColor(sl->decor.left, x1, x1 + (i + 1) * CARD_EDGE - 1,
						y1 + sl->scr_h - 1 - j, border_pen);
				vlineColor(sl->decor.left, x1 + j, y1, y1 + sl->scr_h - 1,
						border_pen);
			}
		}
	}
}

void copy_decorations(slide *sl, int viewx, int viewy)
{
	SDL_Rect dst;
	int x1, y1;
	
	x1 = sl->x - viewx;
	y1 = sl->y - viewy;
	if(sl->decor.top != NULL)
	{
		dst.x = x1;
		dst.y = y1 - sl->decor.top->h;
		SDL_BlitSurface(sl->decor.top, NULL, screen, &dst);
		
		if(sl->decor.right != NULL)
		{
			dst.x = x1 + sl->scaled->w;
			dst.y = y1 - sl->decor.top->h;
			SDL_BlitSurface(sl->decor.right, NULL, screen, &dst);
		}
	}
	if(sl->decor.left != NULL)
	{
		dst.x = x1 - sl->decor.left->w;
		dst.y = y1;
		SDL_BlitSurface(sl->decor.left, NULL, screen, &dst);
		
		if(sl->decor.bottom != NULL)
		{
			dst.x = x1;
			dst.y = y1 + sl->scaled->h;
			SDL_BlitSurface(sl->decor.bottom, NULL, screen, &dst);
		}
	}
}

void mini_copy_decorations(slide *sl, int viewx, int viewy)
{
	SDL_Rect dst;
	int x1, y1;
	
	x1 = (sl->x - viewx) / 3;
	y1 = (sl->y - viewy) / 3;
	if(sl->mini_decor.top != NULL)
	{
		dst.x = x1;
		dst.y = y1 - sl->mini_decor.top->h;
		SDL_BlitSurface(sl->mini_decor.top, NULL, screen, &dst);
		
		if(sl->mini_decor.right != NULL)
		{
			dst.x = x1 + sl->mini->w;
			dst.y = y1 - sl->mini_decor.top->h;
			SDL_BlitSurface(sl->mini_decor.right, NULL, screen, &dst);
		}
	}
	if(sl->mini_decor.left != NULL)
	{
		dst.x = x1 - sl->mini_decor.left->w;
		dst.y = y1;
		SDL_BlitSurface(sl->mini_decor.left, NULL, screen, &dst);
		
		if(sl->mini_decor.bottom != NULL)
		{
			dst.x = x1;
			// dst.y = y1 + sl->mini->h;
			dst.y = y1 + sl->mini_decor.left->h - sl->mini_decor.bottom->h;
			SDL_BlitSurface(sl->mini_decor.bottom, NULL, screen, &dst);
		}
	}
}

void highlight(slide *sl, int viewx, int viewy, int scale)
{
	int x1, y1, x2, y2;
	int clearance, extent, thickness;

	if(scale == 1)
	{
		thickness = 5;
		clearance = 9;
		extent = 90;
	}	
	else if(scale == 3)
	{
		thickness = 3;
		clearance = 5;
		extent = 40;
	}
	else if(scale == 9)
	{
		thickness = 2;
		clearance = 2;
		extent = 20;
	}
	else {
		error("Impossible case in highlight()");
		exit(-1);
	}
	
	if(extent > sl->scr_w / scale)
		extent = sl->scr_w / scale;
	if(extent > sl->scr_h / scale)
		extent = sl->scr_h / scale;
	
	x1 = (sl->x - viewx) / scale - clearance;
	x2 = (sl->x + sl->scr_w - 1 - viewx) / scale + clearance;
	y1 = (sl->y - viewy) / scale - clearance;
	y2 = (sl->y + sl->scr_h - 1 - viewy) / scale + clearance;
	
	for(int i = 0; i < thickness; i++)
	{
		// Top left:
		hlineColor(screen, x1 - i, x1 + extent + i,
				y1 - i, colour->red_pen);
		vlineColor(screen, x1 - i,
				y1 - i, y1 + extent + i, colour->red_pen);
		// Top right:
		hlineColor(screen, x2 - extent - i, x2 + i,
				y1 - i, colour->red_pen);
		vlineColor(screen, x2 + i,
				y1 - i, y1 + extent + i, colour->red_pen);
		// Bottom left:
		hlineColor(screen, x1 - i, x1 + extent + i,
				y2 + i, colour->red_pen);
		vlineColor(screen, x1 - i,
				y2 - extent - i, y2 + i, colour->red_pen);
		// Bottom right:
		hlineColor(screen, x2 - extent - i, x2 + i,
				y2 + i, colour->red_pen);
		vlineColor(screen, x2 + i,
				y2 - extent - i, y2 + i, colour->red_pen);
	}
}

void copy_to_screen(slide *sl, int viewx, int viewy)
{
	SDL_Rect dst;	
	dst.x = sl->x - viewx;
	dst.y = sl->y - viewy;
	if(sl->scaled == NULL)
		error("Tried to render a NULL surface in copy_to_screen");
	int ret = SDL_BlitSurface(sl->scaled, NULL, screen, &dst);
	if(ret != 0)
		error("SDL_BlitSurface returned %d\n", ret);
	if(sl->deck_size > 1)
		copy_decorations(sl, viewx, viewy);
	if(sl->selected)
		highlight(sl, viewx, viewy, 1);
}

void copy_all_to_screen(slidevector *render_list, int viewx, int viewy)
{
	slide *sl;
	for(int i = 0; i < render_list->count(); i++)
	{
		sl = render_list->item(i);
		copy_to_screen(sl, viewx, viewy);
	}
}

void mini_copy_to_screen(slide *sl, int viewx, int viewy)
{
	SDL_Rect dst;
	dst.x = (sl->x - viewx) / 3;
	dst.y = (sl->y - viewy) / 3;
	int ret = SDL_BlitSurface(sl->mini, NULL, screen, &dst);
	if(ret != 0)
		error("SDL_BlitSurface returned %d\n", ret);
	if(sl->deck_size > 1)
		mini_copy_decorations(sl, viewx, viewy);
	if(sl->selected)
		highlight(sl, viewx, viewy, 3);
}

void mini_copy_all_to_screen(slidevector *render_list, int viewx, int viewy)
{
	slide *sl;
	for(int i = 0; i < render_list->count(); i++)
	{
		sl = render_list->item(i);
		mini_copy_to_screen(sl, viewx, viewy);
	}
}

void micro_copy_to_screen(slide *sl, int viewx, int viewy)
{
	SDL_Rect dst;
	dst.x = (sl->x - viewx) / 9;
	dst.y = (sl->y - viewy) / 9;
	int ret = SDL_BlitSurface(sl->micro, NULL, screen, &dst);
	if(ret != 0)
		error("SDL_BlitSurface returned %d\n", ret);
	if(sl->selected)
		highlight(sl, viewx, viewy, 9);
}

void micro_copy_all_to_screen(slidevector *render_list, int viewx, int viewy)
{
	slide *sl;
	for(int i = 0; i < render_list->count(); i++)
	{
		sl = render_list->item(i);
		micro_copy_to_screen(sl, viewx, viewy);
	}
}

void pointer(int x, int y)
{
	int dx, dy, dlen, ex, ey, incx = 0, incy = 0, x1, y1, x2, y2;
	const int shaft_width = 10;
	const int arrow_head = 50;
	double aspect, angle;
	Uint32 pen = colour->yellow_pen;
	
	dx = x - SCREEN_WIDTH / 2;
	dy = y - SCREEN_HEIGHT / 2;
	if(dx == 0)
		dx = 1;
	if(dy == 0)
		dy = 1;
	angle = (double)abs(dx) / (double)abs(dy);
	aspect = (double)SCREEN_WIDTH / (double)SCREEN_HEIGHT;
	if(angle < aspect)
	{
		if(dy < 0)
		{
			// Top edge:
			ex = SCREEN_WIDTH / 2 + (dx * SCREEN_HEIGHT) / 2 / (-dy);
			ey = 0;
		}
		else
		{
			// Bottom edge:
			ex = SCREEN_WIDTH / 2 + (dx * SCREEN_HEIGHT) / 2 / dy;
			ey = SCREEN_HEIGHT - 1;
		}
		incx = 1;
	}
	else
	{
		if(dx > 0)
		{
			// Right edge:
			ex = SCREEN_WIDTH - 1;
			ey = SCREEN_HEIGHT / 2 + (dy * SCREEN_WIDTH) / 2 / dx;
		}
		else
		{
			// Left edge:
			ex = 0;
			ey = SCREEN_HEIGHT / 2 + (dy * SCREEN_WIDTH) / 2 / (-dx);
		}
		incy = 1;
	}
	lineColor(screen, ex, ey, x, y, pen);
	for(int i = 1; i < shaft_width; i++)
	{
		lineColor(screen, ex + i * incx, ey + i * incy, x, y, pen);
		lineColor(screen, ex - i * incx, ey - i * incy, x, y, pen);
	}
	lineColor(screen, ex + shaft_width * incx, ey + shaft_width * incy,
			x, y, colour->black_pen);
	lineColor(screen, ex - shaft_width * incx, ey - shaft_width * incy,
			x, y, colour->black_pen);
	// Arrow head:
	dlen = dx * dx + dy * dy;
	dlen = (int)ceil(sqrt(dlen));
	dx *= arrow_head;
	dy *= arrow_head;
	dx /= dlen;
	dy /= dlen;
	x1 = x + dx + dy / 2;
	y1 = y + dy - dx / 2;
	x2 = x + dx - dy / 2;
	y2 = y + dy + dx / 2;
	filledTrigonColor(screen, x, y, x1, y1, x2, y2, pen);
	trigonColor(screen, x, y, x1, y1, x2, y2, colour->black_pen);
	
	// SDL_Flip(screen);
}
