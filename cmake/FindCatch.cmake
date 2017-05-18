# From: https://github.com/cinemast/libjson-rpc-cpp
# Copyright (C) 2011-2016 Peter Spiess-Knafl

find_path(
    CATCH_INCLUDE_DIR
    NAMES catch.hpp
    DOC "catch include dir"
)

set(CATCH_INCLUDE_DIRS ${CATCH_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Catch DEFAULT_MSG CATCH_INCLUDE_DIR)
mark_as_advanced(CATCH_INCLUDE_DIR)