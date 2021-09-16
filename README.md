# GEOPM PAPI IOGroup

This repo contains an IOGroup plugin for [GEOPM](www.github.com/geopm/geopm/). The plugin
adds the ability for GEOPM to use [PAPI](http://icl.cs.utk.edu/papi/) performance counters.

## Building
If the default dependency locations (described below) are correct, run `make`
to build this plugin. Otherwise, run `make` with overrides as described below.

This IOGroup has a build and runtime dependency on PAPI. By default, it assumes
PAPI is installed to `$HOME/.local/lib` and `$HOME/.local/include`. These can
be overridden by `make` if another location is needed. E.g.
`make PAPI_LIB_DIR=/alternative_lib_dir PAPI_INC_DIR=/alternative_inc_dir`

There is also a dependency on the `geopmpolicy` library. By default, the
makefile uses `$HOME/build/geopm/lib` and `$HOME/build/geopm/include`. If
other directories are needed, `GEOPM_LIB_DIR` and `GEOPM_INC_DIR` can be set,
respectively.

## Running
GEOPM and PAPI are run time dependencies. The plugin executes in GEOPM, and the
Makefile includes a rule that adds the PAPI location to the rpath, so it should
work even if that library is not present in `LD_LIBRARY_PATH`.

This plugin gathers counters from PAPI with system granularity. This is likely not
permitted for unprivileged users by default. You may need to decrease the perf
paranoia level in Linux on the node(s) where the monitored processes will run.
Example to decrease paranoia: `echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid`

The following environment variables must be set to use this plugin:
 * `GEOPM_PLUGIN_PATH` must be set to the directory where the build output
   (`libgeopmiogroup_papi_iogroup.so.0.0.0`) is located. The `make install`
   rule will copy the plugin to that path if it is defined.
 * `GEOPM_PAPI_EVENTS` must be set to a space-separated list of PAPI events you
   are interested in making accessible to GEOPM. Run `papi_avail` and
   `papi_native_avail` to determine which events are available on your system.

*NOTE*: The above environment variables only make PAPI events available to
GEOPM, and do not add the events to any output. To add the events to GEOPM
output, add them to `--geopm-report-signals` or `--geopm-trace-signals`,
as documented in [geopmlaunch(1)](https://geopm.github.io/man/geopmlaunch.1.html).

*Example usage* to count single-precision and double-precision floating point operations in the GEOPM report:

```
OMP_NUM_THREADS=42 \
GEOPM_PLUGIN_PATH="$HOME/geopm_papi_iogroup" \
GEOPM_PAPI_EVENTS="PAPI_DP_OPS PAPI_SP_OPS" \
geopmlaunch srun -N 1 -n 1 --geopm-report-signals=PAPI_DP_OPS,PAPI_SP_OPS -- geopmbench
```

Since no report path override was provided to GEOPM, expect to see the results in the default location at `./geopm.report`. E.g.

```
##### geopm 1.1.0 #####
Start Time: Wed Apr 01 09:00:15 2020
Profile: /home/daniel/build/geopm/bin/geopmbench
Agent: monitor
Policy: {}

Host: mcfly13
Region all2all (0x000000003ddc81bf):
    runtime (sec): 10.0041
    sync-runtime (sec): 9.98393
    package-energy (joules): 1802.26
    dram-energy (joules): 120.414
    power (watts): 180.516
    frequency (%): 133.222
    frequency (Hz): 2.79765e+09
    network-time (sec): 9.96454
    count: 10
    PAPI_DP_OPS: 3.17311e+07
    PAPI_SP_OPS: 0
Region sleep (0x00000000536c798f):
    runtime (sec): 9.9997
    sync-runtime (sec): 9.99998
    package-energy (joules): 1708.52
    dram-energy (joules): 119.443
    power (watts): 170.852
    frequency (%): 132.98
    frequency (Hz): 2.79259e+09
    network-time (sec): 0
    count: 10
    PAPI_DP_OPS: 3.31795e+08
    PAPI_SP_OPS: 0
Region stream (0x00000000d691da00):
    runtime (sec): 2.25906
    sync-runtime (sec): 2.24916
    package-energy (joules): 645.647
    dram-energy (joules): 65.183
    power (watts): 287.061
    frequency (%): 133.017
    frequency (Hz): 2.79335e+09
    network-time (sec): 0
    count: 20
    PAPI_DP_OPS: 4.71828e+10
    PAPI_SP_OPS: 0
Region model-init (0x00000000644f9787):
    runtime (sec): 0.606348
    sync-runtime (sec): 0.607
    package-energy (joules): 158.375
    dram-energy (joules): 17.1892
    power (watts): 260.914
    frequency (%): 133.329
    frequency (Hz): 2.79991e+09
    network-time (sec): 0
    count: 1
    PAPI_DP_OPS: 6.75681e+06
    PAPI_SP_OPS: 0
Region dgemm (0x00000000a74bbf35):
    runtime (sec): 0.289892
    sync-runtime (sec): 0.296087
    package-energy (joules): 70.188
    dram-energy (joules): 4.26305
    power (watts): 237.052
    frequency (%): 126.883
    frequency (Hz): 2.66455e+09
    network-time (sec): 0
    count: 10
    PAPI_DP_OPS: 5.44676e+10
    PAPI_SP_OPS: 0
Region MPI_Barrier (0x000000007b561f45):
    runtime (sec): 8.8448e-05
    sync-runtime (sec): 0
    package-energy (joules): 0
    dram-energy (joules): 0
    power (watts): 0
    frequency (%): 0
    frequency (Hz): 0
    network-time (sec): 8.8448e-05
    count: 10
    PAPI_DP_OPS: 0
    PAPI_SP_OPS: 0
Region unmarked-region (0x00000000725e8066):
    runtime (sec): 0.156684
    sync-runtime (sec): 0.0951638
    package-energy (joules): 16.3325
    dram-energy (joules): 1.28734
    power (watts): 171.625
    frequency (%): 133.371
    frequency (Hz): 2.80079e+09
    network-time (sec): 0
    count: 0
    PAPI_DP_OPS: 2.83647e+07
    PAPI_SP_OPS: 7
Epoch Totals:
    runtime (sec): 22.626
    sync-runtime (sec): 22.6243
    package-energy (joules): 4242.95
    dram-energy (joules): 310.591
    power (watts): 187.539
    frequency (%): 132.922
    frequency (Hz): 2.79136e+09
    network-time (sec): 9.96463
    count: 10
    PAPI_DP_OPS: 1.02042e+11
    PAPI_SP_OPS: 7
    epoch-runtime-ignore (sec): 0
Application Totals:
    runtime (sec): 23.3166
    package-energy (joules): 4406.26
    dram-energy (joules): 328.19
    power (watts): 188.975
    network-time (sec): 9.96463
    ignore-time (sec): 0
    geopmctl memory HWM: 54416 kB
    geopmctl network BW (B/sec): 0
```
