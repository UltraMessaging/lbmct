# Domain IDs with lbmct - Connected Topics for Ultra Messaging

## Introduction

This page is for deployments that use the DRO (Dynamic Routing Option).
Users without DRO can ignore this page.

In a DRO deployment, the network is divided into multiple Topic Resolution
Domains (TRDs), with DROs routing traffic between them.
Each TRD is given a "Domain ID", which is a simple integer.
This is configured into each DRO portal definition.
For the most part, applications do not need to concern themselves with
Domain ID; UM manages their discovery internally, using Topic Resolution (TR).

However, there is one circumstance where the application does need to
be aware of domain IDs: use of the Unicast Immediate Message (UIM) feature.
This specialized method of sending messages requires that the application
provide the network address of the destination context's "request" port.
In a non-DRO environment, this is simply the IP and port.
But in a DRO network, the Domain ID of the destination must be added.

## The Problem

In general, UM does not directly expose to the application the valid UIM
address of a desired destination context.
For example, an application discover a source and want to send a UIM to it.
But there is no API to give the UIM address.

A possible solution is for an application to determine its own UIM address
and advertise it to other applications.
This is reasonably easy in a non-DRO environment: you can inquire
of a running context its request IP and port.
But as of UM version 6.12, there is no API for determining the Domain ID,
which is a necessary part of the UIM address in a DRO environment.

The problem is made more difficult by the fact that it takes non-zero time
for a context to discover its Domain ID.
In some situations, it can take over a second.
In fact, network-wide connectivity is not established until the DRO routes
are discovered, which can take somewhat longer.
This is a general issue with DRO-enabled deployments that applications
must take into account: allowing enough time after startup for connectivity to
be established.

But at least connectivity is established automatically, if not instantaneously.
If an application wants to use UIMs, it still needs to know the proper
Domain IDs, which is currently not disclosed to applications.

## Possible Solutions

### New API for Domain ID

Informatica has a tentative roadmap item for adding an API to inform the
application on Domain ID and route discovery.

However, this does not help the CT package since it is desired to work
with UM versions 6.10 and beyond.
CT needs an existing solution.

### Configure Domain ID

The application can configure its Domain ID.
This is the solution chosen by the CT package.

This has the disadvantage of not being automated.
The application configuration needs to match the local DRO portal's
configuration.
But this method can be paired with the next approach.

### Parse Log Messages

UM applications will see UM log messages of the form:
```
Core-6259-7: Domain ID discovered via x.x.x.x:xxxx; context resides in Domain ID <N>.
```
where `<N>` is a simple integer.
An application can set up a logger application callback with the UM API
[lbm_log()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#aae14099b91f2919f424e81f20ca10951)
which UM calls with log messages.

This is not something that CT can do directly, so it is the application's
responsibility.
One problem with this approach is that the timing of the log message is
not predictable.
For example, if the DRO is down, the time to discover is unbounded.
In fact, if the application is deployed in a non-DRO environment, the
Domain ID will never be discovered.
Some knowledge of the deployment environment is necessary.

**Example**

Here's an example of extracting the Domain ID within the logger callback:
```
int domain_id = 0;  /* Global for demonstration purposes. */

int test_log_cb(int level, const char *message, void *clientd)
{
  int id1 = 0;  /* Major message ID number. */
  int id2 = 0;  /* Minor message ID number. */
  int ofs = 0;  /* Used to verify that sscanf is successful. */

  /* Only parse the message if domain ID isn't discovered yet. */
  if (domain_id == 0) {
    char *substr = strstr((char *)message, "-");  /* Skip to first dash. */
      if (substr != NULL) {
      (void)sscanf(substr, "-%9u-%9u:%n", &id1, &id2, &ofs);
    }

    /* At this point, id1 and id2 will either be zeros (if the log message 
     * has no message ID), or will be the major/minor ID numbers.
     * Check for "Domain ID discovered" log message.
     */
    if (id1 == 6259 && id2 == 7) {
      /* Domain ID discovery:
       *   Core-6259-7: Domain ID discovered via x.x.x.x:xxxx; context resides in Domain ID <N>.
       * Skip past most of the message text.
       */
      substr = strstr((char *)message, "in Domain ID ");
      if (substr != NULL) {
        ofs = 0;
        (void)sscanf(substr, "in Domain ID %9u.%n", &domain_id, &ofs);
        if (ofs != 0) {  /* if successful conversion... */
          printf("domain_ids=%d\n", domain_id);
        }
      }  /* if 'in Domain ID ' found */
    }  /* if 6259-7 */
  }  /* if domain_id == 0 */
...
}  /* test_log_cb */
```
