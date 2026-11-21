# AGENTS.md — AI Agent Guide for HakiOS POCO F6 / Redmi Turbo 3 (`peridot`)

> **HakiOS (Darwin 22.5.0 / iOS 16.5 on Apple XNU `xnu-8796.121.2`) for Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, SM8635)**  
> **Port Maintainer:** amiaxf ([@slayedkaito](https://github.com/slayedkaito))  
> **Repository:** [https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf](https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf)  
> **Branch:** `peridot-2.3.4`

This document provides AI coding agents (Antigravity, Codex, Claude Code, Cursor, etc.) with the exact context, non-negotiable safety rules, driver architecture, build commands, hardware registers, and known pitfalls needed to develop and troubleshoot HakiOS on `peridot`.

---

## 1. Non-Negotiable Safety Rules

1. **RAM-Only Boot (Strictly Zero Flashing):**
   - **NEVER** run `fastboot flash` on ANY partition (`boot`, `vendor_boot`, `init_boot`, `dtbo`, `recovery`, `super`, `userdata`, etc.).
   - Always boot temporarily into RAM using `fastboot boot xnu-usb.img`.
   - A normal reboot (or holding **Power for ~15 seconds**) restores HyperOS / Android completely untouched.
2. **Fastboot Mode Only (Never Fastbootd):**
   - Always enter bootloader fastboot via `adb reboot bootloader`.
   - Never use `fastbootd` (`adb reboot fastboot`).
3. **No Proprietary or Personal Data in Git:**
   - Never commit Apple IPSW contents (`ios165/`, `sysimage/`, `userland/system.apfs`, `userland/root.apfs`, `xnuloader/identity.h`).
   - Never commit raw vendor firmware dumps (`fw-wifi/`, `fw-gpu/`, `fw-audio/`, `fw-bt/`).
   - Never commit private personal data (`fw-wifi/network`, personal MAC addresses, Wi-Fi credentials).
   - Note that `.gitignore` uses an explicit whitelist strategy (`/*` at the root).
4. **Boot-Args Length Limit (Strictly < 607 Characters):**
   - XNU's `PE_parse_boot_argn` buffer is capped at **607 characters**.
   - Hardware defaults for `peridot` belong in `drivers/core/hakios.board.c` (triggered when `/arm-io/device_type == "peridot"`), not accumulated endlessly in the command line.
5. **Clean Working Baseline Cmdline:**
   ```text
   cpus=8 bootcpu=7 ufs=2 touchscreen=1 gpu=1 adsp=1 sound=4 wifi=1 haptic=5 touch_no_rpmh=1 small_cores_after=0 adreno_stream1=0 metal=1 thermal=2
   ```

---

## 2. Target Hardware Map (`peridot` / SM8635 / `cliffs`)

| Subsystem | Hardware / Base Address | Driver & Configuration Notes |
|---|---|---|
| **SoC** | Qualcomm Snapdragon 8s Gen 3 (`SM8635` / `cliffs`) | 4nm TSMC process |
| **CPU Topology** | 8 Cores: 1× Cortex-X4 (`0x700`), 4× Cortex-A720 (`0x300..0x600`), 3× Cortex-A520 (`0x000..0x200`) | `small_cores=0x07`, `bootcpu=7` (Prime X4 core), MPIDR shift 8 |
| **GICv3** | GICD `0x17100000`, GICR `0x17180000`, ITS `0x17140000` | ARM GICv3 interrupt controller |
| **Display** | 1.5K AMOLED (`1220 × 2712 @ 120 Hz`) | UEFI GOP Framebuffer at `0xe3940000` (size `0x02b00000`) |
| **Touchscreen** | Goodix GT9916R (Berlin-D) over SPI (`0xa90000`) | `touchscreen=1 touch_no_rpmh=1`, Reset GPIO 128, IRQ GPIO 129 |
| **GPU** | Qualcomm Adreno 735 (`0x43030b00`) + GMU (`0x07020100`) | `gpu=1 metal=1 adreno_stream1=0`, accelerated via `hakios-metal` |
| **Wi-Fi (WPSS)** | Qualcomm WCN6750 / QCA6750 WPSS + `ath11k` | Reserved memory `0x9bb00000` (size `0x01900000`), BDF `n16t` (`bd_n16t.elf`), TrustZone `pas_id = 6` |
| **Audio (ADSP)** | SM8635 Hexagon ADSP (`adspslpi_region@9d400000`, DTB `0xa1900000`) | `PAS_ADSP = 1`, `PAS_ADSP_DTB = 0x24`, `adsp=1 sound=4` |
| **Haptics** | Awinic AW8697 LRA | `haptic=5` |
| **Thermals** | 3 TSENS blocks (`0x0c222000`, `0x0c223000`, `0x0c224000`) | `thermal=2` |
| **Storage** | UFS 4.0 (`0x01d84000`) | `ufs=2` via UEFI block service |

---

## 3. Wi-Fi Subsystem Architecture & Lessons Learned (Crucial)

The Wi-Fi stack on `peridot` runs Qualcomm's `ath11k` driver inside the XNU kernel over a FreeBSD LinuxKPI shim (`wifi/lx/`), interfacing with Darwin's `Apple80211` framework and the `wifid` userspace daemon.

### 1. Board Data File (BDF) Calibration Mismatch
- **Working BDF:** `bd_n16t.elf` (59,944 bytes) is the **exact calibration file** for POCO F6.
- **Fatal Trap:** Forcing `bd_n16tgl.elf` (59,948 bytes) or other variants loads incorrect RF calibration tables, causing Qualcomm firmware's WHAL receive queue handler to crash immediately on incoming frames:
  ```text
  cmnos_assert.c:400: Asserted in whal_recv.c:whalRxMpduPop:6920
  ```
- **Rule:** Keep `hakios_wifi_board_data_file()` set to `n16t`.

### 2. `sta_ops` Pointer Lifetime & Error 6 (`ENXIO`)
- When the DSP reports an error, `link_thread` in `drivers/wifi/hakios.wifi.c` must **never** call `hakios_sta_attach(NULL)`.
- Setting `sta_ops = NULL` destroys the station interface permanently. Any subsequent toggle action in Settings calls `boundary_power(true)`, which immediately fails with `if (sta_ops == NULL) return ENXIO;` (error 6), causing the UI toggle to bounce back to OFF permanently.
- Instead, notify the framework via `hakios_apple80211_event(HAKIOS_STA_POWER, 0)` so `sta_ops` remains intact to process power-on requests.

### 3. HTC Credit Starvation & WMI Control Commands
- On WCN6750 (AHB bus), WMI commands sent over HTC endpoint 2 must **bypass credit flow control** (`credits = 0`).
- Artificially capping credits at negative values (e.g., `-10`) causes subsequent WMI commands (such as VDEV start or scan requests) to return `-EAGAIN (-35)` timeout, locking `wifid` and freezing the Settings UI.

### 4. Active vs. Passive Scanning
- In `wifi/lx/src/lx_80211.c`, ensure active probing is enabled by default (`quiet = false`) unless `wifi_passive > 0` is explicitly requested.
- Boolean traps like `!lx_os_option("wifi_active")` evaluate to `true` when the argument is omitted, unintentionally forcing passive-only scanning and hiding SSIDs.

### 5. TrustZone PAS ID & Metadata Allocation Boundary
- TrustZone PAS ID for WPSS is **`6`** (do NOT use `ssctl_id = 0x19`).
- Metadata header pages allocated at or above `0x970000000` are rejected by TrustZone with `(2/3, 0x1)`. Constrain metadata allocation below `0x960000000` / `0x970000000`.

---

## 4. Build & Verification Commands

### Step 1: Environment & Config
```bash
make peridot_defconfig
python3 tools/hakios-device.py check peridot
python3 tools/hakios-config.py check
```

### Step 2: Build Kernel
```bash
make kernel JOBS=$(nproc)
```
> Note: `exit status 2 is the kext symbol-set checks failing` is normal for this configuration as long as `make-kernel: built: hakios-kernel` is output and the script exits `0`.

### Step 3: Rebuild ath11k Object (if driver sources modified)
```bash
./tools/mk-ath11k.sh
```

### Step 4: Build UEFI Loader & APFS Images
```bash
make loader
make images
```

---

## 5. Known Pitfalls & Hardware Quirks

1. **AMD Ryzen Fastboot USB 3.x Hang:**
   - AMD Ryzen laptop USB 3.x / Type-C ports frequently hang or drop packets when staging large files (such as `system.apfs`).
   - **Fix:** Always connect via a **USB 2.0 port**, use a **stock 4-pin USB 2.0 Type-A to Type-C cable**, or an inexpensive USB 2.0 hub.
2. **Synchronizing Drivers into XNU:**
   - Drivers in `drivers/` are synced into `xnu-src/xnu-8796.121.2/` via `xnu/sync-drivers.sh` according to `drivers/map`.
   - Modifying files in `xnu/files/` directly requires mirroring them into `xnu-src/` if they are not part of `drivers/map`.
3. **Reference Implementations:**
   - `https://github.com/slayedkaito/xnu-kernel-peridot-amiaxf` (POCO F6, WCN6750 / SM8635)
   - `https://github.com/mariqnetr/hakios-shennong` (Xiaomi 14 Pro, WCN7850 / SM8650)
   - `https://github.com/shimoku74/hakios-xnu-kernel` (Upstream HakiOS XNU)
   - `https://hakios-developers.com/#/port/spacewar` (Nothing Phone 1, WCN6750 baseline)
