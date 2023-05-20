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

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.

