#!/usr/bin/env python
#
# Checking DNotify registration/dregistration when monitoring a
# file as a file, then remove the file, then recreate it as a directory
#
import gamin
import time
import os
import sys
import shutil

ok = 1
top = 0
dbg = 0
db_expect = [ 51, 52, 51, 52 ]
expect = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMDeleted,
          gamin.GAMCreated]

def debug(path, type, data):
    global dbg, db_expect, ok

#    print "Got debug %s, %s, %s" % (path, type, data)
    if dbg < 2 and path[-8:] != "temp_dir":
        print "Error got debug path unexpected %s" % (path)
	ok = 0
    if dbg >= 2 and path[-10:] != "temp_dir/a":
        print "Error got debug path unexpected %s" % (path)
	ok = 0
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

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
open("temp_dir/a", "w").close()

mon = gamin.WatchMonitor()
mon._debug_object("notify", debug, 0)
mon.watch_file("temp_dir/a", callback, 0)
time.sleep(1)
mon.handle_events()
os.unlink("temp_dir/a")
time.sleep(1)
mon.handle_events()
os.mkdir("temp_dir/a")
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir/a")
time.sleep(1)
mon.handle_events()
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if top != 4:
    print "Error: monitor got %d events insteads of 4" % (top)
elif dbg != 4 and gamin.has_debug_api == 1:
    print "Error: debug got %d events insteads of 4" % (dbg)
elif ok == 1:
    print "OK"
