#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

top = 0
sub = 0
def callback(path, event, which):
    global top, sub
#    print "Got callback: %s, %s" % (path, event)
    if which == 0:
        top = top + 1
    elif which == 1:
        sub = sub + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
os.mkdir ("temp_dir/a")

mon = gamin.WatchMonitor()
mon.watch_directory("temp_dir", callback, 0)
mon.watch_directory("temp_dir/a", callback, 1)
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir")
if top != 3:
    print "Error: top monitor got %d events insteads of 3" % (top)
time.sleep(1)
os.mkdir ("temp_dir/a/b")
time.sleep(1)
mon.handle_events()
mon.stop_watch("temp_dir/a")
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if sub != 3:
    print "Error: sub monitor got %d events insteads of 3" % (sub)
else:
    print "OK"
