#!/bin/sh
# Signal 6 is TRAP, handled by check_trap.sh
#grep -ar "was terminated by signal" ~/var/data |grep -v "signal 6" > ~/sig
sed -e "s/.*(PID \([0-9]*\)).*/\1/" ~/sig | while read i; do
  echo "CHECKING $i" && \
  gzip -d ~/var/data/pg_data/core.$i.gz && \
  gdb $HOME/code/yugabyte-db/build/release-clang12-linuxbrew-dynamic-ninja/postgres/bin/postgres \
    $HOME/var/data/pg_data/core.$i -batch
  gzip -9 $HOME/var/data/pg_data/core.$i &
done

# for i in ~/var/data/pg_data/core*; do echo $i; gdb $HOME/code/yugabyte-db/build/release-clang12-linuxbrew-dynamic-ninja/postgres/bin/postgres $i -ex bt -batch; done > ~/code/sqlsmith/postgres_crashes
