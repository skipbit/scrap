# syntax=docker/dockerfile:1
#
# The image the Linux release binary is built in, so that a build fetches
# nothing from the package archives: the job pulls this image by digest, and
# the archives are reached only when the image is built again.
#
# ubuntu:20.04 sets the oldest glibc the binary runs on (2.31). The GCC that
# scrap-toolchain distributes was built on ubuntu:20.04 as well, and its own
# programs link the isl, mpc, mpfr and gmp of that release.
FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils \
        make binutils libc6-dev libisl22 libmpc3 libmpfr6 libgmp10 \
    && rm -rf /var/lib/apt/lists/*

# git comes from the git-core PPA: the version tests read archives made with
# %(describe), which the git of 20.04 does not know.
#
# F911AB184317630C59970973E363C90F8F1B6217, the key Launchpad names as the
# signing key of the PPA. When it changes, the signing_key_fingerprint of
# https://launchpad.net/api/1.0/~git-core/+archive/ubuntu/ppa names the new
# one, which keyserver.ubuntu.com serves.
COPY <<'KEY' /usr/share/keyrings/git-core-ppa.asc
-----BEGIN PGP PUBLIC KEY BLOCK-----

xsFNBGYo2OYBEADVRjI+o29u9izslaVr0Xqj8hpmo/2su/Iey1PgoS6A3hMxR4R4
eZ3u9dRh/gRHXNjxqRMfKj88G6ciqa/7ty8Vfc1eKl3z7yjL1pWOEzcGLKaSB8qd
MmsxCw31nFNEbzlymgK0+KPubQ5OrIzeSikpfDVGT4HLgO42ppGY+cVy2/bbNv6O
mmPXcw8gkxRCWFiGAO5jJYG1SyGbhr9Krbf6o+LDUJeDYPTQRMf702IYZ8Bp00ix
HyK2YOUNM14rr7092o2dw9GKxnJszF4cET+LxRddrREuB3sBlAZav/I0hZtQsKZ3
QTvpStf2MwIy8Ymj94+BsZaktP0d36wigGn8RWJHrhSVyjujS8YxWRWdjrLUI1ra
42pyIYZi39IXtsTM71rihQVrsEHbMQ7a5HGRK6eYyHVPrtsXXykNqhVPekLNiFWm
IekoSH1EwMsQv8y+k3nkCwTCkTHJaueB7awee0Zs5QBwbCm+qPyqCXoVDLEwdQOr
CHKAfNADtsFQsk/yiTC/+Lmtur/okp38VpJWXg8DphHFjd1KwqQ5E9qZi+tXs/JD
UXd93VBUdoGGaHK9fj/URxUBOVopGaOXGVYtGFWPn9q8MaNcrESZ48sXDfsVgUVD
RJ4puKLHjIUtDlCnMQO6lekIhEo4sxtbDnwIUmQp7B9l+U99u+uzBI96cQARAQAB
zShMYXVuY2hwYWQgUFBBIGZvciBVYnVudHUgR2l0IE1haW50YWluZXJzwsGOBBMB
CgA4FiEE+RGrGEMXYwxZlwlz42PJD48bYhcFAmYo2OYCGwMFCwkIBwIGFQoJCAsC
BBYCAwECHgECF4AACgkQ42PJD48bYhdwUQ//UFR3i/6zpizJMTA9mpz7hGC9IJV8
UDoWoaVgl+OR1Ldfz+jvr3K55LZyIMU1o6bbLqbEnoWa2VpRv2za/SCbPqo1igio
p97EJ2irGytFOhCDd+o3s0djfsXpA7jygAK6COnMx3ejnPhaBA194HbYDhp7KA5b
gZcqvYeRN1qk9QL99voFYeUWAPnqkLLrNuAcq9qTotmyYZQI61rdAH8P4odgMtU7
UiS4yLirkAiNCqT+TK07EXvaWSXcqZhgt1HmP+BZhrx/vLT4FlH52CCjamQWeFA2
mLKX/RSx/JTux1UDroX6L9JdUyMzOrLk22Zz9Tb3FK2ysHy72jRKmUv87ctIMcgD
u8BY3Mfe/Rw8vPMuBEaAsVfvWl7uYSt81dzjkxtt6ZvIFF5PBR2IhFJVosDbpSnk
g9/b22Ipjta3ULW8oOaNZjdcRNQ5StpApzZIUoZoP83ZWafwQoSrW7Rz3KvwYAdL
d3OAfuiW9i7YdLCkvaujBt6KA4tn56fcsp14FzLZrgcj+7XUpW4/yEJFHCCu5f2l
NWvN37suUTfzbMLZVS6rC1s3qrjOD+C3dvL/dCUlcTfrrsTcs/UnaVXFq0V6NLhA
ZZxOIVUedn7nGKbaecwTt6taIjpxj0jCBxWy6RcysIkv2xluRcl2UY+HapB8x1Dx
8giHUSvkXHr4c7I=
=uGON
-----END PGP PUBLIC KEY BLOCK-----
KEY

# apt-get update succeeds when the PPA cannot be reached, and the git of 20.04
# is installed instead.
RUN echo 'deb [signed-by=/usr/share/keyrings/git-core-ppa.asc] https://ppa.launchpadcontent.net/git-core/ppa/ubuntu focal main' \
        > /etc/apt/sources.list.d/git-core-ppa.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends git \
    && version=$(dpkg-query -W -f='${Version}' git) \
    && case "$version" in \
        *ppa*) echo "git $version" ;; \
        *) echo "git $version is not from the git-core PPA" >&2; exit 1 ;; \
    esac \
    && rm -rf /var/lib/apt/lists/*

# The CMake of 20.04 is older than the 3.20 the project requires.
ARG CMAKE_VERSION=3.31.12
ARG CMAKE_SHA256=0dc2e9a6860f06bf10bd8fadc03e35d9eeb4df46e33763a7e480e987758f385c
ADD --checksum=sha256:${CMAKE_SHA256} \
    "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    /tmp/cmake.tar.gz
RUN mkdir -p /opt/cmake \
    && tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1 \
    && rm /tmp/cmake.tar.gz
ENV PATH=/opt/cmake/bin:$PATH
