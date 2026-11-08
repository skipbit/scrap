#!/bin/bash
#
# Runs the formatting and analysis tools at the version docker/Dockerfile pins.
# The pull request checks call this too, so a source that passes here passes
# there.
#
#   scripts/tools.sh format        rewrite the sources in place
#   scripts/tools.sh format-check  fail on a source that is not formatted
#   scripts/tools.sh lint          run clang-tidy over the modules the check covers
#
# lint runs 4 clang-tidy at a time, or as many as SCRAP_LINT_JOBS names. It
# keeps the results in build/ctcache and skips a source whose result there was
# clean, unless SCRAP_LINT_CACHE is off. Removing build/ctcache drops them.
#
# Outside the container it starts one and runs itself inside it.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ -z "${SCRAP_TOOLS_CONTAINER:-}" ]; then
  exec env REPO_ROOT="$root" HOST_UID="$(id -u)" HOST_GID="$(id -g)" \
    docker compose -f "$root/docker/docker-compose.yml" run --rm --build tools \
    scripts/tools.sh "$@"
fi

cd "$root"

formatted_sources() {
  find src test -type f \( -name '*.cpp' -o -name '*.h' \) -print0
}

# Lint is scoped to the modules written against the strict check set. Legacy
# modules predate it and are migrated or removed incrementally; the scope
# widens as that migration progresses, and a module added here is expected to
# stay clean from its first commit. Findings in any src/ header pulled in by
# these TUs (see HeaderFilterRegex) also fail the check.
linted_sources() {
  find src/build src/command src/compile src/digest src/fetch src/file src/install src/json src/process src/project src/registry src/tar src/toolchain src/xz -type f -name '*.cpp' -print0
}

# The linted sources as one regular expression over the paths in the
# compilation database, which is how run-clang-tidy takes them. It fails when
# there are none or when one has no entry in the database, as run-clang-tidy
# would skip that source without a word.
linted_sources_regex() {
  linted_sources | python3 -c '
import json, os, re, sys

def path_of(entry):
    if os.path.isabs(entry["file"]):
        return entry["file"]
    return os.path.normpath(os.path.join(entry["directory"], entry["file"]))

with open(sys.argv[1]) as database_file:
    database = {path_of(entry) for entry in json.load(database_file)}
sources = [os.path.abspath(path) for path in sys.stdin.read().split("\0") if path]
if not sources:
    sys.exit("no linted sources")
missing = [path for path in sources if path not in database]
if missing:
    sys.exit("not in the compilation database: " + " ".join(missing))
print("|".join("^" + re.escape(path) + "$" for path in sources))
' build/lint/compile_commands.json
}

case "${1:-}" in
  format)
    formatted_sources | xargs -0 -r clang-format -i
    ;;
  format-check)
    clang-format --version
    formatted_sources | xargs -0 -r clang-format --dry-run --Werror
    ;;
  lint)
    clang-tidy --version
    # --fresh: build/lint may hold a cache made on the host, naming a compiler
    # the image does not have.
    CC=clang CXX=clang++ CXXFLAGS=-stdlib=libc++ cmake --preset lint --fresh
    # run-clang-tidy prints the output of each file whole.
    files_re="$(linted_sources_regex)"
    case "${SCRAP_LINT_CACHE:-on}" in
      on) clang_tidy=clang-tidy-cached ;;
      off) clang_tidy=clang-tidy ;;
      *)
        echo "SCRAP_LINT_CACHE is on or off, not ${SCRAP_LINT_CACHE}" >&2
        exit 2
        ;;
    esac
    CTCACHE_DIR="$root/build/ctcache" run-clang-tidy -clang-tidy-binary "$clang_tidy" \
      -p build/lint -j "${SCRAP_LINT_JOBS:-4}" -warnings-as-errors='*' "$files_re"
    ;;
  *)
    echo "usage: scripts/tools.sh format | format-check | lint" >&2
    exit 2
    ;;
esac
