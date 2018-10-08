#!/usr/bin/env python
#
# Checking DNotify registration/dregistration when monitoring a
# directory as a file
#

import gamin
import time
import os
import sys
import shutil

ok = 1
top = 0
dbg = 0
expect = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMCreated]

def debug(path, type, data):
    global dbg, ok

#    print "Got debug %s, %s, %s" % (path, type, data)
    dbg = dbg + 1
    ok = 0

def callback(path, event, which):
    global top, expect, ok
#    print "Got callback: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if expect[top] != event:
        print "Error got event %d expected %d" % (expect[top], event)
	ok = 0
    top = top + 1


if not os.access('/media/test', os.R_OK | os.X_OK | os.W_OK):
    print "missing access to /media/test, test skipped"
    print "OK"
    sys.exit(0)

shutil.rmtree ("/media/test/a", True)
mon = gamin.WatchMonitor()
mon._debug_object("notify", debug, 0)
mon.watch_directory("/media/test", callback, 0)
time.sleep(1)
mon.handle_events()
os.mkdir ("/media/test/a")
time.sleep(2)
mon.handle_events()
mon.stop_watch("/media/test")
mon.handle_events()
mon.disconnect()
del mon

if top != 3:
    print "Error: monitor got %d events insteads of 3" % (top)
elif dbg != 0 and gamin.has_debug_api == 1:
    print "Error: debug got %d kernel events insteads of 0" % (dbg)
elif ok == 1:
    print "OK"

shutil.rmtree ("/media/test/a", True)
