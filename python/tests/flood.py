#!/usr/bin/env python
#
# Check the flood control under dnotify
# Scenario: the main thread watch a file, a second thread is created
#  writing 10 times a sec to that file, then after 5 seconds it stops
#  and stops modifying the file for 6 seconds.
#  the kernel monitoring should be stopped within one second in the
#  first phase and then reactivated after a few seconds in the second
#  phase.
import shutil
import os
import sys
import signal
import gamin
import time
import thread

threads = 0

# create a 4k block
block = '1234'
i = 0
while i < 10:
    block = block + block
    i = i + 1

def wait_ms(ms = 100):
    delay = 0.001
    delay = delay * ms
    time.sleep(delay)

def resource_update_thread(filename):
    global threads
    global block

    threads = threads + 1
#    print "%s active" % (filename)
    f = open(filename, "w")
    f.write(block)
    i = 0
    while i < 50:
        wait_ms(100)
	i = i + 1
	f.write(block)
#    print "%s quiet" % (filename)
    wait_ms(10000)
    f.write(block)
    f.close()
    wait_ms(1000)
    threads = threads - 1
    thread.exit()

ok = 1
top = 0
dbg = 0
db_expect = [ 51, 54, 55, 52 ]
expect = [gamin.GAMExists, gamin.GAMEndExist]

def debug(path, type, data):
    global dbg, db_expect, ok

#    print "Got debug %s, %s, %s" % (path, type, data)
    if path[-8:] != "temp_dir":
        print "Error got debug path unexpected %s" % (path)
	ok = 0
    if db_expect[dbg] != type:
        print "Error got debug event %d expected %d" % (type, db_expect[dbg])
	ok = 0
    dbg = dbg + 1

def callback(path, event, which):
    global top, expect, ok
    print "Got callback: %s, %s" % (path, event)
    if event == gamin.GAMAcknowledge:
        return
    if top < 2:
	if expect[top] != event:
	    print "Error got event %d expected %d" % (event, expect[top])
	    ok = 0
    elif event != gamin.GAMChanged:
	print "Error got event %d expected %d" % (event, gamin.GAMChanged)
	ok = 0
    top = top + 1

shutil.rmtree ("temp_dir", True)
os.mkdir ("temp_dir")
f = open("temp_dir/a", "w").close()
thread.start_new_thread(resource_update_thread, ("temp_dir/a",))

mon = gamin.WatchMonitor()
mon._debug_object("notify", debug, 0)
mon.watch_file("temp_dir/a", callback, 0)

# wait until the thread finishes, collecting events
wait_ms(100)
while threads > 0:
    mon.handle_events()
    wait_ms(100)

#print "all threads terminated, exiting ..."

mon.stop_watch("temp_dir/a")
time.sleep(1)
mon.handle_events()
mon.disconnect()
del mon
shutil.rmtree ("temp_dir", True)

if top <= 2:
    print "Error: monitor got only %d events" % (top)
elif top >= 15:
    print "Error: event flow didn't worked properly, gor %d events" % (top)
elif dbg != 4 and gamin.has_debug_api == 1:
    print "Error: debug got %d events insteads of 4" % (dbg)
elif ok == 1:
    print "OK"
