#!/usr/bin/env python

import gamin
import time

nb_callbacks=0

def callback(path, event):
    global nb_callbacks
#    print "Got callback: %s, %s" % (path, event)
    nb_callbacks = nb_callbacks + 1

mon = gamin.WatchMonitor()
mon.watch_directory(".", callback)
time.sleep(1)
ret = mon.event_pending()
if ret > 0:
    ret = mon.handle_events()
mon.stop_watch(".")
mon.disconnect()
del mon
if nb_callbacks > 2:
    print 'OK'
else:
    print 'error'
