# Installing Fiordland

This guide takes you from a download to a working desktop. It assumes you are coming from Windows or macOS and have not used Linux before. The same steps are on the website at [fiordlandlinux.org/docs/install](https://fiordlandlinux.org/docs/install/).

## What you need

- A 64-bit PC. UEFI is recommended; older BIOS machines work too.
- 4 GB of RAM at least. 8 GB or more is better.
- 30 GB of disk. Either a whole disk, which is erased, or room next to Windows on a UEFI computer.
- A USB stick of 4 GB or more, and an internet connection during the install. Packages are downloaded fresh.

Back up anything you want to keep before you start. Installing on a whole disk removes everything on it.

## Make the USB stick

1. Download the ISO from [fiordlandlinux.org](https://fiordlandlinux.org). The SHA-256 checksum is listed under the download button. On Linux, check the download with:

   ```
   sha256sum fiordland-*.iso
   ```

2. Write it to the stick:
   - Windows: [Rufus](https://rufus.ie). Pick **DD image mode** when it asks.
   - Windows or macOS: [balenaEtcher](https://etcher.balena.io).
   - Linux:

     ```
     sudo dd if=fiordland-*.iso of=/dev/sdX bs=4M status=progress oflag=sync
     ```

     Replace `sdX` with the USB stick. `lsblk` lists your drives. Picking the wrong one erases it.

## Boot from it

1. Restart and open the boot menu while the logo shows. The key is usually F12, F11, F8, Esc or F2.
2. Turn off **Secure Boot** in the firmware settings first. Fiordland's boot files are not signed yet.
3. Pick the USB stick, then **Fiordland Linux live** from the menu. The entry marked "with speech" starts the same session with a screen reader.

If the disk does not show up later in the installer, change the SATA mode in the firmware settings from RAID or Intel RST to AHCI.

## The live session

The live session starts on the default desktop, KDE Plasma, and opens the installer by itself. It also has an icon on the desktop, so you can start it again if you close it. You are logged in as `fiord` with no password, and nothing is written to your disks until you press Install on the Review page.

The first page offers **Install** and **Keep trying it**. Choose the second to explore the live desktop first.

## The installer

The installer is [rawaru](https://github.com/Fiordland-Linux/rawaru). Press **Install** on the first page, then work through five pages with **Continue** and **Back**.

### Region

Pick your time zone and keyboard layout.

### Disk

Pick the disk, then one of:

- **Erase the disk**: Fiordland takes the whole disk. Everything on it is removed, including other systems.
- **Install alongside Windows**: shrinks the Windows partition (nothing is deleted) and you choose how much space Fiordland gets. Free space on the disk is offered too. This needs a computer that starts in UEFI mode and a disk with a GPT layout.
- **Choose partitions**: for other systems or your own layout.
  - **Install Fiordland on**: any partition of 16 GB or more. Only this partition is formatted (Btrfs).
  - **Boot files go on** (UEFI): an existing FAT32 EFI system partition. It is kept as it is; Fiordland adds its own folder.
  - **Partition editor** opens KDE Partition Manager or GParted, whichever is installed, to shrink, delete or create partitions. The list updates when you close it.

Before **Continue** is enabled you tick a box that confirms the choice: that everything on the disk or partition will be erased, that you have a backup (when installing alongside Windows), or that Fiordland may use the free space.

A UEFI disk without an EFI system partition needs one first: 512 MB, FAT32, with the `boot` and `esp` flags. A BIOS computer with a GPT disk needs a 1 MB partition with the `bios_grub` flag.

Before you install next to Windows, in Windows:

- Turn off **Fast Startup** and shut down fully.
- Turn off **BitLocker** or **Device encryption** for the Windows drive. Encrypted partitions cannot be shrunk.

Windows runs a disk check the first time it starts after the shrink. This is normal.

### Account

Your full name, username, computer name (the default is `fiordland`) and password. **Log in automatically** skips the login screen when the computer starts.

### Desktop

Pick the desktop or window manager. KDE Plasma is the default and the easiest switch from Windows. The window managers are on their own tab. Some choices offer optional dotfiles; the ones that bring their own setup say so, and install it on first login.

To add, remove or swap desktops later, open **Desktops** (`weka`), which comes with the Fiordland desktop.

### Review

The page summarises your language, time zone, keyboard, disk, account and desktop. Click a row to go back and change it. It also shows the graphics setup the installer found. Nothing has been changed yet. Here you choose the kernel:

- **CachyOS kernel** (the default): tuned for responsiveness and games. The Arch kernel stays installed as a backup boot option.
- **Arch kernel**: the stock Arch Linux kernel, nothing else added.

If the CachyOS packages cannot be downloaded, the installer falls back to the Arch kernel and carries on.

Graphics drivers are picked for you. NVIDIA Turing and newer cards get the open NVIDIA driver, older NVIDIA cards use nouveau, and Intel and AMD get the matching Vulkan and VA-API drivers. Multilib is enabled for Steam and Wine.

### Install

Press **Install** on the Review page. The install takes 5 to 20 minutes depending on your internet speed. **Show details** opens the log. When it finishes, **Restart now** reboots into your new system and **Keep exploring** goes back to the live session. If it fails, **Save log** keeps the log and **Try again** returns to the Review page.

Remove the USB stick when the computer restarts.

## What you get

- Btrfs with automatic snapshots before every update. See [rollback.md](rollback.md).
- GRUB on UEFI and BIOS, with other systems found by os-prober. On a dual boot, the GRUB menu lists both Fiordland and Windows.
- zram swap, a firewall (ufw) that blocks incoming connections, Bluetooth, power profiles and Flathub.
- Gaming packages from the `fiordland-gaming` package: Steam, gamemode, MangoHud and gamescope, plus CachyOS's Proton and Wine builds. Remove them with `sudo pacman -Rns fiordland-gaming`.
- Arch packages from the stable channel, see [tawaki.md](tawaki.md#update-channel).

## After the first boot

Connect to the internet from the network icon in the panel, then update once, because the ISO may be a few days old:

```
tawaki update
```

Install your apps with `tawaki install`. See [tawaki.md](tawaki.md).

On the crest desktop, pair Bluetooth devices from the Bluetooth icon in the panel, or Settings → Connected devices → Pair new device. If the device shows a code or needs a PIN, crest asks for it in a prompt.

## If something goes wrong

- The live USB does not boot: turn off Secure Boot, rewrite the stick in DD mode, or try another USB port.
- Anything else: the [Troubleshooting](https://fiordlandlinux.org/wiki/Troubleshooting) page on the wiki, then the [Help desk](https://fiordlandlinux.org/wiki/Help_desk).
- A bug in the installer: open an [issue](https://github.com/Fiordland-Linux/fiordland/issues/new/choose) and attach the log from **Save log**.
