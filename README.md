# lincstation_leds
Daemon to set the Lincstation N2 LEDs on Linux other than Unraid

**Note:** This is a fork of the following repository: https://github.com/fazalmajid/lincstation_leds

The reason for this fork is that the original repository doesn't handle the case where the LEDs are in a blinking state and require a reset. Also, original repo missed convinient installtion process.

**Important:** While the original repository was tested on Alpine Linux, this fork is being developed and tested on Arch Linux. The installation instructions have been updated accordingly.

---

The Lincstation N2 is a fine all-flash NAS. It uses mostly standard PC
hardware, which means you can run your own OS on top of it instead of
proprietary ones like QNAP's QuTS Hero or Synology's whatever (if you weren't
deterred by Synology's abbhorent moves to force you to buy their marked-up
hard drives).

I installed Alpine Linux on mine. Unfortunately, if you do not use the
supplied freemium Unraid software, the LEDs will blink continuously, which is
particularly annoying in my case because they are in my peripheral vision.

Lincstation supplies a closed-source daemon with its Unraid distribution on a
USB stick (I removed mine), apparently written in Go but quite inefficient
because it makes the necessary I2C/SMBus calls to the LEDs by forking to the
i2c-tools utility `i2cset`. Github user ffalt reverse-engineered the protocol
in [this gist](https://gist.github.com/ffalt/984aa3644a90d4230eaf5b129aaf1eeb).

I asked the Anthropic Claude Sonnet 4 LLM to help me write a daemon written in
C to manage the LEDs:
https://claude.ai/public/artifacts/cc0feaf6-524f-431b-b1e8-505ad07f75f3

Claude did a surprisingly decent job of writing the boilerplate, you can see
in the Git history the changes I did to make it work properly, including
subtle bugs around handling rollover or incorrectly thinking disk I/O
utilization is 100% when no disk writes occurred.

I have only tested this on Alpine Linux but there is no reason it shouldn't
work on other flavors of Linux (you may need to make changes for systemd,
though).

## Building

To build, simply run `make`. You will need to have the packages `i2c-tools`
and `i2c-tools-dev` (and optionally `i2c-tools-doc`) installed.

## Installation

### Prerequisites

1. **Install required packages:**
   ```bash
   # Arch Linux (recommended for this fork)
   sudo pacman -S i2c-tools
   
   # Alpine Linux (original repository)
   apk add i2c-tools i2c-tools-dev
   
   # Ubuntu/Debian
   sudo apt install i2c-tools libi2c-dev
   
   # CentOS/RHEL/Fedora
   sudo dnf install i2c-tools i2c-tools-devel
   ```

2. **Ensure I2C modules are loaded:**
   ```bash
   sudo modprobe i2c-dev
   sudo modprobe i2c-i801  # or appropriate I2C driver for your hardware
   sudo modprobe i2c-core
   ```

   **Note for Arch Linux:** The `i2c-tools` package includes both the tools and development headers, so no additional `-dev` package is needed.

### Installing the Service

1. **Build and install:**
   ```bash
   make
   sudo make install
   ```

   This will:
   - Compile the binary
   - Install it to `/usr/local/bin/lincstation_leds`
   - Install the systemd service file to `/etc/systemd/system/lincstation-leds.service`
   - Enable the service to start at boot

2. **Start the service:**
   ```bash
   sudo systemctl start lincstation-leds.service
   ```

3. **Verify it's running:**
   ```bash
   sudo systemctl status lincstation-leds.service
   ```

### Uninstalling

To remove the service and binary:
```bash
sudo make uninstall
```

This will:
- Stop and disable the service
- Remove the binary and service file
- Reload systemd configuration

### Manual Testing

You can test the daemon manually before installing as a service:

```bash
env LEDS_DEBUG=true lincstation_leds
```

By default, it will update the LEDs once per second. If you want something
more real-time, you can change `ACTIVITY_SAMPLE_INTERVAL` in the code to
something shorter. Just be aware that at 10 Hz, it consumes 2–3% CPU on mine.
