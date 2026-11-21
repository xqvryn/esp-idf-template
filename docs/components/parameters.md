# Parameters

`components/parameters` is a typed table: bool, int32, and float. Each value has the range declared when it was defined. `set_*` and `write_text` reject a value outside that range and leave the previous value in place.

`write_text` is the assignment a command handler can call. It accepts `true`/`false`/`1`/`0` for booleans. Integers and floats reject trailing characters. It does not decode a command envelope. The sensor-node envelope stays `set_output` and `stop`.

Persistence is a `BlobStore`. `MemoryParameterStore` is the scripted store used by host tests. `NvsParameterStore` writes namespace `espkit_param`, key `blob`, and does not read or write the network settings in namespace `espkit`. The blob is schema 1, little-endian, and at most 256 bytes. `decode` applies every record or none. An unknown name or an out-of-range value rejects the blob.

An empty store leaves the defaults from `define_*`. Sixteen parameters is the maximum. Names are at most 15 characters.
