# lbmct - Connected Topics for Ultra Messaging

## Introduction

The "lbmct" package is an API wrapper around the Ultra Messaging library which
adds the concept of end-to-end connectedness to UM's normal pub/sub messaging
model.
These "Connected Topics" (CTs) help Publisher and Subscriber applications
keep track of each other.

Instead of creating normal topic Sources and Receivers,
publishers and subscribers create CT Sources and Receivers.
The lbmct layer exchanges handshake messages between the CT Source and
Receiver to establish shared state.
Once state is synchronized, callbacks are made to the publishing and subscribing
applications to deliver connection information and indicate liveness.

Once the connection is live, the application uses the normal methods for
sending and receiving messages.
There is essentially no overhead added in the message path.

When the connection is deleted, either gracefully (via API call) or
abruptly (e.g. by application or network failure),
callbacks are made to the publishing and receiving applications to
deliver deletion events.

The rest of this page provides a high-level description of the CT package.
You can also jump to other pages:
* [Release Notes](Release_notes.md)
* [User Guide](Userguide.md) - explains how to write Connected
Topic applications.
* [API Reference](API.md) - details on each API.
* [Internal Design](Internal_Design.md) - details of the CT implementation.

---

## Features

* Publishing and subscribing applications can provide a block of user-defined
metadata which will be delivered to the connection peer via the connection
create callback.  This is typically used for identification.

* As part of connection creation and deletion, both the publisher and
subscriber are informed of the first and last topic sequence numbers
delivered to the subscriber.

* A single Connected Source can establish connections with
multiple Connected Receivers (on the same topic).
Similarly, a single Connected Receiver can establish connections with
multiple Connected Sources (on the same topic).
Each connection is independent and is maintained separately.

* Once the connection create callback is called, the sending application
can safely send messages without concern of "head loss" for that receiver.

* CT is compatible with most UM features.
But see [Limitations](#limitations).

* Since CT uses external interfaces of UM, it is not tied to a specific
version of UM.
However, it does make use of the [Sending to Sources](https://ultramessaging.github.io/currdoc/doc/Design/umfeatures.html#sendingtosources) feature,
so CT does require at least UM version 6.10.

* CT is provided in source form via the
[Ultra Messaging Github](https://github.com/UltraMessaging).
Users are invited to improve CT and submit pull requests.

* Both normal and Connected Sources and Receivers can share the same
UM context, and even the same transport sessions.
But see [Interoperability](#interoperability).

---

## Opportunities and Limitations

Most of the limitations that follow could be implemented,
with varying degrees of effort.

* CT is not compatible with UM’s queuing features, including ULB.

* CT does not yet work with Persistence.
I see some fundamental problems with a general Persistence solution since
a recovering subscriber may not have access to a live publisher with which
to exchange handshakes.
But there may be some kind of limited version of it that could be implemented.

* CT does not yet work with wildcard receivers.
But it should work fine with appropriate CT API additions.

* CT does not yet work with SmartSources.
However, this work is planned.

* CT does not yet have connection-level keepalive functionality.
This will require discussion as it could introduce undesirable side effects
like latency jitter and scalability problems.

* CT does not yet have a "sequential mode" whereby an application can
create and donate its thread.

* CT does not yet have Java and .NET wrappers.

* Do we need a "Force Quit" flag for connection and CT objectd?

* In a DRO environment, you need to configure CT what the domain ID is.
CT does not discover the domain ID dynamically.
If using DRO, see [Domain ID](Domain_ID.md).

---

## Caveats

* Each connection is reasonably light-weight, but might not scale into
the thousands.

* CT is not officially part of the UM product family.
Support for CT may not be as rapid as product support,
but we are committed to maintaining all of the code in the UM Github,
including CT.
Note that:

   * Informatica does not include CT in its QA testing.
Note however that CT includes a reasonably extensive automated self-test.

   * Informatica does not commit to ensuring that future UM development will
be compatible with CT.
Note however that CT was developed with knowledge of the current UM roadmap,
and every effort has been taken to make it forward compatible with known
product evolution directions.

* The receiver-side BOS/EOS events are suppressed.
Their delivery could not be made reliable due to their
“transport session-level” scope.
However, CT’s per-connection create/delete callbacks are reliable
replacements.

   * Note that source-side connect/disconnect events are *not* suppressed,
but those events are not correlated to a CT connection.
Apps should use CT’s per-connection create/delete callbacks instead.

* CT makes use of receive-side, per-source, delivery controller create/delete
notification (“receiver source_notification_function”),
so the application may not use it.
However, the per-connection create/delete callbacks are reliable replacements.

* CT control handshake messages are sent on the same data path as application
messages, and are therefore delivered to the receiving application as messages.
They are easy to differentiate from application messages by their use of a
CT-specific Message Property.

   * Note that receiving applications can get control messages that are
intended for other connections.
The user is advised to ignore CT control handshake messages,
except that they consume topic-level sequence numbers.

* When connected sources and receivers are deleted by API call,
there is a handshake between the source and receiver to do a clean disconnect.
However, the delete functions return before the handshake is complete.
The application should allow the asynchronous part of the disconnect to
complete normally.

   * Attempting to delete the parent CT object will fail if there are still
connections waiting to be fully disconnected.
There is no “force quit” API, but the process can always be exited.

   * The per-connection delete callback informs the application as
connections are closed.

---

## Interoperability

* CT Receivers make use of the “send to source” feature,
which first became available in UM 6.10.
Note that pre-6.10 versions of UM are still able to receive messages from
a CT Source, as “observers” (see next item).

* A CT Source can be subscribed by a non-CT Receiver.
This is a valid “observer” use case and will not trigger any CT-oriented
activities on the source.
Note that the source will be nominally unaware of non-CT Receivers and will
not be able to issue any kind of warning if this use case is not desired.

* A CT Receiver subscribing to a non-CT Source is not supported.
The CT Receiver will repeatedly send CT control handshake messages to the
source context.
Since it will not receive the proper handshake response,
the receiver will be “deaf” to the non-CT Source.
Thus, it is a bad idea to have both CT and non-CT Sources for the same topic.

* When SmartSource is added, the sender will need to be at least UM 6.10,
although the SmartSource code could be conditionally compiled.

---

You can now jump to other pages:
* [Release Notes](Release_notes.md)
* [User Guide](Userguide.md) - explains how to write Connected
Topic applications.
* [API Reference](API.md) - details on each API function and structure.
* [Internal Design](Internal_Design.md) - details of the CT implementation.
