# HakiOS

English · [中文](README.zh.md) · [Українська](README.uk.md)

**Apple's open-source Darwin kernel (XNU, xnu-8796.121.2) on Android phones**, the way Android is Linux on phones: a boot loader of its own, drivers of its own, a configuration system and a version of its own. The same kernel runs in QEMU with nothing of Apple's and nothing of any phone's (`make qemu-open`). A research bench, not a ROM: by default it runs from RAM, and a reboot gives the phone back to Android.

**Documentation:** [docs/](docs/index.md), and the site, [hackios-web.vercel.app](https://hackios-web.vercel.app).

**News, 4 November 2026:** on the POCO X5 Pro 5G the window server draws through Metal on the Adreno 642L, two ways. hakios-metal: page swipes 110 to 115 frames a second, Settings' scroll 118, the home button 106, rendering at 828x1792 stretched to the panel; 101, about 90 and about 90 at the full 1125x2436. UniXNU's Metal on Vulkan: swipes 50 to 57, scroll 41. The panel is 120 Hz, and **120 frames a second has not been reached**. [Metal on the Adreno](docs/reference/metal.md), [the measurements](docs/reference/metal-measurements.md).

Start with [Getting started](docs/getting-started/index.md), [On the phone](docs/getting-started/phone.md) or [Port a device](docs/porting/index.md). What works, part by part: [the documentation's front page](docs/index.md#what-works).

---

## POCO F6 / Redmi Turbo 3 (`peridot`) Port

> **HakiOS 16.5 (Darwin 22.5.0 / iOS 16.5 on Apple XNU `xnu-8796.121.2`) for Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, Qualcomm Snapdragon 8s Gen 3 / SM8635).**  
> **Maintained & Developed by [amiaxf (slayedkaito on GitHub)](https://github.com/slayedkaito)**  
> **Repository:** [https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf](https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf) (Branch: `peridot-2.3.4`)

This fork provides full bring-up and driver enablement for **Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, SM8635 / `cliffs`)** running iOS 16.5 (`20F66`).

- **Zero-flash RAM boot:** Boots temporarily via `fastboot boot xnu-usb.img` without flashing or modifying any Android partitions (`boot`, `init_boot`, `vendor_boot`, `super`, `userdata`). Rebooting returns directly to HyperOS.
- **Prebuilt UEFI assets included:** [`devices/peridot/prebuilt/`](devices/peridot/prebuilt/) contains `SILICIUM_UEFI.fd`, `BootShim.bin`, `peridot.dtb`, and `mkbootimg.py` so you do not need to build Mu-Silicium UEFI from source.

### What's Working vs What's Not Working (Verified by Kernel Logs)

| Subsystem / Feature | Status | Details & Log Evidence |
|---|:---:|---|
| **RAM Boot & Loader** | 🟢 **Working** | Boots temporarily into RAM via `fastboot boot xnu-usb.img`. Mu-Silicium UEFI GOP display + `loader.efi` stages Mach-O XNU and mounts APFS root & system volume. Zero UFS flash modifications. |
| **CPU & Core Topology** | 🟢 **Working** | 8 cores SMP: 1× Cortex-X4 @ 3.0 GHz (`bootcpu=7`), 4× Cortex-A720 @ 2.8 GHz, 3× Cortex-A520 @ 2.0 GHz. ARM GICv3 interrupt distributor & redistributor routing fully operational. |
| **Display & Refresh Rate** | 🟢 **Working** | 1.5K AMOLED (`1220 × 2712 @ 120 Hz`) driven through UEFI GOP framebuffer (`0xe3940000`). SpringBoard UI runs smooth at 120 Hz. |
| **Touchscreen** | 🟢 **Working** | Goodix GT9916R (Berlin-D) over SPI (`0xa90000`). Multitouch, tap, swipe, and home-bar gestures fully responsive. |
| **GPU & Metal Graphics** | 🟢 **Working** | Qualcomm Adreno 735 (`0x43030b00`) accelerated via `hakios-metal`. TrustZone ZAP shader authenticated and running (`TRUSTZONE HAS AUTHENTICATED AND STARTED THE ZAP SHADER`). 120 FPS window server rendering. |
| **Storage** | 🟢 **Working** | UFS 4.0 storage service via UEFI block interface (`ufs=2`). |
| **Haptics** | 🟢 **Working** | Awinic AW8697 LRA haptic motor integration (`haptic=5`). |
| **Thermals & Sensors** | 🟢 **Working** | Qualcomm TSENS thermal monitoring across 3 sensor blocks (`thermal=2`). |
| **Physical Buttons** | 🟢 **Working** | Volume Up, Volume Down, and Power button events mapped to Darwin HID events. |
| **Wi-Fi Subsystem (WCN6750)** | 🟡 **Partially Working** | • **TrustZone PAS Auth (PAS ID 6):** ✅ Authenticated & DSP bootstrapped.<br>• **ath11k & HTC/WMI over AHB:** ✅ Hardware detected (`wcn6750 hw1.0`, BDF `bd_n16t.elf`), HTC target ready, WMI commands answered.<br>• **Active & Passive Scan:** ✅ Dual-band 2.4 GHz & 5 GHz scanning functional. Nearby APs (e.g. `KOST SITIBURDAN 18`, `ARSHA 4G`) detected and reported to `wifid`.<br>• **Settings UI & Toggle:** ✅ Toggle turns ON and OFF smoothly (`power: off: answered`, `power: on: answered`) without `ENXIO` errors or locking.<br>• **RX Queue & Connection:** 🟡 WHAL receive queue assert (`whalRxMpduPop:6920`) and WPA2 4-way handshake / DHCP connection are actively being refined. |
| **Audio (ADSP)** | 🟡 **Partially Working** | SM8635 Hexagon ADSP SLPI firmware authentication via TrustZone in progress (`PAS_ADSP = 1`, `PAS_ADSP_DTB = 0x24`). AW882xx I2S PA amplifier driver in development. |
| **Bluetooth** | 🟡 **In Progress** | Qualcomm WCN6750 Bluetooth controller firmware initialization under active development. |
| **Cellular (Calls / SMS / Data)** | 🔴 **Not Working** | Qualcomm modem RPC / baseband daemon (`CommCenter`) not implemented in XNU. |
| **Camera (ISP)** | 🔴 **Not Working** | Spectra 18-bit ISP not implemented in Darwin. |
| **Biometrics (Fingerprint / Face ID)** | 🔴 **Not Working** | Goodix under-display optical sensor and Secure Enclave biometrics not supported. |
| **Apple Proprietary Cloud Services** | 🔴 **Not Supported** | iCloud, iMessage, FaceTime, App Store (rejected by Apple activation servers by design). |

### Quick Build & Boot Instructions (`peridot`)

1. **Configure & Build (Linux / WSL2):**
   ```bash
   git checkout peridot-2.3.4
   make peridot_defconfig
   make unpack IPSW=/path/to/iPhone10,3_16.5_20F66_Restore.ipsw
   DEVICE=peridot make firmware
   make kernel JOBS=$(nproc) && make loader && make images
   ```

2. **Boot Tethered (RAM-Only):**
   - Command line (`boot.args`):
     ```text
     cpus=8 bootcpu=7 ufs=2 touchscreen=1 gpu=1 adsp=1 sound=4 wifi=1 haptic=5 touch_no_rpmh=1 small_cores_after=0 adreno_stream1=0 metal=1 thermal=2
     ```
   - Put POCO F6 into Fastboot: `adb reboot bootloader`
   - Boot temporary UEFI: `fastboot boot xnu-usb.img`
   - Once `fastboot getvar product` reports `xnuloader`, stage `system.apfs` and `run-payload.bin`, then run `fastboot oem run`.
   - Read live logs anytime: `fastboot oem log` then `fastboot get_staged run.log`.

3. **Detailed Device Docs & AI Guide:**
   - [`devices/peridot/README.md`](devices/peridot/README.md) — Low-level register and GPIO mapping for `peridot`.
   - [`AGENTS.md`](AGENTS.md) — Comprehensive guide and Wi-Fi architecture for AI coding agents.

---

## Repositories

| Repository | Contents |
|---|---|
| [hakios-xnu-kernel](https://github.com/oosexgang/hakios-xnu-kernel) | **common**: the kernel port, every common driver, the loader, user space, the tools, this documentation, and `devices/qemu` |
| [xnu-kernel-peridot-amiaxf](https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf) | **POCO F6 (`peridot`) port fork**: Snapdragon 8s Gen 3 support, Goodix touchscreen, Adreno 735 GPU, and WCN6750 Wi-Fi bring-up |
| [`hakios-metal/`](hakios-metal) | Metal's command model on the Adreno, through the kernel's GPU driver |
| [hakios-sm7325](https://github.com/oosexgang/hakios-sm7325) | POCO X5 Pro 5G: its defconfig, status, UEFI platform, firmware script, and its SoC's layer (Qualcomm SC7280 family) |
| [hakios-mt6768](https://github.com/oosexgang/hakios-mt6768) | Infinix Hot 20 (MediaTek Helio G85): an experimental port with its own loader. Not in this repository's manifest |
| [hakios-uefi](https://github.com/oosexgang/hakios-uefi) | the firmware that starts the loader: Mu-Silicium with HakiOS's boot path, banner and name, and HakiOS's U-Boot |
| [hackios-web](https://github.com/oosexgang/hackios-web) | the website |
| [hackios-xnu-userland](https://github.com/oosexgang/hackios-xnu-userland) | old: a pointer kept for its links. User space is `userland/` here |

How they fit: [the repositories](docs/porting/repositories.md).

---

## What you bring, and what is never here

Nothing of Apple's and no vendor's firmware is in these repositories. The Darwin user space, if you want it, you lay out yourself from an
IPSW you get yourself, and the firmware comes off your own phone; nothing is downloaded for you, decrypted or circumvented. Apple's
services (activation, Apple ID, iCloud, the App Store) are not supported and not a goal.
[What is in the repository, and what never is](docs/index.md#what-is-in-the-repository-and-what-never-is).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [COMMUNITY.md](COMMUNITY.md).

---

## License and legal

The XNU patch and the files derived from it are under the Apple Public Source License 2.0 ([LICENSE.APSL](LICENSE.APSL)), the rest under
MIT ([LICENSE](LICENSE)), as the file headers say. Third parties: [NOTICE](NOTICE),
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md), [COMPLIANCE.md](COMPLIANCE.md). What is and is not distributed: [LEGAL.md](LEGAL.md).
None of this is legal advice.

HakiOS is an independent research project. It is **not affiliated with or endorsed by Apple**; "Darwin", "iPhone", "XNU" and "Apple" are
named only to describe what the code works with. If you believe a file here infringes anyone's rights, open an issue and it will be
removed.

---

## Team & Credits

- **amiaxf ([@slayedkaito](https://github.com/slayedkaito))** — POCO F6 / Redmi Turbo 3 (`peridot`) port maintainer, driver bring-up, and Wi-Fi subsystem development.
- **HakiOS Team:** @oodze (founder), @netusd (advertising / ceo-dev), @wannqn (dev), @hxfuxyyXD (dev), Rhaenja Hun（杭润嘉） (developer / sponsor).
- **Mu-Silicium Team** — for open-source Qualcomm UEFI platform sources.
- **Reference Porters:** `mariqnetr/hakios-shennong` (Xiaomi 14 Pro), `shimoku74/hakios-xnu-kernel`, Nothing Phone (1) `spacewar` port team.
