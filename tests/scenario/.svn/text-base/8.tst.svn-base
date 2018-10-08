connect test
mondir /tmp/test_gamin
expect 2
mkdir /tmp/test_gamin
expect 1
wait
mkfile /tmp/test_gamin/foo
expect 1
wait
rmfile /tmp/test_gamin/foo
expect 1
disconnect
rmdir /tmp/test_gamin
