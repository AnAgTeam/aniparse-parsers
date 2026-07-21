# Writing a parser

A parser is one content source. This walks through the smallest *complete* one —
the built-in **demo source** (`src/parsers/demo/`, `include/aniparse/parsers/demo/`) —
as a worked example. It implements the whole manga contract (browse, search,
suggest, details, chapters, reader pages, related, serialize) and fetches nothing:
its data is a static catalogue and its images are embedded in the library. So it is
also the reference for two seams — serving bytes without the network, and getting a
parser into a build — and a deterministic fixture for interface tests.

Read it top-down: `DemoParser` → `DemoMangaRootGetter` → `DemoMangaGetter`.

---

## 1. The three objects

| Object | Role | Base |
|---|---|---|
| `DemoParser` | Declares *what the source is* and hands out getters. Does no fetching. | `Parser` |
| `DemoMangaRootGetter` | The library root: browse, search, suggest, open by identity. | `MangaRootGetter` |
| `DemoMangaGetter` | One title: details, chapters, pages, related. | `MangaGetter` |

A parser holds **no mutable state** and every method is `const` — one instance is
registered once and used concurrently. Per-run state lives in the `RequestorContext`
the caller passes in, never in a member. (The demo has nothing to store anyway.)

## 2. The parser — `DemoParser`

Four pure virtuals plus the getter factory:

```cpp
ParserInfo info() const override;               // display name + language (+ icon/banner)
std::string identifier() const override;        // stable routing/config/serialize key: "Demo"
ParserCompatibilities compatibilities() const;  // what the source can do, up front
void emplace_domains(EmplaceDomainsContext&) const;  // hosts it answers for
std::unique_ptr<MangaRootGetter> mangas_getter() const;  // the manga library root
```

**`compatibilities()` is read before any request** — it is how the UI decides what to
offer without a speculative fetch. Declare *every* capability here, including ones a
getter also advertises:

```cpp
.flags = compatibilities_flags::supports_manga_store   // a browsable manga library
       | compatibilities_flags::supports_reading       // that hosts its own pages
       | compatibilities_flags::supports_suggestions;  // and completes search tokens
```

> **Gotcha.** `supports_suggestions` is advertised in *two* places: here (parser
> level, so the UI enables autocomplete up front) **and** in
> `SearchCompatibilities::compatibilities` (getter level). Forgetting the parser-level
> one leaves `suggest()` implemented but never called.

A source implements only the categories it has: override `mangas_getter()` /
`images_getter()` / `animes_getter()` for the ones that exist, leave the rest null.

## 3. The root getter — `DemoMangaRootGetter`

Only `from_serialized()` is mandatory; everything else defaults to `NotImplemented`.
The demo overrides the browse surface.

**Pagination is uniform.** Every listing returns `PageResults<T>` and uses
`results.append(filters.from, item)` — which stamps each item's offset and advances
`next_offset` for you. Honour `filters.from` / `filters.limit`; an empty page is the
end-of-listing signal (there is no "has more" flag). See `page_of()` in
`DemoMangaRootGetter.cpp`.

**The search vocabulary.** `search_support()` returns a `SearchCompatibilities`
declaring the filters/sorts the source accepts; `search()` reads them back off the
query. The demo shows every shape:

- **Sorts** — `SupportedSorts` (key → allowed directions).
- **Free text** — `TextQuery` (the `title` axis).
- **Enumerated, excludable** — the `tag` axis is an `ItemSelection` whose options set
  `exclusive = true`, so a tag can be *required* or *forbidden*. Each key equals a
  `Tag::ref`, so a tag tapped on a card round-trips straight back into a query.
- **Enumerated, plain** — the `status` axis (ongoing / released), no exclusion.
- **Open-vocabulary** — the `character` axis is an **empty** `ItemSelection`: its
  tokens are *not* listed here. A consumer discovers them through `suggest()` and the
  chosen token round-trips back in. This is the pattern for an axis too large to
  enumerate (every tag on a booru, every character).
- **Ranges** — `pages` is a **closed** `IntInterval{1, 30}` (declares its own bounds);
  `year` is an **open** `IntInterval{}` (any value).

**`suggest()`** completes tokens for a partial input, narrowed by an optional `kind`
(a search key). The demo completes titles, tags and characters — the last being the
only way to reach the open-vocabulary axis above.

**`from_serialized()`** is the inverse of the getter's `serialize()`: parse the blob
back into a getter. Keep accepting every form the parser has ever emitted.

## 4. The title getter — `DemoMangaGetter`

Mandatory: `compatibilities()`, `info()`, `chapter_pages()`, `serialize()`. The demo
also does `preview_info`, `chapters_info`, `related`.

- **`info()`** builds a `MangaInfo` (a `MediaInfo common` plus author/artist/counts).
  See `full_of()` in `DemoMangaGetter.cpp` for how title, description
  (`AttributedText`), tags (`Tag{ .name, .ref }` — a non-empty `ref` makes it
  searchable), rating (`Rating::from_score`), status (`AiredStatus`), a
  year-precision `ModelDate`, and the cover (`Image`) are set. An absent value means
  "the source did not state it", never a sentinel.
- **`preview_info()`** returns the light short-card the listing already handed the
  getter (`demo_preview()`), so a browse grid costs no extra request.
- **Chapters round-trip by identity, not by number.** `chapters_info()` yields
  `MangaChapterInfo`s whose opaque `id` is the handle; `MangaChapterInfo::ref()`
  packs it into a `MangaChapterRef` that the consumer passes straight into
  `chapter_pages()`. The demo puts the chapter number in `id`.
- **`chapter_pages()`** returns `MangaPage`s in reading order, each carrying an
  `Image{ .url }`.
- **`serialize()`** encodes **identity only** (`std::to_string(id_)`), never fetched
  info — a restored getter refetches.

## 5. Returning values and errors

Every getter returns `NetworkRequestTask<T>` (an awaitable `expected<T, RequestError>`):

```cpp
co_return value;                                              // success (implicitly wrapped)
co_return make_response_error(RequestErrorCode::NotFound, "…"); // typed failure
co_return unexpected(std::move(result.error()));             // propagate one unchanged
```

Never fold a `Cancelled` error into a degraded success — re-propagate it.

## 6. Images without a network — the asset-client seam

Image bytes are fetched through `RequestorContext::request()`, i.e. the session's one
`ClientContext`. A source whose bytes don't come from a plain GET hooks that seam. The
demo emits `demo://covers/…` URLs and installs `DemoAssetClient` — a `ClientContext`
decorator that intercepts exactly those URLs (serving embedded bytes) and **tail-returns
every other request to the real transport untouched**, so cancellation and timing of
real traffic are unchanged. The session wraps its client once, in the facade's
`make_client()`:

```cpp
return aniparse::parsers::demo::wrap_demo_assets(std::move(base));
```

The same seam is where a scramble/decrypt source would post-process page bytes.

## 7. Getting a parser into a build

- **Built-in (default):** add one line to `emplace_default_parsers()`
  (`src/parsers/DefaultParsers.cpp`) and list the new `.cpp`/`.hpp` files in
  `CMakeLists.txt` (sources are listed explicitly, not globbed). The demo is
  registered this way, so it ships in every build.
- **Bundled by a downstream build (extension seam):** a build that carries extra
  parsers defines the weak `register_extra_parsers()` with its own strong version and
  force-loads it — no change to this tree. `DemoParser` is the shape such a definition
  would `add_parser()`.

## 8. It doubles as a test fixture

Because it is deterministic and network-free, `DemoParser` is the zero-I/O tier for
interface tests: exercise the getter contract (info / chapters / `ref()`→pages /
`serialize()`↔`from_serialized()` round-trip), pagination, query validation
(closed/open ranges, tag exclusion, the open-vocabulary + `suggest()` axis) and the
asset-client seam without touching a live source.
