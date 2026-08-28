# pd-convolution

A partitioned convolution external for [Pure Data](https://puredata.info),
built on [FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver).

| Object | Kind | Does |
| --- | --- | --- |
| `[convolution~]` | signal | convolves its input with an impulse response held in an array |

## Why

Convolving a signal with a recorded space is how a convolution reverb works,
and nothing in vanilla Pd does it: `[rfft~]` and friends can be arranged into a
partitioned convolution, but the object count grows with the length of the
response and the arithmetic runs as a separate pass over memory per partition.
Measured against this external, three different vanilla arrangements of the
same algorithm all came out around forty times more expensive.

The response is convolved in two stages: a head block the size of Pd's own
block, and a larger tail block. The head gives the latency and the tail gives
the efficiency, so a six second impulse response costs little more than a
one second one.

## Using it

```pd
[declare -lib convolution]
```

or start Pd with `-lib convolution`.

```pd
[convolution~ <array> <tail-block>]
```

The array holds the impulse response. `tail-block` is a power of two, 4096 by
default; it trades memory and the work done in a single call against the cost
of a long response.

One signal inlet, one signal outlet. There is no latency: a sample reaches the
outlet in the block that took it in.

| Message in | Argument | Meaning |
| --- | --- | --- |
| `set <array>` | symbol | read the impulse response again, from that array or from the one already named |
| `clear` | | drop the tail still ringing and start the response again |

Rules:

- An array is not watched, so send `set` after writing one. `[soundfiler]`
  reads a file into an array.
- An empty or missing array is silence, not an error to recover from: the
  object keeps working and goes quiet.
- Any impulse response length works, including one shorter than a block.
- Reading an impulse response allocates, so it belongs where a file load
  belongs and not in the middle of a performance. Convolving allocates nothing.
- A change of block size rebuilds the convolver and reads the array again.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The build fetches dependencies. This can be skipped by specifying directories:

```sh
cmake -S . -B build \
  -DPD_INCLUDE_DIR=/Applications/Pd-0.55-2.app/Contents/Resources/src \
  -DFFTCONVOLVER_DIR=/path/to/FFTConvolver
```

On Windows the external has to link against Pd's import library, which source
alone does not provide: add `-DPD_LIBRARY=<path to pd.lib>`.

| Option | Default | Meaning |
| --- | --- | --- |
| `PD_INCLUDE_DIR` | fetched | directory containing `m_pd.h` |
| `PD_LIBRARY` | searched | `pd.lib`; Windows only |
| `FFTCONVOLVER_DIR` | fetched | directory containing `FFTConvolver.h` |
| `GREATEST_INCLUDE_DIR` | fetched | directory containing `greatest.h`; tests only |
| `PD_EXECUTABLE` | searched | `pd`, for the smoke test |
| `CONVOLUTION_BUILD_TESTS` | `ON` | build the test binaries |

### Installing

```sh
cmake --install build --prefix ~/Documents/Pd/externals
```

Writes a `convolution/` directory holding the external, the help patch and the
meta patch. [deken](https://github.com/pure-data/deken) package layout is
followed.

## Testing

`tests/test_core.c` runs the core under greatest, without Pd: an impulse in
must give the impulse response back, at every length from one sample upward,
including lengths that are not a whole number of blocks.

`tests/run_pd_smoke.py` loads the library into a headless Pd and convolves a
constant with a one-sample impulse response, which is the identity, so the
value has to come back unchanged. Point it at a Pd with `-DPD_EXECUTABLE=` or
the `PD` environment variable; without one the test skips rather than fails.

CI builds and tests macOS, Linux and Windows.

## Structure

```text
src/convolution_core.h      convolving a signal with an impulse response
src/convolution_core.cpp    the core, over FFTConvolver's two stage convolver
src/convolution_tilde.c     the [convolution~] class: inlets, outlet, DSP
src/convolution_setup.c     the library setup
```

The core includes no Pd header, so the tests can have it without one. It is the
only C++ here; the class is C.

## Licence

zlib, see [LICENSE](LICENSE). FFTConvolver is MIT, and includes its own FFT, so
there is nothing further to link. Including Pd's `m_pd.h` imposes nothing, so
applications that embed Pd can use this object whatever their own licence is.
greatest is ISC, and is built into the tests alone.
