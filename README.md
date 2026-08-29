# pd-convolution

A partitioned convolution external for [Pure Data](https://puredata.info),
built on [FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver).

| Object | Kind | Does |
| --- | --- | --- |
| `[convolution~]` | signal | convolves its input with an impulse response held in an array |

## Why

Implementing partitioned convolution in vanilla Pd is possible but prohibitively expensive.
The object count grows with the length of the response and the arithmetic needs to run as a
separate pass over memory per partition. For a six second impulse response a vanilla
implementation measured around a hundred times slower than this external.

In the external, the response is convolved in two stages: a head block the size of Pd's own
block, and a larger tail block. The head gives the latency and the tail gives
the efficiency, so a six-second impulse response costs little more than a
one second one.

## Using it

```pd
[convolution~ <array> <tail-block>]
```

One class, so there is nothing to load: Pd finds the external when a patch
asks for the object, as long as it is on the search path.

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

- The array is not watched, so the external needs a fresh set message when the array content changes.
- An empty array produces silence, a missing array is an error.
- Any impulse response length is supported, including one shorter than a block.
- Reading an impulse response allocates memory, so it should not be done during live usage. Convolving allocates nothing.
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
meta patch, which is the layout Pd expects for a directory on its search path. [deken](https://github.com/pure-data/deken) package layout is
followed.

## Testing

`tests/test_core.c` runs the core under
[greatest](https://github.com/silentbicycle/greatest), without Pd: an impulse in
must give the impulse response back, at every length from one sample upward,
including lengths that are not a whole number of blocks.

`tests/run_pd_smoke.py` loads the library into a headless Pd and convolves a
constant with a one-sample impulse response, which is the identity, so the
value has to come back unchanged. Point it at a Pd with `-DPD_EXECUTABLE=` or
the `PD` environment variable; without one the test skips rather than fails.

CI builds and tests macOS, Linux and Windows, and `main` takes changes
through a pull request with all three jobs passing and a review.

## Structure

```text
src/convolution_core.h      convolving a signal with an impulse response
src/convolution_core.cpp    the core, over FFTConvolver's two stage convolver
src/convolution_tilde.c     the [convolution~] class: inlets, outlet, DSP
```

The core includes no Pd header, so the tests can have it without one. It is the
only C++ here; the class is C.

## Licence

zlib, see [LICENSE](LICENSE). FFTConvolver is MIT, and includes its own FFT, so
there is nothing further to link. Including Pd's `m_pd.h` imposes nothing, so
applications that embed Pd can use this object whatever their own licence is.
greatest is ISC, and is built into the tests alone.
