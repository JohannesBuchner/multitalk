/* parse.cpp - DMI - 6-Oct-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

// Prototypes:
void present(char *line, displayline *out, style *st);
void fold_recursive(node *ptr, int fold);

int all_whitespace(const char *s)
{
	while(*s != '\0')
	{
		if(*s != ' ' && *s != '\t')
			return 0;
		s++;
	}
	return 1; // Yep, it's all whitespace
}

enum GlobalOpts { DesignSize, CanvasColour };

int global_option(const char *line, int preparse = 0)
{
	/* (optional spaces) canvascolour=colour [grey]
	                     designsize=x,y
	*/
	svector *opts = new svector();
	opts->add("designsize");
	if(!preparse)
	{
		opts->add("canvascolour");
	}
	const char *name;
	svector *v;
	
	while(*line == ' ' || *line == '\t')
		line++;
	for(int i = 0; i < opts->count(); i++)
	{
		name = opts->item(i);
		if(!strncmp(line, name, strlen(name)))
		{
			line += strlen(name);
			while(*line == ' ' || *line == '\t') line++;
			if(*line != '=')
			{
				delete opts;
				return 0;
			}
			line++;
			while(*line == ' ' || *line == '\t') line++;
			if(*line == '\0')
				error("Missing argument to global option %s", name);

			switch(i)
			{
				case CanvasColour:
					canvas_colour = get_colour_index(line);
					break;
				case DesignSize:
					v = split_list(line, 'x');
					if(v->count() != 2)
						error("Invalid designsize");
					design_width = atoi(v->item(0));
					design_height = atoi(v->item(1));
					if(design_width < 10 || design_height < 10 ||
							design_width > 99999 || design_height > 99999)
						error("Improbable designsize");
					delete v;
					break;
				default:
					error("Impossible case in switch statement");
			}
			delete opts;
			return 1;
		}
	}
	delete opts;
	return 0;
}

void peek_designsize(linefile *talk_lf)
{
	const char *line;
	int lines = talk_lf->count();
	
	for(int i = 0; i < lines; i++)
	{
		line = talk_lf->getline(i);
		if(line[0] == '@')
			global_option(line + 1, 1);
	}
}

slidevector *parse_talk(linefile *talk_lf, stylevector *style_list)
{
	slide *sl = NULL;
	node *context = NULL, *block, *leaf;
	slidevector *talk = new slidevector();
	
	const char *line;
	int len;
	char prev = '#'; // First character of previous line
	char blank_line[1];
	blank_line[0] = '\0';
	int card_mask = 0xFFFF;
	int consumed;
	subimage *img_here = NULL, *img_above = NULL;
	
	int lines = talk_lf->count();
	if(lines < 2)
		error("Talk too short");
	
	for(int i = 0; i < lines; i++)
	{
		line = talk_lf->getline(i);
		len = strlen(line);
		if(all_whitespace(line))
			line = blank_line;
		
		consumed = 0;
		switch(line[0])
		{
			case '@':
				if(prev == '\0' && sl != NULL)
				{
					// Remove blank line at end of previous slide:
					context->children->pop();
				}
				if(global_option(line + 1))
				{
					consumed = 1;
					break;
				}
				sl = new slide;
				sl->repr = NULL; // Linear version not generated yet
				sl->render = sl->scaled = sl->mini = sl->micro = NULL;
				sl->decor.top = sl->decor.bottom = NULL;
				sl->decor.left = sl->decor.right = NULL;
				sl->mini_decor.top = sl->mini_decor.bottom = NULL;
				sl->mini_decor.left = sl->mini_decor.right = NULL;
				sl->st = style_list->default_style();
				sl->embedded_images = new subimagevector;
				sl->visible_images = new subimagevector;
				sl->deck_size = 1;
				sl->card = 1;
				sl->selected = 0;
				
				sl->image_file = NULL; // Text slide so far
				context = new node;
				context->type = NODE_SLIDE;
				context->children = new nodevector;
				context->parent = NULL;
				context->line = new char[len + 1];
				context->local_images = new subimagevector;
				card_mask = 0xFFFF;
				context->card_mask = card_mask;
				if(len < 3)
					error("Slidename too short (line %d)", i);
				if(line[1] == ' ') // Optional space
					strcpy(context->line, line + 2);
				else
					strcpy(context->line, line + 1);
				sl->content = context;
				// Check slide title is unique:
				{
					for(int i = 0; i < talk->count(); i++)
					{
						if(!strcmp(sl->content->line, talk->item(i)->content->line))
							error("Duplicate slide title %s", context->line);
					}
				}

				if(line[1] == '!')
				{
					if(len < 7)
						error("Image filename too short");
					sl->image_file = new char[len];
					strcpy(sl->image_file, line + 2);
				}
				
				talk->add(sl);
				consumed = 1;
				break;
			case '#':
				// Do nothing (ignore line) - fixme for inside $'s case
				consumed = 1;
				break;
			case '[':
				if(len != 1)
					break;
				if(sl == NULL)
					error("Content before first slide");
				if(prev == '[' || prev == ']' || prev == '\0' || prev == '@')
					error("Start-of-block does not follow plain text (line %d)", i);
				if(context->children->count() == 0)
					error("Start-of-block does not follow plain text (line %d)", i);
				block = context->children->top();
				if(block->type != NODE_LEAF || block->local_images != NULL)
					error("Start-of-block does not follow plain text (line %d)", i);
				// Convert this leaf node into a block:
				block->type = NODE_TREE;
				block->children = new nodevector;
				block->local_images = new subimagevector;
				block->folded = 1;
				context = block;
				consumed = 1;
				break;
			case ']':
				if(len != 1)
					break;
				if(sl ==  NULL)
					error("Content before first slide");
				if(context->parent == NULL)
					error("End-of-block encountered whilst not inside a block "
							"(line %d)", i);
				if(context->children->count() == 0)
					error("Empty block (line %d)", i);
				context = context->parent;
				consumed = 1;
				break;
			case '\0':
				// Blank line
				if(sl != NULL && prev != '@') // Ignore at top of file or slide
				{
					leaf = new node;
					leaf->type = NODE_LEAF;
					leaf->parent = context;
					leaf->line = new char[1];
					leaf->line[0] = '\0';
					leaf->card_mask = card_mask;
					context->children->add(leaf);
				}
				consumed = 1;
				break;
			case '%':
				if(!strncmp(line, "%space ", 7))
				{
					int n = atoi(line + 7);
					if(n > 0 && n < SCREEN_HEIGHT)
					{
						if(sl == NULL)
							error("Content before first slide");
						leaf = new node;
						leaf->type = NODE_VSPACE;
						leaf->parent = context;
						leaf->card_mask = card_mask;
						leaf->space = n;
						context->children->add(leaf);
						consumed = 1;
					}
				}
				break;
			case '-':
				if(!strcmp(line, "--"))
				{
					if(sl == NULL)
						error("Content before first slide");
					leaf = new node;
					leaf->type = NODE_RULE;
					leaf->parent = context;
					leaf->card_mask = card_mask;
					context->children->add(leaf);
					consumed = 1;
				}
				break;
			case '!':
				// Slide style, or Image, or Card-set
				{
					style *st;
					int mask;
					int max_card;
					
					if(sl ==  NULL)
						error("Image or style directive before first slide");
					if(len < 2)
						error("Image filename / Style name too short");

					st = style_list->lookup_style(line + 1);
					if(st != NULL)
					{
						// It's a style:
						sl->st = st;
						consumed = 1;
						break;
					}
					
					// Check if any characters non-numeric:
					if(len == 2 && line[1] == '!')
					{
						card_mask = 0xFFFF;
						consumed = 1;
						break;
					}
					mask = 0;
					max_card = -1;
					for(int i = 1; i < len; i++)
					{
						char c = line[i];
						if(c >= '1' && c <= '9')
						{
							int card = c - '1';
							if(card > max_card)
								max_card = card;
							mask |= 1 << card;
						}
						else if(line[i] != ' ' && line[i] != '\t')
						{
							max_card = -1;
							break;
						}
					}
					if(max_card != -1)
					{
						// Update current card_mask:
						card_mask = mask;
						// Update slide's deck_size:
						if(sl->deck_size < max_card + 1)
							sl->deck_size = max_card + 1;
						consumed = 1;
						break;
					}
					
					// Must be an image:
					subimage *img = new subimage;
					img->surface = NULL;
					img->des_x = img->des_y = UNKNOWN_POS;
					img->scr_x = img->scr_y = UNKNOWN_POS;
					img->card_mask = card_mask;
					img->hyperlink = NULL;

					img->path_name = new char[len];
					strcpy(img->path_name, line + 1);

					sl->embedded_images->add(img);
					context->local_images->add(img);
					img_here = img;
					consumed = 1;
				}
				break;
			case ':':
				if(sl == NULL)
					error("Hyperlink before first slide");
				
				if(img_above != NULL)
				{
					img_above->hyperlink = new char[strlen(line)];
					strcpy(img_above->hyperlink, line + 1);
				}
				else
				{
					if(context->children->count() == 0)
						error("Hyperlink with no anchor line");
					context->children->top()->hyperlink = new char[strlen(line)];
					strcpy(context->children->top()->hyperlink, line + 1);
				}
				consumed = 1;
				break;
			case '\\':
				if(!(len == 1 || (len == 2 && line[1] == ')')))
					break;
				
				if(sl == NULL)
					error("Content before first slide");
				
				leaf = new node;
				leaf->type = NODE_LATEX;
				leaf->parent = context;
				leaf->card_mask = card_mask;
				leaf->align = (len == 2 ? 1 : 0);
				leaf->tex = new svector();
				context->children->add(leaf);

				while(1)
				{
					i++;
					if(i == lines)
						error("Unfinished latex section");
					line = talk_lf->getline(i);
					len = strlen(line);
					if(line[0] == '\\' && len == 1)
						break;
					leaf->tex->add(line);
				}		
				
				consumed = 1;
				break;
		}
		if(!consumed)
		{
			// Ordinary text
			if(sl == NULL)
				error("Content before first slide");
			leaf = new node;
			leaf->type = NODE_LEAF;
			leaf->parent = context;
			leaf->line = new char[len + 1];
			leaf->card_mask = card_mask;
			strcpy(leaf->line, line);
			context->children->add(leaf);
		}
		if(line[0] != '#')
		{
			prev = line[0];
			img_above = img_here;
			img_here = NULL;
		}
	}
	if(prev == '\0' && sl != NULL)
	{
		// Remove blank line at end of final slide:
		context->children->pop();
	}
	return talk;
}

node::node()
{
	hyperlink = NULL;
	tex = NULL;
	line = NULL;
	children = NULL;
	local_images = NULL;
	align = 0;
	folded = -1;
	space = -1;
	
	// Uninitialised: card_mask, type, parent
}

node::~node()
{
	if(line != NULL)
		delete[] line;
	if(tex != NULL)
		delete tex;
	if(hyperlink != NULL)
		delete[] hyperlink;
	if(local_images != NULL)
		delete local_images;
}

void free_node(node *context)
{
	if(context->children != NULL)
	{
		node *child;
	
		for(int i = 0; i < context->children->count(); i++)
		{
			child = context->children->item(i);
			free_node(child);
		}
		delete context->children;
	}
	delete context;
}

void printspaces(int indent)
{
	for(int i = 0; i < indent * 3; i++)
		printf(" ");
}

void transfer_images(subimagevector *from, subimagevector *to, int card)
{
	subimage *img;
	for(int i = 0; i < from->count(); i++)
	{
		img = from->item(i);
		if((img->card_mask & (1 << (card - 1))) == 0)
			continue;
		to->add(img);
	}
}

displayline::displayline()
{
	bullet = 0;
	prespace = 0;
	centred = 0;
	heading = 0;
	highlighted = 0;
	rule = 0;
	source = NULL;
	link = NULL;
	link_card = 0;
	import = NULL;
	line = NULL;
	
	// Uninitialised: initial_dm, width, height, y, line_num, exposed, type
}

displayline::~displayline()
{
	if(line != NULL)
	{
		delete[] line;
		line = NULL; // Paranoia
	}
	if(import != NULL)
		SDL_FreeSurface(import);
}

void fold_all(slide *sl)
{
	fold_recursive(sl->content, 1);
}

void unfold_all(slide *sl)
{
	fold_recursive(sl->content, 0);
}

int is_foldable(slide *sl)
{
	nodevector *v;
	node *ptr;
	
	v = sl->content->children;
	for(int i = 0; i < v->count(); i++)
	{
		ptr = v->item(i);
		if(ptr->type == NODE_TREE)
			return 1;
	}
	return 0;
}

void fold_recursive(node *ptr, int fold)
{
	if(ptr->type == NODE_TREE)
		ptr->folded = fold;
	if(ptr->type == NODE_SLIDE || ptr->type == NODE_TREE)
		for(int i = 0; i < ptr->children->count(); i++)
			fold_recursive(ptr->children->item(i), fold);
}

void flatten(node *ptr, displaylinevector *v, subimagevector *visible_images,
		slidevector *talk, int exposed, style *st, int card)
{
	displayline *out;

	if((ptr->card_mask & (1 << (card - 1))) == 0)
		return;
	
	if(ptr->type == NODE_SLIDE)
	{
		// Flatten a slide:
		for(int i = 0; i < ptr->children->count(); i++)
		{
			flatten(ptr->children->item(i), v, visible_images, talk, 0, st, card);
		}
		if(ptr->local_images != NULL)
			transfer_images(ptr->local_images, visible_images, card);
	}
	else if(ptr->type == NODE_LEAF)
	{
		// Flatten a leaf:
		out = new displayline;
		out->type = TYPE_PLAIN;
		out->exposed = exposed;
		out->source = ptr;
		if(ptr->hyperlink != NULL)
		{
			slide *sl = find_title(talk, ptr->hyperlink, &(out->link_card));
			out->link = sl;
		}
		present(ptr->line, out, st);
		v->add(out);
		out->line_num = v->count();
	}
	else if(ptr->type == NODE_TREE && ptr->folded == 1)
	{
		// Flatten just the title of a collapsed section:
		out = new displayline;
		out->type = TYPE_FOLDED;
		out->exposed = exposed;
		out->source = ptr;
		present(ptr->line, out, st);
		v->add(out);
		out->line_num = v->count();
	}
	else if(ptr->type == NODE_LATEX)
	{
		out = new displayline;
		out->type = TYPE_PLAIN;
		out->exposed = exposed;
		out->source = ptr;
		out->import = gen_latex(ptr->tex, st);
		out->height = out->import->h + st->latexspaceabove + st->latexspacebelow;
		out->centred = (ptr->align == 1 ? 1 : 0);
		v->add(out);
		out->line_num = v->count();
	}
	else if(ptr->type == NODE_VSPACE)
	{
		out = new displayline;
		out->type = TYPE_PLAIN;
		out->exposed = exposed;
		out->source = ptr;
		out->height = ptr->space;
		out->line = new char[1];
		out->line[0] = '\0';
		v->add(out);
		out->line_num = v->count();
	}
	else if(ptr->type == NODE_RULE)
	{
		out = new displayline;
		out->type = TYPE_PLAIN;
		out->exposed = exposed;
		out->source = ptr;
		out->height = st->ruleheight + st->rulespaceabove + st->rulespacebelow;
		out->rule = 1;
		out->line = new char[1];
		out->line[0] = '\0';
		v->add(out);
		out->line_num = v->count();
	}
	else if(ptr->type == NODE_TREE && ptr->folded == 0)
	{
		out = new displayline;
		out->type = TYPE_EXPANDED;
		out->exposed = exposed;
		out->source = ptr;
		present(ptr->line, out, st);
		v->add(out);
		out->line_num = v->count();
		for(int i = 0; i < ptr->children->count(); i++)
		{
			flatten(ptr->children->item(i), v, visible_images, talk,
					exposed + 1, st, card);
		}
		if(ptr->local_images != NULL)
			transfer_images(ptr->local_images, visible_images, card);
	}
	else
		error("Impossible case in flatten()");
}

void present(char *line, displayline *out, style *st)
{
	char *s;

	s = line;
	out->height = st->linespacing;
	
	if(line[0] == '^')
	{
		out->bullet = -1;
		s++;
		while(*s == ' ' || *s == '\t')
			s++;
	}
	else if(line[0] == ')')
	{
		out->centred = 1;
		s++;
		while(*s == ' ' || *s == '\t')
			s++;
	}
	else if(line[0] == '>')
	{
		out->heading = 1;
		out->height = st->linespacing + st->titlesize - st->textsize +
				st->headspaceabove + st->headspacebelow;
		s++;
		if(line[1] == ')')
		{
			out->centred = 1;
			s++;
		}
		while(*s == ' ' || *s == '\t')
			s++;
	}
	else
	{
		while(*s == ' ' || *s == '\t')
		{
			if(*s == ' ')
				out->prespace++;
			else if(*s == '\t')
				out->prespace += 3;
			s++;
		}
		while(*s == '*')
		{
			out->bullet++;
			s++;
		}
		while(*s == ' ' || *s == '\t')
		{
			if(*s == ' ')
				out->prespace++;
			else if(*s == '\t')
				out->prespace += 3;
			s++;
		}
		if(out->bullet > 3)
			error("Too many nested bullets");
	}
	
	out->line = new char[strlen(s) + 1];
	strcpy(out->line, s);
}

int str_to_num(const char *s)
{
	int n = 0;
	if(*s == '\0')
		return -1;
	while(1)
	{
		if(*s == '\0')
			break;
		if(*s >= '0' && *s <= '9')
			n = n * 10 + *s - '0';
		else
			return -1;
		s++;
	}
	return n;
}

slide *find_title(slidevector *talk, const char *title, int *card)
{
	slide *sl;
	
	// Try title verbatim:
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(!strcmp(title, sl->content->line))
		{
			*card = 0;
			return sl;
		}
	}

	// Try title with card number:
	int len, pos, n;
	char *t;
	
	len = strlen(title);
	if(len == 0)
		return NULL;
	pos = len - 1;
	while(1)
	{
		if(title[pos] == '.')
			break;
		pos--;
		if(pos < 0)
			return NULL; // No decimal point in slide title
	}
	n = str_to_num(title + pos + 1);
	if(n == -1)
		return NULL; // Characters after decimal point are non-numeric
	t = sdup(title);
	t[pos] = '\0';
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(!strcmp(t, sl->content->line))
		{
			*card = n;
			delete[] t;
			return sl;
		}
	}
	
	delete[] t;
	return NULL;
}

void flatten(slide *sl, slidevector *talk) // Regenerate repr
{
	if(sl->image_file != NULL)
		return;
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
	sl->repr = new displaylinevector();
	sl->visible_images->clear();
	flatten(sl->content, sl->repr, sl->visible_images, talk, 0, sl->st,
			sl->card);
}

void flatten_all(slidevector *talk)
{
	slide *sl;
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		flatten(sl, talk);
	}
}
