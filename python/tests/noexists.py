#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

nb_callbacks=0

def callback(path, event):
    global nb_callbacks
#    print "Got callback: %s, %s" % (path, event)
    nb_callbacks = nb_callbacks + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")

mon = gamin.WatchMonitor()
mon.no_exists()
mon.watch_directory("temp_dir", callback)
time.sleep(1)
ret = mon.event_pending()
if ret != 0:
    print 'error : no event expected'
    shutil.rmtree ("temp_dir", True)
    sys.exit(1)

os.mkdir ("temp_dir/a")
time.sleep(1)
ret = mon.event_pending()
if ret != 1:
    print 'error : created event expected'
    shutil.rmtree ("temp_dir", True)
    sys.exit(1)

mon.handle_one_event()
shutil.rmtree ("temp_dir/a", True)
time.sleep(1)
ret = mon.event_pending()
if ret != 1:
    print 'error : deleted event expected'
    shutil.rmtree ("temp_dir", True)
    sys.exit(1)

ret = mon.handle_events()
mon.stop_watch("temp_dir")
mon.disconnect()
del mon
if nb_callbacks == 2:
    print 'OK'
else:
    print 'error'
