add_rules("mode.debug", "mode.release")

target("ipc")
    set_kind("static")
    add_files("src/*.c")
    add_includedirs("src")

target("winwriter")
    set_kind("binary")
    add_files("examples/windows/winwriter.c")
    add_includedirs("src")

    add_deps("ipc")

target("winreader")
    set_kind("binary")
    add_files("examples/windows/winreader.c")
    add_includedirs("src")

    add_deps("ipc")

target("sender")
    set_kind("binary")
    add_files("examples/simple/sender.c")
    add_includedirs("src")

    add_deps("ipc")

target("recvier")
    set_kind("binary")
    add_files("examples/simple/recvier.c")
    add_includedirs("src")

    add_deps("ipc")

target("ipc_cpp")
    set_kind("static")
    add_files("cpp/*.cpp", "common/hashtable.c")
    add_files("src/*.c")
    add_includedirs("cpp", "common", "src")

target("cpp_transceiver")
    set_kind("binary")
    add_files("examples/cpp/linux/main.cpp")
    add_includedirs("cpp")

    add_links("pthread")
    add_deps("ipc_cpp")

target("cpp_reader")
    set_kind("binary")
    add_defines("WRITER_PROCESS=0")
    add_files("examples/cpp/windows/main.cpp")
    add_includedirs("cpp")
    add_deps("ipc_cpp")

target("cpp_writer")
    set_kind("binary")
    add_defines("WRITER_PROCESS=1")
    add_files("examples/cpp/windows/main.cpp")
    add_includedirs("cpp")
    add_deps("ipc_cpp")

target("cpp_multi_reader")
    set_kind("binary")
    add_defines("WRITER_PROCESS=0")
    add_files("examples/cpp/windows/multi_instance.cpp")
    add_includedirs("cpp")
    add_deps("ipc_cpp")

target("cpp_multi_writer")
    set_kind("binary")
    add_defines("WRITER_PROCESS=1")
    add_files("examples/cpp/windows/multi_instance.cpp")
    add_includedirs("cpp")
    add_deps("ipc_cpp")
--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

