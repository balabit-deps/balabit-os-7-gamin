#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

ok = 1
expect0 = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMCreated]
nb0 = 0
expect1 = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMCreated,
           gamin.GAMCreated]
nb1 = 0
expect2 = [gamin.GAMExists, gamin.GAMEndExist, gamin.GAMCreated,
           gamin.GAMCreated, gamin.GAMDeleted]
nb2 = 0
def callback(path, event, which):
    global ok
    global expect0, nb0
    global expect1, nb1
    global expect2, nb2

#    print "Got callback on %d: %s, %s" % (which, path, event)
    if event == gamin.GAMAcknowledge:
        return
    if which == 0:
        if event != expect0[nb0]:
	    print "Error: monitor %d got event %d expected %d" % (which, event,
	          expect0[nb0])
            ok = 0
        nb0 = nb0 + 1
    elif which == 1:
        if event != expect1[nb1]:
	    print "Error: monitor %d got event %d expected %d" % (which, event,
	          expect1[nb1])
            ok = 0
        nb1 = nb1 + 1
    elif which == 2:
        if event != expect2[nb2]:
	    print "Error: monitor %d got event %d expected %d" % (which, event,
	          expect2[nb2])
            ok = 0
        nb2 = nb2 + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")

mon = gamin.WatchMonitor()
watch0 = mon.watch_directory("temp_dir", callback, 0)
watch1 = mon.watch_directory("temp_dir", callback, 1)
watch2 = mon.watch_directory("temp_dir", callback, 2)
time.sleep(1)
mon.handle_events()

open("temp_dir/a", "w").close()
time.sleep(1)
mon.handle_events()
watch0.cancel()

open("temp_dir/b", "w").close()
time.sleep(1)
mon.handle_events()
watch1.cancel()

os.unlink("temp_dir/a")
time.sleep(1)
mon.handle_events()

mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if nb0 != len(expect0):
    print "Error: monitor 0 got %d events, expecting %d" % (nb0, len(expect0))
    ok = 0
if nb1 != len(expect1):
    print "Error: monitor 1 got %d events, expecting %d" % (nb1, len(expect1))
    ok = 0
if nb2 != len(expect2):
    print "Error: monitor 2 got %d events, expecting %d" % (nb2, len(expect2))
    ok = 0
if ok:
    print "OK"
