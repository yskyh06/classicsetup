# Current milestone

Post-download Retail ISO metadata diagnosis and failed-image retention.

# Changed files

- `include/classicsetup/{download,process,retail,workspace}.h`: diagnostic state,
  process truncation, and retained-image model.
- `src/core/process.c`: larger captured tool output with truncation reporting.
- `src/source/{download_curl,download_model,retail_fido,retail_stub,workspace}.c`:
  safe inspection diagnostics and opt-in completed-ISO retention.
- `tests/source_test.c`: retention/cleanup and long metadata-output tests.
- `run_classicsetup`: preserves the diagnostic opt-in through its sudo path.

# Implementation result

- Confirmed lifecycle: libcurl writes `windows.iso.part`; basic size/ISO/hash
  verification and WIM/ESD inspection both read that partial file. Only after
  successful metadata inspection is it renamed to `windows.iso`.
- On metadata failure, the common failure cleanup called
  `classicsetup_workspace_cleanup_failure()`, which deleted the partial file.
- `CLASSICSETUP_KEEP_FAILED_IMAGE=1` now renames a non-empty regular partial ISO
  to `windows.iso` only after basic verification succeeded and metadata-only
  inspection failed. It remains unverified and cannot become install-ready.
- Default failures and cancelled/incomplete transfers retain the old cleanup.
- Safe diagnostics report local path/size, ISO recognition, inspection stage,
  WIM/ESD extraction, tool exit statuses, image count, architecture/language,
  output truncation, and retention. No signed URI is formatted or logged.
- A code defect consistent with the live failure was found: cancellable process
  capture retained only the last 2,047 bytes, potentially discarding the leading
  `Name` and `Architecture` fields from real `wimlib-imagex info` output. Capture
  is now 65,535 bytes and exposes truncation.
- The exact live failing inspection stage is not yet proven without the retained
  ISO/manual result; no Microsoft request was made in this task.

# Build/test result

- Build: passed with zero project warnings.
- CTest: 17/17 passed.
- Tests cover default cleanup, cancelled/incomplete cleanup, opt-in completed
  image retention, exit-time preservation, strict env value, and 8 KiB metadata
  output retaining parser-required fields.
- `git diff --check`: passed.

# Topics for ChatGPT to explain

- Why the ISO remains `.part` through metadata inspection and how diagnostic
  retention differs from verified promotion.
- How the next retained run identifies extraction, wimlib, or metadata mismatch.

# Issues/cautions

- Retained ISO path and manual `file`/`7z`/wimlib results are pending one user-run
  Ubuntu VM download with `CLASSICSETUP_KEEP_FAILED_IMAGE=1`.
- The extracted install image temporarily needs substantial additional free
  space; extractor exit diagnostics will expose a space-related failure.
- Fido, Microsoft requests, WebView, signed-link caching, and destructive storage
  code were not changed.
