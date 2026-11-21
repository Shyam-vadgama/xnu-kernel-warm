# HakiOS 16.5 for POCO F6 / Redmi Turbo 3 (`peridot`)

English · [中文](README.zh.md) · [Українська](README.uk.md)

> **HakiOS (Darwin 22.5.0 / iOS 16.5 on Apple XNU `xnu-8796.121.2`) ported to Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, Qualcomm Snapdragon 8s Gen 3 / SM8635).**  
> **Maintained & Developed by [amiaxf (slayedkaito on GitHub)](https://github.com/slayedkaito)**  
> Repository: [https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf](https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf)

---

## Overview

HakiOS is an experimental research project bringing Apple's open-source Darwin XNU kernel to modern Android hardware. This fork provides a complete bring-up for the **Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, SM8635 / `cliffs`)** running iOS 16.5 (`20F66`).

It runs **temporarily from RAM (Zero-Flash RAM Boot)** via `fastboot boot xnu-usb.img`. No Android partitions (`boot`, `init_boot`, `vendor_boot`, `super`, `userdata`) are ever modified or overwritten. A normal reboot (or holding Power for ~15 seconds) returns directly to HyperOS completely untouched.

---

## What's Working vs What's Not Working

| Subsystem / Feature | Status | Technical Details |
|---|:---:|---|
| **RAM Boot & Loader** | 🟢 **Working** | Boots temporarily into RAM via `fastboot boot xnu-usb.img`. Mu-Silicium UEFI initializes GOP display, and `loader.efi` stages and executes Mach-O XNU with APFS root & system volume. |
| **CPU & Core Topology** | 🟢 **Working** | 8 cores SMP: 1× Cortex-X4 @ 3.0 GHz (`bootcpu=7`), 4× Cortex-A720 @ 2.8 GHz, 3× Cortex-A520 @ 2.0 GHz. ARM GICv3 interrupt distributor & redistributor routing. |
| **Display & Refresh Rate** | 🟢 **Working** | 1.5K AMOLED (`1220 × 2712 @ 120 Hz`) driven via UEFI GOP framebuffer (`0xe3940000`). SpringBoard runs butter-smooth at 120 Hz. |
| **Touchscreen** | 🟢 **Working** | Goodix GT9916R (Berlin-D) over SPI (`0xa90000`). Multitouch, tap, swipe, and home-bar gestures fully responsive. |
| **GPU & Metal Acceleration** | 🟢 **Working** | Qualcomm Adreno 735 (`0x43030b00`) hardware acceleration via `hakios-metal`. Smooth 120 FPS window server rendering and Metal shaders. |
| **Storage** | 🟢 **Working** | UFS 4.0 storage service via UEFI block interface (`ufs=2`). |
| **Haptics** | 🟢 **Working** | Awinic AW8697 LRA haptic motor (`haptic=5`). |
| **Thermals & Clocks** | 🟢 **Working** | Qualcomm TSENS thermal monitoring across 3 sensor blocks (`thermal=2`). |
| **Physical Buttons** | 🟢 **Working** | Power, Volume Up, and Volume Down key events mapped to Darwin HID events. |
| **Wi-Fi Subsystem (WCN6750)** | 🟡 **Partially Working** | • **TrustZone PAS Auth (PAS ID 6):** ✅ Authenticated & DSP bootstrapped.<br>• **ath11k & HTC/WMI over AHB:** ✅ Connected and operational.<br>• **Active & Passive Scanning:** ✅ Dual-band 2.4 GHz & 5 GHz scanning functional. Surrounding SSIDs populate in Settings.<br>• **Settings UI & Toggle:** ✅ Toggle turns ON/OFF smoothly without permanent locking or `ENXIO` errors.<br>• **Association & Handshake:** 🟡 WPA2/WPA3 4-way handshake & DHCP completion are currently being refined. |
| **Audio (ADSP)** | 🟡 **Partially Working** | SM8635 ADSP SLPI firmware authenticated via TrustZone (`PAS_ADSP = 1`, `PAS_ADSP_DTB = 0x24`). AW882xx I2S amplifier audio playback pipeline is in development. |
| **Bluetooth** | 🟡 **In Progress** | Qualcomm WCN6750 Bluetooth controller firmware initialization under active development. |
| **Cellular (Calls / SMS / Data)** | 🔴 **Not Working** | Qualcomm modem RPC / baseband daemon (`CommCenter`) not ported to XNU. |
| **Camera (ISP)** | 🔴 **Not Working** | Spectra 18-bit ISP not implemented in Darwin. |
| **Biometrics (Fingerprint / Face ID)** | 🔴 **Not Working** | Goodix under-display optical sensor and Secure Enclave biometrics not supported. |
| **Apple Proprietary Cloud Services** | 🔴 **Not Supported** | iCloud, iMessage, FaceTime, App Store (rejected by Apple activation servers by design). |

---

## Hardware Specifications (`peridot`)

- **Device:** Xiaomi POCO F6 / Redmi Turbo 3 (Codename: `peridot`)
- **SoC:** Qualcomm Snapdragon 8s Gen 3 (`SM8635` / `cliffs`, 4nm TSMC)
- **CPU:** 1× Cortex-X4 @ 3.0 GHz + 4× Cortex-A720 @ 2.8 GHz + 3× Cortex-A520 @ 2.0 GHz
- **GPU:** Qualcomm Adreno 735 (`hakios-metal` enabled)
- **Display:** 6.67" Flow AMOLED, 1220 × 2712 pixels, 120 Hz, HDR10+, Dolby Vision
- **Touch IC:** Goodix GT9916R (Berlin-D) over SPI
- **Wi-Fi / BT:** Qualcomm WCN6750 (ath11k, WPSS DSP)
- **Audio DSP:** Qualcomm Hexagon ADSP + Awinic AW882xx Smart PA
- **Haptics:** Awinic AW8697 LRA motor

---

## Prebuilt UEFI Assets Included

The repository includes prebuilt UEFI binaries in [`devices/peridot/prebuilt/`](devices/peridot/prebuilt/), so you do **not** need to compile Mu-Silicium UEFI from source:
- `SILICIUM_UEFI.fd` (3 MiB UEFI firmware built for `peridot`)
- `BootShim.bin`
- `peridot.dtb`
- `mkbootimg.py`

---

## Building from Source (Linux / WSL2 Ubuntu/Debian)

### Prerequisites
Install the required build dependencies:
```bash
sudo apt-get update
sudo apt-get install -y build-essential clang llvm lld flex bison libssl-dev libelf-dev python3 python3-pip curl git
```

### 1. Clone & Checkout
```bash
git clone -b peridot-2.3.4 https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf.git
cd xnu-kernel-peridot-amiaxf
```

### 2. Configure Device
```bash
make peridot_defconfig
python3 tools/hakios-device.py check peridot
python3 tools/hakios-config.py check
```

### 3. Extract iOS 16.5 IPSW
Supply your own **iOS 16.5 (`20F66`) IPSW for iPhone X (`iPhone10,3` / `iPhone10,6`)**:
```bash
make unpack IPSW=/path/to/iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw
```

### 4. Pull Proprietary Firmware from POCO F6
With your POCO F6 booted into HyperOS and USB Debugging enabled:
```bash
DEVICE=peridot make firmware
```

### 5. Build Kernel, Loader, and Disk Images
```bash
make kernel JOBS=$(nproc)
make loader
make images
```

Outputs generated:
- `hakios-kernel` (XNU Mach-O kernel binary)
- `xnuloader/loader.efi` (UEFI bootloader)
- `userland/root.apfs` (RAM disk)
- `userland/system.apfs` (iOS 16.5 system root)

---

## Booting on POCO F6 (Tethered RAM Boot)

### Recommended Command Line (`boot.args`)
```text
cpus=8 bootcpu=7 ufs=2 touchscreen=1 gpu=1 adsp=1 sound=4 wifi=1 haptic=5 touch_no_rpmh=1 small_cores_after=0 adreno_stream1=0 metal=1 thermal=2
```

### Boot Sequence
1. Put the POCO F6 into Fastboot mode:
   ```bash
   adb reboot bootloader
   ```
2. Boot temporary UEFI via `fastboot`:
   ```bash
   fastboot boot xnu-usb.img
   ```
3. Once `fastboot getvar product` reports `xnuloader`:
   ```bash
   fastboot oem ramdisk
   fastboot stage userland/system.apfs
   fastboot stage run-payload.bin
   fastboot oem run
   ```
4. Read live kernel / SpringBoard logs anytime:
   ```bash
   fastboot oem log
   fastboot get_staged run.log
   ```

---

## Documentation & AI Agent Guidance

- [`AGENTS.md`](AGENTS.md) — Comprehensive hardware map, Wi-Fi driver architecture, and troubleshooting guide for AI coding agents.
- [`devices/peridot/README.md`](devices/peridot/README.md) — Low-level device tree, GPIO, and register mappings for `peridot`.
- [`docs/`](docs/index.md) — General HakiOS architecture documentation.

---

## Credits & Acknowledgements

- **amiaxf ([@slayedkaito](https://github.com/slayedkaito))** — POCO F6 (`peridot`) port maintainer, driver bring-up, and Wi-Fi subsystem development.
- **HakiOS Team (@oodze, @netusd, @wannqn, @hxfuxyyXD, Rhaenja Hun)** — Core XNU Darwin port and framework.
- **Mu-Silicium Team** — Open-source UEFI implementation for Qualcomm SoCs.
- **Upstream Reference Porters:**
  - `mariqnetr/hakios-shennong` (Xiaomi 14 Pro / SM8650)
  - `shimoku74/hakios-xnu-kernel`
  - Nothing Phone (1) `spacewar` port team

---

## License & Legal

The XNU patch and derived files are licensed under the Apple Public Source License 2.0 ([LICENSE.APSL](LICENSE.APSL)), and other components under MIT ([LICENSE](LICENSE)).

HakiOS is an independent research project and is **not affiliated with or endorsed by Apple Inc.** All trademarks belong to their respective owners.
