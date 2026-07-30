# `liblimeade`

The code in this repository defines the Limeade protocol. The Limeade protocol
is a means of communicating system behavior, including process data, resource
usage data, and syscalls, over a network through SSH.

## "Why?"

This idea originally sprouted when I wanted to exfiltrate this type of
information from a VM as a means of malware analytics. However, this type
of technology can also be utilized for heuristic scanning, allowing a single
machine to do heuristics on multiple devices simultaneously, increasing both
the context that the antivirus can utilize to make decisions and the sanctity
of the antivirus itself. Additionally, storing and processing this type of data
could aid in digital forensics by centralizing important data.

## Building

NOTE
---

I haven't even released v0.1, everything's prone to change, little is prone to
working.

The shared library can be built with:

```bash
make so
```

There is not yet a direct means of installation/uninstallation

## Documentation

There is not yet any documentation. However, the headers are well commented.

## Licensing

The code in this respository (exceptions below) is under the AGPLv3
license, except for the code in the `tests` directory, which is under the
Unlicense.

**If you contribute code to this repository, it is assumed that it is under the
Unlicense if in the `tests` directory, or under the AGPLv3 if anywhere else. If
you wish for your contribution to be licensed differently, _you must say so_,
or else your contribution will be assumed to be under the same license as the
other code in its directory. Please note that code that is insisted to be under
a different license will most likely be rejected in order to avoid tainting the
code's licensing.**
