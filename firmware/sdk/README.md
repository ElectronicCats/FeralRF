# TI SDK Setup

## Download

1. Go to: https://www.ti.com/tool/download/SIMPLELINK-CC13XX-CC26XX-SDK
2. Login with TI account (free registration)
3. Download version **7.10.01.24**

## Installation

### Linux
```bash
chmod +x simplelink_cc13xx_cc26xx_sdk_7_10_01_24.run
./simplelink_cc13xx_cc26xx_sdk_7_10_01_24.run --mode unattended --prefix /path/to/FeralRF/firmware/sdk
```

### Windows
Run the installer and select `firmware/sdk/` as destination.

## Expected Structure
```
firmware/sdk/
└── simplelink_cc13xx_cc26xx_sdk_7_10_01_24/
    ├── source/
    │   ├── ti/
    │   ├── kernel/
    │   └── third_party/
    ├── tools/
    └── ...
```

## Verify
```bash
ls firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24/source/ti/drivers/rf/
```
Should show: `RF.h`, `RFCC26XX_multiMode.h`, etc.
