# SASL authentication in IRC

© 2014 Mantas Mikulėnas &lt;<grawity@gmail.com>&gt;

This documentation is released under Creative Commons 3.0 Attribution license.

---

This is a description of server-server protocol, intended for ircd and services developers. For the client-server protocol descriptions, intended for client & bot developers, see the IRCv3 [sasl-3.1][] and [sasl-3.2][] specifications.

---

Despite at least four ircds having added SASL support by now, there has been no documentation on how this should be done from the server side.

While SASL is a generic authentication layer, used by over a dozen Internet protocols, its implementation in IRC is somewhat different than anywhere else, since the authentication is almost never performed by the IRC server (ircd) itself, but rather forwarded to a services pseudoserver. This means that adding SASL support isn't a matter of linking to the right library like libgsasl, but rather a matter of exchanging data with another server on the network.

SASL is documented in [RFC 4422][sasl]; in short, it is based around authentication "mechanisms", each of which defines a series of messages that the client and server should exchange, the most common mechanism being PLAIN ([RFC 4616][sasl-plain]) where the only exchange consists of the username and password being sent directly.

For an IRC server that relies on separate services (which this document is aimed at), the exact SASL mechanisms being used are not important, as it only has to relay the mechanism name and data between client and services.

(An ircd _could_ act as the SASL server by itself, either by using libgsasl, Dovecot, Cyrus libsasl, or by implementing some mechanisms internally – to the client it would not matter. So far, however, this has never been done (aside from Microsoft's IRCX servers), mostly because the existing services packages provide sufficient features, and they already handle old-style "/msg"-based authentication anyway, so this would only make sense when writing a new ircd that has all services features built in.)

## Requirements for ircds

To add SASL support to an ircd, it needs to have three features:

  * _unique IDs_ to distinguish between connections,
  * a link _protocol command_ to carry the SASL exchange between you and the services pseudoserver,
  * and ability to store the _account name_ that each user has logged in to.

### Link protocol updates

Almost all existing implementations rely on a services pseudoserver (such as Atheme, X3, or Anope) to actually process the SASL exchange and authenticate the user, in a similar way to how services handle old-style authentication "/msg NickServ …" and so on. Therefore they add a server-to-server command encapsulating the SASL exchange between ircd and services.

The `SASL` command varies slightly between protocols, but the basic parameters are the same: target server for routing the message; client UID; SASL message type; and 1-2 data parameters. (In protocols that support it ENCAP or a similar feature should be used, to let the messages safely pass through out-of-date hubs which otherwise drop unknown commands.)

### Unique connection IDs

On IRC, the SASL authentication happens during user "registration", that is, _before_ annoucing the user to the rest of the network. This allows removing such race conditions as the user being visible before their vhost/cloak is applied, or trying to join "registered-only" channels before their account name gets set. (However, SASL by itself is not enough for this; the server-to-server protocol also needs to consider these race conditions, for example, the EUID extension to TS6. This is problematic with protocols that first introduce the user and only then use ENCAP or METADATA to broadcast their account name…)

The unique ID requirement exists because, as mentioned above, at the time of SASL authentication the user hasn't been announced to the network yet and quite possibly might not even have chosen a nickname at all. (It is **not** required that NICK/USER commands be sent before trying SASL authentication – they could arrive during or after it.) Besides, clients can change the nickname at any time. This means that services need a way to track authentication state per connection.

For TS6 and IRCnet protocols, which already use UIDs everywhere, this is not a problem; in other protocols (like TS5, P10, RFC2813) the ircd will need to generate a temporary "pseudo-UID" (aka PUID) for each connection. It is recommended to do this by concatenating the server name, a separator like "!", and a random cookie – this also makes it easier for services to send the reply back to the correct server. (Of course, it would be even better to upgrade your protocol to TS6, but this takes more time…)

### Account names

While not specific to SASL (e.g. also seen in QuakeNet), all ircds that implement it are capable of storing the user's account/login name (for account-based services packages like Atheme, X3, or even modern Anope versions).

During SASL authentication, the services will send another command to set the account name. Some ircds (e.g. Nefarious) use `SASL` message type "L", while others add a dedicated command like `SVSLOGIN` or `ENCAP * SVSLOGIN`; the exact choice is not very important.

Yet others use generic commands like `PROP` or `METADATA`. This mostly works, but can be problematic – it must work with not-yet-announced users, so if the protocol has `METADATA` but lacks UIDs, then the command will need to be updated to accept pseudo-UIDs in addition to nicknames. For another, services may want to know the difference between an account name that was received during a netburst (e.g. if a distant server splits and rejoins) and should be accepted, versus one that a server attempted to set for an existing user (bypassing services) which services may want to reject.

## Server-to-server SASL protocol

The client's server and the services pseudoserver exchange SASL messages containing the client's unique ID, message type, and attached data (usually one or two parameters). These messages are encapsulated in protocol-specific commands (e.g. "SASL" or "ENCAP SASL").

### Start of authentication

When the client initiates authentication, the server sends a "S" message on behalf of the client, with the chosen SASL mechanism name as data.

If the server has any external authentication data (e.g. TLS client certificate fingerprint, Unix user ID) it is sent as an additional parameter to the "S" message. The external data MUST be sent when the chosen mechanism is "EXTERNAL", and SHOULD be sent in all other cases as well (so that it could be used as an additional authentication factor). The data format is unspecified, though it is recommend to follow existing implementations.

The ircd SHOULD also send a "H" message containing the client's host address; such a "H" message MUST precede any "S" messages.

### Bad mechanism indication

If the client chooses an unknown mechanism, services first send a "M" message containing a comma-separated mechanism list (corresponding to a 908 numeric), followed by a "D" message containing "F" to signal failure (corresponding to a 904 numeric).

### Back-and-forth

If a _known_ mechanism is chosen, the server (on behalf of the client) and services exchange "C" commands containing Base64-encoded mechanism data, or `+` if the mechanism data is empty.

As soon as the mechanism is chosen, services MUST send the initial reply. If there is no data to send (e.g. if a "client-first" mechanism was chosen), services MUST still send an empty reply \[as above\].

(However, for certain compatibility reasons, it is advised that services treat incoming "S" and "C" messages identically – that is, after the mechanism was chosen, accept authentication data in either "S" or "C" messages.)

If the initial "S" message was broadcast (e.g. "ENCAP \* …") and no reply from services was received, the server SHOULD abort authentication after a timeout, by sending a "D" message containing "A" to services (corresponding to numeric 906 to the client).

### Success indication

After successful authentication, services first send a "L" message (or protocol-specific equivalent) containing the user's account name \[i.e. the authzid\], followed by a "D" message containing "S" to signal success.

It is up to the server protocol implementors whether a SASL "L" message or a protocol-specific command is used. (For example, TS6 uses the "SVSLOGIN" command, while m\_spanningtree uses "METADATA".)

The ircd MUST store the account name for the connection.

### Failed authentication indication

If the client chooses a _known_ mechanism but sends bad authentication data, services send a "D" message containing "F" to signal failure (corresponding to a 904 numeric).

### Client abort (implicit)

If the client either disconnects or completes registration (by sending "NICK" + "USER" + CAP END") in the middle of SASL authentication exchange, the server must abort the authentication by sending a "D" message containing "A" to services, and a 906 numeric to the client (if still connected).

## Interaction between server-server and RFC 1459 client-server protocols

The server MUST support the IRCv3 "capability-negotiation" and "sasl" extensions. All examples in this document use the `CAP` and `AUTHENTICATE` commands and numeric replies defined by IRCv3.

The server MAY, upon receiving the `PASS` command, use the password to automatically perform SASL PLAIN authentication with services on behalf of the client.

The server SHOULD NOT support the IRCX `AUTH` command.

### From IRCv3 SASL protocol to services

On receiving `AUTHENTICATE <data>` from the client:

  - If this is a new SASL session, the server sends "H" and "S" messages to services (as documented in previous section). This first message has the SASL mechanism name as data.

  - Otherwise, the server sends a "C" message to services, containing the same data.

The "AUTHENTICATE" parameter MUST be at least 1 byte and at most 400 bytes long. The ircd MUST reject longer parameters with ERR\_SASLTOOLONG. If the client needs to send a larger reply, it must fragment it as specified in the IRCv3 "sasl" extension.

If the client disconnects during an active SASL session, the server must abort the session by sending a "D" message containing "A" (abort) to services.

If the client completes registration (sends `NICK`, `USER`, `CAP END`), the server must abort the session, send a "D" message containing "A" to services, and send numeric 906 to the client.

### From services to IRCv3 SASL protocol

On receiving a "C" (client) message from services, the server sends an `AUTHENTICATE <data>` reply to the client.

On receiving a "D" (done) message from services, the server sends an apropriate numeric and forgets the SASL session data.

  - If the "D" message data equals "S", the client is successfully authenticated, and numeric 903 is sent. (If the server doesn't know the account name yet, it's dealing with buggy services and should squit/jupe/napalm them as needed.)

  - If the "D" message data equals "F", the authentication failed and numeric 904 is sent. The server MAY disconnect the client for exceeding a failure limit; otherwise, the client MAY immediately start a new session by sending `AUTHENTICATE` again.

On receiving a "L" (login) message from services, the server stores the first parameter as the user's account name and sends numeric 900 to the client.

On receiving a "M" (mechlist) message from services, the server sends numeric 908 to the client, containing the received data as `<list>`. The server must not assume failed authentication from a "M" message alone.

Services never send "H" or "S" messages.

### Summary of "SASL" message types

  - type "C" – client data
      - sent in both directions
      - data is the SASL buffer (directly converted to/from "AUTHENTICATE …data…")
  - type "D" – authentication done
      - sent in both directions
      - data "A" (from ircd) – auth aborted by client
      - data "S" (from services) – auth successful
      - data "F" (from services) – auth failed
  - type "H" – client host address
      - sent by ircd
      - data[0] is the client's visible hostname
      - data[1] is the IP address (may be "`0`" if server refuses to reveal it)
      - data[2] (optional) is "`P`" to indicate plain-text (non-TLS) connection, any other non-empty string to indicate TLS
  - type "L" – user login
      - sent by services
      - data is the account name
  - type "M" – supported mechanisms
      - sent by services
      - data is a comma-separated mechanism list, re-sent to user as RPL\_SASLMECHS
  - type "S" – start authentication
      - sent by ircd
      - data is the mechanism given by user
      - extdata is the SASL EXTERNAL authentication data supplied by ircd

### Summary of numeric formats

    RPL_LOGGEDIN      "900 <nick> %s!%s@%s %s :You are now logged in as %s."
    RPL_LOGGEDOUT     "901 <nick> %s!%s@%s :You are now logged out."
    ERR_NICKLOCKED    "902 <nick> :You must use a nick assigned to you." // What's it for?
    RPL_SASLSUCCESS   "903 <nick> :SASL authentication successful"
    ERR_SASLFAILED    "904 <nick> :SASL authentication failed"
    ERR_SASLTOOLONG   "905 <nick> :SASL message too long"
    ERR_SASLABORTED   "906 <nick> :SASL authentication aborted"
    ERR_SASLALREADY   "907 <nick> :You have already completed SASL authentication"
    RPL_SASLMECHS     "908 <nick> <list> :are available SASL mechanisms"

## Existing S2S protocol extensions

### TS6 protocol

Refer to [ts6-protocol.txt][charybdis-ts6] in Charybdis source tree for the authoritative version of this documentation.

Command name: `SASL` (always ENCAP)

Command parameters:

  - source UID (either client or services)
  - target UID (either services or client) or `*`
  - message type ("S", "D", etc.)
  - data (may be more than one parameter)

The `SASL` command requires ENCAP. The first message may be broadcast or sent to a configured services server name. All later messages are sent directly to the apropriate server.

The first message (sent on behalf of the client) has `*` as the target UID, as the SASL agent is not known yet. All later messages are exchanged between the client's and the agent's (service's) UIDs. Note that some services use their SID, instead of an UID, as the SASL agent.

The account name is set using a separate `SVSLOGIN` command, which is documented in [ts6-protocol.txt][charybdis-ts6].

### InspIRCd (m\_spanningtree) version of TS6

The `SASL` command is the same as in TS6 (except InspIRCd prefers SIDs in ENCAP instead of server names, but that is out of scope for this file).

The account name is set using the [`METADATA` command][inspircd-metadata], in the "accountname" field.

### UnrealIRCd 3.2 protocol

Command name: `SASL` (token `SY`)

Command parameters:

  - target server name
  - client PUID
  - message type ("S", "D", etc.)
  - data (may be more than one parameter)

Since the protocol in version 3.2 does not have unique user IDs, the ircd generates a pseudo-UID consisting of the server name, a "!" character,  and a random cookie. (This is similar to UIDs, which also start with the server's ID.)

The account name is set using a `SVSLOGIN` command (token `SZ`):

Command parameters:

  - target server name
  - client PUID
  - account name

Note that the target server parameter is always specified by its full name, never by "@numeric".

### Nefarious2 version of P10 protocol

Command token: `SASL`

Command parameters:

  - target server numeric or "\*"
  - client PUID
  - message type ("S", "D", etc.)
  - data (may be more than one parameter)

Since the ircd assigns UIDs very late (immediately before introducing the user), during SASL authentication it generates a pseudo-UID consisting of the server ID, a "!" character, and a random cookie. (This is similar to UIDs, which also start with the server's ID.)

The accountname is set using the "L" SASL message.

## Examples

TS6 example of successful EXTERNAL authentication:

    --> :0HA ENCAP *            SASL     0HAAAAF37 * H poseidon.int 2001:db8::1a36
    --> :0HA ENCAP *            SASL     0HAAAAF37 * S EXTERNAL 57366a8747e...
    <-- :5RV ENCAP hades.arpa   SASL     5RVAAAAA0 0HAAAAF37 C +
    --> :0HA ENCAP services.int SASL     0HAAAAF37 5RVAAAAA0 C Z3Jhd2l0eQ==
    <-- :5RV ENCAP hades.arpa   SVSLOGIN 0HAAAAF37 * * * grawity
    <-- :5RV ENCAP hades.arpa   SASL     5RVAAAAA0 0HAAAAF37 D S

TS6 example of a bad mechanism error:

    --> :0HA ENCAP *          SASL 0HAAAAF37 * H poseidon.int 192.0.42.7
    --> :0HA ENCAP *          SASL 0HAAAAF37 * S DIGEST-MD5
    <-- :5RV ENCAP hades.arpa SASL 5RVAAAAA0 0HAAAAF37 M PLAIN,EXTERNAL,GSSAPI
    <-- :5RV ENCAP hades.arpa SASL 5RVAAAAA0 0HAAAAF37 D F

InspIRCd 2.x m\_spanningtree example of successful SCRAM-SHA-1 authentication:

    --> :1SP ENCAP    *   SASL 1SP000251 * S SCRAM-SHA-1
    <-- :00A ENCAP    1SP SASL 00AAAAAAG 1SP000251 C +
    --> :1SP ENCAP    *   SASL 1SP000251 00AAAAAAG C biwsbj1ncmF3aXR5LHI9L1NRQz...
    <-- :00A ENCAP    1SP SASL 00AAAAAAG 1SP000251 C cj0vU1FDN3RqM0xMWOEhUZk...
    --> :1SP ENCAP    *   SASL 1SP000251 00AAAAAAG C Yz1iaXdzLHI9L1NRQzd0ajNMTF...
    <-- :00A ENCAP    1SP SASL 00AAAAAAG 1SP000251 C dj03bGdLeElRRHBRT3Ss1Zn...
    --> :1SP ENCAP    *   SASL 1SP000251 00AAAAAAG C +
    <-- :00A ENCAP    1SP SASL 00AAAAAAG 1SP000251 C +
    <-- :00A METADATA 1SP000251 accountname :grawity
    <-- :00A ENCAP    1SP SASL 00AAAAAAG 1SP000251 D S

UnrealIRCd 3.2 example of successful PLAIN authentication (without tokens or server-numerics):

    --> :hades.arpa SASL     services.int hades.arpa!23.760 S PLAIN
    <-- :SaslServ   SASL     hades.arpa   hades.arpa!23.760 C +
    --> :hades.arpa SASL     services.int hades.arpa!23.760 C Z3Jhd2l0eQB...
    <-- :SaslServ   SVSLOGIN hades.arpa   hades.arpa!23.760 grawity
    <-- :SaslServ   SASL     hades.arpa   hades.arpa!23.760 D S

UnrealIRCd 3.2 example of the client aborting authentication (note that D A is sent by the ircd, and services do not reply to it):

    --> :hades.arpa SASL services.int hades.arpa!24.5541 S PLAIN
    <-- :SaslServ   SASL hades.arpa   hades.arpa!24.5541 C +
    --> :hades.arpa SASL services.int hades.arpa!24.5541 D A

Nefarious2 P10 example of succesful PLAIN authentication (ircd has numeric `Aq`, services `AB`):

    --> Aq SASL *  Aq!17.1504627973 S :PLAIN
    <-- AB SASL Aq Aq!17.1504627973 C +
    --> Aq SASL AB Aq!17.1504627973 C :Z3Jhd2l0eQB...
    <-- AB SASL Aq Aq!17.1504627973 L grawity
    <-- AB SASL Aq Aq!17.1504627973 D S

## Client examples

The client-to-server dialogue is already documented by the IRCv3 capability specification and by the IRCv3 "sasl" extension, but here's an example for comparison with the above server-to-server exchanges.

    --> AUTHENTICATE DIGEST-MD5
    <-- :hades.arpa 908 grw PLAIN,EXTERNAL :are the available SASL mechanisms
    <-- :hades.arpa 904 grw :SASL authentication failed

    --> AUTHENTICATE PLAIN
    <-- AUTHENTICATE +
    --> AUTHENTICATE Z3Jhd2l0eQB...
    <-- :hades.arpa 900 grw grw!~root@rain.local grawity :You are now logged in as grawity.
    <-- :hades.arpa 903 grw :SASL authentication successful

("CAP REQ" is required but it is not technically part of the SASL exchange; it does not cause any message to services, therefore it was left out of this example. Likewise, PASS, NICK, and USER were omitted because they are not relevant to the SASL exchange and can be sent at any time.)

[charybdis-ts6]: https://github.com/atheme/charybdis/blob/master/doc/technical/ts6-protocol.txt
[inspircd-metadata]: https://github.com/attilamolnar/wiki/blob/gh-pages/Modules/spanningtree/commands/METADATA.md
[sasl]: https://tools.ietf.org/html/rfc4422
[sasl-plain]: https://tools.ietf.org/search/rfc4616
[sasl-3.1]: http://ircv3.net/specs/extensions/sasl-3.1.html
[sasl-3.2]: http://ircv3.net/specs/extensions/sasl-3.2.html

<!--
vim: ts=4:sw=4:et
-->