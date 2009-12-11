/* files.cpp - DMI - 21-11-2006

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

extern Config *config;

// Prototypes:
svector *search_text_file(const char *dir, const char *filename);
svector *do_load_text_file(const char *filename);

char *expand_tilde(const char *s)
{
	char *t;
	struct passwd *pw;
	
	if(s[0] != '~')
		return NULL;
	if(s[1] == '/')
	{
		pw = getpwuid(getuid());
		t = new char[strlen(s) + strlen(pw->pw_dir) + 1];
		sprintf(t, "%s/%s", pw->pw_dir, s + 2);
	}
	else
	{
		s++;
		char *name = sdup(s);
		
		int pos = 0;
		while(s[pos] != '/' && s[pos] != '\0')
			pos++;
		if(s[pos] == '\0')
			return NULL; // Could be a directory, can't be a file
		name[pos] = '\0';
		pw = getpwnam(name);
		delete[] name;
		if(pw == NULL)
			return NULL;
		t = new char[strlen(s) + strlen(pw->pw_dir) + 1];
		sprintf(t, "%s/%s", pw->pw_dir, s + pos + 1);
	}
	return t;
}

svector *load_text_file(const char *filename)
{
	svector *vec;
	char *t;

	if(filename[0] == '/')
		return do_load_text_file(filename);
	t = expand_tilde(filename);
	if(t != NULL)
	{
		vec = do_load_text_file(t);
		delete[] t;
		return vec;
	}
	
	vec = search_text_file(config->project_dir, filename);
	if(vec != NULL) return vec;
	vec = search_text_file(config->home_style_dir, filename);
	if(vec != NULL) return vec;
	vec = search_text_file(config->env_style_dir, filename);
	if(vec != NULL) return vec;
	vec = search_text_file(config->sys_style_dir, filename);
	if(vec != NULL) return vec;
	
	error("Cannot locate text file <%s>", filename);
	return NULL;
}

svector *search_text_file(const char *dir, const char *filename)
{
	char *text_file;
	svector *vec;
	
	if(dir == NULL) // Possible if environment variable not set
		return NULL;	
	text_file = combine_path(dir, filename);
	if(fexists(text_file))
	{
		vec = do_load_text_file(text_file);
		delete[] text_file;
		return vec;
	}
	delete[] text_file;
	return NULL;
}

svector *do_load_text_file(const char *filename)
{
	linefile *lf;
	svector *vec;
	
	lf = new linefile();
	if(lf->load(filename) < 0)
	{
		error("Cannot open text file <%s>", filename);
		return NULL;
	}
	
	vec = new svector();
	for(int i = 0; i < lf->count(); i++)
	{
		vec->add(lf->getline(i));
	}
	delete lf;
	return vec;
}

int fexists(const char *file_path)
{
	struct stat buf;
	if(stat(file_path, &buf) == 0)
		return 1;
	else
		return 0;
}

char *sdup(const char *s)
{
	char *sd;
	sd = new char[strlen(s) + 1];
	strcpy(sd, s);
	return sd;
}

char *combine_path(const char *path, const char *file)
{
	char *s;
	
	s = new char[strlen(path) + strlen(file) + 2];
	sprintf(s, "%s/%s", path, file);
	return s;
}

char *replace_extension(const char *s, const char *extension)
{
	int len = strlen(s);
	char *extended;
	
	for(int pos = len - 1; pos >= 0; pos--)
	{
		if(s[pos] == '/')
		{
			// No extension
			extended = new char[len + strlen(extension) + 2];
			sprintf(extended, "%s.%s", s, extension);
			return extended;
		}
		if(s[pos] == '.')
		{
			if(pos == len - 1)
			{
				// No extension
				extended = new char[len + strlen(extension) + 2];
				sprintf(extended, "%s%s", s, extension);
				return extended;
			}
			if(!strcmp(extension, s + pos + 1))
			{
				// Extension OK
				extended = sdup(s);
				return extended;
			}
			else
			{
				// Different extension (replace):
				extended = new char[pos + strlen(extension) + 2];
				strncpy(extended, s, pos + 1);
				strcpy(extended + pos + 1, extension);
				return extended;
			}
		}
	}
	// No extension
	extended = new char[len + strlen(extension) + 2];
	sprintf(extended, "%s.%s", s, extension);
	return extended;
}

char *get_path(const char *s)
{
	int len = strlen(s);
	char *path;
	
	for(int pos = len - 1; pos >= 0; pos--)
	{
		if(s[pos] == '/')
		{
			if(pos == 0)
			{
				path = new char[2];
				strcpy(path, "/");
			}
			else
			{
				path = new char[pos + 1];
				strncpy(path, s, pos);
				path[pos] = '\0';
			}
			return path;
		}
	}
	path = new char[2];
	strcpy(path, ".");
	return path;
}

char *get_file(const char *s)
{
	int len = strlen(s);
	char *path;
	
	for(int pos = len - 1; pos >= 0; pos--)
	{
		if(s[pos] == '/')
		{
			path = new char[len];
			strcpy(path, s + pos + 1);
			return path;
		}
	}
	path = sdup(s);
	return path;
}

int absolute_path(char *s)
{
	return ((s[0] == '/') ? 1 : 0);
}

Config *init_paths(const char *talk_ref)
{
	Config *config = new Config();
	const char *sys_prefix = "/usr/local/share";
	
	config->talk_path = replace_extension(talk_ref, "talk");
	config->graph_path = replace_extension(talk_ref, "graph");
	config->project_dir = get_path(talk_ref);
	config->latex_dir = replace_extension(talk_ref, "latex");
	config->html_dir = replace_extension(talk_ref, "html");
	config->sys_dir = combine_path(sys_prefix, "multitalk");
	config->sys_image_dir = combine_path(config->sys_dir, "gfx");
	config->sys_style_dir = combine_path(config->sys_dir, "styles");
	config->sys_font_dir = combine_path(config->sys_dir, "fonts");
	config->proj_style_dir = combine_path(config->project_dir, "styles");
	config->proj_font_dir = combine_path(config->project_dir, "fonts");
	config->sys_rc_dir = sdup("/etc");

	config->caption = new char[strlen(config->talk_path) + 20];
	sprintf(config->caption, "Multitalk - %s", config->talk_path);
	
	char *e = getenv("MULTITALK_DIR");
	if(e == NULL)
	{
		config->env_dir = NULL;
		config->env_style_dir = NULL;
		config->env_font_dir = NULL;
		config->env_image_dir = NULL;
	}
	else
	{
		config->env_dir = sdup(e);
		config->env_style_dir = combine_path(config->env_dir, "styles");
		config->env_font_dir = combine_path(config->env_dir, "fonts");
		config->env_image_dir = combine_path(config->env_dir, "gfx");
	}
	
	uid_t id;
	struct passwd *pw;
	
	id = getuid();
	pw = getpwuid(id);
	if(pw == NULL)
		error("Can't lookup home directory");
	config->home_dir = sdup(pw->pw_dir);
	config->home_style_dir = combine_path(config->home_dir, ".multitalk/styles");
	config->home_font_dir = combine_path(config->home_dir, ".multitalk/fonts");
	config->home_image_dir = combine_path(config->home_dir, ".multitalk/gfx");
	config->home_rc_dir = combine_path(config->home_dir, ".multitalk");

	if(debug & DEBUG_PATHS)
	{	
		printf("=== Directories ===\n");
		printf("talk_path = %s\n", config->talk_path);
		printf("project_dir = %s\n", config->project_dir);
		printf("latex_dir = %s\n", config->latex_dir);
		printf("html_dir = %s\n", config->html_dir);
		printf("sys_style_dir = %s\n", config->sys_style_dir);
		printf("home_style_dir = %s\n", config->home_style_dir);
		printf("===================\n");
	}
	
	return config;
}
