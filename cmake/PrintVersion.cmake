# Report what the build will say about its version, to a caller outside CMake.
#
# The release workflow has to know two things: which tag the build describes
# itself by, and what the binary should print. Deriving either in shell would
# be a second encoding of rules that live here -- which tags count as
# releases, and how the banner is composed -- and the two would drift.
#
# Expects SOURCE_DIR, NAME and OUTPUT on the command line. Writes two lines to
# OUTPUT: the git description, then the banner. They go to a file because
# message() writes to stderr, which a caller cannot capture without also
# capturing anything else CMake says.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectVersion.cmake")

scrap_version_detect(version describe "${SOURCE_DIR}")
scrap_version_banner(banner "${NAME}" "${version}" "${describe}")

file(WRITE "${OUTPUT}" "${describe}\n${banner}\n")
