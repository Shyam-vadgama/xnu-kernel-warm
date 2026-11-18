# AGENTS.md — AI Agent Guide for HakiOS POCO F6 / Redmi Turbo 3 (`peridot`)

This document gives AI coding agents (Antigravity, Codex, Claude Code, Cursor, etc.) the exact context, safety rules, build commands, hardware map, and known pitfalls needed to help any **Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`, SM8635)** user build and boot **HakiOS (iOS 16.5 on XNU)** cleanly.

---

## 1. Non-Negotiable Safety Rules

1. **RAM-Only Boot (Zero Flash):**
   - **NEVER** run `fastboot flash` on any partition (`boot`, `vendor_boot`, `init_boot`, `dtbo`, `recovery`, `userdata`, etc.).
   - Always boot temporarily via `fastboot boot xnu-usb.img`. A normal reboot (or holding **Power for ~15 seconds**) restores HyperOS / Android completely untouched.
2. **Use Bootloader Fastboot, Never Fastbootd:**
   - Always use `adb reboot bootloader`, **never** `adb reboot fastboot`.
3. **Never Commit Proprietary or Personal Files:**
   - Do **not** commit Apple IPSW contents (`ios165/`, `sysimage/rootfs`, `userland/system.apfs`, `userland/root.apfs`, `xnuloader/identity.h`), vendor firmware dumps (`fw-wifi/`, `fw-gpu/`, `fw-audio/`, `fw-bt/`), or personal user data (`fw-wifi/network`, personal MAC addresses).
   - Note that `.gitignore` is a **whitelist** (`/*` at top). Do not blindly `git add -f` build artifacts.
4. **Boot-Args Limit (607 bytes):**
   - XNU's boot-args string is capped at **607 characters**. Keep `peridot` hardware defaults in `drivers/core/hakios.board.c` (triggered automatically when `/arm-io/device_type == "peridot"`), not on the command line.
5. **Hardware Safety:**
   - Ensure the phone bootloader is unlocked (UBL) and battery is reasonably charged (ideally >= 30–50%) before long testing sessions.

---

## 2. Target Hardware Reference (`peridot` / SM8635 / `cliffs`)

| Subsystem | Hardware / Address | Notes |
|---|---|---|
| **Device** | Xiaomi POCO F6 / Redmi Turbo 3 (`peridot`) | Qualcomm Snapdragon 8s Gen 3 (`SM8635` / `cliffs`) |
| **CPU Topology** | 8 cores: 3× Cortex-A520 (`0x000..0x200`), 4× Cortex-A720 (`0x300..0x600`), 1× Cortex-X4 (`0x700`) | `small_cores=0x07`, `bootcpu=7` (Prime X4), MPIDR shift 8 |
| **GICv3** | GICD `0x17100000`, GICR `0x17180000`, ITS `0x17140000` | ARM GICv3 |
| **Display** | 1.5K AMOLED `1220 × 2712 @ 120 Hz` | UEFI GOP framebuffer `0xe3940000` (size `0x02b00000`) |
| **Touchscreen** | Goodix GT9916R (Berlin-D) over SPI (`0xa90000`) | `touchscreen=1 touch_no_rpmh=1`, Reset GPIO 128, IRQ GPIO 129 |
| **GPU** | Adreno 735 (`0x43030b00`) + GMU (`0x07020100`) | `gpu=1 metal=1 adreno_stream1=0` |
| **Wi-Fi (WPSS)** | WCN6750 / QCA6750 WPSS + `ath11k` QMI/GLINK | Reserved mem `wpss_region@9bb00000` (`0x01900000`), BDF `n16t` (`bd_n16t.elf`), **TrustZone `pas_id` = `6`** *(do NOT confuse with `ssctl_id = 0x19`!)* |
| **Audio (ADSP)** | SM8635 ADSP (`adspslpi_region@9d400000`, DTB `0xa1900000`) | `PAS_ADSP = 1`, `PAS_ADSP_DTB = 0x24`, `adsp=1 sound=4` |
| **Haptics** | AW8697 LRA | `haptic=5` |
| **Thermal** | 3 TSENS blocks (`0x0c222000`, `0x0c223000`, `0x0c224000`) | `thermal=2` |
| **Storage** | UFS (`0x01d84000`) via loader block service | `ufs=2` |

### Prebuilt UEFI & Boot Files Already Included
`devices/peridot/prebuilt/` already contains everything needed to assemble the temporary RAM-boot image (`xnu-usb.img`) — users do **not** need to compile Mu-Silicium UEFI from scratch:
- `devices/peridot/prebuilt/SILICIUM_UEFI.fd` (3 MiB UEFI image for `peridot`)
- `devices/peridot/prebuilt/BootShim.bin`
- `devices/peridot/prebuilt/peridot.dtb`
- `devices/peridot/prebuilt/mkbootimg.py`
- `devices/peridot/prebuilt/ramdisk`

---

## 3. End-to-End Build Workflow (Linux or WSL2 Debian/Ubuntu)

Make sure you are on branch **`peridot-2.3.4`**.

### Step 1: Configure & Validate Device Tree / Config
```bash
make peridot_defconfig
python3 tools/hakios-device.py check peridot
python3 tools/hakios-config.py check
```
Both checks must pass cleanly (`all hold together`).

### Step 2: Unpack the User's Own iOS 16.5 (`20F66`) IPSW
Each user must supply their own **iOS 16.5 (build `20F66`) IPSW for iPhone X (`iPhone10,3` / `iPhone10,6`)**:
```bash
make unpack IPSW=/path/to/iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw
```
This populates `sysimage/` (or `ios165/`) with the rootfs, cryptex, dyld shared cache (`20F66__iPhone10,3_6`), and `DeviceTree.d22ap.im4p` needed by `xnuloader/mkidentity.sh`.

### Step 3: Pull Vendor Firmware from the User's Own POCO F6
Connect the POCO F6 booted in **HyperOS / Android** with USB Debugging enabled, then pull proprietary firmware from the phone itself:
```bash
DEVICE=peridot make firmware
```
Verify the pulled files exist in:
- `fw-gpu/`: `gen71100_sqe.fw`, `gen71100_gmu.bin`, `gen71100_zap.mbn`
- `fw-wifi/`: `wpss.mdt`, `wpss.b00..b12`, `bd_n16t.elf` / `bdwlan.elf`, `regdb_xiaomi.bin` / `regdb.bin`, `Data.msc` (and optionally `wlan_mac.bin` or `mac` for the unit's own Wi-Fi MAC)
- `fw-audio/`: `adsp.mdt`, `adsp.b*`, `adsp_dtbs.*`, `aw882xx_acf.bin`
- `fw-bt/`: Bluetooth firmware blobs (if testing `bt=1`)

### Step 4: Build XNU Kernel, UEFI Loader, and APFS Images
```bash
make kernel JOBS=$(nproc)
make loader
make images
```
Expected build outputs:
- `hakios-kernel` (~15.5 MB XNU Mach-O binary)
- `xnuloader/loader.efi` (~71 KB UEFI loader built with `xnuloader/identity.h` from the user's IPSW)
- `userland/root.apfs` (~11 MB small RAM disk containing `fw/*`, daemons, and injected dylibs)
- `userland/system.apfs` (~2.5 GB iOS 16.5 system volume)

> **Important Note on `make kernel`:** `tools/make-kernel.sh` will print `exit status 2 is the kext symbol-set checks failing, as they do on this configuration` at the end. This is **normal and expected** as long as it prints `make-kernel: built: hakios-kernel` and exits `0`.

### Step 5: Boot on POCO F6 (RAM-Only)
Recommended safe boot-args for `peridot`:
```text
cpus=8 bootcpu=7 ufs=2 touchscreen=1 gpu=1 adsp=1 sound=4 wifi=1 haptic=5 touch_no_rpmh=1 small_cores_after=0 adreno_stream1=0 metal=1 thermal=2
```
Boot sequence:
1. Reboot phone to Fastboot: `adb reboot bootloader`
2. Pack `usb-payload.bin` (containing `loader.efi`, `usb=1`, `kernel.ver`) and wrap `BootShim.bin` + `SILICIUM_UEFI.fd` + `usb-payload.bin` + `peridot.dtb` into Android Boot Image v4 (`xnu-usb.img`).
3. Boot UEFI temporarily in RAM: `fastboot boot xnu-usb.img`
4. Wait until `fastboot getvar product` reports `xnuloader`.
5. Upload `system.apfs` once per UEFI session:
   ```bash
   fastboot oem ramdisk
   fastboot stage userland/system.apfs
   ```
6. Pack `run-payload.bin` (`hakios-kernel` + `root.apfs` + cmdline), stage, and launch:
   ```bash
   fastboot stage run-payload.bin
   fastboot oem run
   ```
7. Fetch live kernel/launchd/SpringBoard logs at any time while running:
   ```bash
   fastboot oem log
   fastboot get_staged run.log
   ```

---

## 4. Known Pitfalls & Troubleshooting for AI Agents

1. **AMD Ryzen Windows USB 3.x / Type-C Fastboot Hang:**
   - On Windows laptops with AMD Ryzen xHCI controllers, `fastboot boot` or `fastboot stage` (especially the 2.5 GB `system.apfs`) frequently stalls or fails on USB 3.x / Type-C ports.
   - **Fix (no `regedit` needed):** Instruct the user to use a **USB 2.0 port**, a **4-pin USB 2.0 Type-A to Type-C cable** (such as the stock Xiaomi charger cable), an inexpensive **USB 2.0 hub**, or Linux / WSL2 via `usbipd-win`.
2. **Editing Kernel Files Under `xnu/files/` vs `drivers/`:**
   - `xnu/sync-drivers.sh` only syncs files listed in `drivers/map` into `xnu-src/xnu-8796.121.2/`.
   - If you edit files under `xnu/files/` (for example `xnu/files/iokit/Kernel/MiniIOMobileFramebuffer.cpp`), you **must** also update the corresponding file in `xnu-src/xnu-8796.121.2/` before running `tools/make-kernel.sh`.
3. **Wi-Fi TrustZone PAS ID (`step 8 failed`):**
   - If the boot log shows `TRUSTZONE DOES NOT TAKE THE HEADER: smc 0x42000201, arguments 0x19` returning `2, answer 0x1`, verify `soc_pas_wpss=6` in `devices/peridot/soc.args` and `{ "pas_wpss", 6U }` in `drivers/core/hakios.board.c`. `0x19` is `ssctl_id`, whereas TrustZone PAS expects `6`.
4. **Config Validation Conflicts:**
   - Do not duplicate boot-args in `CONFIG_BOOTARGS` (e.g. `wifi=1`) if a dedicated `CONFIG_*` symbol (like `CONFIG_WIFI=y`) already generates it — `hakios-device.py check peridot` will flag it.
5. **WSL2 / DrvFs (`/mnt/c/...`) Considerations:**
   - Avoid running recursive `grep` across the whole repo root over `/mnt/c/` (because `ios165/` and `userland/system.apfs` contain multi-GB images). Target specific subdirectories (`drivers/`, `devices/peridot/`, `userland/src/`, `xnuloader/`).
6. **Optional Display / Dynamic Island Testing:**
   - The iOS 16.5 (`20F66`) dyld shared cache contains Apple's native `SBSystemApertureController` / `_UIStatusBarVisualProvider_DynamicSplit` classes. Look at `MiniIOMobileFramebuffer.cpp` (`fbwidth`, `fbheight`, `fbscale`, `fbsubtype`) when experimenting with alternate iPhone display classes (e.g. `1179x2556` subtype `2556` for iPhone 14 Pro).
