/* config.cpp - DMI - 3-Jan-2007

Copyright (C) 2007 David Ingram

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

static const int WARP_STEPS = 16;

static const char *LATEX_CMD = "latex";
static const char *DVIPS_CMD = "dvips";
static const char *CONVERT_CMD = "convert";

static const char *config_file_name = "multitalk.conf";
	
// Prototypes:
void read_options(const char *dir_path, Options *options);

Options *init_options(Config *config)
{
	Options *options = new Options();
	
	read_options(config->sys_rc_dir, options);
	read_options(config->env_dir, options);
	read_options(config->home_rc_dir, options);
	
	return options;
}

void read_options(const char *dir_path, Options *options)
{
	char *config_file_path;
	dictionary *d;
	
	if(dir_path == NULL) // Possible if environment var not set
		return;
	
	config_file_path = combine_path(dir_path, config_file_name);
	if(fexists(config_file_path))
	{
		d = new dictionary();
		d->load(config_file_path);
		options->update(d);
		delete d;
	}
	delete[] config_file_path;
}

Options::Options()
{
	latexcmd = sdup(LATEX_CMD);
	dvipscmd = sdup(DVIPS_CMD);
	convertcmd = sdup(CONVERT_CMD);
	warpsteps = WARP_STEPS;
}

void Options::update(dictionary *d)
{
	set_string_property(d, "latexcmd", &latexcmd);
	set_string_property(d, "dvipscmd", &dvipscmd);
	set_string_property(d, "convertcmd", &convertcmd);
	set_integer_property(d, "warpsteps", &warpsteps);
}
