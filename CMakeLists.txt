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

# The assembly differs per architecture: one object library per slice, platform_<arch>, compiled
# for that slice only. They feed the fat libsystem_platform and the per-slice static archives.
set(platform_arch_objects "")
foreach (arch IN LISTS DARLING_ARCHS)
	if (arch STREQUAL "x86_64")
		set(platform_arch_sources
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
	elseif (arch STREQUAL "arm64")
		set(platform_arch_sources
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
	elseif (arch STREQUAL "i386")
		set(platform_arch_sources
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
	else ()
		message(FATAL_ERROR "libplatform: no sources for the ${arch} slice")
	endif ()
	add_darling_object_library(platform_${arch} ARCHS ${arch} SOURCES ${platform_arch_sources})
	list(APPEND platform_arch_objects $<TARGET_OBJECTS:platform_${arch}>)
endforeach ()

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
	src/os/wait_on_address.c
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
	OBJECTS ${platform_arch_objects}
	LINK_FLAGS "-Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases"
	SIBLINGS system_kernel compiler_rt system_c system_dyld
	)
set_target_properties(platform PROPERTIES OUTPUT_NAME "system_platform")
#make_fat(platform)
#set_property(TARGET platform APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases")

#target_link_libraries(platform PRIVATE system_kernel compiler_rt)

# The static library dyld links: one archive per slice, joined into platform_static with lipo.
foreach (arch IN LISTS DARLING_ARCHS)
	add_darling_static_library(platform_static_${arch} ARCHS ${arch}
		SOURCES ${platform_sources} $<TARGET_OBJECTS:platform_${arch}>)
	set_property(TARGET platform_static_${arch} APPEND_STRING PROPERTY
		COMPILE_FLAGS " -DVARIANT_STATIC -fvisibility=hidden")
	add_dependencies(platform_static_${arch} ranlib ${APPLE_TARGET_TRIPLET_PRIMARY}-ar lipo)
endforeach ()
darling_lipo_static_library_slices(platform_static ${DARLING_ARCHS})

install(TARGETS platform DESTINATION libexec/darling/usr/lib/system)

if (ENABLE_TESTS)
	include(darling_exe)
	add_darling_executable(os_sync_wait_on_address
		tests/os_sync_wait_on_address.c
	)
	install(TARGETS os_sync_wait_on_address
		DESTINATION libexec/darling/usr/libexec/test/platform)
	# In-prefix test; the ___bzero return-register quirk only exists in the x86_64 assembly.
	if (TARGET_x86_64)
		add_darling_executable(darling_bzero_return_regress
			tests/darling_bzero_return_regress.c
		)
		install(TARGETS darling_bzero_return_regress
			DESTINATION libexec/darling/usr/libexec/test/platform)
	endif ()
endif ()

# These files implement memset, memmove, memcmp and memchr themselves.
# Without -fno-builtin the compiler may recognise their loops as those
# very functions and turn them into calls to themselves.
set_source_files_properties(
	src/string/generic/bzero.c
	src/string/generic/memchr.c
	src/string/generic/memcmp.c
	src/string/generic/memmove.c
	PROPERTIES COMPILE_OPTIONS "-fno-builtin")
