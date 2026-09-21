# eon_dsp (vendored)

Header-only DSP core used by the SEOUL DSP sound-quality upgrade (Plan B).
Vendored rather than fetched so the DSP evidence in this repository stays bound
to an exact, inspectable revision.

| Field | Value |
| --- | --- |
| Source checkout | `/Users/sungha/Desktop/EON LLM wiki/EON Audio Plugin/eon_dsp` |
| Source remote | none configured (local-only repository) |
| Revision | `c8e71f3c826e43f574804ecf63edaea1f34120ad` |
| Revision subject | `test: harden eon dsp core contracts` |
| Revision date | 2026-09-20 03:29:27 +0900 |
| Copied | 2026-09-22 |
| Copied scope | `Dsp/` only (11 headers), plus this file and `REVISION` |
| Copy verification | `diff -rq` against the source `Dsp/` reports byte-identical |

## What was deliberately not copied

- `eon_dsp/Tests/` - the upstream suite (ADAA/oversampling, stages/triode,
  solvers/WDF/ZDF) stays upstream. This repository has its own vendored-copy
  guard instead: `Tests/EonDspContractTests.cpp` (`ctest -R EonDspContract`).
- `eon_dsp/CMakeLists.txt`, `NOTES.md` - upstream build/test scaffolding.
- `eon_dsp/third_party/` (simde, sst-basic-blocks, sst-cpputils git
  submodules) - **not referenced by any `Dsp/*.h` header**, so copying them
  would only add unused vendored trees. Verified with:

      grep -rn 'sst::\|simde' third_party/eon_dsp/Dsp   # no matches
      grep -n '#include' third_party/eon_dsp/Dsp/*.h    # std + intra-eon only

## External dependency audit

Every `Dsp/*.h` includes only the C++ standard library and other `Dsp/*.h`
headers. The single platform-specific include is `<immintrin.h>` in
`RtGuard.h`, guarded by `#if defined (__SSE__)`; the arm64 path uses inline
`fpcr` flush-to-zero access instead. This is why these headers build and unit
test without JUCE.

## License status

The source repository contains **no LICENSE file**. The commits are authored by
the same account that owns this product (`Dolceandgabanna03aw`), so this is
treated as first-party code. This is recorded, not resolved: an explicit
first-party license must be added to the source repository before any
distribution that includes these headers.

## Vendored header hashes (SHA-256, as copied)

    2aac9bc95f613e72f65e43e7dcb47363cd210c39cef245fc8df605950e1f0f44  Dsp/Adaa.h
    d7e6e507e89a423e62f89872bb9a287e323a9fe8fb13b331ea846bd946c545b4  Dsp/Measure.h
    6c314580c2fc10beb84b01bf8d751093b050b66d3097aa9fd413add8ce8fcd74  Dsp/Oversampling.h
    33feb482fc6ba5a85a10b88328441b1eefcd24a61f739c13943c1b5b671c26b1  Dsp/Rng.h
    71447285e085d983bf01a683dc88e93787034c9517cec3217087bebd554b96b8  Dsp/RtGuard.h
    d37a1341d79566155e68bced2158d8f41316328a4a794c51f66a592ec98cddc4  Dsp/Solvers.h
    b7f38e092173584fcf926a2b79a2db2efe8b87d414d1a6040f644051e9eb49bd  Dsp/Stages.h
    bc8612dbf386d104cb41339aa83200b31fd42c8f4d5c3a5649e04f9cb7f05d7f  Dsp/Transformer.h
    2fc0f9518ebebd9e2c7b538899a52a085d689ad9cd6b476386bb690e006a8b7a  Dsp/Triode.h
    122e32eb4630165b13571f553af11e3036bb3b406d9404bc380f2cb3b5735ebb  Dsp/Wdf.h
    fd92e4814432828622a8d21077495150cde2a15cf509621d9093704146aa3c78  Dsp/Zdf.h

## How to refresh this copy

    shasum -a 256 third_party/eon_dsp/Dsp/*.h     # record the new hashes
    diff -rq <source>/Dsp third_party/eon_dsp/Dsp # confirm what changed
    printf '%s\n' <new-commit-sha> > third_party/eon_dsp/REVISION
    ctest --test-dir Build-Release -R EonDspContract --output-on-failure

Update this file in the same commit: revision, date, hash list, and any new
external dependency. A refresh that changes behaviour without a contract-test
update is a review failure, not a routine bump.
