mkdir /tmp/test_gamin
mkfile /tmp/test_gamin/foo
connect test
mondir /tmp/test_gamin
expect 3
# for some reason if we don't wait here the server does not
# notify the changes made to test1 when it gets the dnotify event
wait
append /tmp/test_gamin/foo
expect 1
kill
events
sleep
append /tmp/test_gamin/foo
expect 1
rmfile /tmp/test_gamin/foo
expect 1
disconnect
rmdir /tmp/test_gamin
