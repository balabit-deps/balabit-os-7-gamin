#!/usr/bin/env python
#
# Checking DNotify registration/dregistration when monitoring a
# directory as a directory and also watching the parent directory,
# then stop watching the children directory and check we are still watching
# the parent one correctly
#
import gamin
import time
import os
import sys
import shutil

ok = 1
top = 0
top2 = 0
dbg = 0
db_expect = [ 51, 51, 52, 52 ]
expect = [gamin.GAMExists, gamin.GAMExists, gamin.GAMEndExist,
          gamin.GAMCreated]
expect2 = [gamin.GAMExists, gamin.GAMEndExist ]

def debug(path, type, data):
    global dbg, db_expect, ok

#    print "Got debug %s, %s, %s" % (path, type, data)
    if db_expect[dbg] != type:
        print "Error got debug event %d expected %d" % (type, db_expect[dbg])
	ok = 0
    dbg = dbg + 1

def callback(path, event, which):
    global top, expect, ok
#    print "Got callback: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if expect[top] != event:
        print "Error got event %d expected %d" % (expect[top], event)
	ok = 0
    top = top + 1

def callback2(path, event, which):
    global top2, expect2, ok
#    print "Got callback2: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if expect2[top2] != event:
        print "Error got event %d expected %d" % (expect2[top2], event)
	ok = 0
    top2 = top2 + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
os.mkdir ("temp_dir/a")

mon = gamin.WatchMonitor()
mon._debug_object("notify", debug, 0)
mon.watch_directory("temp_dir", callback, 0)
mon.watch_directory("temp_dir/a", callback2, 0)
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir/a")
time.sleep(1)
mon.handle_events()
open("temp_dir/b", "w").close()
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir")
time.sleep(1)
mon.handle_events()
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if top != 4:
    print "Error: monitor got %d events insteads of 4" % (top)
elif top2 != 2:
    print "Error: monitor got %d events insteads of 2" % (top2)
elif dbg != 4 and gamin.has_debug_api == 1:
    print "Error: debug got %d events insteads of 4" % (dbg)
elif ok == 1:
    print "OK"
