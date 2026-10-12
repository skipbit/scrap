#!/bin/sh
#
# Installs a prebuilt scrap from a GitHub release.
#
#   curl -fsSL https://skipbit.github.io/scrap/install.sh | sh
#   curl -fsSL https://skipbit.github.io/scrap/install.sh | sh -s -- v0.3.0
#
# Without a version it installs the latest release, which is never a
# prerelease; a prerelease is installed only when named. The archive is
# checked against the .sha256 published beside it before anything is
# installed. scrap goes to $SCRAP_HOME/bin, ~/.scrap/bin by default.
#
# When that directory is not on PATH and a terminal is attached, it asks
# before adding it to the shell's startup file; otherwise it prints the line
# to add.

set -eu

repo="https://github.com/skipbit/scrap"

say() {
  printf '%s\n' "$*"
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

need() {
  command -v "$1" > /dev/null 2>&1 || die "$1 is required"
}

platform() {
  case "$(uname -s)" in
    Linux) os=linux ;;
    Darwin) os=darwin ;;
    *) os="$(uname -s)" ;;
  esac
  case "$(uname -m)" in
    x86_64 | amd64) arch=x86_64 ;;
    arm64 | aarch64) arch=aarch64 ;;
    *) arch="$(uname -m)" ;;
  esac
  # A shell running under Rosetta reports x86_64 on Apple silicon.
  if [ "$os-$arch" = darwin-x86_64 ] && [ "$(sysctl -n hw.optional.arm64 2> /dev/null)" = 1 ]; then
    arch=aarch64
  fi
  case "$os-$arch" in
    linux-x86_64 | darwin-aarch64) printf '%s\n' "$os-$arch" ;;
    *) die "no prebuilt scrap for $os-$arch; prebuilt: linux-x86_64, darwin-aarch64" ;;
  esac
}

# GitHub redirects /releases/latest to the latest release's tag page, and to
# the release list when there is none.
latest() {
  location="$(curl -fsS -o /dev/null -w '%{redirect_url}' "$repo/releases/latest")"
  case "$location" in
    */releases/tag/v*) printf '%s\n' "${location##*/tag/}" ;;
    *) die "could not find the latest release (redirected to '$location')" ;;
  esac
}

# Downloads the file $1 of release $tag into the work directory.
download() {
  status="$(curl -fsSL -o "$work/$1" -w '%{http_code}' "$repo/releases/download/$tag/$1")" && return
  if [ "$status" = 404 ]; then
    die "$tag has no $1: the release does not exist or has no prebuilt scrap for $target"
  fi
  die "could not download $1 from $tag"
}

sha256() {
  if command -v sha256sum > /dev/null 2>&1; then
    sha256sum "$1" | cut -d ' ' -f 1
  else
    shasum -a 256 "$1" | cut -d ' ' -f 1
  fi
}

# Prints the startup file for the user's shell and the line that puts $1 on
# PATH there, separated by a newline; nothing for a shell it does not know.
path_entry() {
  case "${SHELL:-}" in
    */zsh)
      printf '%s\n' "${ZDOTDIR:-$HOME}/.zshrc" "export PATH=\"$1:\$PATH\""
      ;;
    */bash)
      if [ "$(uname -s)" = Darwin ]; then
        printf '%s\n' "$HOME/.bash_profile" "export PATH=\"$1:\$PATH\""
      else
        printf '%s\n' "$HOME/.bashrc" "export PATH=\"$1:\$PATH\""
      fi
      ;;
    */fish)
      printf '%s\n' "$HOME/.config/fish/conf.d/scrap.fish" "fish_add_path -g \"$1\""
      ;;
  esac
}

add_to_path() {
  bindir="$1"
  case ":$PATH:" in
    *":$bindir:"*) return ;;
  esac

  entry="$(path_entry "$bindir")"
  if [ -z "$entry" ]; then
    say "Add $bindir to PATH to run scrap by name."
    return
  fi
  rc="$(printf '%s\n' "$entry" | sed -n 1p)"
  line="$(printf '%s\n' "$entry" | sed -n 2p)"

  if [ -f "$rc" ] && grep -qxF "$line" "$rc"; then
    say "$rc already adds $bindir to PATH; open a new shell to use it."
    return
  fi

  answer=n
  if (: < /dev/tty) 2> /dev/null; then
    printf 'Add %s to PATH in %s? [y/N] ' "$bindir" "$rc" > /dev/tty
    read -r answer < /dev/tty || answer=n
  fi
  case "$answer" in
    y | Y | yes | YES)
      mkdir -p "$(dirname "$rc")"
      printf '\n%s\n' "$line" >> "$rc"
      say "Added to $rc; open a new shell to use it."
      ;;
    *)
      say "To run scrap by name, add this line to $rc:"
      say ""
      say "  $line"
      ;;
  esac
}

main() {
  if [ "$#" -gt 1 ]; then
    die "usage: install.sh [version]"
  fi

  need curl
  need tar
  if ! command -v sha256sum > /dev/null 2>&1; then
    need shasum
  fi

  target="$(platform)"
  if [ "$#" -eq 1 ]; then
    tag="v${1#v}"
  else
    tag="$(latest)"
  fi
  name="scrap-${tag#v}-$target"

  work="$(mktemp -d)"
  trap 'rm -rf "$work"' EXIT

  say "Downloading $name.tar.gz"
  download "$name.tar.gz"
  download "$name.tar.gz.sha256"

  expected="$(cut -d ' ' -f 1 "$work/$name.tar.gz.sha256")"
  actual="$(sha256 "$work/$name.tar.gz")"
  if [ -z "$expected" ] || [ "$actual" != "$expected" ]; then
    die "$name.tar.gz has sha256 $actual, expected '$expected'"
  fi

  # The owner recorded in the archive is the build machine's user.
  tar --no-same-owner -xzf "$work/$name.tar.gz" -C "$work"

  # Run before it replaces anything, so a scrap this machine cannot run, as
  # on a glibc older than the release requires, leaves the old one in place.
  banner="$("$work/$name/scrap" --version)" ||
    die "the downloaded scrap does not run on this machine"

  home="${SCRAP_HOME:-$HOME/.scrap}"
  mkdir -p "$home/bin"
  # Replaced by a rename, so a scrap that is running keeps its own file.
  cp "$work/$name/scrap" "$home/bin/scrap.new"
  chmod 755 "$home/bin/scrap.new"
  mv -f "$home/bin/scrap.new" "$home/bin/scrap"

  say "Installed $banner to $home/bin/scrap"
  add_to_path "$home/bin"
}

main "$@"
