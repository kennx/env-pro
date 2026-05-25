# BSEC Review Fixes Demo Checklist

## Flash

1. Run `pio run -t upload`.
2. Open a serial monitor at `115200`.

## Cold Boot Expectations

1. The screen title shows `ENV-Pro`.
2. `IAQ` shows `--` and the status line shows `WAIT` or `WARMUP`, not a green `0`.
3. `eCO2` shows `-- ppm`, not `CO2 0 ppm`.
4. `TEMP` / `HUM` / `PRES` appear on the screen.

## Warmup / Calibration Expectations

1. The status line changes to `WARMUP` while `accuracy == 0`.
2. The status line changes to `CAL(1)` or `CAL(2)` while `accuracy == 1` or `2`.
3. `TEMP` and `HUM` update before IAQ/eCO2 become stable.

## Stable Output Expectations

1. The status line changes to `READY(3)` once `accuracy == 3`.
2. `IAQ` shows a numeric value with color mapping.
3. `eCO2` shows a numeric value and the label stays `eCO2`.
4. `PRES` shows a numeric value in `hPa`.

## Serial Diagnostic Expectations

1. The serial monitor periodically prints a line in this shape:
   `BSEC diag iaq=... acc=... eCO2=... runin=... stab=...`
2. `runin` and `stab` should eventually reach `1` on a healthy warm sensor.
3. If `runin=1` and `stab=1` but `acc` remains `1`, treat that as a baseline-training/environment signal rather than startup warmup.

## Persistence Expectations

1. Establish a blank baseline first: run `pio run -t erase`, then `pio run -t upload`, and confirm the serial monitor shows `No compatible BSEC state`.
2. On that blank boot, start a timer at the `ENV-Pro BSEC2 init` serial log.
3. Record one comparison metric from the blank boot:
   - `T_first_valid_blank`: time until `IAQ` and `eCO2` first change from `--` to numeric values.
   - Or `T_stable_blank`: time until the status line first shows `READY(3)`.
4. Leave the device running long enough for at least one `BSEC state saved` serial log.
   - The current firmware saves state on a gated schedule, so this is not an immediate boot-time event.
   - In the normal path, the save interval is about 6 hours, and saving also requires valid output plus `accuracy >= 1` and `runin = 1`.
   - If you do not want to wait for a full save window, record this section as "save window not reached yet" instead of treating the absence of the log as a failure.
5. Reboot the device without erasing flash.
6. Confirm the serial monitor prints `BSEC state restored`.
7. Measure the same metric again on the restored boot:
   - `T_first_valid_restored` or `T_stable_restored`.
8. Accept persistence only if the restored boot is measurably faster than the blank baseline for the same metric:
   - `T_first_valid_restored < T_first_valid_blank`, or
   - `T_stable_restored < T_stable_blank`.
9. Record the two measured times in the demo notes so the comparison is explicit.

## IAQ Training Drill

1. If the device stays at `CAL(1)` for more than 30 minutes, first confirm the serial log still shows fresh `BSEC diag ...` lines.
2. Expose the sensor to a clearly different air sample, such as cleaner air and then short exhaled breath or another brief higher-VOC source.
3. Watch whether `acc` advances to `2` or `3` after that change.
4. Record whether the stall was resolved by the configuration fix alone or only after the air-exposure drill.

## Stale/Error Expectations

1. If the sensor stops producing outputs for more than about 10 seconds, the status line changes to `STALE`.
2. If `envSensor.run()` fails, the status line changes to `ERROR`.
