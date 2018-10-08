#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

top = 0
ok = 1
expect = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMDeleted]
def callback(path, event):
    global top, expect, ok
#    print "Got callback: %s, %s" % (path, event)
    if expect[top] != event:
        print "Error got event %d expected %d" % (event, expect[top])
	ok = 0
    top = top + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
fd = os.open("temp_dir/bigfile", os.O_WRONLY | os.O_CREAT)
os.lseek(fd, 2200000000L, 0)
os.write(fd, "foo")
os.close(fd)

mon = gamin.WatchMonitor()
mon.watch_file("temp_dir/bigfile", callback)
time.sleep(1)
mon.handle_events()
os.unlink("temp_dir/bigfile")
time.sleep(1)
mon.handle_events()
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if top != 3:
    print "Error: top monitor got %d events insteads of 3" % (top)
    sys.exit(1)

if ok == 1:
    print "OK"
