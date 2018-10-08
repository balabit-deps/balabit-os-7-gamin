#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

top = 0
def callback(path, event):
    global top
#    print "Got callback: %s, %s" % (path, event)
    top = top + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
os.mkdir ("temp_dir/a")
open("temp_dir/b", "w").close()

mon = gamin.WatchMonitor()
mon.watch_directory("temp_dir", callback)
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir")
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
if top != 4:
    print "Error: top monitor got %d events insteads of 4" % (top)
    sys.exit(1)

print "OK"
