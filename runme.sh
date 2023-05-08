#!/bin/bash

make build_init
make build_test

# create io files
touch /tmp/in
touch /tmp/in_test
touch /tmp/out
touch /tmp/out_test

#create conf file
touch /tmp/myconf

echo $(pwd)/test 1 2 3 /tmp/in_test /tmp/out_test > /tmp/myconf
echo /bin/sleep 5 /tmp/in /tmp/out >> /tmp/myconf
echo /bin/sleep 10 /tmp/in /tmp/out >> /tmp/myconf

./myinit /tmp/myconf

sleep 1

echo First three tasks: > result.txt
ps -ef | grep "/bin/sleep 5" >> result.txt
ps -ef | grep "/bin/sleep 10" >> result.txt
ps -ef | grep "test" >> result.txt

pkill -f "/bin/sleep 10"

sleep 1

echo Three tasks after kill second task: >> result.txt
ps -ef | grep "/bin/sleep 5" >> result.txt
ps -ef | grep "/bin/sleep 10" >> result.txt
ps -ef | grep "test" >> result.txt

# to see all tasks complete
sleep 10

echo /bin/sleep 3 /tmp/in /tmp/out > /tmp/myconf

killall -HUP myinit

echo One task running: >> result.txt

ps -ef | grep "/bin/sleep 3" >> result.txt

cat /tmp/myinit.log >> result.txt

killall myinit
