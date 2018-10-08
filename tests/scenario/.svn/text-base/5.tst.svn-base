mkdir /tmp/test_gamin
mkfile /tmp/test_gamin/foo
connect test
monfile /tmp/test_gamin/foo
expect 2
wait
append /tmp/test_gamin/foo
expect 1
disconnect
rmfile /tmp/test_gamin/foo
rmdir /tmp/test_gamin
