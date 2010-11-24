all: multitalk

SYSPREFIX = /usr/local
USERPREFIX = ${HOME}

install:
	cp multitalk ${SYSPREFIX}/bin
	mkdir -p ${SYSPREFIX}/share/multitalk/fonts
	mkdir -p ${SYSPREFIX}/share/multitalk/styles
	mkdir -p ${SYSPREFIX}/share/multitalk/gfx
	cp -a fonts/* ${SYSPREFIX}/share/multitalk/fonts
	cp -a gfx/* ${SYSPREFIX}/share/multitalk/gfx
	cp -d styles/*.style ${SYSPREFIX}/share/multitalk/styles
#	cp -id styles/*.style ${SYSPREFIX}/share/multitalk/styles

userinstall:
	mkdir -p ${USERPREFIX}/bin
	cp multitalk ${USERPREFIX}/bin
	mkdir -p ${USERPREFIX}/.multitalk/fonts
	mkdir -p ${USERPREFIX}/.multitalk/styles
	mkdir -p ${USERPREFIX}/.multitalk/gfx
	cp -a fonts/* ${USERPREFIX}/.multitalk/fonts
	cp -a gfx/* ${USERPREFIX}/.multitalk/gfx
	cp -d styles/*.style ${USERPREFIX}/.multitalk/styles
#	cp -id styles/*.style ${USERPREFIX}/.multitalk/styles

SDL_CFLAGS=`sdl-config --cflags`
SDL_LIB=`sdl-config --libs`
CCFLAGS=-Wall -ansi -Wextra -pedantic -O3

multitalk: multitalk.o datatype.o sdltools.o parse.o graph.o style.o \
files.o render.o latex.o web.o config.o multitalk.h
	g++ ${CCFLAGS} -o multitalk multitalk.o datatype.o sdltools.o parse.o graph.o \
	style.o files.o render.o latex.o web.o config.o -L${HOME}/lib -lSDL_image \
	-lSDL_ttf \
	${SDL_LIB} -lSDL_gfx

multitalk.o : multitalk.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} multitalk.cpp

sdltools.o : sdltools.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} sdltools.cpp

render.o : render.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} render.cpp

latex.o : latex.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} latex.cpp

web.o : web.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} web.cpp

config.o : config.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} config.cpp

datatype.o : datatype.cpp datatype.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} datatype.cpp

graph.o : graph.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} graph.cpp

style.o : style.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} style.cpp

files.o : files.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} files.cpp

parse.o : parse.cpp multitalk.h
	g++ ${CCFLAGS} -I${HOME}/include -c ${SDL_CFLAGS} parse.cpp

clean:
	rm -f multitalk *.o
