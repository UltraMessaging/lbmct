# lbmct v0.4 API Reference - Connected Topics for Ultra Messaging

This page provides details on each API function and structure.

## lbmct_create

```
int lbmct_create(lbmct_t **ctp, lbm_context_t *ctx, lbmct_config_t *config,
  const char *metadata, size_t metadata_sz);
```

Create a CT object, associating it with a context.
A CT object uses an internal thread to manage the connections.

**Parameters**

* **`ctp`** - A pointer to a pointer to a CT object.
Will be filled in by this function to point to the newly created
lbmct_t object.

* **`ctx`** - A normal UM context object.
Only one CT object may be associated with a given context object.

* **`config`** - Pointer to configuration structure.
Pass NULL for defaults.
See [lbmct_config_t](#lbmct_config_t).

* **`metadata`** - An optional pointer to a block of application
identification information.
The information will be copied.
If no metadata is desired, pass NULL.

* **`metadata_sz`** - Number of bytes pointed to by `metadata`.
If no metadata is desired, pass zero.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_delete

```
int lbmct_delete(lbmct_t *ct);
```

Deletes a CT object.
It is the application's responsibility to first delete all CT Sources and
receivers and wait for their graceful deletions to complete.

**Parameters:**

* **`ct`** - A pointer to a CT object.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_src_create

```
int lbmct_src_create(lbmct_src_t **ct_srcp, lbmct_t *ct, const char *topic_str,
  lbm_src_topic_attr_t *src_attr,
  lbm_src_cb_proc src_cb,
  lbmct_src_conn_create_function_cb src_conn_create_cb,
  lbmct_src_conn_delete_function_cb src_conn_delete_cb,
  void *clientd);
```

Creates a CT Source object.

**Parameters:**

* **`ct_srcp`** - A pointer to a pointer to a CT Source object.
Will be filled in by this function to point to the newly created
lbmct_src_t object.

* **`ct`** - A pointer to a CT object.

* **`topic_str`** - The topic string.
Topic strings should be limited in length to 246 characters
(not including the final null).

* **`src_attr`** - Pointer to a Src Topic attribute object for passing in
options.
This is the same attribute object that is used with normal source creation.
See [lbm_src_topic_attr_create()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#ad6ed0c9ec2565764a31f2db09a0e43c3).

* **`src_cb`** - Pointer to application callback function.
The underlying UM source calls this function in response to events related
to the source.
If NULL, source events are not delivered.
See [lbm_src_cb_proc](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a0fe745a748ae36759c312737d9f127f7).

* **`src_conn_create_cb`** - Pointer to application callback function.
The CT Source calls this function each time a new connection is established
for this topic.
See [lbmct_src_conn_create_function_cb](#lbmct_src_conn_create_function_cb).

* **`src_conn_delete_cb`** - Pointer to application callback function.
The CT Source calls this function each time an existing function is closed
on this topic.
See [lbmct_src_conn_delete_function_cb](#lbmct_src_conn_delete_function_cb).

* **`clientd`** - A pointer to client (application) state data.
This pointer is not dereferenced by CT.
Rather, it is simply passed through to the src_cb, src_conn_create_cb,
and src_conn_delete_cb application callback functions.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_src_conn_create_function_cb

```
typedef void *(*lbmct_src_conn_create_function_cb)(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *clientd);
```

Function pointer for connection creation.
The CT Source calls this application callback when a new connection is
established.
The function should return a pointer to its own connection-specific state,
or NULL if no state is needed.
See [lbmct_src_create()](#lbmct_src_create)

**Parameters:**

* **`src_conn`** - A pointer to a CT connection object.
This object is not created directly by the application, but rather is
created and managed by the CT Source.

* **`peer_info`** - A pointer to a structure containing information about
the connecting CT Receiver.
See [lbmct_peer_info_t](#lbmct_peer_info_t).

* **`clientd`** - A pointer to application state.
This is a copy of the clientd pointer passed to
[lbmct_src_create()](#lbmct_src_create).

**Returns**

Pointer to application's connection-specific state information, or NULL
if not needed.
This pointer is passed to the
[lbmct_src_conn_delete_function_cb](#lbmct_src_conn_delete_function_cb]
application callback when the connection is closed.


## lbmct_src_conn_delete_function_cb

```
typedef void *(*lbmct_src_conn_delete_function_cb)(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *clientd, void *conn_clientd);
```

Function pointer for connection deletion.
The CT Source calls this application callback when a new connection is
closed.
See [lbmct_src_create()](#lbmct_src_create)

**Parameters:**

* **`src_conn`** - A pointer to a CT connection object.
This object is not created directly by the application, but rather is
created and managed by the CT Source.

* **`peer_info`** - A pointer to a structure containing information about
the connecting CT Receiver.
See [lbmct_peer_info_t](#lbmct_peer_info_t).

* **`clientd`** - A pointer to application state.
This is a copy of the clientd pointer passed to
[lbmct_src_create()](#lbmct_src_create).

* **`conn_clientd`** - A pointer to application state.
This is a copy of the pointer returned by
[lbmct_src_conn_create_function_cb][#lbmct_src_conn_create_function_cb]
at connection creation time.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_src_get_um_src

```
lbm_src_t *lbmct_src_get_um_src(lbmct_src_t *ct_src);
```

Function to get the underlying UM Source object associated with a CT Source.
That UM Source object is needed for sending messages.

**Parameters:**

* **`ct_src`** - A pointer to a CT Source object.

**Returns**

UM Source object pointer.


## lbmct_src_delete

```
int lbmct_src_delete(lbmct_src_t *ct_src);
```

Initiates deletion of a CT Source object.
Any open connections associated with the Source are gracefully closed.

**Parameters:**

* **`ct_src`** - A pointer to a CT Source object.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_rcv_create

```
int lbmct_rcv_create(lbmct_rcv_t **ct_rcvp, lbmct_t *ct, const char *topic_str,
  lbm_rcv_topic_attr_t *rcv_attr,
  lbm_rcv_cb_proc rcv_cb,
  lbmct_rcv_conn_create_function_cb rcv_conn_create_cb,
  lbmct_rcv_conn_delete_function_cb rcv_conn_delete_cb,
  void *clientd);
```

Creates a CT Receiver object.

**Parameters:**

* **`ct_rcvp`** - A pointer to a pointer to a CT Receiver object.
Will be filled in by this function to point to the newly created
lbmct_rcv_t object.

* **`ct`** - A pointer to a CT object.

* **`topic_str`** - The topic string.
Topic strings should be limited in length to 246 characters
(not including the final null).

* **`rcv_attr`** - Pointer to a Src Topic attribute object for passing in
options.
This is the same attribute object that is used with normal receiver creation.
See [lbm_rcv_topic_attr_create()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#ad6ed0c9ec2565764a31f2db09a0e43c3).

* **`rcv_cb`** - Pointer to application callback function.
The underlying UM Receiver calls this function in response to events related
to the receiver.
See [lbm_rcv_cb_proc](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a0fe745a748ae36759c312737d9f127f7).

* **`rcv_conn_create_cb`** - Pointer to application callback function.
The CT Receiver calls this function each time a new connection is established
for this topic.
See [lbmct_rcv_conn_create_function_cb](#lbmct_rcv_conn_create_function_cb).

* **`rcv_conn_delete_cb`** - Pointer to application callback function.
The CT Receiver calls this function each time an existing function is closed
on this topic.
See [lbmct_rcv_conn_delete_function_cb](#lbmct_rcv_conn_delete_function_cb).

* **`clientd`** - A pointer to client (application) state data.
This pointer is not dereferenced by CT.
Rather, it is simply passed through to the rcv_cb, rcv_conn_create_cb,
and rcv_conn_delete_cb application callback functions.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_rcv_conn_create_function_cb

```
typedef void *(*lbmct_rcv_conn_create_function_cb)(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *clientd);
```

Function pointer for connection creation.
The CT Receiver calls this application callback when a new connection is
established.
The function should return a pointer to its own connection-specific state,
or NULL if no state is needed.
See [lbmct_rcv_create()](#lbmct_rcv_create)

**Parameters:**

* **`rcv_conn`** - A pointer to a CT connection object.
This object is not created directly by the application, but rather is
created and managed by the CT Receiver.

* **`peer_info`** - A pointer to a structure containing information about
the connecting CT Source.
See [lbmct_peer_info_t](#lbmct_peer_info_t).

* **`clientd`** - A pointer to application state.
This is a copy of the clientd pointer passed to
[lbmct_rcv_create()](#lbmct_rcv_create).

**Returns**

Pointer to application's connection-specific state information, or NULL
if not needed.
This pointer is passed to the receiver callback (rcv_cb) in the
[lbm_msg_t](https://ultramessaging.github.io/currdoc/doc/API/structlbm__msg__t__stct.html)
structure field `source_clientd`.
It is also passed to the
[lbmct_rcv_conn_delete_function_cb](#lbmct_rcv_conn_delete_function_cb]
application callback when the connection is closed.


## lbmct_rcv_conn_delete_function_cb

```
typedef void *(*lbmct_rcv_conn_delete_function_cb)(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *clientd, void *conn_clientd);
```

Function pointer for connection deletion.
The CT Receiver calls this application callback when a new connection is
closed.
See [lbmct_rcv_create()](#lbmct_rcv_create)

**Parameters:**

* **`rcv_conn`** - A pointer to a CT connection object.
This object is not created directly by the application, but rather is
created and managed by the CT Receiver.

* **`peer_info`** - A pointer to a structure containing information about
the connecting CT Source.
See [lbmct_peer_info_t](#lbmct_peer_info_t).

* **`clientd`** - A pointer to application state.
This is a copy of the clientd pointer passed to
[lbmct_rcv_create()](#lbmct_rcv_create).

* **`conn_clientd`** - A pointer to application state.
This is a copy of the pointer returned by
[lbmct_rcv_conn_create_function_cb][#lbmct_rcv_conn_create_function_cb]
at connection creation time.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_rcv_delete

```
int lbmct_rcv_delete(lbmct_rcv_t *ct_rcv);
```

Initiates deletion of a CT Receiver object.
Any open connections associated with the Receiver are gracefully closed.

**Parameters:**

* **`ct_rcv`** - A pointer to a CT Receiver object.

**Returns**

0 for Success and -1 for Failure.
On Failure, use
[lbm_errmsg()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a22a4650d3a5b649c84a0c05adedcc055)
to obtain a pointer to an ASCII string containing the error message.


## lbmct_debug_dump

```
void lbmct_debug_dump(lbmct_t *ct, const char *msg);
```

Write UM log messages containing diagnostic information about the CT object,
including a list of recent events.
See [Internal_Design.md#recent-events](Internal_Design.md#recent-events).

**Parameters:**

* **`ct`** - A pointer to a CT object.

* **`msg`** - A pointer to an ASCII string to be printed with the diagnostic
information.

**Returns**

none.


## lbmct_peer_info_t

```
typedef struct {
  int status;  /* LBMCT_CONN_STATUS_* */
  lbm_uint32_t flags;  /* Bitmap of LBMCT_PEER_INFO_FLAGS_* */
  char *src_metadata;
  size_t src_metadata_len;
  char *rcv_metadata;
  size_t rcv_metadata_len;
  char rcv_source_name[LBM_MSG_MAX_SOURCE_LEN+1];  /* Not very useful to app. */
  lbm_uint_t rcv_start_seq_num;  /* Receive-side sequence number of CRSP. */
  lbm_uint_t rcv_end_seq_num;  /* Receive-side sequence number or DRSP. */
} lbmct_peer_info_t;
```

This structure is supplied to the application by the connection create and
delete application callbacks.
It contains information about both the CT Source and CT Receiver.

Note that there are circumstances where not all of the information is
available when the structure is passed to the application.
For example, the connection create callback does not yet know what the
`rcv_end_seq_num` will be, since that information is determined when
the connection is closed.

The `flags` field is a bitmap indicating which of the subsequent fields are
valid.
The bits are:
```
#define LBMCT_PEER_INFO_FLAGS_SRC_METADATA 0x1
#define LBMCT_PEER_INFO_FLAGS_SRC_METADATA_LEN 0x2
#define LBMCT_PEER_INFO_FLAGS_RCV_METADATA 0x4
#define LBMCT_PEER_INFO_FLAGS_RCV_METADATA_LEN 0x8
#define LBMCT_PEER_INFO_FLAGS_RCV_SOURCE_NAME 0x10
#define LBMCT_PEER_INFO_FLAGS_RCV_START_SEQ_NUM 0x20
#define LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM 0x40
```

### lbmct_peer_info_t Fields

#### `lbmct_peer_info_t.status`
Indicates health of the connection at that point in time.
Possible values: LBMCT_CONN_STATUS_OK, LBMCT_CONN_STATUS_BAD_CLOSE.

#### `lbmct_peer_info_t.flags`
Bitmap indicating which subsequent fields are set.

#### `lbmct_peer_info_t.src_metadata`
Pointer to the CT Source's application metadata
supplied in the publisher's call to [lbmct_create()][#lbmct_create].
If the publisher did not supply metadata,
the LBMCT_PEER_INFO_FLAGS_SRC_METADATA bit will be zero in the `flags` field.

#### `lbmct_peer_info_t.src_metadata_len`
Number of bytes of metadata pointed to by
`rcv_metadata`.

#### `lbmct_peer_info_t.rcv_metadata`
Pointer to the CT Receiver's applicaiton metadata
supplied in the subscriber's call to [lbmct_create()][#lbmct_create].
If the subscriber did not supply metadata,
the LBMCT_PEER_INFO_FLAGS_SRC_METADATA bit will be zero in the `flags` field.

#### `lbmct_peer_info_t.src_metadata_len`
Number of bytes of metadata pointed to by
`src_metadata`.

#### `lbmct_peer_info_t.rcv_source_name`
Source "name" as seen by receiver.
This field is only filled in on the subscriber side.
Note that especially in a DRO environment, different subscribers can
see different source names for the same actual source.
The IP and Port will be that of the DRO's proxy source, not the
originating source's IP and Port.

#### `lbmct_peer_info_t.rcv_start_seq_num`
Topic-level sequence number of the CRSP handshake
received by the subscriber which established the connection.

#### `lbmct_peer_info_t.rcv_end_seq_num`
Topic-level sequence number of the DRSP handsnake
received by the subscriber which closed the connection.


## lbmct_config_t

```
typedef struct lbmct_config_t_stct {
  lbm_uint32_t flags;  /* LBMCT_CONFIG_FLAGS_... */
  lbm_uint32_t test_bits;  /* Set bits for internal testing. */
  int domain_id;   /* Domain ID for context passed into lbmct_create(). */
  int delay_creq;  /* Time (in ms) to delay sending initial CREQ handshake. */
  int retry_ivl;   /* Timeout to retry a handshake. */
  int max_tries;   /* Give up after this many handshake tries. */
  int pre_delivery; /* Enables delivery of received msgs before handshakes. */
} lbmct_config_t;
```

This structure is set by the application and passed in at CT object
creation time.
The application can selectively set as many or as few fields as desired.
For fields that are not set, the default values used by CT are as follows:

The default values for the fields are:
```
#define LBMCT_CT_CONFIG_DEFAULT_TEST_BITS  0x00000000
#define LBMCT_CT_CONFIG_DEFAULT_DOMAIN_ID  -1
#define LBMCT_CT_CONFIG_DEFAULT_DELAY_CREQ 10    /* 10 ms */
#define LBMCT_CT_CONFIG_DEFAULT_RETRY_IVL  1000  /* 1 sec */
#define LBMCT_CT_CONFIG_DEFAULT_MAX_TRIES  5
#define LBMCT_CT_CONFIG_DEFAULT_PRE_DELIVERY 0
```

The `flags` field is a bitmap indicating which of the subsequent fields are
valid.
The application must "OR" together the bits corresponding to the fields that
it has set.
The bits are:
```
#define LBMCT_CT_CONFIG_FLAGS_TEST_BITS  0x00000001
#define LBMCT_CT_CONFIG_FLAGS_DOMAIN_ID  0x00000002
#define LBMCT_CT_CONFIG_FLAGS_DELAY_CREQ 0x00000004
#define LBMCT_CT_CONFIG_FLAGS_RETRY_IVL  0x00000008
#define LBMCT_CT_CONFIG_FLAGS_MAX_TRIES  0x00000010
#define LBMCT_CT_CONFIG_FLAGS_PRE_DELIVERY 0x00000020
```

### lbmct_config_t Fields

#### `lbmct_config_t.flags`
Bitmap indicating which subsequent fields are set
by the application

#### `lbmct_config_t.test_bits`
Bitmap used to control internal behavior for unit testing.
Not for normal use.

#### `lbmct_config_t.domain_id`
For DRO environments,
the publisher should set this to the Domain ID for this application.
For non-DRO environments, this should be set to 0.
If using DRO, the value can be left at its default of -1, and the CT receiver
will use the "send to source" UIM addressing for all handshake messages.
See [Domain ID](Domain_ID.md).

#### `lbmct_config_t.delay_creq`
When a receiver discovers a source, it delays sending its
connect request message.
This is to avoid "head loss" on the source's transport session.

#### `lbmct_config_t.retry_ivl`
The CT layer uses handshake control messages to
manage connections.
There are situations where these messages can be lost,
so a retry mechanism is used to recover lost handshake control messages.
The `retry_ivl` field indicates the time interval used for retries.
NOTE: this retry mechanism is for the opening and closing of connections.
They are not a keepalive mechanism to detect failure of an active connection.

#### `lbmct_config_t.max_tries`
Maximum number of attempts to exchange a given
handshake control message before CT gives up and deletes the connection.

#### `lbmct_config_t.pre_delivery`
When set to 1, enables the delivery of received
messages from non-CT sources (i.e. before the connect handshake completes).
For those messages received outside of the normal connected state, the
[lbm_msg_t](https://ultramessaging.github.io/currdoc/doc/API/structlbm__msg__t__stct.html)
structure field `source_clientd` will be NULL.
