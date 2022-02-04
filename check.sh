#!/bin/sh
# Usage:
# ./sqlsmith --target="host=127.0.0.1 port=5433 dbname=yugabyte user=yugabyte password=yugabyte" --verbose > out
# ./check.sh out
cat $* | grep "Error: " | sed -e "s/^\.*//" |
egrep -v "(cannot cast|does not exist|invalid|could not convert|cannot be matched|cannot accept|could not identify|argument list must have even number of elements|out of range|is not a valid binary digit|unrecognized|could not open server file|could not stat file|interval units|not recognized|string is not a valid identifier|division by zero|negative substring length not allowed|out of valid range|value too long for type|cannot take logarithm of zero|unrecognized privilege type|duplicate key value violates unique constraint|violates not-null constraint|Not implemented: Read request with row mark types must be part of a transaction|has not been populated|non-exclusive backup in progress|Corruption: Invalid length of binary data with TransactionId|could not find array type for data type|must be a row type|wrong flag in flag array|cannot get array length of a non-array|is in the future|precision must be between 0 and 6|is not an integer|numeric field overflow|value overflows numeric format|lower bound cannot equal upper bound|null_value_treatment must be|logical decoding requires wal_level >= logical|is not supported|cannot be null|cannot take logarithm of a negative number|remainder for hash partition must be less than modulus|could not open file|flag array element is not a string|count must be greater than zero|null character not permitted|encoding conversion from|log format|time zone|must request at least 2 points|is not a valid encoding name|is not a valid encoding code|bit string length|bit string too long|requested length|cannot accumulate arrays of different dimensionality|backup label too long|too short|could not read file|Timed out:|conflicting values for|cannot accumlate null arrays|cross-database references are not implemented|must be greater than zero|is not a hash partitioned table|requested character too large for encoding|cannot accumulate null arrays|range constructor flags argument must not be null|cannot display a value of type|field name must not be null|recovery is not in progress|lastval is not yet defined in this session|exclusive backup not in progress|argument declared|range lower bound must be less than or equal to range upper bound)" | # Expected
grep -v "errstart was not called" | # https://github.com/yugabyte/yugabyte-db/issues/11066
grep -v "Illegal state: Transaction for catalog table write operation" | # https://github.com/yugabyte/yugabyte-db/issues/11221
grep -v "Remote error: Illegal state" | # https://github.com/yugabyte/yugabyte-db/issues/11222
grep -v "unexpected duplicate for tablespace 0, relfilenode 0" | # https://github.com/yugabyte/yugabyte-db/issues/11223
grep -v "cache lookup failed for" | # https://github.com/yugabyte/yugabyte-db/issues/11224
grep -v "could not open relation with OID" | # https://github.com/yugabyte/yugabyte-db/issues/11225
grep -v "Operation failed. Try again" | # https://github.com/yugabyte/yugabyte-db/issues/11226
sort | uniq -c | sort -n
