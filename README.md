----------------------------------------------------------------

**The GoAT code is an academic research prototype, and meant to elucidate protocol details and for proofs-of-concept, and benchmarking. It has not been developed in a production environment and is not meant for deployment.**

----------------------------------------------------------------

# GoAT

GoAT is a file geolocation protocol (formally a Proof of Geo-Retrievability scheme). Using GoAT, you can prove that a particular piece of data is stored within a geographic region. GoAT targets somewhat coarse geolocation radii, useful to prove file replication. For example, GoAT can used to prove that three replicas of a file *F* are stored in USA, Asia and Europe respectively. Unlike existing geolocation systems that rely on trusted-verifiers, GoAT uses *existing internet servers* as geolocation anchors, thereby decentralizing trust over a large fraction of the internet.

- For more information, please have a look at our paper.

- Two PoRets have been implemented: `Shacham-Waters` and `Merkle-Tree`. Please look at the corresponding folders for how to install and run.

- Experiment results can be found in the `experiments` folder.

## Anchors

Anchors are internet servers located at a publicly known location that serve authenticated time.

[Roughtime](https://datatracker.ietf.org/doc/html/draft-roughtime-aanchal-04): 64 byte nonce, 64-byte signature

[TLS RFC](https://tools.ietf.org/html/rfc5246#section-7.4.3): 32 byte client random (we use it as nonce), 256-byte signature (Assuming RSA). 

Roughtime installation script is `scripts/install/install-roughenough.sh`.

<!-- Example command: `target/release/roughenough-client roughtime.int08h.com 2002 -t out -c cdfd795bf0d31eb763620a2c184cc1a49f9060c925c75fc157a7d00276d82c97a660d890a5d2f6b7f60295068042d66efd684f52a79aaf93f463b0d08d34a643 -p 016e6e0284d24c37c6e4d7d8d5b4e1d3c1949ceaa545bf875616c9dce0c9bec1` -->
