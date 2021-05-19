# TLSe

Single C file TLS 1.3, 1.2, 1.1 and 1.0(without the weak ciphers) implementation, using [libtomcrypt](https://github.com/libtom/libtomcrypt "libtomcrypt") as crypto library. It also supports DTLS 1.2 and 1.0. Before using tlse.c you may want to download and compile tomcrypt; alternatively you may use libtomcrypt.c (see Compiling). I'm working at an alternative efficient RSA signing, DH and Curve25519 implementation, to allow the compilation, alternatively, without tomcrypt, on devices where memory and code size is an issue.

**Note**: It does not implement 0-RTT. Client-side TLS 1.3 support is experimental.

Like this project ? You may donate Bitcoin for this project at 14LqvMzFfaJ82C7wY5iavvTf9HPELYWsax

![](https://raw.githubusercontent.com/eduardsui/edwork/master/bwallet.png)

Compiling
----------

Simple TLS client:
`$ gcc tlshello.c -o tlshello -ltomcrypt -ltommath -DLTM_DESC`  

For debuging tls connections, the DEBUG flag must be set (-DDEBUG).

Simple TLS server:
`$ gcc tlsserverhello.c -o tlsserverhello -ltomcrypt -ltommath -DLTM_DESC`  

The entire library is a single c file that you just include in your source.

The library may also use the libtomcrypt.c amalgamation. In this case, the client may be compiled:

`$ gcc tlshello.c -o tlshello -DTLS_AMALGAMATION`

and the server:

`$ gcc tlsserverhello.c -o tlsserverhello -DTLS_AMALGAMATION`

tlse.h is optional (is safe to just include tlse.c). Alternatively, you may include tlse.h and add tlse.c to your makefile (useful when linking against C++).

If thread-safety is needed, you need to call `tls_init()` before letting any other threads in, and not use the same object from multiple threads without a mutex. Other than that, TLSe and libtomcrypt are thread-safe. Also, you may want to define LTC_PTHREAD if you're using libtomcrypt.

TLSe supports KTLS on linux kernel 4.13 or higher. KTLS is a TLS implementation in the linux kernel. If TLS_RX is not defined, KTLS is send-only (you may use send/sendfile to send data, but you may not use recv). Also, the negotiation must be handled by TLSe. If KTLS support is needed, define WITH_KTLS (compile with -DWITH_KTLS). Note that is not clear which header should be included for linux structure, you may need to check these structures and constants: https://github.com/torvalds/linux/blob/master/Documentation/networking/tls.txt.

Usage
----------

You just 
`#include "tlse.c"`
in your code. Everything is a single file.

Features
----------

The main feature of this implementation is the ability to serialize TLS context, via tls_export_context and re-import it, via tls_import_context in another pre-forked worker process (socket descriptor may be sent via sendmsg).

For now it supports TLS 1.2, TLS 1.1 + 1.0 (when TLS_LEGACY_SUPPORT is defined / default is on), RSA, ECDSA, DHE, ECDHE  ciphers:
``TLS_DHE_RSA_WITH_AES_128_CBC_SHA, TLS_DHE_RSA_WITH_AES_256_CBC_SHA, TLS_DHE_RSA_WITH_AES_128_CBC_SHA256, TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, TLS_DHE_RSA_WITH_AES_256_GCM_SHA384, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256` and `TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384``.

The following ciphers are supported but disabled by default:
``TLS_RSA_WITH_AES_128_CBC_SHA, TLS_RSA_WITH_AES_256_CBC_SHA, TLS_RSA_WITH_AES_128_CBC_SHA256, TLS_RSA_WITH_AES_256_CBC_SHA256, TLS_RSA_WITH_AES_128_GCM_SHA256, TLS_RSA_WITH_AES_256_GCM_SHA384``. To enable these ciphers, TLSe must be compiled with ``-DNO_TLS_ROBOT_MITIGATION``. ROBOT attack is mitigated by default, but it is recommended to disable RSA encryption to avoid future vulnerabilities.

TLSe now supports ChaCha20/Poly1305 ciphers: `TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256`,  `TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256` and `TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256`. These ciphers are enabled by default.

It has a low level interface, efficient for non-blocking, asynchronous sockets, and a blocking, libssl-style interface.

It implements all that is needed for the TLS protocol version 1.2 and a pem/der parser. From tomcrypt it uses RSA, ECDSA and AES(GCM and CBC) encryption/decryption, SHA1, SHA256, SHA384, SHA512 and HMAC functions.

Now it supports client certificate. To request a client certificate, call ``tls_request_client_certificate(TLSContext *)`` following ``tls_accept(TLSContext *)``.

It implements SNI extension (Server Name Indication). To get the SNI string call ``tls_sni(TLSContext *)``.
It also implements SCSV and ALPN (see ``tls_add_alpn(struct TLSContext *, const char *)`` and ``const char *tls_alpn(struct TLSContext *)``.

The library supports certificate validation by using ``tls_certificate_chain_is_valid``, ``tls_certificate_chain_is_valid_root``, ``tls_certificate_valid_subject`` and ``tls_certificate_is_valid``(checks not before/not after). Note that certificates fed to ``tls_certificate_chain_is_valid`` must be in correct order (certificate 2 signs certificate 1, certificate 3 signs certificate 2 and so on; also certificate 1 (first) is the certificate to be used in key exchange).

This library was written to be used by my other projects [Concept Applications Server](https://github.com/Devronium/ConceptApplicationServer "Concept Application Server") and [Concept Native Client](https://github.com/Devronium/ConceptClientQT "Concept Client QT")

Examples
----------
1. [examples/tlsclienthello.c](https://github.com/eduardsui/tlslayer/blob/master/examples/tlsclienthello.c) simple client example
2. [examples/tlshelloworld.c](https://github.com/eduardsui/tlslayer/blob/master/examples/tlshelloworld.c) simple server example
3. [examples/tlssimple.c](https://github.com/eduardsui/tlslayer/blob/master/examples/tlssimple.c) simple blocking client using libssl-ish API
4. [examples/tlssimpleserver.c](https://github.com/eduardsui/tlslayer/blob/master/examples/tlssimpleserver.c) simple blocking server using libssl-ish API

After compiling the examples, in the working directory, you should put fullchain.pem and privkey.pem in a directory called testcert for running the server examples. I've used [letsencrypt](https://github.com/letsencrypt/letsencrypt) for certificate generation (is free!).

Important security note
----------

Note that for DTLS, it doesn't implement a state machine, so using this DTLS implementation with UDP (server) may expose your server to DoS attack.

License
----------
Public domain, BSD, MIT. Choose one.
