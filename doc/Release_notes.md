# lbmct v0.5 Release Notes - Connected Topics for Ultra Messaging

## Version 0.5

Support Windows DLL.

Release date: 07-Dec-2018

### New Features

* Configuration option [pre_delivery](API.md#lbmct_config_tpre_delivery)
allows a connected receiver to get messages from a legacy non-connected
source.

* Added support for deployments where it is not possible to inform CT about
the domain IDs of the source and receivers.
Changed default domain ID to -1.
If no valid domain ID is sent, the receiver sends all UIMs to the
source's "source string".
Note that this has a performance penalty (how much depends on the size
of the receiving context's resolver cache).

* Modified Google Test self-test to be Windowds-friendly.
It is now possible to run the self-test suite under Visual Studio.

* Minor changes to improve Windows DLL.

See [Feature List](../README.md#features) for full feature list.

### Fixed Limitations

* None.

---

## Version 0.4

Support Windows DLL.

Release date: 27-Nov-2018

### New Features

* Support for Windows DLL (issue #2).
See [Windows DLL Build](Internal_Design.md#windows-dll-build)

See [Feature List](../README.md#features) for full feature list.

### Fixed Limitations

* None.

---

## Version 0.3

Add doc that CT is incompatible with Late Join.
See [Opportunities and Limitations](../README.md#opportunities-and-limitations).

Release date: 16-Nov-2018

### New Features

* None.

See [Feature List](../README.md#features) for full feature list.

### Fixed Limitations

* None.

---

## Version 0.2

Windows-specific bug fix.

Release date: 16-Nov-2018

### New Features

* None.

See [Feature List](../README.md#features) for full feature list.

### Fixed Limitations

* Issue #1 - Win apps hang in most API calls.  **FIXED**

---

## Version 0.1

This is the first public release of CT.

Release date: 15-Nov-2018

### Features

* See [Feature List](../README.md#features)

### Fixed Limitations

* None.
