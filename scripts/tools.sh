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
  find src/build src/command src/compile src/process src/project src/toolchain -type f -name '*.cpp' -print0
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
    CC=clang CXX=clang++ CXXFLAGS=-stdlib=libc++ cmake --preset lint
    linted_sources | xargs -0 -r clang-tidy -p build/lint --warnings-as-errors='*'
    ;;
  *)
    echo "usage: scripts/tools.sh format | format-check | lint" >&2
    exit 2
    ;;
esac
