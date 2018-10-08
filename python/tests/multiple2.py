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

main = gamin.WatchMonitor()
main.watch_directory("temp_dir", callback, 11)

i = 0
while i < 10:
    mon = gamin.WatchMonitor()
    mon.watch_directory("temp_dir", callback, i)
    mons.append(mon)
    i = i + 1

time.sleep(1)
main.handle_events()
for mon in mons:
    mon.handle_events()

os.mkdir ("temp_dir/a")
os.mkdir ("temp_dir/b")
time.sleep(1)
main.handle_events()
for mon in mons:
    mon.handle_events()
    mon.stop_watch("temp_dir")
    mon.disconnect()

i = 0
while i < 10:
    if not nb_callbacks.has_key(i):
        print "Error: monitor %d didn't got events" % (i)
	sys.exit(1)
    if nb_callbacks[i] != 4:
        print "Error: monitor %d got %d out of 4 events" % (i, nb_callbacks[i])
	sys.exit(1)
    i = i + 1

shutil.rmtree ("temp_dir/a", True)
shutil.rmtree ("temp_dir/b", True)
time.sleep(1)
main.handle_events()
main.stop_watch("temp_dir")
main.disconnect()

shutil.rmtree ("temp_dir", True)

if not nb_callbacks.has_key(11):
    print "Error: main monitor didn't got events"
    sys.exit(1)
if nb_callbacks[11] != 6:
    print "Error: main monitor got %d out of 6 events" % (nb_callbacks[11])
    sys.exit(1)
print "OK"
