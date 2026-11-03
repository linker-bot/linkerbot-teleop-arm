# Safety boundary fixes

The process opens preconfigured CAN interfaces. Configure bitrate and bring the chosen interface up outside the process; the library no longer executes sudo/ip/chmod or modifies other interfaces. The bitrate constructor argument is validated for compatibility, not applied to the operating system.

Startup calibration is rejected. Automatic restarts must not replay zeroing commands. Explicit resetZero accepts 0..13 or 255 and successful send does not prove calibration completed.

Classic payloads over 8 bytes and nonstandard IDs are rejected before transmission. Receive filters discard invalid length, RTR, extended and error frames. Test-only adopted datagram sockets exercise the dependency SocketCANTransport read/write paths without hardware or root.

Printing and version reads now use the writer locks. The current runtime uses Session routes and complete response snapshots; cached reads explicitly expose validity and timestamps. See SESSION.md for ownership and recovery contracts.
