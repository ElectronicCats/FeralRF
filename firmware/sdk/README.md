# TI SDK Setup

## Git Submodule (Automated)

The TI SimpleLink SDK is included as a git submodule:

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/user/FeralRF.git

# Or initialize after clone
git submodule update --init --recursive
```

## SDK Version

- **SDK**: SimpleLink Low Power F2 SDK
- **Version**: 7.10.01.24
- **Source**: https://github.com/TexasInstruments/simplelink-lowpower-f2-sdk
- **Tag**: `cc13xx_cc26xx_sdk_7_10_01_24`

## Manual Update (if needed)

```bash
cd firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24
git fetch origin
git checkout cc13xx_cc26xx_sdk_7_10_01_24
```

## Structure

```
firmware/sdk/
├── README.md (this file)
├── pico-sdk/                    # Pico SDK (submodule)
└── simplelink_cc13xx_cc26xx_sdk_7_10_01_24/  # TI SDK (submodule)
    ├── source/
    │   ├── ti/
    │   │   ├── drivers/
    │   │   ├── devices/
    │   │   └── ble5stack/
    │   ├── kernel/
    │   └── third_party/
    ├── tools/
    └── examples/
```

## Verify

```bash
ls firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24/source/ti/drivers/rf/
```
Should show: `RF.h`, `RFCC26XX_multiMode.h`, etc.

## Notes

- The GitHub SDK omits full documentation and some examples
- Documentation available at: https://dev.ti.com/tirex/
- For full SDK with all examples, download from TI website
