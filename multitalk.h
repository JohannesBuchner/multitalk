/* multitalk.h - DMI - 29-Sept-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

class nodevector;
class displaylinevector;
class subimagevector;
class style;
struct logo;

struct Config
{
	char *talk_path, *graph_path, *project_dir, *latex_dir, *html_dir;
	char *sys_dir, *sys_style_dir, *sys_font_dir, *sys_image_dir;
	char *env_dir, *env_style_dir, *env_font_dir, *env_image_dir;
	char *proj_style_dir, *proj_font_dir;
	char *home_dir, *home_style_dir, *home_font_dir, *home_image_dir;
	char *sys_rc_dir, *home_rc_dir;
	char *caption;
};

class Options
{
	public:
			
	const char *latexcmd;
	const char *dvipscmd;
	const char *convertcmd;
	int warpsteps;
	
	Options();
	void update(dictionary *d);
};

enum DisplaylineType { TYPE_PLAIN, TYPE_FOLDED, TYPE_EXPANDED };

enum NodeType { NODE_LEAF, NODE_SLIDE, NODE_TREE, NODE_LATEX, NODE_VSPACE,
		NODE_RULE };

class node
{
	public:
			
	node();
	~node();
	
	int type;
	int card_mask;
	node *parent;         // Used for all except SLIDE
	
	char *line;           // Content (LEAF), heading (TREE), title (SLIDE)
	int space;            // VSPACE only
		
	svector *tex;         // LATEX only
	int align;            // LATEX only (for left-aligned, 1 for centred)
	
	nodevector *children; // SLIDE and TREE only
	int folded;           // TREE only
	
	char *hyperlink;      // NULL if not a hyperlink
	subimagevector *local_images;
};

struct decorations
{
	SDL_Surface *top, *bottom, *left, *right;
};

struct slide
{
	int deck_size, card;
	SDL_Surface *render, *scaled, *mini, *micro;
	decorations decor, mini_decor;
	int x, y; // Position in screen coords
	int scr_w, scr_h, des_w, des_h; // Dimensions in screen and design coords
	node *content;
	/* The slide's title is the content node's "line". The number of
		toplevel items is the content node's "num_children". */
	displaylinevector *repr; // Current on-screen line by line representation
	char *image_file;
	style *st;
	subimagevector *embedded_images;
	subimagevector *visible_images;
	int selected;
};

struct subimage
{
	char *path_name;
	int scr_x, scr_y, des_x, des_y; // Position in screen and design coords
	int scr_w, scr_h, des_w, des_h; // Dimensions in screen and design coords
	int card_mask;
	SDL_Surface *surface;
	char *hyperlink;	
};

struct DrawMode
{
	int ttmode, boldmode, italicmode;
	int current_text_colour_index;
	int lastshift;
};

class displayline
{
	public:
			
	displayline();
	~displayline();
			
	int line_num;
	int type; // TYPE_PLAIN, TYPE_FOLDED, TYPE_EXPANDED
	int exposed;
	int bullet; // 0 if none, 1 = large, 2 = medium, 3 = small, -1 if continued
	int prespace;
	int centred;
	int heading;
	int width; // Filled in for each line when the slide is measured
	int height, y; // height and y are both in design coords
	node *source; // Useful for the latter two types to deal with mouse clicks
	slide *link; // Hyperlink target
	int link_card; // Card to change to in linked slide's stack
	int highlighted;
	DrawMode initial_dm;
	SDL_Surface *import; // For "image lines" (e.g. Latex), alternative to line
	int rule;   // Flag indicating a rule, alternative to line
	char *line; // Actual text, after the folding handle (NULL if import used)
	// Note, text may still include these special chars: { $, *, /, \, % }
	// Note, line is also NULL for vertical spacers & rules
};

struct hotspot
{
	slide *sl;
	int card, unfold;
	int x1, y1, x2, y2;
};

class hotspotvector : public pvector // wrapper class
{
	public:
			
		void add(hotspot *x) { pvector::add((void *)x); }
		hotspot *item(int n) { return (hotspot *)pvector::item(n); }
};

class nodevector : public pvector // wrapper class
{
	public:
			
		void add(node *x) { pvector::add((void *)x); }
		void set(int n, node *x) { pvector::set(n, (void *)x); }		
		node *item(int n) { return (node *)pvector::item(n); }
		void push(node *x) { pvector::push((void *)x); }
		node *pop() { return (node *)pvector::pop(); }
		node *top() { return (node *)pvector::top(); }
};

class slidevector : public pvector // wrapper class
{
	public:
			
		void add(slide *x) { pvector::add((void *)x); }
		void set(int n, slide *x) { pvector::set(n, (void *)x); }		
		slide *item(int n) { return (slide *)pvector::item(n); }
		void push(slide *x) { pvector::push((void *)x); }
		slide *pop() { return (slide *)pvector::pop(); }
		slide *top() { return (slide *)pvector::top(); }
		int find(slide *x) { return pvector::find((void *)x); }
};

class subimagevector : public pvector // wrapper class
{
	public:
			
		void add(subimage *x) { pvector::add((void *)x); }
		void set(int n, subimage *x) { pvector::set(n, (void *)x); }		
		subimage *item(int n) { return (subimage *)pvector::item(n); }
		void push(subimage *x) { pvector::push((void *)x); }
		subimage *pop() { return (subimage *)pvector::pop(); }
		subimage *top() { return (subimage *)pvector::top(); }
};

class displaylinevector : public pvector // wrapper class
{
	public:
			
		void add(displayline *x) { pvector::add((void *)x); }
		void set(int n, displayline *x) { pvector::set(n, (void *)x); }		
		displayline *item(int n) { return (displayline *)pvector::item(n); }
		void push(displayline *x) { pvector::push((void *)x); }
		displayline *pop() { return (displayline *)pvector::pop(); }
		displayline *top() { return (displayline *)pvector::top(); }
};

class stylevector : public pvector // wrapper class
{
	public:
			
		void add(style *x) { pvector::add((void *)x); }
		void set(int n, style *x) { pvector::set(n, (void *)x); }		
		style *item(int n) { return (style *)pvector::item(n); }
		void push(style *x) { pvector::push((void *)x); }
		style *pop() { return (style *)pvector::pop(); }
		style *top() { return (style *)pvector::top(); }
		
		style *default_style();
		style *lookup_style(const char *name);
};

class logovector : public pvector // wrapper class
{
	public:
			
		void add(logo *x) { pvector::add((void *)x); }
		void set(int n, logo *x) { pvector::set(n, (void *)x); }		
		logo *item(int n) { return (logo *)pvector::item(n); }
		void push(logo *x) { pvector::push((void *)x); }
		logo *pop() { return (logo *)pvector::pop(); }
		logo *top() { return (logo *)pvector::top(); }
};

class Uint32vector : public intvector // wrapper class
{
	public:

		void add(Uint32 n) { intvector::add((int)n); }
		void set(int pos, Uint32 n) { intvector::set(pos, (int)n); }
		Uint32 item(int pos) { return (Uint32)intvector::item(pos); }
		void push(Uint32 n) { intvector::push((int)n); }
		Uint32 pop() { return (Uint32)intvector::pop(); }
		Uint32 top() { return (Uint32)intvector::top(); }
};

class SDL_Color_pvector : public pvector // wrapper class
{
	public:
			
		void add(SDL_Color *x) { pvector::add((void *)x); }
		void set(int n, SDL_Color *x) { pvector::set(n, (void *)x); }		
		SDL_Color *item(int n) { return (SDL_Color *)pvector::item(n); }
		int find(SDL_Color *x) { return pvector::find((void *)x); }
};

class colour_definition
{
	public:
			
	SDL_Color black_text;
	SDL_Color red_text;
	SDL_Color purple_text;
	
	Uint32 white_fill;
	Uint32 light_grey_fill;
	Uint32 grey_fill;
	Uint32 dark_grey_fill;
	Uint32 black_fill;
	Uint32 light_green_fill;
	Uint32 cyan_fill;
	Uint32 yellow_fill;
	Uint32 red_fill;
	Uint32 orange_fill;
	Uint32 purple_fill;
	
	Uint32 white_pen;
	Uint32 black_pen;
	Uint32 red_pen;
	Uint32 orange_pen;
	Uint32 yellow_pen;
	Uint32 green_pen;
	Uint32 light_green_pen;
	Uint32 sky_pen;
	Uint32 cyan_pen;
	Uint32 blue_pen;
	Uint32 grey_pen;
	Uint32 dark_grey_pen;
	Uint32 light_grey_pen;
	Uint32 brown_pen;
	Uint32 purple_pen;
	Uint32 pink_pen;
	Uint32 magenta_pen;
	
	svector *names;
	Uint32vector *pens; // For drawing
	SDL_Color_pvector *inks; // For text
	Uint32vector *fills;
	
	colour_definition();
	int search_add(const char *hex);
};

struct logo
{
	char *image_file;
	int x, y;
	SDL_Surface *image;
};

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

const int UNKNOWN_POS = -66666;

extern int SCREEN_WIDTH, SCREEN_HEIGHT;
extern int design_width, design_height;
extern int scalep, scaleq;
/* Everything is held in screen coordinates, so only the following need
	scaling: the slide bitmaps themselves, positions of hotspots within
	slides (text lines for hyperlinks & fold/unfold, plus subimages for
	movement and hyperlinking), and the slide positions in the .graph file. */

extern int CARD_EDGE;
extern const int TITLE_EDGE;
extern colour_definition *colour;
extern SDL_Surface *screen;
extern int fullscreen;
extern int export_html;
extern int canvas_colour;

extern int debug;
const int DEBUG_BASELINES = 1 << 0;
const int DEBUG_PATHS = 1 << 1;

// Typedefs:
typedef const char *constCharPtr;

// From sdltools.cpp:
void init_sdl(const char *caption, int offscreen);
SDL_Surface *load_png(const char *filename, int alpha);
SDL_Surface *load_local_png(const char *filename, int alpha);
void init_colours();
TTF_Font *init_font(Config *config, const char *font_file, int size);
int render_text(const char *s, TTF_Font *font, SDL_Color *color,
		SDL_Surface *surface, int x, int y);
int render_text(const char *s, TTF_Font *font, int colour_index,
		SDL_Surface *surface, int x, int y, int underlined);
SDL_Surface *alloc_surface(int w, int h);
void clear_surface(SDL_Surface *surface, Uint32 co);

// From parse.cpp:
slidevector *parse_talk(linefile *talk_lf, stylevector *style_list);
void flatten_all(slidevector *talk);
void flatten(slide *sl, slidevector *talk);
void free_node(node *context);
slide *find_title(slidevector *talk, const char *title, int *card);
void peek_designsize(linefile *talk_lf);
void fold_all(slide *sl);
void unfold_all(slide *sl);
int is_foldable(slide *sl);

// From graph.cpp:
void save_slide_positions(Config *config, slidevector *talk);
void load_slide_positions(Config *config, slidevector *talk);

// From style.cpp:
stylevector *load_styles(Config *config);
int get_colour_index(const char *name);

// From style.cpp for config.cpp:
void set_integer_property(dictionary *d, const char *name, int *dest);
void set_string_property(dictionary *d, const char *name, constCharPtr *dest);

// From multitalk.cpp
void error(const char *format, ...);
SDL_Surface *alloc_surface(int w, int h);
int to_screen_coords(int x);
int to_design_coords(int x);

// From multitalk.cpp for web.cpp
extern SDL_Surface *radar;
extern int RADAR_WIDTH, RADAR_HEIGHT, RADAR_MAG;
void clear_radar();
void render_radar(int cx, int cy, hotspotvector *hsv = NULL,
		int slide_num = -1);
void goto_card(slide *sl, int n);
void redraft(slide *sl);

// From files.cpp
Config *init_paths(const char *talk_ref);
char *combine_path(const char *path, const char *file);
int fexists(const char *file_path);
svector *load_text_file(const char *filename);
char *sdup(const char *s);
char *expand_tilde(const char *s);

// From config.cpp
Options *init_options(Config *config);

// From render.cpp
void render_slide(slide *sl);
void measure_slide(slide *sl);
void render_line(slide *sl, displayline *out, DrawMode *dm,
		SDL_Surface *surface);
void copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void mini_copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void micro_copy_all_to_screen(slidevector *render_list, int viewx, int viewy);
void pointer(int x, int y);
void reallocate_surfaces(slide *sl);
void scale(slide *sl);

// From latex.cpp
SDL_Surface *gen_latex(svector *tex, style *st);

// From web.cpp
void gen_html(slidevector *talk);
