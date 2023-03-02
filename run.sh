#!/usr/bin/env bash
for i in {1..4}; do
  ./sqlsmith --verbose --target="host=localhost port=6875 dbname=materialize user=materialize" > $i.log &
done
wait
