# Minuet firmware

[Minuet](https://minuet.brownstudios.dev) upgrades your [MAXXAIR Maxxfan](https://www.maxxair.com/products/fans/maxxfan-deluxe/) with a brushless DC motor, home automation features, and accessories.

This repository contains the Minuet firmware configuation based on ESPHome.  You are welcome to modify the Minuet firmware and hardware subject to the [license terms](#notice) and we encourage you to make your own accessories.  We welcome thoughtful contributions.

*Refer to the [Minuet website](https://minuet.brownstudios.dev) for documentation and project updates.*

*Refer to the [Minuet main repository](https://github.com/brown-studios/minuet) for the hardware design files.*

## Project status

Please support this project by subscribing to our funding campaign at [CrowdSupply](https://www.crowdsupply.com/brown-studios/minuet) and by pre-ordering a Minuet upgrade kit once they become available. 💜

> [!IMPORTANT]
> Minuet is in active development and it is not yet ready for production.  There may still be bugs and some features may not work as intended.  We do not recommend assembling your own kit from scratch at this time.

## How to modify the firmware

The Minuet firmware uses [ESPHome](https://esphome.io/), an open-source framework for building smart devices.  This guide assumes that you will be using ESPHome and that you are somewhat familiar with it already or are willing to learn.

Work is in progress to certify Minuet as [Made for ESPHome](https://esphome.io/guides/made_for_esphome/) to ensure a smooth out-of-the-box customization experience.

### Caveat

The Minuet firmware is a relatively complex application that uses advanced features of the framework including packages, custom components, and a fair amount of C++.  This guide is meant to get you started but it can't cover all possible challenges you might encounter along the way even if you are already familiar with ESPHome.

Minuet relies on some implementation details that are not considered part of ESPHome's stable API boundary and they might break when a new version of ESPHome is released.

> [!TIP]
> We recommend waiting for official Minuet firmware release announcements before updating your customized firmware.  Don't try to rebuild the Minuet firmware immediately after each new version of ESPHome because it might not compile on that release and it may take time to fix and test.

### Files

The Minuet firmware is built from ESPHome configuration YAML files in the [config](./config) directory.

The top-level configuration template is [minuet_template.yaml](./config/minuet_template.yaml).  It configures important settings such as the WiFi connection, included accessories, and extra components that you may have added.  To customize the Minuet firmware, make a copy of `minuet_template.yaml` and modify it according to the commented instructions within the file.

The [minuet](./config/minuet) subdirectory contains the core implementation the Minuet firmware.  If possible, please avoid directly modifying the files in the `minuet` subdirectory because that will make it more difficult for you to upgrade to newer firmware versions; use substitution varibles to override parameters instead.

Each YAML file is subdivided into [packages](https://esphome.io/components/packages/) of components related to a particular subsystem for ease of maintenance.  If you're adding a new subsystem, then you should create a new package for it to keep the components tidy.  Many packages include C++ *(.h)* header files for lower level implementation details.

### Naming identifiers

To prevent conflicts with end-user firmware customization, all Minuet component identifiers in YAML have the `minuet_` prefix.

Similarly, Minuet C++ declarations reside in the `minuet` namespace and preprocessor macros have the `MINUET_` prefix.

### Getting started with the tools

To get started, you will need to install the ESPHome tools either in [Home Assistant](https://esphome.io/guides/getting_started_hassio) or on the [command-line](https://esphome.io/guides/getting_started_command_line).  You may also find the [Samba share](https://github.com/home-assistant/addons/blob/master/samba/DOCS.md) and [File Editor](https://github.com/home-assistant/addons/blob/master/configurator/DOCS.md) Home Assistant add-ons helpful for transferring and editing files.

- Familiarize yourself with the [ESPHome](https://esphome.io/) tools that you will be using to build the firmware.
- Download the most recent contents of this repository.
- Make a copy of [minuet_template.yaml](./config/minuet_template.yaml) with a name of your choice, such as `my_minuet.yaml`.
- Edit `my_minuet.yaml` to your heart's content.
- Set the `minuet_origin` substitution variable within `my_minuet.yaml` to decide where the configuration will pull the main implementation files from.
  - `local`: Use the version of Minuet already checked out in a local subdirectory.  Ensure that the [minuet](./config/minuet) subdirectory is located next to `my_minuet.yaml`.  If you are using the ESPHome Builder add-on for Home Assistant, copy the `minuet` subdirectory into the `/homeassistant/esphome` directory next to `my_minuet.yaml`.
  - `remote_stable`: Pull the latest release version of Minuet from GitHub (recommended default).
  - `remote_main`: Pull the latest development version of Minuet from GitHub (may not be fully tested).
- Compile the unmodified firmware to confirm that your development environment works correctly before you start making changes.
- Compile the modified firmware.
- Flash the firmware to the device using an over-the-air firmware update or a USB cable.
- Watch the debug logs to see what's happening.

> [!NOTE]
> The device must be powered by 12 V DC nominal supply (100 mA minimum recommended) for programming; it is not powered from USB.  If the ESPHome tools are having trouble connecting to the device to flash the firmware over USB, confirm the serial port path then reset into the bootloader and try again.  To reset into the bootloader, use a jumper to connect the `BOOT` pin at the expansion port to `GND` before applying power.

### Advice

We recommend that you compile your WiFi network credentials, API encryption key, and OTA password secrets directly into your custom firmware so they remain durable even across a factory reset.  You can also configure a fallback access point and captive portal for recovery.  Or you can completely disable the radios if you don't need connectivity!

Use over-the-air software updates to avoid removing the Minuet circuit board from your fan just to access the USB port for programming.

### Development configurations

This repository includes a few additional configurations that are useful for development and deployment.

- [minuet_factory.yaml](./config/minuet_factory.yaml): builds the firmware installed at the factory and supplied to devices via OTA.
- [minuet_factory_debug.yaml](./config/minuet_factory_debug.yaml): builds the factory firmware with some additional debugging features enabled.
- [minuet_minimal.yaml](./config/minuet_minimal.yaml): builds firmware will all optional features disabled, e.g. no WiFi, no accessories, no diagnostics, etc.
- [minuet_template_local.yaml](./config/minuet_template_local.yaml): builds [minuet_template.yaml](./config/minuet_template.yaml) with `minuet_origin` set to `local`.

### Custom accessories

If you develop custom accessories, then you will need to add components to your firmware to support the extra hardware.

The Minuet firmware automatically detects and configures well-known accessories that are plugged into the `I2C` or `EXPANSION` port.  Refer to [config/minuet/accessory.yaml] to see how that's done if you're making something to share with others.

## Contributing to the Minuet firmware

You can do a lot of cool stuff with Minuet and ESPHome like plugging in I2C sensors, making accessories, and adding home automation features to enrich your living space.

Feel free to suggest ideas for improvement to Minuet using the Github issue tracker.  You can also send pull requests to fix bugs and to add features.  We recommend consulting the developers for guidance if you have big changes in mind.  We reserve the right to not accept contributions for any reason.

> [!IMPORTANT]
> We will not accept contributions that have been substantially produced with generative AI tools.  As an open-source project, we expect you to warrant that your contributions are your own work and that they comply with all applicable licenses.

We look forward to hearing your thoughts and seeing the cool stuff that you make with Minuet!

## External components

Minuet uses these external components for some of its functions.  You can also use them in your own projects.

- [esphome-maxxfan-protocol](https://github.com/brown-studios/esphome-maxxfan-protocol): Maxxfan infrared remote control protocol
- [esphome-mcf8316](https://github.com/brown-studios/esphome-mcf8316): MCF8316 brushless DC motor driver with field-oriented control

## Development

This project follows the ESPHome code style with [pre-commit](https://pre-commit.com/) hooks for linting and code formatting.

To run the style checks manually on all files and fix issues, run the following command:

```bash
uv run pre-commit run --all-files
```

## Contribution guidelines

We welcome thoughtful contributions to this project made without the use of generative AI / LLM tools.

## Acknowledgements

Thanks to [skypeachblue](https://github.com/skypeachblue) and [wingspinner](https://github.com/wingspinner) for publishing information about their [reverse engineering](https://github.com/skypeachblue/maxxfan-reversing) of the Maxxfan IR remote control protocol.  It helped me create the [esphome-maxxfan-protocol](https://github.com/brown-studios/esphome-maxxfan-protocol) component for this project.

## Notice

The Minuet software, documentation, design, and all copyright protected artifacts are released under the terms of the [MIT license](LICENSE).
