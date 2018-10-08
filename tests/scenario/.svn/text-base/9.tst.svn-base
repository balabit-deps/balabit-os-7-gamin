mkdir /tmp/test_gamin
connect test
monfile /tmp/test_gamin
expect 2
#this should generate a Changed event...
wait
mkfile /tmp/test_gamin/foo
expect 1
wait
#this should not generate an event...
append /tmp/test_gamin/foo
#this should generate a Changed event...
rmfile /tmp/test_gamin/foo
expect 1
disconnect
rmdir /tmp/test_gamin
