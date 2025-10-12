# Lumen Server Setup

Patches IDA Pro to skip CA certificate validation for the Lumen private server.

For more information about Lumen, see [naim94a/lumen](https://github.com/naim94a/lumen).

## Installation

1. Copy `ida-patcher.json` to your IDA Pro `plugins` directory
2. Copy `hexrays.crt` to your IDA Pro installation directory

## Configuration

1. Go to `Options -> General -> Lumina`
2. Deselect `Use public`
3. Set `lumen.abda.nl` as your primary host
4. Set `1235` as your primary port
5. Set `guest` as the username and password
