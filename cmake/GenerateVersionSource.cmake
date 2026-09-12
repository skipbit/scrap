# Regenerate the version source from the current state of the repository.
#
# Run in script mode from a build-time target. project(VERSION) can only take
# the version at configure time, but what the binary prints should not wait
# for a reconfigure: a commit, a tag or an edit to the working tree all move
# the description. configure_file leaves the output untouched when the text is
# unchanged, so a build that changes nothing costs one git invocation and no
# recompilation.
#
# Expects SOURCE_DIR, NAME, INPUT and OUTPUT on the command line.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectVersion.cmake")

scrap_version_detect(version describe "${SOURCE_DIR}")
scrap_version_banner(SCRAP_VERSION_BANNER "${NAME}" "${version}" "${describe}")

configure_file("${INPUT}" "${OUTPUT}" @ONLY)
