# Shared Objects prefetcher for ELF

Pre-fetches data in ELF file in parallel into page cache to hopefully
execute binaries faster.

## Building

```
g++ -std=c++20 ./configure.cc -o configure
./configure
ninja
```

## Running

The loader will try to chain-load the specified program, and in
background pre-load the necessary shared objects.

```
./out/sopreload /usr/bin/blender --help
```

This should hopefully load the binary slightly faster on cold storage case.

For debugging and analysis this will output the json trace file for loading in the stdout.


In order to do prefetching only use the preload_only.
This is useful to find performance bottlenecks in prefetching.

```
./out/preload_only /usr/bin/blender --help
```

## Benchmark results

Running `blender --help` which usually took 488ms for cold start takes 403ms.

```
$ ./out/benchmark 20  /usr/bin/blender --help
Uncached
491510 488116 490450 488650 491817 487221 489352 486204 483876 487373 479775 493265 496639 485626 487546 488174 484019 488856 486128 487017 
mean: 488080
Cached
249618 250375 255472 253583 224395 250906 260080 248060 249563 251113 246303 250274 248730 257889 247030 256786 248502 250862 218814 251379 
mean: 248486
$ ./out/benchmark 20 ./out/preload_only /usr/bin/blender 
Uncached
58935 60762 65543 66732 63353 62280 63202 60573 64667 62977 66384 61333 64929 62375 63395 60673 66338 63589 62504 65368 
mean: 63295
Cached
8695 7161 7261 9265 7774 8189 7166 7146 9719 7328 7335 6957 14768 7257 8222 7870 8037 8189 6952 9039 
mean: 8216
$ ./out/benchmark 20 ./out/sopreload /usr/bin/blender --help
Uncached
401095 414171 405886 395763 382416 400172 401504 397551 410193 396670 417944 446995 398018 416395 398040 406689 395209 401106 390313 399213 
mean: 403767
Cached
256908 228118 256842 256628 253763 251576 224173 224972 251348 231314 255382 251439 253238 255929 253997 246072 251852 250630 218704 253806 
mean: 246334
```

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.

