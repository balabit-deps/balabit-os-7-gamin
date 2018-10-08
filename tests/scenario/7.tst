mkdir /tmp/test_gamin
connect test
monfile /tmp/test_gamin/missing
expect 2
mkfile /tmp/test_gamin/missing
expect 1
append /tmp/test_gamin/missing
expect 1
rmfile /tmp/test_gamin/missing
expect 1
disconnect
rmdir /tmp/test_gamin
