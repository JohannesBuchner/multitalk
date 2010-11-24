/* datatype.cpp - DMI - 28-Sept-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "datatype.h"

int to_screen_coords(int x);
int to_design_coords(int x);

typedef void *voidptr;
typedef char *charptr;

svector *split_list(const char *s, char sep)
{
	svector *v = new svector();
	const char *start = s, *ptr = s;
	
	while(1)
	{
		if(*ptr == '\0')
		{
			v->add(start, ptr - start);
			break;
		}
		else if(*ptr == sep)
		{
			v->add(start, ptr - start);
			start = ptr + 1;
		}
		ptr++;
	}
	return v;
}

/* intvector */

intvector::intvector()
{
	capacity = 5;
	data = new int[capacity];
	size = 0;
}

intvector::~intvector()
{
	delete[] data;
}

void intvector::expand_capacity()
{
	int *bigger = new int[capacity * 2];
	for(int i = 0; i < capacity; i++)
		bigger[i] = data[i];
	delete[] data;
	data = bigger;
	capacity *= 2;
}

void intvector::add(int n)
{
	if(size == capacity)
		expand_capacity();
	data[size++] = n;
}

void intvector::set(int pos, int n)
{
	if(pos < 0 || pos >= size)
		return;
	data[pos] = n;
}

void intvector::del(int pos)
{
	if(pos < 0 || pos >= size)
		return;
	for(int i = pos; i < size - 1; i++)
		data[i] = data[i + 1];
	size--;
}

int intvector::item(int pos)
{
	if(pos < 0 || pos >= size)
		return -1;
	
	return data[pos];
}

void intvector::push(int n)
{
	add(n);
}

int intvector::pop()
{
	if(size == 0)
		return -1;
	return data[--size];
}

int intvector::top()
{
	if(size == 0)
		return -1;
	return data[size - 1];
}

int intvector::count()
{
	return size;
}

/* charvector */

charvector::charvector()
{
	capacity = 10;
	data = new char[capacity];
	size = 0;
}

charvector::~charvector()
{
	delete[] data;
}

void charvector::expand_capacity()
{
	char *bigger = new char[capacity * 2];
	for(int i = 0; i < capacity; i++)
		bigger[i] = data[i];
	delete[] data;
	data = bigger;
	capacity *= 2;
}

void charvector::add(char c)
{
	if(size == capacity)
		expand_capacity();
	data[size++] = c;
}

void charvector::set(int pos, char c)
{
	if(pos < 0 || pos >= size)
		return;
	data[pos] = c;
}

void charvector::del(int pos)
{
	if(pos < 0 || pos >= size)
		return;
	for(int i = pos; i < size - 1; i++)
		data[i] = data[i + 1];
	size--;
}

char charvector::item(int pos)
{
	if(pos < 0 || pos >= size)
		return -1;
	
	return data[pos];
}

void charvector::push(char c)
{
	add(c);
}

char charvector::pop()
{
	if(size == 0)
		return -1;
	return data[--size];
}

char charvector::top()
{
	if(size == 0)
		return -1;
	return data[size - 1];
}

int charvector::count()
{
	return size;
}

/* pvector */

pvector::pvector()
{
	capacity = 10;
	data = new voidptr[capacity];
	size = 0;
}

pvector::~pvector()
{
	delete[] data;
}

void pvector::expand_capacity()
{
	void **new_data;
	
	capacity *= 2;
	new_data = new voidptr[capacity];
	for(int i = 0; i < size; i++)
	{
		new_data[i] = data[i];
	}
	delete[] data;
	data = new_data;
}

void pvector::add(void *x)
{
	if(size == capacity)
		expand_capacity();	
	data[size++] = x;
}

void pvector::set(int n, void *x)
{
	if(n < 0 || n >= size)
		return;
	data[n] = x;
}

void pvector::promote(int n)
{
	void *temp;
	
	if(n < 1 || n >= size)
		return;
	temp = data[n];
	for(int i = n - 1; i >= 0; i--)
		data[i + 1] = data[i];
	data[0] = temp;
}

void pvector::demote(int n)
{
	void *temp;
	
	if(n < 0 || n >= size - 1)
		return;
	temp = data[n];
	for(int i = n; i < size - 1; i++)
		data[i] = data[i + 1];
	data[size - 1] = temp;
}

void pvector::del(int n)
{
	if(n < 0 || n >= size)
		return;
	for(int i = n; i < size - 1; i++)
		data[i] = data[i + 1];
	size--;
}

int pvector::find(void *x)
{
	for(int i = 0; i < size; i++)
		if(data[i] == x)
			return i;
	return -1;
}

void *pvector::item(int n)
{
	if(n < 0 || n >= size)
		return NULL;
	
	return data[n];
}

void pvector::push(void *x)
{
	add(x);
}
		
void *pvector::pop()
{
	if(size == 0)
		return NULL;
	return data[--size];
}

void *pvector::top()
{
	if(size == 0)
		return NULL;
	return data[size - 1];
}

int pvector::count()
{
	return size;
}

void pvector::clear()
{
	size = 0;
}

/* svector */
		
svector::svector()
{
	capacity = 5;
	data = new charptr[5];
	size = 0;
}

svector::~svector()
{
	for(int i = 0; i < size; i++)
		delete[] data[i];
	
	delete[] data;
}

void svector::clear()
{
	for(int i = 0; i < size; i++)
		delete[] data[i];
	size = 0;
}

int svector::find(const char *s)
{
	for(int i = 0; i < size; i++)
		if(!strcmp(s, data[i]))
			return i;
	return -1;
}

int svector::find_ignore_case(const char *s)
{
	for(int i = 0; i < size; i++)
		if(!strcasecmp(s, data[i]))
			return i;
	return -1;
}

void svector::add(const char *s)
{
	if(size == capacity)
		expand_capacity();
	
	if(s == NULL || s[0] == '\0')
	{
		data[size] = new char[1];
		data[size][0] = '\0';
		size++;
		return;
	}
		
	data[size] = new char[strlen(s) + 1];
	strcpy(data[size], s);
	size++;
}

void svector::add_skipnewline(const char *s, int chars)
{
	if(size == capacity)
		expand_capacity();

	StringBuf *sb = new StringBuf();
	for(int i = 0; i < chars; i++)
	{
		if(s[i] != '\n')
			sb->cat(s[i]);
	}
	sb->cat('\0');
	data[size] = sb->compact();
	delete sb;
	size++;
}

void svector::add(const char *s, int chars)
{
	if(size == capacity)
		expand_capacity();
	
	if(s == NULL || s[0] == '\0')
		chars = 0;
		
	data[size] = new char[chars + 1];
	if(chars > 0)
	{
		strncpy(data[size], s, chars);
	}
	data[size][chars] = '\0';
	size++;
}

void svector::expand_capacity()
{
	char **new_data;
	
	capacity *= 2;
	new_data = new charptr[capacity];
	for(int i = 0; i < size; i++)
	{
		new_data[i] = data[i];
	}
	delete[] data;
	data = new_data;
}

const char *svector::item(int n)
{
	if(n < 0 || n >= size)
		return NULL;
	
	return data[n];
}

int svector::count()
{
	return size;
}

/* StringBuf */

StringBuf::StringBuf()
{
	capacity = 50;
	buf = new char[capacity];
	used = 0;
	frozen = NULL;
}

StringBuf::~StringBuf()
{
	delete[] buf;
	if(frozen != NULL)
		delete[] frozen;
}

void StringBuf::clear()
{
	// Keep the same capacity.
	used = 0;
	if(frozen != NULL)
	{
		delete[] frozen;
		frozen = NULL;
	}
}

int StringBuf::length()
{
	return used;
}

char *StringBuf::repr()
{
	if(frozen != NULL)
		delete[] frozen;
	frozen = new char[used + 1];
	if(used > 0)
		memcpy(frozen, buf, used);
	frozen[used] = '\0';
	return frozen;
}

char *StringBuf::compact()
{
	char *s = new char[used + 1];
	if(used > 0)
		memcpy(s, buf, used);
	s[used] = '\0';
	return s;
}

void StringBuf::check_expand(int required)
{
	if(required > capacity)
	{
		char *new_buf;
		int new_cap;

		new_cap = capacity * 2;	
		if(new_cap < required) new_cap = required * 2;
		new_buf = new char[new_cap];
		memcpy(new_buf, buf, used);
		capacity = new_cap;
		delete[] buf;
		buf = new_buf;
	}
}

void StringBuf::cat(int n)
{
	static char s[50];
	
	sprintf(s, "%d", n);
	cat(s);
}

void StringBuf::cat_hex(int n)
{
	static char s[10];
	
	sprintf(s, "%02X", n);
	cat(s);
}

void StringBuf::cat(double d)
{
	static char s[50];
	
	sprintf(s, "%f", d);
	cat(s);
}

void StringBuf::cat(const char *s)
{
	int len = strlen(s);
	check_expand(used + len);
	memcpy(buf + used, s, len);
	used += len;
}

void StringBuf::cat(char c)
{
	check_expand(used + 1);
	buf[used++] = c;
}

/* linefile */

int linefile::readline(FILE *fp, char *buf, int max_len)
{
	int len;
	
	if(feof(fp))
		return -1;
	buf[0] = '\0';
	if (fgets(buf, max_len, fp) == NULL) {
		len = -1;
	} else {
		len = strlen(buf);
		if(len > 0 && buf[len - 1] == '\n')
			buf[--len] = '\0';
	}
	return len;
}

int linefile::load(const char *filename)
{
	FILE *fp;
	char *buf;
	int len;
	const int MAX_LINE_LEN = 1000;
	
	fp = fopen(filename, "r");
	if(fp == NULL)
		return -1;
	
	buf = new char[MAX_LINE_LEN];
	while(!feof(fp))
	{
		len = readline(fp, buf, MAX_LINE_LEN);
		if(len == -1)
			break;
		// if(len > 0)
		v->add(buf);
	}
	
	delete[] buf;
	fclose(fp);
	return 0;
}

linefile::linefile()
{
	v = new svector();
}

int linefile::save(const char *filename)
{
	const char *s;
	FILE *fp;
	
	fp = fopen(filename, "w");
	if(!fp)
		return -1;
	for(int i = 0; i < v->count(); i++)
	{
		s = v->item(i);
		fprintf(fp, "%s\n", s);
	}
	fclose(fp);
	return 0;
}

linefile::~linefile()
{
	delete v;
}

int linefile::count()
{
	return v->count();
}

const char *linefile::getline(int n)
{
	if(n < 0 || n >= v->count())
		return NULL;	
	return v->item(n);
}

int linefile::search(char *line)
{
	for(int i = 0; i < v->count(); i++)
		if(!strcmp(line, v->item(i)))
			return 1;
	return 0;
}

void linefile::addline(const char *s)
{
	v->add(s);
}

/* graphfile */

graphfile::graphfile()
{
	typevec = new charvector();
	titlevec = new svector();
	xvec = new intvector();
	yvec = new intvector();
}

graphfile::~graphfile()
{
	delete typevec;
	delete titlevec;
	delete xvec;
	delete yvec;
}

int graphfile::load(const char *filename)
{
	const int max_line_len = 1000;
	linefile *lf;
	const char *s;
	int len, conversions;
	
	char type;
	char *title;
	int x, y;
	
	lf = new linefile();
	int ret = lf->load(filename);
	if(ret < 0)
		return ret;
	title = new char[max_line_len];
	for(int i = 0; i < lf->count(); i++)
	{
		s = lf->getline(i);
		len = strlen(s);
		if(len > max_line_len)
		{
			printf("Buffer overflow in graphfile::load()\n");
			exit(0);
		}
		if(s[0] == '@' || s[0] == '+')
		{
			int pos;
			conversions = sscanf(s, "%c%d\t%d\t%n", &type, &x, &y, &pos);
			if(type == '@')
			{
				x = to_screen_coords(x);
				y = to_screen_coords(y);
			}
			strcpy(title, s + pos);
			if(conversions == 3 || conversions == 4)
				add(type, title, x, y);
			else
			{
				printf("Error in graphfile::load()\n");
				exit(0);
			}
		}
	}
	delete lf;
	delete[] title;
	return 0;
}

int graphfile::save(const char *filename)
{
	const int max_line_len = 1000;
	linefile *lf;
	char *buf = new char[max_line_len];
	
	lf = new linefile();
	for(int i = 0; i < typevec->count(); i++)
	{
		if(strlen(titlevec->item(i)) > max_line_len - 100)
		{
			printf("Buffer overflow in graphfile::save()\n");
			exit(0);
		}
		if(typevec->item(i) == '@')
		{
			sprintf(buf, "%c%d\t%d\t%s", typevec->item(i),
					to_design_coords(xvec->item(i)),
					to_design_coords(yvec->item(i)), titlevec->item(i));
		}
		else
		{
			sprintf(buf, "%c%d\t%d\t%s", typevec->item(i),
					xvec->item(i), yvec->item(i), titlevec->item(i));
		}
		lf->addline(buf);
	}
	int ret = lf->save(filename);
	delete lf;
	delete[] buf;
	return ret;
}

int graphfile::count()
{
	return typevec->count(); // Arbitrary which vector we use here
}

void graphfile::add(char type, const char *title, int x, int y)
{
	typevec->add(type);
	titlevec->add(title);
	xvec->add(x);
	yvec->add(y);
}

const char *graphfile::get_title(int n)
{
	if(n < 0 || n >= titlevec->count())
		return NULL;
	return titlevec->item(n);
}

int graphfile::get_x(int n)
{
	if(n < 0 || n >= xvec->count())
		return -1;
	return xvec->item(n);
}

int graphfile::get_y(int n)
{
	if(n < 0 || n >= yvec->count())
		return -1;
	return yvec->item(n);
}

char graphfile::get_type(int n)
{
	if(n < 0 || n >= typevec->count())
		return '\0';
	return typevec->item(n);
}
		
/* dictionary */

dictionary::dictionary()
{
	names = new svector();
	values = new svector();
}

dictionary::~dictionary()
{
	delete names;
	delete values;
}

int dictionary::scan_equals(const char *line)
{
	int len = strlen(line);
	for(int i = 0; i < len; i++)
	{
		if(line[i] == '=')
			return i;
	}
	return -1;
}

int dictionary::load(const char *filename)
{
	linefile *lf;
	const char *line;
	int pos;
	
	lf = new linefile();
	int ret = lf->load(filename);
	if(ret < 0)
		return ret;
	for(int i = 0; i < lf->count(); i++)
	{
		line = lf->getline(i);
		if(line[0] == '#')
			continue;
		pos = scan_equals(line);
		if(pos < 1)
			continue;
		names->add(line, pos);
		values->add(line + pos + 1);
	}
	delete lf;
	return 0;
}

int dictionary::save(const char *filename)
{
	FILE *fp = fopen(filename, "w");
	if(fp == NULL)
		return -1;
	for(int i = 0; i < names->count(); i++)
	{
		fprintf(fp, "%s=%s\n", names->item(i), values->item(i));
	}
	fclose(fp);
	return 0;
}

int dictionary::count()
{
	return names->count();
}

const char *dictionary::get_name(int n)
{
	if(n < 0 || n >= names->count())
		return NULL;
	return names->item(n);
}

const char *dictionary::get_value(int n)
{
	if(n < 0 || n >= values->count())
		return NULL;
	return values->item(n);
}

const char *dictionary::lookup(const char *name)
{
	for(int i = 0; i < names->count(); i++)
	{
		if(!strcmp(names->item(i), name))
			return values->item(i);
	}
	return NULL;
}

const char *dictionary::lookup_ignore_case(const char *name)
{
	for(int i = 0; i < names->count(); i++)
	{
		if(!strcasecmp(names->item(i), name))
			return values->item(i);
	}
	return NULL;
}

void dictionary::add(const char *name, const char *value)
{
	names->add(name);
	values->add(value);
}
