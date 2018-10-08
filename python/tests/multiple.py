#!/usr/bin/env python

import gamin
import time
import os
import sys
import shutil

nb_callbacks={}
mons = []

def callback(path, event, number):
    global nb_callbacks
#    print "Got callback: %d, %s, %s" % (number, path, event)
    if nb_callbacks.has_key(number):
        nb_callbacks[number] = nb_callbacks[number] + 1
    else:
	nb_callbacks[number] = 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")

i = 0
while i < 10:
    mon = gamin.WatchMonitor()
    mon.watch_directory("temp_dir", callback, i)
    mons.append(mon)
    i = i + 1

time.sleep(1)
for mon in mons:
    mon.handle_events()

os.mkdir ("temp_dir/a")
os.mkdir ("temp_dir/b")
time.sleep(1)
for mon in mons:
    mon.handle_events()
    mon.stop_watch("temp_dir")
    mon.disconnect()

shutil.rmtree ("temp_dir", True)

i = 0
while i < 10:
    if not nb_callbacks.has_key(i):
        print "Error: monitor %d didn't got events" % (i)
	sys.exit(1)
    if nb_callbacks[i] != 4:
        print "Error: monitor %d got %d out of 4 events" % (i, nb_callbacks[i])
	sys.exit(1)
    i = i + 1

print "OK"
