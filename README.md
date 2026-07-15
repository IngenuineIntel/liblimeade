# `liblimeade`

This repository holds a C implementation of the Limeade protocol, a protocol
to transmit live information about a computer such as resource usage,
past and present processes, and even syscall history. Limeade is designed to be
used in a host-client configuration, having only a single host, and a variable
amount of clients. Limeade also provides a way to interface control over the
clients by keeping track of different sessions and providing the ability to
execute commands on client machines.

This protocol operates within an SSH subsystem. This is so that this protocol
can be used in production/professional environments, without me having to
build/maintain/patch encryption systems that would take development time away
from the acutal protocol; no encryption I build is going to be better than a
project as large as OpenSSH, not to mention authentication.

## Purpose & Capability

This code is licensed under the GNU General Public License, version 3 or later.
This, for one, exempts me from liability caused by maluse. For two, this
ensures that programs utilizing this protocol are open source, perpetuating the
development of open source tools for fleet management and remote monitoring, of
which there currently aren't many. Additionally, the level of remote monitoring
this protocol is capable of makes feasible the concept of centralized malware
analytics and historical perspective on how malware attacks could have unfolded.
Additionally, this protocol can be used for the analysis of malware behavior in
a closed environment.
