# Fuzz seed corpora

Every seed uses the reviewable transport form `hex:` followed by an even number
of hexadecimal digits. The harness first exercises the file's literal bytes and
then decodes the transport form, so mutations remain arbitrary while committed
seeds remain readable.

`binary_file/` covers empty complete/incomplete input, one and multiple frames,
truncated prefixes and payloads, and trailing data after a zero marker.
`decode_one/` covers all three decoder error classes. `differential_decode/`
starts from the same reviewed payloads and compares generated decode results
and values with the independent oracle. `replay/` adds unknown and invalid-size
framed messages; it also receives all BinaryFILE error seeds.

At configure time, `fuzz/generate_corpus.cmake` creates isolated corpora below
the build directory. It deterministically adds one payload seed and one complete
BinaryFILE seed for each of the 23 reviewed fixtures in
`tests/fixtures/itch50/`, plus an aggregate complete replay. The fixture
`raw_hex` and `raw_size` fields are validated and remain the source of truth;
the script never rewrites reviewed fixture content. `MANIFEST.txt` records the
fixture-to-seed mapping without host-dependent paths.

The fuzz preset therefore produces:

- `build/fuzz/fuzz/corpus/binary_file`;
- `build/fuzz/fuzz/corpus/decode_one`;
- `build/fuzz/fuzz/corpus/differential_decode`; and
- `build/fuzz/fuzz/corpus/replay`.
