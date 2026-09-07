# hlolli_wg_double_bass

A sample-free, four-string double-bass plugin for Csound 7 and browser WASM,
derived from [hlolli_wg_violin](https://github.com/hlolli/hlolli_wg_violin).
One canonical C source supplies native and browser builds; no audio or model
files are loaded at runtime.

The plugin supports bowed and plucked articulations, harmonics, sympathetic
strings, and a shared body renderer. An optional infinite-bow Bach demo offers
continuous legato phrasing with mechanical noise bursts disabled.

**Status:** development release. Geometry, runtime safety, demo playback, and
native/browser agreement are tested. The fixed sound model remains labelled
`violin-derived`; it has not passed independent double-bass acoustic validation.

## Build and play the demos

Requires CMake 3.16+, a C11 compiler, Python 3.8+, and Csound 7 with its
development headers. For a Csound source build on Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DHLOLLI_CSOUND_SOURCE_DIR=/path/to/csound \
  -DHLOLLI_CSOUND_BUILD_DIR=/path/to/csound/build \
  -DCSOUND_EXECUTABLE=/path/to/csound/build/csound
cmake --build build
ctest --test-dir build --output-on-failure
/path/to/csound/build/csound --opcode-lib=./build/libhlolli_wg_double_bass.so \
  -odac examples/basic.csd
```

For installed headers, use `-DHLOLLI_CSOUND_INCLUDE_DIR=/path/to/include/csound`
instead of the source/build header paths. macOS builds use the module's
`.dylib` filename instead of `.so`. To render a WAVE file, replace `-odac`
with `-W -s -o /tmp/double-bass-basic.wav`.

| Score | Length | What to try |
| --- | ---: | --- |
| [basic.csd](examples/basic.csd) | 12.5 s | Four open strings, one handle, shared body, passive arco release |
| [solo_techniques.csd](examples/solo_techniques.csd) | 56.5 s | Slurs, double stops, all nine modes, harmonics, bow changes, sympathy and strange-control comparisons |
| [section.csd](examples/section.csd) | 10 s | Three independent players at A4 = 442 Hz, one body per handle |
| [bach_menuet_i.csd](examples/bach_menuet_i.csd) | 93.75 s | Bach BWV 1007 Menuet I, both repeats, G major; adapted from public-domain Mutopia edition 517 |
| [bach_menuet_i_legato.csd](examples/bach_menuet_i_legato.csd) | 93.75 s | Selected infinite-bow interpretation; use the legato module below |
| [wasm_smoke.csd](examples/wasm_smoke.csd) | 1 s | Browser smoke score; the browser target uses a no-device output path |

Use one voice per active physical string and one resonance renderer per
handle. Keep voices processing after a zero arco gate to hear their passive
tails; keep the body renderer running beyond the last voice. The section score
shows the numeric-A4 constructor and scales its sounding pitches by the same
`442/440` ratio. The basic and solo scores use the no-argument constructor.
There is no string/profile constructor.

The native demos are checked by CTest and all six scores are included
by `cmake --install build`. When the sibling browser workbench and Bun are
available at configure time, run the browser smoke, short PCM parity, and
original/midpoint three-bar phrases, and complete 93.75-second legato parity
target with `cmake --build build --target hlolli_wg_double_bass_check_wasm`.
The full-legato browser check uses the same optional module and installed-demo
source as the supported performance below.

## Infinite-bow Bach demo

The selected performance preserves the original body resonance. It joins
adjacent same-string notes within four-bar phrases, retains contact and direction
at joins, and applies a 30 ms force pulse (+0.135 above held force) at 88
new attacks, leaving 180 continuations unpulsed. Mechanical bursts are disabled; ongoing friction and finger
noise remain. This is an idealized musical interpretation, not literal bowing
or independent acoustic validation. The original Bach score remains available.

The normal build also produces `libhlolli_wg_double_bass_legato.so` (on macOS,
`.dylib`). This alternative is derived from the canonical C source with only
the mechanical injection multiplied by zero, preserving the selected PCM and
random-number sequence. It exposes the same opcodes and constructors. Load
**one module, not both**, in a Csound process:

```sh
/path/to/csound/build/csound \
  --opcode-lib=./build/libhlolli_wg_double_bass_legato.so \
  -W -f -o /tmp/bach-legato.wav examples/bach_menuet_i_legato.csd
```

Replace the file-output options with `-odac` for playback. The score deliberately
keeps a low output gain; adjust listening volume rather than
normalizing notes separately. Its pitch expression fixes A4 at 440 Hz.

Installation puts the alternative module in
`share/hlolli_wg_double_bass/legato/`, outside Csound's auto-loaded plugin folder.
When the standard plugin is installed in an auto-loaded location, use a separate
Csound plugin search directory containing neither variant (the `OPCODE7DIR64`
environment setting), then explicitly load the legato module as above.
The installed score is in `share/hlolli_wg_double_bass/examples/`.
The standard plugin, fixed model and other articulation defaults are unchanged.

## License

GPL-3.0-only. See [LICENSE](LICENSE). The Bach composition and source edition
are public domain; the score implementation uses the project license.
