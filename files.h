#include "sdltools.h"

char *get_path(const char *s);
Config *init_paths(const char *talk_ref);
char *combine_path(const char *path, const char *file);
int fexists(const char *file_path);
svector *load_text_file(const char *filename);
char *sdup(const char *s);
char *expand_tilde(const char *s);

