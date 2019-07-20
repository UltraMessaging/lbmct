# lbmct v0.6 Connected Topics for Ultra Messaging

The "lbmct" package is an API wrapper around the Ultra Messaging library which
adds the concept of end-to-end connectedness to UM's normal pub/sub messaging
model.
These "Connected Topics" (CTs) help Publisher and Subscriber applications
keep track of each other.

See [lbmct documentation](https://ultramessaging.github.io/lbmct/) for overview
and user manual.
This API reference assumes familiarity with the information in that documentation.

The main classes of the public API are:
<ul>
<li>{@link com.latencybusters.lbmct.LbmCt} - an "instance" of the connected topics wrapper.  Can contain any number of CT sources and receivers.
</li>
<li>{@link com.latencybusters.lbmct.LbmCtSrc} - a connected receiver, associated with a UM topic.
</li>
<li>{@link com.latencybusters.lbmct.LbmCtRcv} - a connected source, associated with a UM topic.
</li>
</ul>

Supporting classes are:
<ul>
<li>{@link com.latencybusters.lbmct.LbmCtConfig} - configure the CT wrapper.
</li>
<li>{@link com.latencybusters.lbmct.LbmCtPeerInfo} - holds information about the source and receiver applications.
</li>
<li>{@link com.latencybusters.lbmct.LbmCtRcvConn} - minor class provided to give access to some additional information.
</li>
</ul>
