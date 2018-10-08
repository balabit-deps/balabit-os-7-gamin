PYTESTS=        		\
		basic.py	\
		basic2.py	\
		basic3.py	\
		basic4.py	\
		basic5.py	\
		basic6.py	\
		bigfile.py	\
		noexists.py	\
		dnotify.py	\
		dnotify2.py	\
		dnotify3.py	\
		dnotify4.py	\
		dnotify5.py	\
		dnotify6.py	\
		dnotify7.py	\
		dnotify8.py	\
		dnotify9.py	\
		dnotify10.py	\
		dnotify11.py	\
		dnotify12.py	\
		dnotify13.py	\
		dnotify15.py	\
		flood.py	\
		flood2.py	\
		flood3.py	\
		flood4.py	\
		level.py	\
		multiple.py	\
		multiple2.py	\
		multiple3.py	\
		nokernel.py 	\
		readonly.py

EXTRA_DIST = $(PYTESTS)

if WITH_PYTHON
check-local: tests
tests: $(PYTESTS)
	@(LD_LIBRARY_PATH="..:../.libs:../libgamin:../libgamin/.libs"; \
	  export LD_LIBRARY_PATH; \
	  PYTHONPATH="..:../.libs:../libgamin:../libgamin/.libs:$(srcdir)/..:$$PYTHONPATH" ; \
	  export PYTHONPATH; \
	  unset GAM_DEBUG; \
	  GAMIN_DEBUG_SERVER="../../server/gam_server" ;\
	  export GAMIN_DEBUG_SERVER ;\
	  GAM_CLIENT_ID=test; \
	  export GAM_CLIENT_ID; \
	  for test in $(PYTESTS) ; \
	  do echo $$test ; \
	  log=`$(PYTHON) $(srcdir)/$$test` ; \
	  if [ "`echo $$log | grep OK`" = "" ] ; then \
	  echo "-- $$test" ; echo "$$log" ; fi ; done)
else
tests:
endif
