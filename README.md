# liblimeade

In this repository is a C library for the Limeade protocol.

## And what is that?

The Limeade protocol is means of communicating in-depth system behavior,
including process data, resource usage, and even individual syscalls over
a network in a way that is performant. Currently, this library supports
communication either through an SSH subsystem or through a UDP server, though
SSH is recommended for both encryption & confirmed delivery.

## And why would you do that?

The idea for such a thing originally sprouted when I wanted to exfiltrate
this type of information from a VM as a means of malware analytics. However,
this type of technology can also be utilzed for hueristic scanning, allowing
a single machine to do hueristics on multiple devices simultaneously,
increasing both the context that the antivirus can utilize to make decisions
& the sanctity of the antivirus itself. Additionally, storing and processing
this type of data could aid in digital forensics by centralizing important data.

> [!NOTE]
This library is still very new, and likewise, it is very unstable. As much, any
contributions are welcome as I attempt to reach a stable release of _v0.1_.

## Building

```bash
make so
```

## Installing

```bash
make install
```

## Documentation

Currently, the best form of documentation is the code in the `tests` directory,
as it should work with the library's current state at all times.

## Licensing

The code in this respository (exceptions below) is under the AGPLv3
license, except for the code in the `tests` directory, which is under the
Unlicense.

> [!WARNING]
**If you contribute code to this repository, it is assumed that it is under the
Unlicense if in the `tests` directory, or under the AGPLv3 if anywhere else. If
you wish for your contribution to be licensed differently, _you must say so_,
or else your contribution will be assumed to be under the same license as the
other code in its directory. Please note that code that is insisted to be under
a different license will most likely be rejected in order to avoid tainting the
code's licensing.**
