# Unfolded Circle Dock 2 Firmware Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

_Changes in the next release_

---

## 0.10.0 - 2024-02-12
### New Features
- Allow disabling the status LED with a brightness value set to 0, except for OTA update, identify, and factory reset indications ([#51](https://github.com/unfoldedcircle/feature-and-bug-tracker/issues/51)).

### Bug Fixes
- Disable ethernet LED if there's no link ([#36](https://github.com/unfoldedcircle/feature-and-bug-tracker/issues/36)).  
  With a link and the LED brightness set to 0, the LED will be switched off 3 seconds after receiving an IP address.
- Improved OTA update error handling for invalid, aborted or dropped uploads.

### Changes
- Reduce LED pattern debug logging

## 0.9.2 - 2023-07-11
### Bug Fixes
- Additional OTA update error codes and improved error handling
  
## 0.9.0 - 2023-07-06
### New Features
- GlobalCache TCP API support for sending IR codes
- Factory reset indication with red blinking LED

### Bug Fixes
- Speedup WiFi connection startup

### Other
- Updated system libraries with general bug fixes
