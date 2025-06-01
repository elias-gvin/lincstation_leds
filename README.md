# lincstation_leds
Daemon to set the Lincstation N2 LEDs on Linux other than Unraid

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

To build, simply run `make`. You will need to have the packages `i2c-tools`
and `i2c-tools-dev` (and optionally `i2c-tools-doc`) installed.

You can activate debug output on stdout by calling:

```
env LEDS_DEBUG=true lincstation_leds
```

By default, it will update the LEDs once per second. If you want something
more real-time, you can change `ACTIVITY_SAMPLE_INTERVAL` in the code to
something shorter. Just be aware that at 10 Hz, it consumes 2–3% CPU on mine.
