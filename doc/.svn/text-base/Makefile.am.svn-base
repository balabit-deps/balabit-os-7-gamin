## Process this file with automake to produce Makefile.in
EXTRA_DIST= client_server.fig  client_server.gif \
	    server_structs.fig  server_structs.gif

all: web $(top_srcdir)/NEWS

PAGES= contacts.html downloads.html index.html overview.html \
       devel.html news.html using.html security.html internals.html

web: $(PAGES)

$(PAGES): gamin.html site.xsl
	-@(if [ -x $(bindir)/xsltproc ] ; then \
	  echo "Rebuilding the HTML Web pages from gamin.html" ; \
	  $(bindir)/xsltproc --nonet -o index.html --html $(top_srcdir)/doc/site.xsl $(top_srcdir)/doc/gamin.html ; fi );
	-@(if [ -x $(bindir)/xmllint ] ; then \
	  echo "Validating the HTML Web pages" ; \
	  $(bindir)/xmllint --nonet --valid --noout $(PAGES) ; fi );

$(top_srcdir)/NEWS: $(top_srcdir)/doc/news.xsl $(top_srcdir)/doc/news.html
	-@(if [ -x $(bindir)/xsltproc ] ; then \
	  echo "Rebuilding the NEWS file" ; \
	  $(bindir)/xsltproc --nonet $(top_srcdir)/doc/news.xsl $(top_srcdir)/doc/news.html > $(top_srcdir)/NEWS ; fi );

