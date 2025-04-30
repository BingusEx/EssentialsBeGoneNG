cmake_minimum_required(VERSION 3.21)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

########################################################################################################################
## Define project
########################################################################################################################
project(
        EssentialsBegone
        VERSION 1.7.0
        DESCRIPTION ""
        LANGUAGES CXX
)
        
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
add_compile_definitions(_DISABLE_EXTENDED_ALIGNED_STORAGE)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")


########################################################################################################################
## Configure target DLL
########################################################################################################################

include(FetchContent)

FetchContent_Declare(
  simpleini
  GIT_REPOSITORY https://github.com/brofield/simpleini.git
  GIT_TAG        master
)

FetchContent_MakeAvailable(simpleini)


include(XSEPlugin)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CLIB_UTIL_INCLUDE_DIRS}
	${simpleini_SOURCE_DIR}
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	SimpleIni::SimpleIni
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
	src/PCH.h
)