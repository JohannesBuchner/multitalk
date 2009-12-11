/* latex.cpp - DMI - 24-Dec-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include "datatype.h"
#include "multitalk.h"

extern Config *config;
extern Options *options;
extern int force_latex;

int hash_tex(svector *tex)
{
	int h, count;
	const char *s;
	int len;
	
	h = 0;
	count = 0;
	for(int i = 0; i < tex->count(); i++)
	{
		s = tex->item(i);
		len = strlen(s);
		count += len + 1;
		for(int j = 0; j < len; j++)
		{
			h <<= 7;
			h += s[j] & 0x7F;
			h = h % 999983;
		}
		h <<= 4;
		h += '\n';
		h = h % 999983;
	}
	h = (h * 1000) + (count % 1000);
	return h;
}

SDL_Surface *gen_latex(svector *tex, style *st)
{
	int h = hash_tex(tex);
	
	char texfilename[20];
	char dvifilename[20];
	char psfilename[20];
	char pngfilename[20];
	char auxfilename[20];
	char logfilename[20];
	
	sprintf(texfilename, "%09d.tex", h);
	sprintf(dvifilename, "%09d.dvi", h);
	sprintf(psfilename, "%09d.ps", h);
	sprintf(pngfilename, "%09d.png", h);
	sprintf(auxfilename, "%09d.aux", h);
	sprintf(logfilename, "%09d.log", h);

	const char *latexcmd = options->latexcmd;
	const char *dvipscmd = options->dvipscmd;
	const char *convertcmd = options->convertcmd;

	double pagewidth = (double)(st->latexwidth) / (double)(st->latexscale);
	
	linefile *lf;
	StringBuf *sb;
	int ret;
	
	if(!fexists(config->latex_dir))
	{
		ret = mkdir(config->latex_dir, S_IRWXU | S_IRWXG | S_IRWXO);
		if(ret != 0)
			error("Cannot make LaTeX directory.");
	}
	
	const int path_max = 300;
	char *current_dir;
	current_dir = new char[path_max];
	if(getcwd(current_dir, path_max) == NULL)
		error("Can't get current working directory");
	ret = chdir(config->latex_dir);
	if(ret != 0)
		error("Can't change to latex directory");

	if(!fexists(pngfilename) || force_latex)
	{
		double r, g, b;
		SDL_Color *fg, *bg;
		
		fg = colour->inks->item(st->textcolour);
		bg = colour->inks->item(st->bgcolour);
		
		lf = new linefile();
		sb = new StringBuf();
		lf->addline("\\documentclass[fleqn]{article}");
		lf->addline("\\usepackage[latin1]{inputenc}");
		lf->addline("\\usepackage{color}");
		if(st->latexpreinclude != NULL)
		{
			for(int i = 0; i < st->latexpreinclude->count(); i++)
				lf->addline(st->latexpreinclude->item(i));
		}
		r = (double)bg->r / 255.0;
		g = (double)bg->g / 255.0;
		b = (double)bg->b / 255.0;
		sb->clear();
		sb->cat("\\definecolor{bg}{rgb}{");
		sb->cat(r);
		sb->cat(", ");
		sb->cat(g);
		sb->cat(", ");
		sb->cat(b);
		sb->cat("}");
		lf->addline(sb->repr());
		r = (double)fg->r / 255.0;
		g = (double)fg->g / 255.0;
		b = (double)fg->b / 255.0;
		sb->clear();
		sb->cat("\\definecolor{fg}{rgb}{");
		sb->cat(r);
		sb->cat(", ");
		sb->cat(g);
		sb->cat(", ");
		sb->cat(b);
		sb->cat("}");
		lf->addline(sb->repr());
		lf->addline("\\pagestyle{empty}");
		lf->addline("\\pagecolor{bg}");
		sb->clear();
		sb->cat("\\setlength{\\textwidth}{");
		sb->cat(pagewidth);
		sb->cat("in}");
		lf->addline(sb->repr());
		lf->addline("\\sloppy");
		lf->addline("\\begin{document}");
		sb->clear();
		sb->cat("\\renewcommand{\\baselinestretch}{");
		sb->cat((double)(st->latexbaselinestretch) / 100.0);
		sb->cat("}");
		lf->addline(sb->repr());
		lf->addline("\\mathindent 0cm");
		lf->addline("\\parindent 0cm");
		lf->addline("\\color{fg}");
		lf->addline("\\sffamily");
		if(st->latexinclude != NULL)
		{
			for(int i = 0; i < st->latexinclude->count(); i++)
				lf->addline(st->latexinclude->item(i));
		}

		for(int i = 0; i < tex->count(); i++)
			lf->addline(tex->item(i));
		lf->addline("\\\\");
		lf->addline("\\end{document}");	
		lf->save(texfilename);
		delete lf;

		sb->clear();
		sb->cat(latexcmd);
		sb->cat(" -interaction=batchmode ");
		sb->cat(texfilename);
		ret = system(sb->repr());
		// Delete temporary LaTeX files:
		unlink(texfilename);
		unlink(auxfilename);
		unlink(logfilename);
		if(ret == -1 || WEXITSTATUS(ret) == 127)
			error("Failed to invoke latex");
		ret = WEXITSTATUS(ret);
		if(ret != 0)
		{
			printf("Warning: latex error[s] in the following section "
					"(proceeding anyway)...\n");
			for(int i = 0; i < tex->count(); i++)
				printf("> %s\n", tex->item(i));
		}

		sb->clear();
		sb->cat(dvipscmd);
		sb->cat(" -E -q -o ");
		sb->cat(psfilename);
		sb->cat(" ");
		sb->cat(dvifilename);
		ret = system(sb->repr());
		unlink(dvifilename);
		if(ret == -1 || WEXITSTATUS(ret) == 127)
			error("Failed to invoke dvips");
		ret = WEXITSTATUS(ret);
		if(ret != 0)
			error("Abnormal dvips return code %d", ret);

		sb->clear();	
		sb->cat(convertcmd);
		// sb->cat(" +antialias -units PixelsPerInch -density ");
		sb->cat(" -units PixelsPerInch -density ");
		sb->cat(st->latexscale);
		sb->cat(" -transparent \\#");
		// "FF" for 255 etc:
		sb->cat_hex(bg->r);
		sb->cat_hex(bg->g);
		sb->cat_hex(bg->b);
		sb->cat(" ");
		sb->cat(psfilename);
		sb->cat(" ");
		sb->cat(pngfilename);
		ret = system(sb->repr());
		unlink(psfilename);
		if(ret == -1 || WEXITSTATUS(ret) == 127)
			error("Failed to invoke convert command");
		ret = WEXITSTATUS(ret);
		if(ret != 0)
			error("Abnormal return code %d from convert", ret);
		
		delete sb;
	}
		
	SDL_Surface *surface = load_local_png(pngfilename, 1);
	
	ret = chdir(current_dir);
	if(ret != 0)
		error("Can't return to current directory");
	delete current_dir;
	
	return surface;
}
