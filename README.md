# Minuet firmware

[Minuet](https://github.com/brown-studios/minuet) upgrades your [MAXXAIR Maxxfan](https://www.maxxair.com/products/fans/maxxfan-deluxe/) with a brushless DC motor, home automation features, and accessories.

This repository contains the Minuet firmware configuation based on ESPHome.  You are welcome to modify the Minuet firmware and hardware subject to the [license terms](#notice) and we encourage you to make your own accessories.  We welcome thoughtful contributions.

*Refer to the [Minuet main repository](https://github.com/brown-studios/minuet) for documentation and hardware design files.*

## ESPHome configuration YAML

[ESPHome](https://esphome.io/) is an open-source firmware framework.

The Minuet firmware is built from ESPHome configuration YAML files.

The top-level configuration file is [minuet.yaml](./minuet.yaml).  It configures important settings such as the WiFi connection, hardware version, included accessories, and extra components you may have added.  To customize the Minuet firmware, make a copy of `minuet.yaml` and modify it according to the instructions in the comments within the file.

The [minuet](./minuet) directory contains several more configuration YAML files and some C++ code that implements the core of the Minuet firmware.  If possible, please avoid modifying the files in the `minuet` directory because that will make it more difficult for you to upgrade to newer firmware versions.

### How to modify the firmware

The Minuet firmware uses [ESPHome](https://esphome.io/).  This guide assumes that you will be using ESPHome and that you are somewhat familiar with it already or are willing to learn.

To get started, you will need to install the ESPHome tools either in [Home Assistant](https://esphome.io/guides/getting_started_hassio) or on the [command-line](https://esphome.io/guides/getting_started_command_line).  You may also find the [Samba share](https://github.com/home-assistant/addons/blob/master/samba/DOCS.md) and [File Editor](https://github.com/home-assistant/addons/blob/master/configurator/DOCS.md) Home Assistant add-ons helpful for transferring and editing files.

- Familiarize yourself with the [ESPHome](https://esphome.io/) tools that you will be using to build the firmware.
- Download the most recent contents of this repository.
- Make a copy of `minuet.yaml` with a name of your choice, such as `my_minuet.yaml`.  Or [use `minuet.yaml` as a package](#development-configuration-using-minuetyaml-as-a-package).
- Ensure that the `minuet` directory is next to `my_minuet.yaml`.  If you are using the ESPHome Builder add-on for Home Assistant, copy the `minuet` directory into the `/homeassistant/esphome` directory next to `my_minuet.yaml`.
- Compile the unmodified firmware to confirm that your development environment works correctly before you start making changes.
- Edit `my_minuet.yaml` to your heart's content.
- Compile the modified firmware.
- Flash the firmware to the device using an over-the-air firmware update or a USB cable.
- Watch the debug logs to see what's happening.

> [!NOTE]
> The device must be powered by 12 V DC nominal supply (100 mA minimum recommended) for programming; it is not powered from USB.  If the ESPHome tools are having trouble connecting to the device to flash the firmware over USB, confirm the serial port path then reset into the bootloader and try again.  To reset into the bootloader, press and hold the `BOOT` button, tap `RESET`, then release `BOOT`.

> [!TIP]
> We recommend compiling your WiFi SSID and password into the firmware during development (instead of relying on the captive portal WiFi setup method) to ensure that your device can still connect to your WiFi network after a factory reset of the non-volatile storage.  Use over-the-air software updates to avoid removing the Minuet circuit board from your fan just to access the USB port for programming.

### Packages

The YAML configuration is subdivided into [packages](https://esphome.io/components/packages/) of components for ease of maintenance.  Each package declares a group of components related to a particular subsystem such as the fan motor driver, keypad, thermostat, or an accessory.  If you're adding a new subsystem, then you should create a new package for it to keep the components tidy.

Some files contain collections of packages when one package just isn't enough.

### Development configuration: using `minuet.yaml` as a package

Instead of copying and editing `minuet.yaml` as a template for your configuration, you can include `minuet.yaml` unmodified as a package and layer your changes on top.  This method has some limitations but it can be useful for firmware development.

Create an empty YAML file with a name of your choice, such as `my_minuet.yaml`.

Add the following contents to the file to get started.  Change this example to your liking.  Your declarations override those in the base template.  Use the [`!remove`]((https://esphome.io/components/packages/)) directive to remove sections in the base template that you don't need.

> [!NOTE]
> The development configuration method cannot modify substitution variables that are baked into `minuet.yaml`; you'll have to use the standard configuration method or modify `minuet.yaml` for some use-cases.

<details>
<summary>Example my_minuet.yaml</summary>

```yaml
# There are many Minuet configurations and this one is mine
packages:
  # Include the base configuration template
  template: !include minuet.yaml

  # Apply customizations to the template
  my_customizations:
    # Give your Minuet a custom name if you want.
    esphome:
      name: my-minuet
      friendly_name: My Minuet
      name_add_mac_suffix: false

    # Set the WiFi credentials for your network and remove the captive portal to
    # improve security and reliability.
    wifi:
      ssid: !secret wifi_ssid
      password: !secret wifi_password
      fast_connect: true
      ap: !remove
    captive_portal: !remove

    # If you plan to make changes to the core firmware with Minuet plugged into your
    # computer via USB (instead of WiFi), set `baud_rate` to enable USB and UART logging.
    logger:
      level: DEBUG
      baud_rate: 921600
```
</details>

### Supporting hardware expansion

When you attach additional hardware to Minuet via the `QWIIC` connector or `EXPANSION` port, you will need to add components to the firmware to support the expansion.

Follow the instructions in `minuet.yaml` and either uncomment and configure the package needed for your hardware or add ESPHome components to the `my_package` package as shown near the end of the file.

### Identifiers

To prevent conflicts with end-user firmware customization, all Minuet component identifiers in YAML have the `minuet_` prefix.

Similarly, Minuet C++ declarations reside in the `minuet` namespace and preprocessor macros have the `MINUET_` prefix.

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

## Implementation notes

### Temperature units

The original Maxxfan thermostat natively uses Fahrenheit temperature units.  The `auto` button on the keypad is labeled `HOLD TO SET 78 °F` or `HOLD TO SET 26 °C` depending on the regional variant and both versions (presumably) set the same temperature (78 °F).  The infrared remote control always sends Fahrenheit temperature units to the controller in 1 °F steps even when configured to display Celsius.

Minuet remains compatible with these original expectations and it expands the range of values to allow finer control for users of both temperature systems when accessed via the API (such as by Home Assistant or an app).  The keypad and remote control behavior is unchanged.  The visual step provides a hint to the user interface about the granularity of the temperature setpoint and that it should be displayed with 1 digit after the decimal point.

| Temperature attribute | Maxxfan                  | Minuet                   |
| --------------------- | ------------------------ | ------------------------ |
| Minimum               | 29 °F (approx. -1.67 °C) | 23 °F (exactly -5 °C)    |
| Maximum               | 99 °F (approx. 37.22 °C) | 122 °F (exactly 50 °C)   |
| Default               | 78 °F (approx. 25.56 °C) | 78 °F (approx. 25.56 °C) |
| Keypad step           | 1 °F (approx. 0.56 °C)   | 1 °F (approx. 0.56 °C)   |
| Remote control step   | 1 °F (approx. 0.56 °C)   | 1 °F (approx. 0.56 °C)   |
| Visual step           | N/A                      | 0.36 °F (exactly 0.5 °C) |

ESPHome uses Celsius temperature units internally so the Minuet firmware converts the units as required and represents them as single-precision floating point values without rounding.

It's too bad the original designers didn't choose 77 °F as the default because that would have converted to exactly 25 °C.

## Acknowledgements

Thanks to [skypeachblue](https://github.com/skypeachblue) and [wingspinner](https://github.com/wingspinner) for publishing information about their [reverse engineering](https://github.com/skypeachblue/maxxfan-reversing) of the Maxxfan IR remote control protocol.  It helped me create the [esphome-maxxfan-protocol](https://github.com/brown-studios/esphome-maxxfan-protocol) component for this project.

## Notice

The Minuet software, documentation, design, and all copyright protected artifacts are released under the terms of the [MIT license](LICENSE).
