/* web.cpp - DMI - 25-Aug-2008

Copyright (C) 2008 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

extern Config *config;
extern Options *options;

// Values for flags (0x01 to 0xFF is card number):
const int NameRelative = 0x100;
const int NameUnfolded = 0x200;

char *make_pathname(const char *s, const char *ext, int flags = 0)
{
	char c;
	StringBuf *sb;
	int len;
	int card;

	sb = new StringBuf();
	card = flags & 0xFF;
	len = strlen(s);

	if((flags & NameRelative) == 0) // Surely C operator precedence is wrong!
	{
		sb->cat(config->html_dir);
		sb->cat('/');
	}
	for(int i = 0; i < len; i++)
	{
		c = s[i];
		if(c == ' ')
			sb->cat('_');
		else if(c >= 'a' && c <= 'z')
			sb->cat(c);
		else if(c >= 'A' && c <= 'Z')
			sb->cat(c);
		else if(c >= '0' && c <= '9')
			sb->cat(c);
		else if(i == 0)
			sb->cat('_');
		else if(c == '.')
			sb->cat(c);
		else
			sb->cat('-');
	}
	if(card > 0)
	{
		sb->cat('.');
		sb->cat(card);
	}
	if(flags & NameUnfolded)
		sb->cat("_unfolded");
	sb->cat(ext);
	
	char *t = sb->compact();
	delete sb;
	return t;
}

void generate_imagemap(linefile *lf, const char *name, hotspotvector *hsv,
		int background)
{
	hotspot *hs;
	char *target_filename;
	StringBuf *sb;

	sb = new StringBuf();
	sb->clear();
	sb->cat("<map name=\"");
	sb->cat(name);
	sb->cat("\">");
	lf->addline(sb->repr());
	for(int j = 0; j < hsv->count(); j++)
	{
		hs = hsv->item(j);
		sb->clear();
		sb->cat("<area shape=rectangle coords=\"");
		sb->cat(hs->x1);
		sb->cat(",");
		sb->cat(hs->y1);
		sb->cat(",");
		sb->cat(hs->x2);
		sb->cat(",");
		sb->cat(hs->y2);
		sb->cat("\" href=\"");
		target_filename = make_pathname(hs->sl->content->line, ".html",
				NameRelative + hs->card + hs->unfold);
		sb->cat(target_filename);
		delete[] target_filename;
		sb->cat("\"");
		lf->addline(sb->repr());
		sb->clear();
		sb->cat("   onMouseOver=\"self.status='");
		if(hs->unfold)
			sb->cat("Unfold");
		else
			sb->cat(hs->sl->content->line);
		sb->cat("'; return true\"");
		lf->addline(sb->repr());
		lf->addline("   onMouseOut=\"self.status=''\">");
	}
	if(background)
	{
		lf->addline("<area shape=default href=\"index.html\"");
		lf->addline("   onMouseOver=\"self.status=''; return true\"");
		lf->addline("   onMouseOut=\"self.status=''\">");
	}
	lf->addline("</map>");
	delete sb;
}

void gen_index(slidevector *talk) // Creates index.html
{
	StringBuf *sb;
	linefile *lf;
	int minx, miny, maxx, maxy;
	char *html_pathname;
	char *nav_bmp_pathname, *nav_png_pathname, *nav_png_filename;
	const char *convertcmd = options->convertcmd;
	int ret;
	int cx, cy;
	hotspotvector *hsv;
	slide *sl;

	sb = new StringBuf();
	hsv = new hotspotvector();
	minx = miny = 999999;
	maxx = maxy = -999999;
	
	for(int i = 0; i < talk->count(); i++)
	{
		// Update talk bounds:
		sl = talk->item(i);
		if(sl->x < minx) minx = sl->x;
		if(sl->y < miny) miny = sl->y;
		if(sl->x + sl->scr_w - 1 > maxx) maxx = sl->x + sl->scr_w - 1;
		if(sl->y + sl->scr_h - 1 > maxy) maxy = sl->y + sl->scr_h - 1;
	}
	
	html_pathname = make_pathname("index", ".html");
	nav_bmp_pathname = make_pathname("index", "_nav.bmp");
	nav_png_pathname = make_pathname("index", "_nav.png");
	nav_png_filename = make_pathname("index", "_nav.png", NameRelative);
	printf("Exporting %s\n", html_pathname);
	
	// Render, output and convert nav display of entire talk:
	RADAR_MAG = 20;
	RADAR_WIDTH = (maxx - minx + 1) / RADAR_MAG;
	RADAR_HEIGHT = (maxy - miny + 1) / RADAR_MAG;	
	SDL_FreeSurface(radar);
	radar = alloc_surface(RADAR_WIDTH, RADAR_HEIGHT);
	cx = (minx + maxx) / 2;
	cy = (miny + maxy) / 2;
	clear_radar();
	render_radar(cx, cy, hsv, 0);
	ret = SDL_SaveBMP(radar, nav_bmp_pathname);
	if(ret < 0)
		error("Error saving %s", nav_bmp_pathname);
	
	// Convert nav display:
	sb->clear();
	sb->cat(convertcmd);
	sb->cat(" ");
	sb->cat(nav_bmp_pathname);
	sb->cat(" ");
	sb->cat(nav_png_pathname);
	ret = system(sb->repr());
	if(ret == -1 || WEXITSTATUS(ret) == 127)
		error("Failed to invoke convert command");
	ret = WEXITSTATUS(ret);
	if(ret != 0)
		error("Abnormal return code %d from convert", ret);
	unlink(nav_bmp_pathname);				
	
	// Output HTML for contents page index.html:
	lf = new linefile();
	lf->addline("<html>");
	lf->addline("<head><title>");
	lf->addline(config->caption);
	lf->addline("</title></head>");
	lf->addline("<body>");
	generate_imagemap(lf, "nav", hsv, 0);
	sb->clear();
	sb->cat("<h2 align=center>");
	sb->cat(config->talk_path);
	sb->cat(": ");
	sb->cat(talk->item(0)->content->line);
	sb->cat("</h2>");
	lf->addline(sb->repr());
	lf->addline("<center>");
	sb->clear();
	sb->cat("<img src=\"");
	sb->cat(nav_png_filename);
	sb->cat("\" usemap=\"#nav\" border=0>");
	lf->addline(sb->repr());
	lf->addline("<p>");
	lf->addline("Generated by "
			"<a href=\"http://www.srcf.ucam.org/~dmi1000/multitalk/\">"
			"Multitalk</a>");
	lf->addline("</center>");
	lf->addline("</body></html>");
	lf->save(html_pathname);
	delete lf;
	
	for(int j = 0; j < hsv->count(); j++)
		delete hsv->item(j);
	
	delete[] nav_bmp_pathname;
	delete[] nav_png_pathname;
	delete[] nav_png_filename;
	delete[] html_pathname;
	
	delete hsv;
	delete sb;
}

void gen_html(slidevector *talk)
{
	slide *sl;
	const char *title;
	char *html_pathname;
	char *pic_bmp_pathname, *pic_png_pathname, *pic_png_filename;
	char *nav_bmp_pathname, *nav_png_pathname, *nav_png_filename;
	const char *convertcmd = options->convertcmd;
	StringBuf *sb;
	linefile *lf;
	int ret;
	int cx, cy;
	hotspotvector *hsv_nav, *hsv_image;
	subimage *img;
	displayline *out;

	export_html = 2; // Second phase (disables "rendering" messages)
	if(!fexists(config->html_dir))
	{
		ret = mkdir(config->html_dir, S_IRWXU | S_IRWXG | S_IRWXO);
		if(ret != 0)
			error("Cannot make HTML directory.");
	}
	
	gen_index(talk);
	
	RADAR_WIDTH = 200;
	RADAR_HEIGHT = 100;
	RADAR_MAG = 20;
	SDL_FreeSurface(radar);
	radar = alloc_surface(RADAR_WIDTH, RADAR_HEIGHT);
	
	sb = new StringBuf();
	hsv_nav = new hotspotvector();
	hsv_image = new hotspotvector();
	
	for(int i = 0; i < talk->count(); i++)
	{
		int card, unfold;
		
		sl = talk->item(i);
		title = sl->content->line;
		
		unfold = 0;
		if(sl->deck_size > 1)
			card = 1;
		else
			card = 0;
		
		while(1) // Iterate through possibilities for card and unfold
		{
			pic_bmp_pathname = make_pathname(title, ".bmp", card + unfold);
			pic_png_pathname = make_pathname(title, ".png", card + unfold);
			pic_png_filename = make_pathname(title, ".png", NameRelative
					+ card + unfold);
			nav_bmp_pathname = make_pathname(title, "_nav.bmp", card + unfold);
			nav_png_pathname = make_pathname(title, "_nav.png", card + unfold);
			nav_png_filename = make_pathname(title, "_nav.png", NameRelative
					+ card + unfold);
			html_pathname = make_pathname(title, ".html", card + unfold);
			printf("Exporting %s\n", html_pathname);

			if(sl->image_file == NULL)
			{
				for(int j = 0; j < sl->visible_images->count(); j++)
				{
					img = sl->visible_images->item(j);
					if(img->hyperlink != NULL)
					{
						hotspot *hs = new hotspot();

						hs->sl = find_title(talk, img->hyperlink, &(hs->card));
						if(hs->sl == NULL)
							error("Hyperlink target <%s> not found", img->hyperlink);
						hs->unfold = 0;
						hs->x1 = img->des_x;
						hs->y1 = img->des_y;
						hs->x2 = img->des_x + img->des_w - 1;
						hs->y2 = img->des_y + img->des_h - 1;
						hsv_image->add(hs);
					}
				}

				for(int j = 0; j < sl->repr->count(); j++)
				{
					out = sl->repr->item(j);
					if(out->link != NULL)
					{
						hotspot *hs = new hotspot();

						hs->sl = out->link;
						hs->card = out->link_card;
						hs->unfold = 0;
						hs->x1 = 0;
						hs->x2 = sl->des_w - 1;
						hs->y1 = out->y;
						hs->y2 = out->y + out->height - 1;
						hsv_image->add(hs);
					}
					if(out->type == TYPE_FOLDED || out->type == TYPE_EXPANDED)
					{
						hotspot *hs = new hotspot();

						hs->sl = sl;
						hs->card = card;
						if(out->type == TYPE_FOLDED)
							hs->unfold = NameUnfolded;
						else
							hs->unfold = 0;
						hs->x1 = 0;
						hs->x2 = sl->des_w - 1;
						hs->y1 = out->y;
						hs->y2 = out->y + out->height - 1;
						hsv_image->add(hs);
					}
				}
			}
			
			// Save image:
			if(sl->render == NULL)
				error("Tried to export a NULL surface");		
			ret = SDL_SaveBMP(sl->render, pic_bmp_pathname);
			if(ret < 0)
				error("Error saving %s", pic_bmp_pathname);

			// Convert image:
			sb->clear();
			sb->cat(convertcmd);
			sb->cat(" ");
			sb->cat(pic_bmp_pathname);
			sb->cat(" ");
			sb->cat(pic_png_pathname);
			ret = system(sb->repr());
			if(ret == -1 || WEXITSTATUS(ret) == 127)
				error("Failed to invoke convert command");
			ret = WEXITSTATUS(ret);
			if(ret != 0)
				error("Abnormal return code %d from convert", ret);
			unlink(pic_bmp_pathname);

			// Render and output nav display:
			cx = sl->x + sl->scr_w / 2;
			cy = sl->y + sl->scr_h / 2;
			clear_radar();
			render_radar(cx, cy, hsv_nav);
			ret = SDL_SaveBMP(radar, nav_bmp_pathname);
			if(ret < 0)
				error("Error saving %s", nav_bmp_pathname);

			// Convert nav display:
			sb->clear();
			sb->cat(convertcmd);
			sb->cat(" ");
			sb->cat(nav_bmp_pathname);
			sb->cat(" ");
			sb->cat(nav_png_pathname);
			ret = system(sb->repr());
			if(ret == -1 || WEXITSTATUS(ret) == 127)
				error("Failed to invoke convert command");
			ret = WEXITSTATUS(ret);
			if(ret != 0)
				error("Abnormal return code %d from convert", ret);
			unlink(nav_bmp_pathname);

			// Generate image maps like this:
			/*
			<map name="foo">
				<area shape=rectangle coords="x1,y1,x2,y2" href="bar.html">
				<area shape=rectangle coords="x1,y1,x2,y2" href="bar.html"
					onMouseOver="self.status='Component metadata'; return true"
					onMouseOut="self.status=''">
				...
			</map>
			<img src="alpha.png" usemap="#foo" border=0>
			*/

			// Output HTML for page:
			lf = new linefile();
			lf->addline("<html>");
			lf->addline("<head><title>");
			lf->addline(title);
			lf->addline("</title></head>");
			lf->addline("<body>");
			generate_imagemap(lf, "nav", hsv_nav, 1);
			if(hsv_image->count() > 0)
				generate_imagemap(lf, "links", hsv_image, 0);
			lf->addline("<center>");
			sb->clear();
			sb->cat("<img src=\"");
			sb->cat(nav_png_filename);
			sb->cat("\" usemap=\"#nav\" border=0>");
			lf->addline(sb->repr());
			lf->addline("<p>");
			// lf->addline("<br>");
			if(sl->deck_size > 1)
			{
				char *link_pathname;
				
				sb->clear();
				sb->cat("Card ");
				sb->cat(card);
				sb->cat(" of ");
				sb->cat(sl->deck_size);
				sb->cat(". Turn to ");
				if(card > 1)
				{
					link_pathname = make_pathname(title, ".html", card - 1 +
							NameRelative);
					sb->cat("<a href=\"");
					sb->cat(link_pathname);
					sb->cat("\">previous card</a>");
					if(card < sl->deck_size)
						sb->cat(" or ");
					delete[] link_pathname;
				}
				if(card < sl->deck_size)
				{
					link_pathname = make_pathname(title, ".html", card + 1 +
							NameRelative);
					sb->cat("<a href=\"");
					sb->cat(link_pathname);
					sb->cat("\">next card</a>");
					delete[] link_pathname;
				}
				lf->addline(sb->repr());
				// lf->addline("<br>");
				lf->addline("<p>");
			}
			sb->clear();
			sb->cat("<img src=\"");
			sb->cat(pic_png_filename);
			if(hsv_image->count() > 0)
				sb->cat("\" usemap=\"#links\" border=0>");
			else
				sb->cat("\">");			
			lf->addline(sb->repr());
			lf->addline("</center>");
			/*
			lf->addline("<p align=right>");
			lf->addline("<a href=\"index.html\">&lt;INDEX&gt;</a>");
			*/
			lf->addline("</body></html>");
			lf->save(html_pathname);
			
			if(card == 1 && unfold == 0)
			{
				/* It would be much more elegant here to symlink
					foo.1.html to foo.html, but not all web servers follow
					symbolic links, so we have to make a copy of the file
					to be sure it will work: */
				delete[] html_pathname;
				html_pathname = make_pathname(title, ".html", 0);
				lf->save(html_pathname);
			}
			
			delete lf;

			delete[] pic_bmp_pathname;
			delete[] pic_png_pathname;
			delete[] pic_png_filename;
			delete[] nav_bmp_pathname;
			delete[] nav_png_pathname;
			delete[] nav_png_filename;
			delete[] html_pathname;

			for(int j = 0; j < hsv_nav->count(); j++)
				delete hsv_nav->item(j);
			for(int j = 0; j < hsv_image->count(); j++)
				delete hsv_image->item(j);
			hsv_nav->clear();
			hsv_image->clear();
			
			// See if there are any more versions of this slide to do:
			if(sl->deck_size == 1 || card == sl->deck_size)
			{
				// No more cards in deck:
			
				if(!is_foldable(sl))
					break;	
				if(unfold == NameUnfolded)
				{
					/* Refold the slide before jumping to the next slide, so
						that it has the correct dimensions for other slide's
						nav images: */
					fold_all(sl);
					redraft(sl);
					break;
				}
				
				unfold = NameUnfolded;
				unfold_all(sl);
				redraft(sl);
				if(sl->deck_size > 1)
					card = 1;
				continue;
			}
			card++;
			goto_card(sl, card);
		}
	}

	delete hsv_nav;
	delete hsv_image;
	delete sb;
}
