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
	if (TARGET_x86_64)
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
		)
	elseif (TARGET_ARM64)
		set(platform_64bit_sources
			src/ucontext/arm64/_ctx_start.S
			src/ucontext/arm64/_setcontext.S
			src/ucontext/arm64/getcontext.S
			src/cachecontrol/arm64/cache.S
			src/setjmp/arm64/setjmp.S
		)
	endif ()
	add_darling_object_library(platform_64bit ${platform_64bit_sources})
	set_target_properties(platform_64bit PROPERTIES COMPILE_FLAGS "-arch ${APPLE_ARCH_64BIT}")
else (NO_ARCH_64BIT_SPECIFIC_CODE)
	add_darling_object_library(platform_64bit empty.c)
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
	OBJECTS $<TARGET_OBJECTS:platform_32bit> $<TARGET_OBJECTS:platform_64bit>
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
	add_darling_static_library(platform_static64 SOURCES ${platform_sources} $<TARGET_OBJECTS:platform_64bit>)
	set_target_properties(platform_static64 PROPERTIES COMPILE_OPTIONS "-arch;${APPLE_ARCH_64BIT};-DVARIANT_STATIC;-fvisibility=hidden")
	add_dependencies(platform_static64 ranlib ${APPLE_TARGET_TRIPLET_PRIMARY}-ar lipo)
endif (BUILD_TARGET_64BIT)

install(TARGETS platform DESTINATION libexec/darling/usr/lib/system)
