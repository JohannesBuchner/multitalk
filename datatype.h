/* datatype.h - DMI - 28-Sept-2006

Copyright (C) 2006 David Ingram

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (version 2) as
published by the Free Software Foundation. */

class intvector
{
	public:

		intvector();
		~intvector();
		
		void add(int n);
		void set(int pos, int n);
		void del(int pos);
		
		int item(int pos);
			
		void push(int n);
		int pop();
		int top();
		
		int count();
	
	private:
		
		int capacity;
		int size;
		int *data;
		
		void expand_capacity();
};

class charvector
{
	public:

		charvector();
		~charvector();
		
		void add(char c);
		void set(int pos, char c);
		void del(int pos);
		
		char item(int pos);
			
		void push(char c);
		char pop();
		char top();
		
		int count();
	
	private:
		
		int capacity;
		int size;
		char *data;
		
		void expand_capacity();
};

class pvector // void pointer vector / stack
{
	public:
			
		pvector();
		~pvector();
		
		void add(void *x);
		void set(int n, void *x);
		void del(int n);
		
		void *item(int n);
		int find(void *x);
		
		void push(void *x);
		void *pop();
		void *top();
		void promote(int n);
		void demote(int n);
		
		int count();
		void clear();
	
	private:
			
		int capacity;
		int size;
		void **data;
		
		void expand_capacity();
};

class cpvector : public pvector // wrapper class for char ptrs
{
	public:
			
		void add(char *x) { pvector::add((void *)x); }
		void set(int n, char *x) { pvector::set(n, (void *)x); }		
		char *item(int n) { return (char *)pvector::item(n); }
		void push(char *x) { pvector::push((void *)x); }
		char *pop() { return (char *)pvector::pop(); }
		char *top() { return (char *)pvector::top(); }
};

/* An svector is different from a cpvector, because it allocates
	storage for strings which are added, performs deep copies, and
	frees them when it is destroyed:
*/

class svector
{
	public:
			
		svector();
		~svector();
		
		void add(const char *s);
		void add(const char *s, int chars);
		void add_skipnewline(const char *s, int chars);
		const char *item(int n);
		
		int count();
		void clear();
		int find(const char *s);
		int find_ignore_case(const char *s);
	
	private:
			
		int capacity;
		int size;
		char **data;
		
		void expand_capacity();
};

class StringBuf
{
	public:
			
		StringBuf();
		~StringBuf();
	
		char *compact();
		/* The string returned by compact() must be deleted by the caller,
			and is *not* removed when the StringBuf itself is deleted. */
					
		char *repr();
		/* The internal buffer returned by repr() must *not* be deleted by
			the caller. It is removed automatically when the StringBuf is
			deleted, and is also overwritted by each subsequent call to repr(). */
		
		void cat(const char *s);
		void cat(char c);
		void cat(int n);
		void cat(double d);
		void cat_hex(int n);
		
		void clear();
		int length();
		
	private:
			
		char *buf; // Internal buffer - not null terminated
		int used, capacity;
		char *frozen; // Used by repr function
		
		void check_expand(int required);
};

class linefile
{
	public:
			
		linefile();
		~linefile();

		// load and save return 0 if OK, or -1 on error:
		int load(const char *filename);
		int save(const char *filename);
				
		int count();
		
		const char *getline(int n);
		void addline(const char *s);
		int search(char *line);
	
	private:
			
		svector *v;
	
		int readline(FILE *fp, char *buf, int max_len);
};

class graphfile
{
	public:
			
		graphfile();
		~graphfile();

		// load and save return 0 if OK, or -1 on error:
		int load(const char *filename);
		int save(const char *filename);
				
		int count();
		void add(char type, const char *title, int x, int y);
		
		const char *get_title(int n);
		int get_x(int n);
		int get_y(int n);
		char get_type(int n);
			
	private:
	
		svector *titlevec;
		intvector *xvec;
		intvector *yvec;
		charvector *typevec;
};

class dictionary
{
	public:
			
		dictionary();
		~dictionary();

		int load(const char *filename);
		int save(const char *filename);
				
		int count();
		const char *get_name(int n);
		const char *get_value(int n);
		const char *lookup(const char *name);
		const char *lookup_ignore_case(const char *name);
		void add(const char *name, const char *value);
			
	private:
			
		svector *names;
		svector *values;
		
		int scan_equals(const char *line);
};

svector *split_list(const char *s, char sep);
