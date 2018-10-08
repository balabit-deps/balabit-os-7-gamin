#!/usr/bin/env python

import gamin
import time

def callback(path, event):
    #print "Got callback: %s, %s" % (path, event)
    pass

mon = gamin.WatchMonitor()
mon.watch_directory(".", callback)
mon.handle_one_event()
time.sleep(1)
mon.handle_events()
mon.stop_watch(".")
mon.disconnect()
del mon

print "OK"
