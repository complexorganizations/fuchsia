radarutil
==========

The radarutil tool can be used to test the performance of radar drivers and
clients under different conditions.

Usage
----------

The radarutil tool takes the following arguments:

- Burst processing time (`--burst-process-time`/`-p`): The amount of time to
  sleep for each burst to simulate processing delay. Requires a suffix (`h`,
  `m`, `s`, `ms`, `us`, `ns`) indicating the units. Defaults to no delay.
- Run time (`--time`/`-t`): The amount of time spend reading bursts before
  exiting. Requires a suffix indicating the units. Incompatible with the burst
  count option. Defaults to 1 second.
- Burst count (`--burst-count`/`-b`): The number of bursts to read before
  exiting. Incompatible with the run time option.
- VMO count (`--vmos`/`-v`): The number of VMOs to register and use for reading
  bursts. Defaults to 10.
- Output file (`--output`/`-o`): Path of the file to write bursts to, or `-` for
  stdout. If omitted, received bursts are not written.
- Burst period (`--burst-period-ns`): The time between radar bursts reported by
  this sensor. Must be greater than zero.
- Max error rate (`--max-error-rate`): The maximum allowable error rate in
  errors per million bursts. radarutil will return nonzero if this rate is
  exceeded. Requires either `--burst-period-ns` or `--burst-count`.

For example, to sleep 3 milliseconds for each burst, run for 5 minutes, and
register 20 VMOs, run: `radarutil -p 3ms -t 5m -v 20`

radarutil will return a nonzero status if any burst or driver errors are
received when `--max-error-rate` is not specified. It will exit immediately upon
encountering a driver error, but will continue reading after burst errors.
