#!/bin/sh
# Usage:
# ./sqlsmith --target="host=127.0.0.1 port=5433 dbname=yugabyte user=yugabyte password=yugabyte" --verbose > out
# ./check.sh out
grep "Error: " $* |
egrep -v "(cannot cast|does not exist|invalid|could not convert|cannot be matched|cannot accept|could not identify|argument list must have even number of elements)" | # Expected
grep -v "errstart was not called" | # https://github.com/yugabyte/yugabyte-db/issues/11066
sort | uniq -c | sort -n
