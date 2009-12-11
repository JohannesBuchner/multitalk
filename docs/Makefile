all: ps

ps: multitalk.ps

pdf: multitalk.pdf

%.pdf: %.ps
	ps2pdf -dPDFsettings=/prepress $<

%.ps: %.dvi
	${TEXDIR}/bin/dvips -o $@ $<
#	${TEXDIR}/bin/dvips -t a4 -o $@ $<
#	${TEXDIR}/bin/dvips -t landscape -t a4 -o $@ $<
	
%.dvi: %.tex
	latex $<

f:
	latex multitalk.tex
	${TEXDIR}/bin/dvips -o multitalk.ps multitalk.dvi
#	ps2pdf -dPDFsettings=/prepress multitalk.ps

clean:
	rm -f *.ps *.dvi *.aux *.log *.out *.toc

reallyclean: clean
	rm -f *.pdf
