# TI SDK Setup

The CC1352 firmware links against the TI SimpleLink Low Power F2 SDK, included
here as a git submodule.

## Get the submodule

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/ElectronicCats/FeralRF.git

# Or initialize after clone
git submodule update --init --recursive
```

## SDK

- **SDK:** SimpleLink Low Power F2 SDK
- **Version:** 8.30.01.01
- **Source:** https://github.com/TexasInstruments/simplelink-lowpower-f2-sdk
- **Tag:** `lpf2-8.30.01.01`
- **Path:** `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/`

## Build caveat

The open-source GitHub SDK ships only the RF-core prebuilt lib. The CC1352
build also links three precompiled libs (drivers, driverlib, sysbios) that come
only from TI's full installer SDK. Either install that and pass
`-DTI_SDK_FULL=~/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01`, or copy those
three `.a`/`.lib` files into the submodule (see the repo `README.md` build
section).

## Verify

```bash
ls firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/drivers/rf/
```
Should show `RF.h`, `RFCC26X2_multiMode.h`, etc.
