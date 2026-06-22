include(FetchContent)

set(MDF_BOOST_VERSION "1.85.0")
set(MDF_BOOST_ARCHIVE_SHA256
    "be0d91732d5b0cc6fbb275c7939974457e79b54d6f07ce2e3dfdd68bef883b0b")
option(MDF_FETCH_BOOST
       "Download pinned Boost headers when a system Boost is not found" ON)

function(mdf_configure_boost_headers out_found out_target)
  find_package(Boost QUIET)
  if (Boost_FOUND)
    if (TARGET Boost::headers)
      set(${out_target} Boost::headers PARENT_SCOPE)
    elseif (TARGET Boost::boost)
      set(${out_target} Boost::boost PARENT_SCOPE)
    else()
      add_library(mdf-boost-headers INTERFACE)
      target_include_directories(mdf-boost-headers INTERFACE ${Boost_INCLUDE_DIRS})
      set(${out_target} mdf-boost-headers PARENT_SCOPE)
    endif()
    set(${out_found} TRUE PARENT_SCOPE)
    return()
  endif()

  if (NOT MDF_FETCH_BOOST)
    set(${out_found} FALSE PARENT_SCOPE)
    set(${out_target} "" PARENT_SCOPE)
    return()
  endif()

  string(REPLACE "." "_" mdf_boost_version_path "${MDF_BOOST_VERSION}")
  FetchContent_Declare(
      mdf_boost
      URL
        "https://archives.boost.io/release/${MDF_BOOST_VERSION}/source/boost_${mdf_boost_version_path}.tar.gz"
      URL_HASH SHA256=${MDF_BOOST_ARCHIVE_SHA256}
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(mdf_boost)

  add_library(mdf-boost-headers INTERFACE)
  target_include_directories(mdf-boost-headers INTERFACE
                             ${mdf_boost_SOURCE_DIR})
  set(${out_found} TRUE PARENT_SCOPE)
  set(${out_target} mdf-boost-headers PARENT_SCOPE)
endfunction()
