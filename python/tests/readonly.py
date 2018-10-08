#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

top = 0
ok = 1
expect = [gamin.GAMExists, gamin.GAMEndExist]

def callback(path, event):
    global top, expect, ok
#    print "Got callback: %s, %s" % (path, event)
    if expect[top] != event:
        print "Error got event %d expected %d" % (event, expect[top])
	ok = 0
    top = top + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
open("temp_dir/a", "w").close()
os.chmod ("temp_dir", 0000)
mon = gamin.WatchMonitor()
mon.watch_directory("temp_dir", callback)
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir")
mon.disconnect()
del mon
os.chmod ("temp_dir", 0755)
shutil.rmtree ("temp_dir", True)
if top != 2:
    print "Error: top monitor got %d events insteads of 4" % (top)
    sys.exit(1)

if ok:
    print "OK"
