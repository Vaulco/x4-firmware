Firmware for the **Xteink X4** e-paper display reader (unaffiliated with Xteink). Built using **PlatformIO** and targeting the **ESP32-C3** microcontroller.

---

### I want to contribute code (no hardware needed)

```
git clone https://github.com/Vaulco/x4-firmware.git
cd xteink-reader
```

### I have the hardware

Connect your Xteink X4 to your computer via USB-C and run the following command.

```sh
pio run --target upload
```

## Internals

The E-Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only
has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### Data caching

XTC files are cached to the SD card for reading progress tracking. This cache directory exists at `.ereader` on the SD card. The structure is as follows:

```
.ereader/
├── xtc_12471232/        # Each XTC is cached to a subdirectory named `xtc_<hash>`
│   └── progress.bin     # Stores reading progress (page number)
│
└── xtc_189013891/
```

Deleting the `.ereader` directory will clear the entire cache. 

Due the way it's currently implemented, the cache is not automatically cleared when a book is deleted and moving a book file will use a new cache directory, resetting the reading progress.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).