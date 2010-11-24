
#include "sdltools.h"

class style
{
	public:
	
	char *name;
	
	// Colour palette indexes:
	int barcolour;
	int textcolour;
	int bgcolour;
	int linkcolour;
	int titlecolour;
	int headingcolour;
	int bullet1colour;
	int bullet2colour;
	int bullet3colour;
	int bordercolour;
	int foldcollapsedcolour;
	int foldexpandedcolour;
	int foldexposed1colour;
	int foldexposed2colour;
	int foldexposed3colour;
	int highlightcolour;
	int rulecolour;

	// Numerical values:
	int bullet1size;
	int bullet2size;
	int bullet3size;
	int titlesize;
	int textsize;
	int fixedsize;
	int linespacing;
	int titlespacing;
	int picturemargin;
	int topmargin;
	int bottommargin;
	int leftmargin;
	int foldmargin;
	int rightmargin;
	int latexwidth;
	int latexscale;
	int latexbaselinestretch;
	int latexspaceabove;
	int latexspacebelow;
	int ruleheight;
	int rulespaceabove;
	int rulespacebelow;
	int headspaceabove;
	int headspacebelow;
	
	// Percentages:
	int rulewidth;
	
	// Booleans:
	int underlinelinks;
	int enablebar;
	int bgbar;
	
	// Enumerations:
	int pictureborder;
	int slideborder;
	int barborder;

	// Fonts (filenames):
	const char *titlefont;
	const char *textfont;
	const char *fixedfont;
	const char *boldfont;
	const char *italicfont;
	
	TTF_Font *title_font, *heading_font, *text_font, *fixed_font, *bold_font, *italic_font;
	int title_ascent, text_ascent, fixed_ascent, bold_ascent, italic_ascent;
	int title_descent, text_descent, fixed_descent, bold_descent, italic_descent;
	int text_space_width, fixed_space_width;

	// Other filenames:
	svector *latexinclude;
	svector *latexpreinclude;
	
	// Pictures (filenames):
	const char *bullet1icon;
	const char *bullet2icon;
	const char *bullet3icon;
	const char *bgimage;
	const char *bgtexture;
	const char *foldcollapsedicon;
	const char *foldexpandedicon;

	SDL_Surface *bullet1, *bullet2, *bullet3;
	SDL_Surface *background, *texture;
	SDL_Surface *collapsedicon, *expandedicon;
	
	// An array of logos:
	logovector *logos;
	
	style(const char *name, style *inherit);
	void update(dictionary *d);
	
	private:
			
	void init_fonts(style *inherit);
	void parse_logo(const char *s);
};

stylevector *load_styles(Config *config);
int get_colour_index(const char *name);

