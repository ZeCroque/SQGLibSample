cmake_minimum_required(VERSION 3.22)

project(
	SQGLibSample
	VERSION 1.0.0
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

set_from_environment(Skyrim64Path)
if(NOT DEFINED Skyrim64Path)
	message(FATAL_ERROR "Skyrim64Path is not set")
else()
	cmake_path(CONVERT ${Skyrim64Path} TO_CMAKE_PATH_LIST Skyrim64PathConverted)
endif()

option(LEGACY_SCRIPT_PATH "Configure project to use Scripts/Source instead of the new (and stupid) Source/Scripts path")
if(${LEGACY_SCRIPT_PATH})
	set(SCRIPT_FOLDER "Scripts/Source")
else()
	set(SCRIPT_FOLDER "Source/Scripts")
endif()

set(LOG_FOLDER "$ENV{homedrive}$ENV{homepath}/Documents/My Games/Skyrim Special Edition/Logs")
cmake_path(CONVERT ${LOG_FOLDER} TO_CMAKE_PATH_LIST LOG_FOLDER)

option(NOTES_FOLDER_PATH "If you version notes/TODOs etc.. outside git and want to have them appear in VSC, you can specify this path")
if(NOT ${NOTES_FOLDER_PATH} STREQUAL "OFF")
	set(NOTES_JSON ",\n\t\t{\n\t\t\t\"name\": \"Notes\",\n\t\t\t\"path\": \"${NOTES_FOLDER_PATH}\"\n\t\t}")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Template.code-workspace.in" 
	"${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.code-workspace"
	@ONLY
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Template.ppj.in" 
	"${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.ppj"
	@ONLY
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tasks.json.in" 
	"${CMAKE_CURRENT_SOURCE_DIR}/.vscode/tasks.json"
	@ONLY
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PUBLIC_HEADER_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/includes/)
set(PUBLIC_HEADER_DIR ${PUBLIC_HEADER_ROOT_DIR}/SQG/)

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/includes/Plugin.h")
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.h.in"
	"${VERSION_HEADER}"
	@ONLY
)

set(PUBLIC_HEADERS
)

set(PRIVATE_HEADERS
	${SOURCE_DIR}/PCH.h
)

set(HPP
)

set(CPP
	${SOURCE_DIR}/main.cpp
)

set(SOURCES
	${CPP}
	${HPP}
)

set_source_files_properties(${HPP} PROPERTIES HEADER_FILE_ONLY TRUE)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

source_group("Private/Includes" REGULAR_EXPRESSION  ${CMAKE_CURRENT_BINARY_DIR}/includes/*)
source_group(TREE ${SOURCE_DIR} PREFIX "Private/Includes" FILES ${PRIVATE_HEADERS})
source_group(TREE ${SOURCE_DIR} PREFIX "Private/Sources" FILES ${SOURCES})
source_group(TREE ${PUBLIC_HEADER_DIR} PREFIX "Public" FILES  ${PUBLIC_HEADERS})

add_library(
	"${PROJECT_NAME}"
	SHARED
	${PRIVATE_HEADERS}
	${PUBLIC_HEADERS}
	${SOURCES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

set_property(DIRECTORY
	PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}"
)

option(MO2_INSTANCE "Name of the instance of MO2 to run")
option(MO2_PROFILE "Name of the MO2 profile required (useful to have MO2 write to mod folder instead of overwrite in this profile)")
option(MO2_TARGET "Name of the MO2 target that will be run (expected to point toward SkyrimSE.exe with extra dll loaded)")
set_from_environment(MO2Path)
if(NOT DEFINED MO2Path)
	message("MO2Path is not set")
else()
	set_target_properties("${PROJECT_NAME}"
		PROPERTIES
			VS_DEBUGGER_COMMAND ${MO2Path}/ModOrganizer.exe
			VS_DEBUGGER_COMMAND_ARGUMENTS  "$<IF:$<BOOL:${MO2_PROFILE}>,-p \"${MO2_PROFILE}\",>  \"moshortcut://$<IF:$<BOOL:${MO2_INSTANCE}>,${MO2_INSTANCE},Skyrim Special Edition>:$<IF:$<BOOL:${MO2_TARGET}>,${MO2_TARGET},Skyrim Special Edition>\""
	)
	file(COPY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Template.ChildProcessDbgSettings ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.ChildProcessDbgSettings)
endif()

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_20
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/sdl"	# Enable Additional Security Checks
			"/utf-8"	# Set Source and Executable character sets to UTF-8
			"/Zi"	# Debug Information Format

			"/permissive-"	# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"/wd4200" # nonstandard extension used : zero-sized array in struct/union

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/includes
		${SOURCE_DIR}
		${HEADER_DIR}
	PUBLIC
		${PUBLIC_HEADER_ROOT_DIR}
)	

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/SQGLib" SQGLib EXCLUDE_FROM_ALL)
if(NOT TARGET CommonLibSSE::CommonLibSSE)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSSE" CommonLibSSE EXCLUDE_FROM_ALL)
endif()

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		CommonLibSSE::CommonLibSSE
		SQGLib
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
		${SOURCE_DIR}/PCH.h
)	

install(
	FILES
		"$<TARGET_FILE:${PROJECT_NAME}>"
	DESTINATION "SKSE/Plugins"
	COMPONENT "main"
)

install(
	FILES
		"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "/"
	COMPONENT "pdbs"
)

add_custom_command(
	TARGET "${PROJECT_NAME}"
	POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_SOURCE_DIR}/Data/SKSE/Plugins/"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_SOURCE_DIR}/Data/SKSE/Plugins/"
	VERBATIM
)

include(cmake/packaging.cmake)
