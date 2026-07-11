# Minuet board setup

This document describes how to prepare a brand new Minuet board for use.

## Process

TODO: Write scripts for this.

Set the eFuses according to the board version and patch.

```bash
espefuse --chip esp32c6 --port /dev/tty.usbmodem1201 --extend-efuse-table esp_efuse_custom_table.csv summary
```

Flash the initial firmware configuration.

## Artifacts

## Hardware information

The provisioning process sets eFuses (one time writable bits of data) in the microcontroller to encode hardware information.  The firmware reads these values at runtime to check for potential compatibility issues and ensure safe operation.

- `USER_DATA.BOARD_SERIES`: Identifies the manufacturing run that produced the circuit board, may be 0 if not applicable.
- `USER_DATA.BOARD_VERSION_MAJOR`: The circuit board major version number as printed on the silkscreen, such as `4` for "v4.0".
- `USER_DATA.BOARD_VERSION_MINOR`: The circuit board minor version number as printed on the silkscreen, such as `1` for "v4.1".
- `USER_DATA.BOARD_PATCH_*`: A set of 4 flags labeled `A` through `D` that each indicate whether some specific hardware modification was applied to the board post-manufacturing to work around a known issue, such as removing or replacing a component.  The interpretation of patch flags depends on the board version.

Known board versions and patches:

| Major | Minor | Patches | Notes                                    |
| ----- | ----- | ------- | ---------------------------------------- |
|     4 |     0 |         | prototype                                |
|     4 |     0 |       A | removed C9 (ACC_ID pin filter capacitor) |
