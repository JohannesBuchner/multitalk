/* graph.cpp - DMI - 24-10-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

// Prototypes:
void fill_graph(slidevector *talk, graphfile *graph);

graphfile *open_graph(const char *graph_filename)
{
	graphfile *graph;

	if(fexists(graph_filename))
	{
		graph = new graphfile();
		int ret = graph->load(graph_filename);
		if(ret < 0)
			return NULL;
	}
	else
	{
		graph = NULL;
	}
	return graph;
}

void save_slide_positions(Config *config, slidevector *talk)
{
	graphfile *graph;

	graph = new graphfile();	
	fill_graph(talk, graph);
	
	graph->save(config->graph_path);
}

void fill_graph(slidevector *talk, graphfile *graph)
{
	slide *sl;
	subimage *img;
	const char *title;
	int x, y;
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		title = sl->content->line;
		x = sl->x;
		y = sl->y;
		if(sl->card > 1)
		{
			x -= CARD_EDGE * (sl->card - 1);
			y += CARD_EDGE * (sl->card - 1);
		}
		graph->add('@', title, x, y);
		for(int j = 0; j < sl->embedded_images->count(); j++)
		{
			img = sl->embedded_images->item(j);
			title = img->path_name;
			x = img->des_x;
			y = img->des_y;
			graph->add('+', title, x, y);
		}
	}	
}

void read_subimage_positions(slide *sl, graphfile *graph, int start)
{
	// Warning: quadratic complexity
	subimage *img;
	int x, y;
	while(1)
	{
		if(start >= graph->count() || graph->get_type(start) != '+')
			return; // End of sub-coordinate data
		x = graph->get_x(start);
		y = graph->get_y(start);
		/* Try to find an image on the slide with matching name and
			co-ordinates still unknown (there may be several images with
			the same filename): */
		for(int i = 0; i < sl->embedded_images->count(); i++)
		{
			img = sl->embedded_images->item(i);
			if(img->des_x == UNKNOWN_POS && img->des_y == UNKNOWN_POS &&
					!strcmp(img->path_name, graph->get_title(start)))
			{
				img->des_x = x;
				img->des_y = y;
				img->scr_x = to_screen_coords(img->des_x);
				img->scr_y = to_screen_coords(img->des_y);
				break;
			}
		}
		start++;
	}
}

void reconstruct_positions(slidevector *talk, graphfile *graph,
		const char *graph_filename)
{
	/* Slides may have been
		- reordered (use same coords as before)
		- renamed (guess from position, then use same coords as before)
		- deleted (remove graph coords)
		- added (position below preceding slide)
		Images within slides may have been
		- added or deleted (reordering or renaming assumed less likely)
	*/
	slide *sl;
	const char *title;
	int x, y, lastx, lasty;
	int pos;
	int next_record;
	intvector *unused_pos = new intvector;
	
	printf("Reconstructing graph file.\n");
	
	// Mark slide locations as unknown:
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		sl->x = UNKNOWN_POS;
		sl->y = UNKNOWN_POS;
	}
	
	// Read in all positions from the graph which have a corresponding slide:
	// Warning - Quadratic complexity
	next_record = 0;
	while(next_record < graph->count())
	{
		if(graph->get_type(next_record) != '@')
		{
			next_record++;
			continue;
		}
		x = graph->get_x(next_record);
		y = graph->get_y(next_record);
		title = graph->get_title(next_record);

		int j;		
		for(j = 0; j < talk->count(); j++)
		{
			sl = talk->item(j);
			if(!strcmp(title, sl->content->line))
			{
				sl->x = x;
				sl->y = y;
				read_subimage_positions(sl, graph, next_record + 1);
				break;
			}
		}
		if(j == talk->count()) // Couldn't find a match
			unused_pos->add(next_record);
		
		next_record++;
	}

	// Try to make use of any unused positional data on renamed slides:
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(sl->x == UNKNOWN_POS && sl->y == UNKNOWN_POS)
		{
			for(int j = 0; j < unused_pos->count(); j++)
			{
				pos = unused_pos->item(j);
				if(pos == 0 || i == 0)
				{
					if(pos == 0 && i == 0)
					{
						// Assume first slide was renamed; match it up with coords:
						sl->x = graph->get_x(pos);
						sl->y = graph->get_y(pos);
						read_subimage_positions(sl, graph, pos + 1);
						unused_pos->del(j);
						break;
					}
				}
				else
				{
					// Did the previous slide match graph (pos - 1)?
					slide *prev = talk->item(i - 1);
					int lastpos = pos - 1;
					while(graph->get_type(lastpos) != '@' && lastpos > 0)
						lastpos--;
					if(prev->x == graph->get_x(lastpos) &&
						prev->y == graph->get_y(lastpos))
					{
						// Assume current slide renamed; match it up with coords:
						sl->x = graph->get_x(pos);
						sl->y = graph->get_y(pos);
						read_subimage_positions(sl, graph, pos + 1);
						unused_pos->del(j);
						break;
					}
				}
			}
		}
	}
	delete unused_pos;
		
	// Put slides with no position below the preceding slide:
	lastx = 50;
	lasty = -250;
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		if(sl->x == UNKNOWN_POS && sl->y == UNKNOWN_POS)
		{
			sl->x = lastx - 50;
			sl->y = lasty + 250;
		}
		lastx = sl->x;
		lasty = sl->y;
	}

	// Move any subimages which we still don't know the position of to (0,0):
	subimage *img;
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		for(int j = 0; j < sl->embedded_images->count(); j++)
		{
			img = sl->embedded_images->item(j);
			if(img->des_x == UNKNOWN_POS && img->des_y == UNKNOWN_POS)
			{
				img->des_x = img->scr_x = 0;
				img->des_y = img->scr_y = 0;
			}
		}
	}
	
	// Create a new graph for only/all the slides we have, in the correct order:
	delete graph;
	graph = new graphfile();
	fill_graph(talk, graph);	
	graph->save(graph_filename);
}

void load_slide_positions(Config *config, slidevector *talk)
{
	slide *sl;
	const char *title;
	graphfile *graph;
	int next_record = 0, num_records;
	subimage *img;
	
	graph = open_graph(config->graph_path);
	if(graph == NULL)
	{
		graph = new graphfile(); // Empty
		reconstruct_positions(talk, graph, config->graph_path);
		return;
	}
	num_records = graph->count();
	
	for(int i = 0; i < talk->count(); i++)
	{
		sl = talk->item(i);
		title = sl->content->line;
		if(next_record >= num_records ||
				graph->get_type(next_record) != '@' ||
				strcmp(title, graph->get_title(next_record)))
		{
			// At least one missing/mismatched name; have to do it the hard way:
			reconstruct_positions(talk, graph, config->graph_path);
			return;
		}
		sl->x = graph->get_x(next_record);
		sl->y = graph->get_y(next_record);
		next_record++;
		// Read in sub-image positions:
		for(int j = 0; j < sl->embedded_images->count(); j++)
		{
			img = sl->embedded_images->item(j);
			if(graph->get_type(next_record) != '+' ||
					strcmp(img->path_name, graph->get_title(next_record)))
			{
				reconstruct_positions(talk, graph, config->graph_path);
				return;
			}
			img->des_x = graph->get_x(next_record);
			img->des_y = graph->get_y(next_record);
			img->scr_x = to_screen_coords(img->des_x);
			img->scr_y = to_screen_coords(img->des_y);
			next_record++;
		}
	}
	if(next_record < num_records)
	{
		// We didn't use all the graph records, hence some are redundant
		reconstruct_positions(talk, graph, config->graph_path);
	}		
}
