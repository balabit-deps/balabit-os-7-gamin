mkdir /tmp/test_gamin
chmod /tmp/test_gamin 000
connect test
mondir /tmp/test_gamin
wait
expect 2
disconnect
chmod /tmp/test_gamin 755
rmdir /tmp/test_gamin
