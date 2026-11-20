project(libplatform)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -Wno-error=int-conversion")
# -Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-no_arch_warnings -Wl,-bind_at_load")

add_definitions(-DPRIVATE)

include_directories(BEFORE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/private
	${CMAKE_CURRENT_SOURCE_DIR}/internal
	${CMAKE_CURRENT_SOURCE_DIR}/os/resolver
)

if (TARGET_x86_64 OR TARGET_ARM64)
	# One object library per 64-bit slice. Each is compiled for its own architecture only:
	# the assembly differs per architecture and cannot be built with the fat -arch flags.
	# `platform_64bit` is the primary slice's library, as before; other slices get
	# `platform_64bit_<arch>`. They feed the fat `platform` dylib and the per-slice static archives.
	set(platform_64bit_objects "")
	foreach (arch64 IN LISTS APPLE_ARCH_64BIT_LIST)
		if (arch64 STREQUAL "x86_64")
			set(platform_64bit_sources
				src/setjmp/x86_64/_setjmp.S
				src/setjmp/x86_64/setjmp.S
				src/setjmp/x86_64/_sigtramp.S
				src/ucontext/x86_64/_ctx_start.S
				src/ucontext/x86_64/getcontext.S
				src/ucontext/x86_64/_setcontext.S
				src/cachecontrol/x86_64/cache.S
				src/atomics/x86_64/pfz.S
				src/atomics/x86_64/OSAtomic.S
				src/string/x86_64/bzero.S
			)
		elseif (arch64 STREQUAL "arm64")
			set(platform_64bit_sources
				src/cachecontrol/arm64/cache.S
				src/setjmp/arm64/setjmp.S
				src/ucontext/arm64/_ctx_start.S
				src/ucontext/arm64/getcontext.S
				src/ucontext/arm64/_setcontext.S
				src/string/arm64/string.c
			)
			# Optimized even when the tree isn't, and without builtins: its loops
			# must not turn into calls to themselves.
			set_source_files_properties(src/string/arm64/string.c PROPERTIES COMPILE_OPTIONS "-O2;-fno-builtin;-ffreestanding")
		else ()
			message(FATAL_ERROR "libplatform: no sources for the ${arch64} slice")
		endif ()
		if (arch64 STREQUAL APPLE_ARCH_64BIT)
			set(platform_64bit_target platform_64bit)
		else ()
			set(platform_64bit_target platform_64bit_${arch64})
		endif ()
		add_darling_object_library(${platform_64bit_target} ${platform_64bit_sources})
		set_target_properties(${platform_64bit_target} PROPERTIES COMPILE_FLAGS "-arch ${arch64}")
		set(platform_64bit_target_${arch64} ${platform_64bit_target})
		list(APPEND platform_64bit_objects $<TARGET_OBJECTS:${platform_64bit_target}>)
	endforeach (arch64)
	unset(platform_64bit_target)
else (NO_ARCH_64BIT_SPECIFIC_CODE)
	add_darling_object_library(platform_64bit empty.c)
	set(platform_64bit_objects $<TARGET_OBJECTS:platform_64bit>)
endif ()

if (TARGET_i386)
	if (TARGET_i386)
		set(platform_32bit_sources
			src/setjmp/i386/_setjmp.S
			src/setjmp/i386/setjmp.S
			src/setjmp/i386/_sigtramp.S
			src/ucontext/i386/_ctx_start.S
			src/ucontext/i386/getcontext.S
			src/ucontext/i386/_setcontext.S
			src/atomics/i386/pfz.S
			src/atomics/i386/OSAtomic.S
			src/cachecontrol/i386/cache.S
		)
	endif ()
	add_darling_object_library(platform_32bit ${platform_32bit_sources})
	set_target_properties(platform_32bit PROPERTIES COMPILE_FLAGS "-arch ${APPLE_ARCH_32BIT}")
else (NO_ARCH_32BIT_SPECIFIC_CODE)
	add_darling_object_library(platform_32bit empty.c)
endif ()

set(platform_sources
	src/init.c
	src/force_libplatform_to_build.c
	src/atomics/init.c
	src/atomics/common/MKGetTimeBaseInfo.c
	src/cachecontrol/generic/cache.c
	#src/introspection/introspection.c
	src/os/alloc_once.c
	src/os/atomic.c
	src/os/lock.c
	src/os/semaphore.c
	src/setjmp/generic/setjmperr.c
	src/setjmp/generic/sigaction.c
	src/setjmp/generic/sigtramp.c
	src/simple/asl.c
	src/simple/getenv.c
	src/simple/string_io.c
	src/string/generic/bzero.c
	src/string/generic/ffsll.c
	src/string/generic/flsll.c
	src/string/generic/memccpy.c
	src/string/generic/memchr.c
	src/string/generic/memcmp.c
	src/string/generic/memmove.c
	src/string/generic/memset_pattern.c
	src/string/arm64/static.c
	src/string/generic/strchr.c
	src/string/generic/strcmp.c
	src/string/generic/strcpy.c
	src/string/generic/strlcat.c
	src/string/generic/strlcpy.c
	src/string/generic/strlen.c
	src/string/generic/strncmp.c
	src/string/generic/strncpy.c
	src/string/generic/strnlen.c
	src/string/generic/strstr.c
	src/ucontext/generic/getmcontext.c
	src/ucontext/generic/makecontext.c
	src/ucontext/generic/setcontext.c
	src/ucontext/generic/swapcontext.c
)

set(DYLIB_INSTALL_NAME "/usr/lib/system/libsystem_platform.dylib")
add_circular(platform FAT
	SOURCES ${platform_sources}
	OBJECTS $<TARGET_OBJECTS:platform_32bit> ${platform_64bit_objects}
	LINK_FLAGS "-Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases"
	SIBLINGS system_kernel compiler_rt system_c system_dyld
	)
set_target_properties(platform PROPERTIES OUTPUT_NAME "system_platform")
#make_fat(platform)
#set_property(TARGET platform APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases")

#target_link_libraries(platform PRIVATE system_kernel compiler_rt)

if (BUILD_TARGET_32BIT)
	add_darling_static_library(platform_static32 SOURCES ${platform_sources} $<TARGET_OBJECTS:platform_32bit>)
	set_target_properties(platform_static32 PROPERTIES COMPILE_FLAGS "-arch ${APPLE_ARCH_32BIT} -DVARIANT_STATIC -fvisibility=hidden")
	add_dependencies(platform_static32 ranlib ${APPLE_TARGET_TRIPLET_PRIMARY}-ar lipo)
endif (BUILD_TARGET_32BIT)

if (BUILD_TARGET_64BIT)
	list(LENGTH APPLE_ARCH_64BIT_LIST platform_64bit_arch_count)
	if (platform_64bit_arch_count GREATER 1)
		# A static archive cannot mix architectures: one archive per slice, joined with lipo.
		foreach (arch64 IN LISTS APPLE_ARCH_64BIT_LIST)
			set(DARLING_LIB_64BIT_ARCH ${arch64})
			add_darling_static_library(platform_static64_${arch64} 64BIT_ONLY SOURCES ${platform_sources} $<TARGET_OBJECTS:${platform_64bit_target_${arch64}}>)
			unset(DARLING_LIB_64BIT_ARCH)
			set_target_properties(platform_static64_${arch64} PROPERTIES COMPILE_OPTIONS "-arch;${arch64};-DVARIANT_STATIC;-fvisibility=hidden")
			add_dependencies(platform_static64_${arch64} ranlib ${APPLE_TARGET_TRIPLET_PRIMARY}-ar lipo)
		endforeach (arch64)
		darling_lipo_static_library_slices(platform_static64 ${APPLE_ARCH_64BIT_LIST})
	else ()
		add_darling_static_library(platform_static64 SOURCES ${platform_sources} ${platform_64bit_objects})
		set_target_properties(platform_static64 PROPERTIES COMPILE_OPTIONS "-arch;${APPLE_ARCH_64BIT};-DVARIANT_STATIC;-fvisibility=hidden")
		add_dependencies(platform_static64 ranlib ${APPLE_TARGET_TRIPLET_PRIMARY}-ar lipo)
	endif ()
endif (BUILD_TARGET_64BIT)

install(TARGETS platform DESTINATION libexec/darling/usr/lib/system)

# These files implement memset, memmove, memcmp and memchr themselves.
# Without -fno-builtin the compiler may recognise their loops as those
# very functions and turn them into calls to themselves.
set_source_files_properties(
	src/string/generic/bzero.c
	src/string/generic/memchr.c
	src/string/generic/memcmp.c
	src/string/generic/memmove.c
	PROPERTIES COMPILE_OPTIONS "-fno-builtin")
