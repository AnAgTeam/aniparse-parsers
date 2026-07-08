# aniparse-parsers

Canonical, runnable **example parsers** for [libaniparse](https://github.com/AnAgTeam/aniparse) —
small, real implementations that show how to build a source parser against the library's public
parser API.

Every parser here targets an **official, public, sanctioned developer API**: no scraping, no
authentication, no access-control circumvention. They exist to demonstrate the library, and to serve
as a clean reference when writing your own parsers.

## Parsers

| Parser | Source | API shape | Notes |
|--------|--------|-----------|-------|
| `AniListParser` | [AniList](https://anilist.co) | GraphQL (POST) | search / info / genre filter / routing |
| `KitsuParser`   | [Kitsu](https://kitsu.io)     | JSON:API (offset paging, sideloaded relationships) | search / info / slug-or-id routing |

These are **metadata catalogs** — they describe manga (titles, covers, genres, ratings, status) but
host no chapter content. The reading path (`chapter_pages`) is therefore intentionally left as the
base `NotImplemented`: it is not a gap, it is the honest shape of a metadata source.

Between them they exercise the library across two very different API styles — a GraphQL endpoint
(single POST, query + variables) and a JSON:API service (offset pagination, `?include=` sideloading)
— using the same `Parser` / `MangaRootGetter` / `MangaGetter` model, `Parser::make_config`, and the
typed `RequestorContext::request_json` GET/POST helpers.

## Build

libaniparse is included as a git submodule, so clone recursively:

```sh
git clone --recursive https://github.com/AnAgTeam/aniparse-parsers
cd aniparse-parsers
# or, if you already cloned without --recursive:
git submodule update --init --recursive
```

Configure and build with a CMake preset (needs a C++20 toolchain and `VCPKG_ROOT` set):

```sh
cmake --preset x64-debug        # Windows/MSVC; or linux-debug / macos-debug
cmake --build out/build/x64-debug
```

This builds the `aniparse-parsers` static library and a live demo executable
(`aniparse-parsers-example`) that searches both sources, prints the top result, and routes a couple
of URLs against the real APIs.

## Use

```cpp
#include <aniparse/Client.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/parsers/DefaultParsers.hpp>

aniparse::ParserStore store;
aniparse::parsers::emplace_default_parsers(store);   // registers AniList + Kitsu

// ... drive them through RequestorContext, exactly like the demo in examples/example.cpp
```

Or register a single parser directly:

```cpp
#include <aniparse/parsers/anilist/AniListParser.hpp>
store.add_parser(std::make_shared<aniparse::parsers::AniListParser>());
```

## License

See [LICENSE](LICENSE). The parsed services are the property of their respective owners; this
project only implements clients for their public APIs.
