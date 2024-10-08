# SASL
This implementation is based on the documentation from Mantas Mikulėnas. Read *sasl_original.md* first.

The following things are different:
* logout is not supported
* abortion of authentication is not supported, the user has to disable SASL explicitly at his IRC client if he wants to connect without authentication
* the L-message contains the cloak (`L <loginName> <cloak>`) and the ircd sets it after successful authentication
* the H-message contains a hostmask (`H <nick>[[ident]@ip]`)
* additional N-message to allow the SASL service to send a NOTICE to a user (`N :<notice>`)
* services as described in *SERVICE.txt* will be used

You need to set up at least one service with the flags SERVICE_WANT_SASL and SERVICE_WANT_ENCAP.

    S%::1%password%SASLService%0xc00000%1

## Example of a successful authentication


    [User -> Server]    CAP LS
    [User -> Server]    NICK patrick
    [User -> Server]    USER patrick patrick localhost :patrick
    // The server sends a list of his capabilities
    [Server -> User]    :irc1.localhost CAP * LS :sasl
    // The client requests the SASL capability
    [User -> Server]    CAP REQ :sasl
    [Server -> User]    :irc1.localhost CAP * ACK :sasl
    // The client says that he is going to use "PLAIN" mechanism
    [User -> Server]    AUTHENTICATE PLAIN
    // The server finds the closest SASL service and assigns him to the user. This and all following messages will be forwarded to this service.
    [Server -> Service] :000B SASL 000BAAAAD SASLService@irc1.localhost H patrick[patrick@127.0.0.1]
    [Server -> Service] :000B SASL 000BAAAAD SASLService@irc1.localhost S PLAIN
    // The SASL service says that it supports the "PLAIN" mechanism
    [Service -> Server] ENCAP 000B SASL 000BAAAAD * C +
    [Server -> User]    AUTHENTICATE +
    // The user sends the Base64 encoded credentials to the server
    [User -> Server]    AUTHENTICATE dXNlcjEAdXNlcjEAdXNlcjEtcGFzc3dvcmQxMjM=
    // The server forwards the message to the SASL service
    [Server -> Service] :000B SASL 000BAAAAD SASLService@irc1.localhost C dXNlcjEAdXNlcjEAdXNlcjEtcGFzc3dvcmQxMjM=
    // The service validates the credentials and sends success messages including the cloaked hostname
    [Service -> Server] ENCAP 000B SASL 000BAAAAD * L user1 spoof1.ircnet.com
    [Service -> Server] ENCAP 000B SASL 000BAAAAD * D S
    // The server sets the cloaked hostname and sends a success message to the client
    [Server -> User]    :irc1.localhost 900 patrick :You are now logged in as user1.
    [Server -> User]    :irc1.localhost 903 patrick :SASL authentication successful
    // The client completes the registration
    [User -> Server]    CAP END
    [Server -> User]    :irc1.localhost 001 patrick :Welcome to the Internet Relay Network patrick!patrick@spoof1.ircnet.com
