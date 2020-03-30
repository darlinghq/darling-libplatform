project(libplatform)

cmake_minimum_required(VERSION 2.4.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc")
# -Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-no_arch_warnings -Wl,-bind_at_load")

add_definitions(-DPRIVATE)

include_directories(BEFORE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/private
	${CMAKE_CURRENT_SOURCE_DIR}/internal
)

if (TARGET_x86_64)
	set(platform_x86_64_sources
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
	add_darling_object_library(platform_x86_64 ${platform_x86_64_sources})
	set_target_properties(platform_x86_64 PROPERTIES COMPILE_FLAGS "-arch x86_64")
else (TARGET_x86_64)
	add_darling_object_library(platform_x86_64 empty.c)
endif (TARGET_x86_64)

if (TARGET_i386)
	set(platform_i386_sources
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
	add_darling_object_library(platform_i386 ${platform_i386_sources})
	set_target_properties(platform_i386 PROPERTIES COMPILE_FLAGS "-arch i386")
else (TARGET_i386)
	add_darling_object_library(platform_i386 empty.c)
endif (TARGET_i386)

set(platform_sources
	src/setjmp/generic/sigtramp.c
	src/setjmp/generic/setjmperr.c
	src/os/semaphore.c
	src/os/atomic_up.c
	src/os/lock.c
	src/os/lock_wfe.c
	src/os/lock_up.c
	src/os/resolver.c
	src/os/alloc_once.c
	src/os/atomic.c
	src/ucontext/generic/getmcontext.c
	src/ucontext/generic/makecontext.c
	src/ucontext/generic/swapcontext.c
	src/ucontext/generic/setcontext.c
	src/force_libplatform_to_build.c
	src/init.c
	src/cachecontrol/generic/cache.c
	src/string/generic/memccpy.c
	src/string/generic/memmove.c
	src/string/generic/strcmp.c
	src/string/generic/flsll.c
	src/string/generic/memcmp.c
	src/string/generic/strchr.c
	src/string/generic/ffsll.c
	src/string/generic/strncmp.c
	src/string/generic/bzero.c
	src/string/generic/memset_pattern.c
	src/string/generic/memchr.c
	src/atomics/common/MKGetTimeBaseInfo.c
	src/atomics/init.c
	src/simple/string_io.c
	src/simple/asl.c
	src/simple/getenv.c
	#src/introspection/introspection.c
)


set(DYLIB_INSTALL_NAME "/usr/lib/system/libsystem_platform.dylib")
add_circular(platform FAT
	SOURCES ${platform_sources}
	OBJECTS $<TARGET_OBJECTS:platform_i386> $<TARGET_OBJECTS:platform_x86_64>
	LINK_FLAGS "-Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases"
	SIBLINGS system_kernel compiler_rt system_c system_dyld
	)
set_target_properties(platform PROPERTIES OUTPUT_NAME "system_platform")
#make_fat(platform)
#set_property(TARGET platform APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/xcodeconfig/libplatform.aliases")

#target_link_libraries(platform PRIVATE system_kernel compiler_rt)

set(CMAKE_AR "${CMAKE_BINARY_DIR}/src/external/cctools-port/cctools/ar/x86_64-apple-darwin11-ar")
set(CMAKE_RANLIB "${CMAKE_BINARY_DIR}/src/external/cctools-port/cctools/misc/ranlib")

if (TARGET_i386)
	add_library(platform_static32 STATIC ${platform_sources} $<TARGET_OBJECTS:platform_i386>)
	set_target_properties(platform_static32 PROPERTIES COMPILE_FLAGS "-arch i386 -DVARIANT_STATIC -fvisibility=hidden")
	add_dependencies(platform_static32 ranlib x86_64-apple-darwin11-ar lipo)
endif (TARGET_i386)

if (TARGET_x86_64)
	add_library(platform_static64 STATIC ${platform_sources} $<TARGET_OBJECTS:platform_x86_64>)
	set_target_properties(platform_static64 PROPERTIES COMPILE_OPTIONS "-arch;x86_64;-DVARIANT_STATIC;-fvisibility=hidden")
	add_dependencies(platform_static64 ranlib x86_64-apple-darwin11-ar lipo)
endif (TARGET_x86_64)

install(TARGETS platform DESTINATION libexec/darling/usr/lib/system)
