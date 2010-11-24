/* style.cpp - DMI - 1-11-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

typedef TTF_Font *TTF_FontPtr;
typedef SDL_Surface *SDL_SurfacePtr;

static const char *TITLE_FONT_FILE = "luxi/luxisb.ttf";
static const char *TEXT_FONT_FILE = "luxi/luxisr.ttf";
static const char *FIXED_FONT_FILE = "luxi/luximr.ttf";
static const char *BOLD_FONT_FILE = "luxi/luxisb.ttf";
static const char *ITALIC_FONT_FILE = "luxi/luxisri.ttf";

static const int TITLE_FONT_SIZE = 40;
static const int TEXT_FONT_SIZE = 36;
static const int FIXED_FONT_SIZE = 26;
static const int HEADING_SPACING = 70;
static const int LINE_SPACING = 42;

static const int LEFT_MARGIN = 15;
static const int RIGHT_MARGIN = 30;
static const int FOLD_MARGIN = 30;
static const int TOP_MARGIN = 0;
static const int BOTTOM_MARGIN = 20;

static const int PICTURE_MARGIN = 15;

static const int LATEX_WIDTH = 550;
static const int LATEX_SCALE = 300;
static const int LATEX_BASELINE_STRETCH = 85;
static const int LATEX_SPACE_ABOVE = 5;
static const int LATEX_SPACE_BELOW = 0;

static const int RULE_WIDTH = 85;
static const int RULE_HEIGHT = 2;
static const int RULE_SPACE_ABOVE = 10;
static const int RULE_SPACE_BELOW = 5;

static const int HEAD_SPACE_ABOVE = 5;
static const int HEAD_SPACE_BELOW = 10;

extern Config *config;

style *stylevector::default_style()
{
	style *sty;
	
	if(count() == 0)
		error("No default style");
	sty = item(0);
	if(strcmp(sty->name, "Default") != 0)
		error("Default style not found");
	return sty;
}

style *stylevector::lookup_style(const char *name)
{
	style *sty;
	for(int i = 0; i < count(); i++)
	{
		sty = item(i);
		if(!strcmp(sty->name, name))
			return sty;
	}
	return NULL;
}

char *get_file_stem(const char *filename)
{
	char *stem;
	int len = strlen(filename);
	int pos = len - 1;
	
	while(1)
	{
		if(filename[pos] == '.')
			break;
		if(pos == 0)
			return NULL; // No dot
		pos--;
	}
	if(pos == 0)
		return NULL; // Dot is first character - hence no stem
	stem = new char[pos + 1];
	strncpy(stem, filename, pos);
	stem[pos] = '\0';
	return stem;
}

char *get_file_extension(const char *filename)
{
	char *extension;
	int len = strlen(filename);
	int pos = len - 1;
	
	while(1)
	{
		if(filename[pos] == '.')
			break;
		if(pos == 0)
			return NULL; // No dot
		pos--;
	}
	if(pos == len - 1)
		return NULL; // Dot is last character - hence no extension
	extension = new char[len + 1];
	strcpy(extension, filename + pos + 1);
	return extension;
}

void scan_style_dir(const char *dir_path, stylevector *style_list,
		style *def_st)
{
	DIR *dir_stream;
	struct dirent *de;
	char *style_file_path;
	char *stem, *extension;
	dictionary *d;
	style *st;
	
	if(dir_path == NULL) // Possible if environment variable not set
		return;
	dir_stream = opendir(dir_path);
	if(dir_stream == NULL)
	{
		return;
		// error("Can't open styles directory");
	}
	while(1)
	{
		de = readdir(dir_stream);
		if(de == NULL)
			break;
		if(de->d_name[0] == '.')
			continue;
		extension = get_file_extension(de->d_name);
		if(strcmp(extension, "style") != 0)
		{
			delete[] extension;
			continue;
		}
		delete[] extension;
		stem = get_file_stem(de->d_name);
		if(!strcmp(stem, "Default"))
		{
			// We've already taken care of the Default.style definitions
			delete[] stem;
			continue;
		}
		style_file_path = new char[strlen(dir_path) + strlen(de->d_name) + 5];
		sprintf(style_file_path, "%s/%s", dir_path, de->d_name);
		// printf("Loading style %s from %s\n", stem, style_file_path);
		d = new dictionary();
		d->load(style_file_path);

		st = style_list->lookup_style(stem);
		if(st == NULL)
		{
			// New style:
			st = new style(stem, def_st);
			st->update(d);
			style_list->add(st);
		}
		else
		{
			// Override some settings in an existing style:
			st->update(d);
		}
		
		delete[] stem;
		delete d;
		delete[] style_file_path;
	}
}

void read_default(const char *dir_path, style *def_st)
{
	char *default_file_path;
	dictionary *d;
	
	if(dir_path == NULL) // Possible if environment var not set
		return;
	default_file_path = combine_path(dir_path, "Default.style");
	if(fexists(default_file_path))
	{
		d = new dictionary();
		d->load(default_file_path);
		def_st->update(d);
		delete d;
	}
	delete[] default_file_path;
}

stylevector *load_styles(Config *config)
{
	stylevector *style_list;
	style *def_st;

	style_list = new stylevector();
	def_st = new style("Default", NULL);
	style_list->add(def_st);

	read_default(config->sys_style_dir, def_st);
	read_default(config->env_style_dir, def_st);
	read_default(config->home_style_dir, def_st);
	read_default(config->proj_style_dir, def_st);

	scan_style_dir(config->sys_style_dir, style_list, def_st);
	scan_style_dir(config->env_style_dir, style_list, def_st);
	scan_style_dir(config->home_style_dir, style_list, def_st);
	scan_style_dir(config->proj_style_dir, style_list, def_st);
	
	return style_list;
}

int get_colour_index(const char *name)
{
	int colour_index;
	
	if(name[0] == '#')
	{
		colour_index = colour->search_add(name + 1);
		if(colour_index == -1)
			error("Incorrect hex colour %s", name);
	}
	else
	{
		colour_index = colour->names->find_ignore_case(name);
		if(colour_index == -1)
			error("Unknown colour %s", name);
	}
	return colour_index;
}

void set_colour_property(dictionary *d, const char *name, int *dest)
{
	const char *value;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
		*dest = get_colour_index(value);
}

void set_integer_property(dictionary *d, const char *name, int *dest)
{
	const char *value;
	int data;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		data = atoi(value);
		*dest = data;
	}
}

void set_percentage_property(dictionary *d, const char *name, int *dest)
{
	const char *value;
	int data;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		data = atoi(value);
		if(data < 0 || data > 100)
			error("Property value %d is not in the range [0,100]", data);
		*dest = data;
	}
}

void set_enum_property(dictionary *d, const char *name, int *dest)
{
	const char *value;
	int data;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		data = atoi(value);
		if(data < 0 || data > 10)
			error("Value out of range for property %s", name);
		*dest = data;
	}
}

void set_boolean_property(dictionary *d, const char *name, int *dest)
{
	const char *value;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		if(!strcasecmp(value, "yes"))
			*dest = 1;
		else if(!strcasecmp(value, "no"))
			*dest = 0;
		else
			error("Non-boolean value for property %s", name);
	}
}

void set_string_property(dictionary *d, const char *name, constCharPtr *dest)
{
	const char *value;
	char *buf;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		if(*dest != NULL)
			delete[] (*dest);
		
		buf = new char[strlen(value) + 1];
		strcpy(buf, value);
		*dest = buf;
	}
}

void set_surface_property(dictionary *d, const char *name, constCharPtr *dest,
		SDL_SurfacePtr *surface, int alpha)
{
	const char *value;
	char *buf;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		buf = new char[strlen(value) + 1];
		strcpy(buf, value);
		*dest = buf;
		*surface = load_png(value, alpha);
	}
}

void set_file_property(dictionary *d, const char *name, svector **text)
{
	const char *value;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		if(*text != NULL)
			delete (*text);
		*text = load_text_file(value);
	}
}

void set_font_property(dictionary *d, const char *name, constCharPtr *dest,
		TTF_FontPtr *font, int size)
{
	const char *value;
	char *buf;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		buf = new char[strlen(value) + 1];
		strcpy(buf, value);
		*dest = buf;
		/* Note: would be nice to deallocate old font here, but not always
			safe to do so if this is an inherited style and font ptr is shared */
		*font = init_font(config, value, size);
	}
}

void set_ptsize_property(dictionary *d, const char *name, int *dest,
		TTF_FontPtr *font, const char *fontname)
{
	const char *value;
	int data;
	
	value = d->lookup_ignore_case(name);
	if(value != NULL)
	{
		data = atoi(value);
		*dest = data;
		/* Note: would be nice to deallocate old font here, but not always
			safe to do so if this is an inherited style and font ptr is shared */
		*font = init_font(config, fontname, data);
	}
}

void style::init_fonts(style *inherit)
{
	if(inherit == NULL)
	{
		title_font = init_font(config, TITLE_FONT_FILE, TITLE_FONT_SIZE);
		heading_font = init_font(config, TITLE_FONT_FILE, (TITLE_FONT_SIZE + TEXT_FONT_SIZE) / 2);
		text_font = init_font(config, TEXT_FONT_FILE, TEXT_FONT_SIZE);
		fixed_font = init_font(config, FIXED_FONT_FILE, FIXED_FONT_SIZE);
		bold_font = init_font(config, BOLD_FONT_FILE, TEXT_FONT_SIZE);
		italic_font = init_font(config, ITALIC_FONT_FILE, TEXT_FONT_SIZE);
	}
	else
	{
		title_font = inherit->title_font;
		heading_font = inherit->heading_font;
		text_font = inherit->text_font;
		fixed_font = inherit->fixed_font;
		bold_font = inherit->bold_font;
		italic_font = inherit->italic_font;
	}		
	
	int w, h;
	TTF_SizeUTF8(text_font, " ", &w, &h);
	text_space_width = w;
	TTF_SizeUTF8(fixed_font, " ", &w, &h);
	fixed_space_width = w;
	
	title_ascent = TTF_FontAscent(title_font);
	title_descent = TTF_FontDescent(title_font);
	text_ascent = TTF_FontAscent(text_font);
	text_descent = TTF_FontDescent(text_font);
	fixed_ascent = TTF_FontAscent(fixed_font);
	fixed_descent = TTF_FontDescent(fixed_font);
	bold_ascent = TTF_FontAscent(bold_font);
	bold_descent = TTF_FontDescent(bold_font);
	italic_ascent = TTF_FontAscent(italic_font);
	italic_descent = TTF_FontDescent(italic_font);
}

void style::update(dictionary *d)
{
	set_colour_property(d, "barcolour", &barcolour);
	set_colour_property(d, "textcolour", &textcolour);
	set_colour_property(d, "bgcolour", &bgcolour);
	set_colour_property(d, "linkcolour", &linkcolour);
	set_colour_property(d, "titlecolour", &titlecolour);
	set_colour_property(d, "headingcolour", &headingcolour);
	
	set_colour_property(d, "bullet1colour", &bullet1colour);
	set_colour_property(d, "bullet2colour", &bullet2colour);
	set_colour_property(d, "bullet3colour", &bullet3colour);

	set_colour_property(d, "rulecolour", &rulecolour);
		
	set_percentage_property(d, "rulewidth", &rulewidth);
	
	set_integer_property(d, "ruleheight", &ruleheight);
	set_integer_property(d, "rulespaceabove", &rulespaceabove);
	set_integer_property(d, "rulespacebelow", &rulespacebelow);
	
	set_integer_property(d, "headspaceabove", &headspaceabove);
	set_integer_property(d, "headspacebelow", &headspacebelow);
	
	set_integer_property(d, "bullet1size", &bullet1size);
	set_integer_property(d, "bullet2size", &bullet2size);
	set_integer_property(d, "bullet3size", &bullet3size);
	
	set_ptsize_property(d, "titlesize", &titlesize, &title_font, titlefont);
	set_ptsize_property(d, "textsize", &textsize, &text_font, textfont);
	set_ptsize_property(d, "textsize", &textsize, &bold_font, boldfont);
	set_ptsize_property(d, "textsize", &textsize, &italic_font, italicfont);
	set_ptsize_property(d, "titlesize", &textsize, &heading_font, textfont);
	set_ptsize_property(d, "textsize", &textsize, &heading_font, textfont);
	set_ptsize_property(d, "fixedsize", &fixedsize, &fixed_font, fixedfont);
	
	set_integer_property(d, "linespacing", &linespacing);
	set_integer_property(d, "titlespacing", &titlespacing);

	set_integer_property(d, "latexwidth", &latexwidth);
	set_integer_property(d, "latexscale", &latexscale);
	set_integer_property(d, "latexbaselinestretch", &latexbaselinestretch);
	set_integer_property(d, "latexspaceabove", &latexspaceabove);
	set_integer_property(d, "latexspacebelow", &latexspacebelow);
		
	set_surface_property(d, "bullet1icon", &bullet1icon, &bullet1, 1);
	set_surface_property(d, "bullet2icon", &bullet2icon, &bullet2, 1);
	set_surface_property(d, "bullet3icon", &bullet3icon, &bullet3, 1);

	set_file_property(d, "latexinclude", &latexinclude);
	set_file_property(d, "latexpreinclude", &latexpreinclude);
		
	set_font_property(d, "titlefont", &titlefont, &title_font, titlesize);
	set_font_property(d, "titlefont", &textfont, &heading_font, (titlesize + textsize) / 2);
	set_font_property(d, "textfont", &textfont, &text_font, textsize);
	set_font_property(d, "fixedfont", &fixedfont, &fixed_font, fixedsize);
	set_font_property(d, "boldfont", &boldfont, &bold_font, textsize);
	set_font_property(d, "italicfont", &italicfont, &italic_font, textsize);
	
	int w, h;		
	TTF_SizeUTF8(text_font, " ", &w, &h);
	text_space_width = w;
	TTF_SizeUTF8(fixed_font, " ", &w, &h);
	fixed_space_width = w;
	
	title_ascent = TTF_FontAscent(title_font);
	title_descent = TTF_FontDescent(title_font);
	text_ascent = TTF_FontAscent(text_font);
	text_descent = TTF_FontDescent(text_font);
	fixed_ascent = TTF_FontAscent(fixed_font);
	fixed_descent = TTF_FontDescent(fixed_font);
	bold_ascent = TTF_FontAscent(bold_font);
	bold_descent = TTF_FontDescent(bold_font);
	italic_ascent = TTF_FontAscent(italic_font);
	italic_descent = TTF_FontDescent(italic_font);
	
	set_integer_property(d, "picturemargin", &picturemargin);
	set_integer_property(d, "topmargin", &topmargin);
	set_integer_property(d, "bottommargin", &bottommargin);
	set_integer_property(d, "leftmargin", &leftmargin);
	set_integer_property(d, "rightmargin", &rightmargin);
	set_integer_property(d, "foldmargin", &foldmargin);
	
	set_colour_property(d, "foldcollapsedcolour", &foldcollapsedcolour);
	set_colour_property(d, "foldexpandedcolour", &foldexpandedcolour);
	set_colour_property(d, "foldexposed1colour", &foldexposed1colour);
	set_colour_property(d, "foldexposed2colour", &foldexposed2colour);
	set_colour_property(d, "foldexposed3colour", &foldexposed3colour);
	
	set_colour_property(d, "highlightcolour", &highlightcolour);
	
	set_surface_property(d, "bgimage", &bgimage, &background, 0);
	set_surface_property(d, "bgtexture", &bgtexture, &texture, 0);
	
	set_surface_property(d, "foldcollapsedicon", &foldcollapsedicon,
			&collapsedicon, 1);
	set_surface_property(d, "foldexpandedicon", &foldexpandedicon,
			&expandedicon, 1);
	
	set_colour_property(d, "bordercolour", &bordercolour);
	
	set_enum_property(d, "pictureborder", &pictureborder);
	set_enum_property(d, "slideborder", &slideborder);
	set_enum_property(d, "barborder", &barborder);
	
	set_boolean_property(d, "underlinelinks", &underlinelinks);
	set_boolean_property(d, "enablebar", &enablebar);
	set_boolean_property(d, "bgbar", &bgbar);

	// Logos:
	const char *name, *value;
	for(int i = 0; i < d->count(); i++)
	{
		name = d->get_name(i);
		value = d->get_value(i);
		if(!strcasecmp("logo", name))
		{
			parse_logo(value);
		}
	}
}

void style::parse_logo(const char *s)
{
	// String format: x,y,path/to/picture.png
	logo *lo;
	int comma1, comma2;
	
	if(s[0] == '\0' || s[0] == ',')
		error("Incorrect logo syntax");
	comma1 = 1;
	while(s[comma1] != ',')
	{
		if(s[comma1] == '\0')
			error("Incorrect logo syntax");
		comma1++;
	}
	if(s[comma1 + 1] == '\0' || s[comma1 + 1] == ',')
		error("Incorrect logo syntax");
	comma2 = comma1 + 2;
	while(s[comma2] != ',')
	{
		if(s[comma2] == '\0')
			error("Incorrect logo syntax");
		comma2++;
	}
	if(s[comma2 + 1] == '\0')
		error("Incorrect logo syntax");
	
	lo = new logo();
	lo->x = atoi(s);
	lo->y = atoi(s + comma1 + 1);
	lo->image_file = new char[strlen(s)];
	strcpy(lo->image_file, s + comma2 + 1);
	lo->image = load_png(lo->image_file, 1);
	logos->add(lo);
}

style::style(const char *s, style *inherit)
{
	name = new char[strlen(s) + 1];
	strcpy(name, s);

	// Colour palette indexes:
	barcolour = colour->names->find("yellow");
	textcolour = colour->names->find("black");
	bgcolour = colour->names->find("white");
	linkcolour = colour->names->find("purple");
	titlecolour = colour->names->find("black");
	headingcolour = colour->names->find("black");
	bullet1colour = colour->names->find("yellow");
	bullet2colour = colour->names->find("cyan");
	bullet3colour = colour->names->find("blue");
	bordercolour = colour->names->find("black");
	foldcollapsedcolour = colour->names->find("green");
	foldexpandedcolour = colour->names->find("yellow");
	foldexposed1colour = colour->names->find("sky");
	foldexposed2colour = colour->names->find("grey");
	foldexposed3colour = colour->names->find("black");
	highlightcolour = colour->names->find("red");
	rulecolour = colour->names->find("black");
	
	// Numerical values:
	bullet1size = 9;
	bullet2size = 7;
	bullet3size = 6;
	titlesize = TITLE_FONT_SIZE;
	textsize = TEXT_FONT_SIZE;
	fixedsize = FIXED_FONT_SIZE;
	linespacing = LINE_SPACING;
	titlespacing = HEADING_SPACING;
	picturemargin = PICTURE_MARGIN;
	topmargin = TOP_MARGIN;
	bottommargin = BOTTOM_MARGIN;
	leftmargin = LEFT_MARGIN;
	foldmargin = FOLD_MARGIN;
	rightmargin = RIGHT_MARGIN;
	latexwidth = LATEX_WIDTH;
	latexscale = LATEX_SCALE;
	latexbaselinestretch = LATEX_BASELINE_STRETCH;
	latexspaceabove = LATEX_SPACE_ABOVE;
	latexspacebelow = LATEX_SPACE_BELOW;
	ruleheight = RULE_HEIGHT;
	rulespaceabove = RULE_SPACE_ABOVE;
	rulespacebelow = RULE_SPACE_BELOW;
	headspaceabove = HEAD_SPACE_ABOVE;
	headspacebelow = HEAD_SPACE_BELOW;

	// Percentages:
	rulewidth = RULE_WIDTH;
		
	// Booleans:
	underlinelinks = 1;
	enablebar = 1;
	bgbar = 1;
	
	// Enumerations:
	pictureborder = 1;
	slideborder = 1;
	barborder = 1;

	// Fonts (filenames):
	titlefont = TITLE_FONT_FILE;
	textfont = TEXT_FONT_FILE;
	fixedfont = FIXED_FONT_FILE;
	boldfont = BOLD_FONT_FILE;
	italicfont = ITALIC_FONT_FILE;
	
	// Pictures (filenames):
	bullet1icon = NULL;
	bullet2icon = NULL;
	bullet3icon = NULL;
	bgimage = NULL;
	bgtexture = NULL;
	foldcollapsedicon = NULL;
	foldexpandedicon = NULL;

	bullet1 = bullet2 = bullet3 = NULL;
	background = texture = NULL;
	collapsedicon = expandedicon = NULL;

	// Other filenames:
	latexinclude = NULL;
	latexpreinclude = NULL;
	
	// An array of logos:
	logos = new logovector();
	
	init_fonts(inherit);
}
