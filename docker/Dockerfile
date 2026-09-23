# The formatting and analysis tools, at the version the checks use.
#
# The LLVM packages are pinned to one upstream release so that a later archive
# update cannot move to another one, which would change how the sources are
# laid out or what the analyser reports. `.clang-format` sets no column limit,
# so a line broken on purpose survives; a different clang-format would still
# disagree about everything else. The Ubuntu revision is left open: the
# updates pocket keeps only its latest one, so pinning it would break the
# build at the next update.
#
# clang-tidy reads a compilation database, which records the compiler and the
# standard library headers the build used. The image carries both, with CMake
# to configure the build inside it: a database made on the host names a
# compiler this image does not have.
FROM ubuntu:24.04

ARG LLVM_VERSION=18
ARG LLVM_PACKAGE_VERSION=1:18.1.3-*

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        "clang-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION}" \
        "clang-format-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION}" \
        "clang-tidy-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION}" \
        "libc++-${LLVM_VERSION}-dev=${LLVM_PACKAGE_VERSION}" \
        "libc++abi-${LLVM_VERSION}-dev=${LLVM_PACKAGE_VERSION}" \
        ca-certificates \
        cmake \
        git \
        make \
    && ln -s "/usr/bin/clang-${LLVM_VERSION}" /usr/local/bin/clang \
    && ln -s "/usr/bin/clang++-${LLVM_VERSION}" /usr/local/bin/clang++ \
    && ln -s "/usr/bin/clang-format-${LLVM_VERSION}" /usr/local/bin/clang-format \
    && ln -s "/usr/bin/clang-tidy-${LLVM_VERSION}" /usr/local/bin/clang-tidy \
    && rm -rf /var/lib/apt/lists/*

# scripts/tools.sh runs the tools directly when it finds this.
ENV SCRAP_TOOLS_CONTAINER=1
