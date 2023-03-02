#!/usr/bin/env bash
for i in {1..8}; do
  ./sqlsmith --verbose --target="host=localhost port=6875 dbname=materialize user=materialize" > log$i 2> /dev/null &
done
wait
