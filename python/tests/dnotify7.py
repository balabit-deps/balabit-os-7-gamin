#!/usr/bin/env python
#
# Checking DNotify registration/dregistration when monitoring a
# file as a file, then remove the file, then recreate it as a file
# in addition the directory is being watched, as a rsult internally
# the dnotify monitoring sequence is different since we keep
#
import gamin
import time
import os
import sys
import shutil

ok = 1
dbg = 0
db_expect = [ 51, # directory watch
              53, # file watch count incremented
	      53, # file disapear count decremented
	      53, # file recreated
	      53, # removal of watch on file
	      52  # removal of watch on directory
	      ]
top_f = 0
expect_f = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMDeleted,
            gamin.GAMCreated]
top_d = 0
expect_d = [gamin.GAMExists,  # directory exists
            gamin.GAMExists,  # file a exists
	    gamin.GAMEndExist,# end listing
	    gamin.GAMDeleted, # file a removed
            gamin.GAMCreated  # file a created
	   ] 

def debug(path, type, data):
    global dbg, db_expect, ok

#    print "Got debug %s, %s, %s" % (path, type, data)
    if path[-8:] != "temp_dir":
        print "Error got debug path unexpected %s" % (path)
	ok = 0
    if db_expect[dbg] != type:
        print "Error got debug event %d expected %d" % (type, db_expect[dbg])
	ok = 0
    dbg = dbg + 1

def callback_file(path, event):
    global top_f, expect_f, ok
#    print "Got callback: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if expect_f[top_f] != event:
        print "Error got file event %d expected %d" % (expect_f[top_f], event)
	ok = 0
    top_f = top_f + 1

def callback_dir(path, event):
    global top_d, expect_d, ok
#    print "Got callback: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if expect_d[top_d] != event:
        print "Error got dir event %d expected %d" % (expect_d[top_d], event)
	ok = 0
    top_d = top_d + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
open("temp_dir/a", "w").close()

mon = gamin.WatchMonitor()
mon._debug_object("notify", debug, 0)
mon.watch_directory("temp_dir", callback_dir)
mon.watch_file("temp_dir/a", callback_file)
time.sleep(1)
mon.handle_events()
os.unlink("temp_dir/a")
time.sleep(1)
mon.handle_events()
open("temp_dir/a", "w").close()
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir/a")
mon.stop_watch("temp_dir")
time.sleep(1)
mon.handle_events()
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if top_f != 4:
    print "Error: file monitor got %d events insteads of 4" % (top_f)
if top_d != 5:
    print "Error: dir monitor got %d events insteads of 4" % (top_d)
elif dbg != 6 and gamin.has_debug_api == 1:
    print "Error: debug got %d events insteads of 6" % (dbg)
elif ok == 1:
    print "OK"
