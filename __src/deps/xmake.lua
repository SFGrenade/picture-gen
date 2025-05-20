package("iir1")
    set_homepage("http://berndporr.github.io/iir1")
    set_description("DSP IIR realtime filter library written in C++")
    set_license("mit")

    add_deps("cmake")

    add_urls("https://github.com/berndporr/iir1/archive/refs/tags/$(version).tar.gz")
    add_urls("http://berndporr.github.io/iir1.git")
    add_versions("1.9.5", "beb16142e08e5f68010c6e5014dea2276ea49b71a258439eff09c5ee3f781d88")

    add_includedirs("include")

    on_install(function(package)
        local configs = {
            "-DIIR1_BUILD_TESTING=OFF",
            "-DIIR1_BUILD_DEMO=OFF",
            "-DIIR1_INSTALL_STATIC=ON",
        }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        if package:config("shared") and package:is_plat("windows") then
            table.insert(configs, "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON")
        end
        import("package.tools.cmake").install(package, configs)
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            void test() {
                const int order = 4; // 4th order (=2 biquads)
                Iir::Butterworth::LowPass<order> f;
                const float samplingrate = 1000; // Hz
                const float cutoff_frequency = 5; // Hz
                f.setup (samplingrate, cutoff_frequency);
            }
        ]]}, {configs = {languages = "c++17"}, includes = "Iir.h"}))
    end)

-- has weird results
package("sndfilter")
    set_homepage("https://github.com/velipso/sndfilter")
    set_description("Algorithms for sound filters, like reverb, dynamic range compression, lowpass, highpass, notch, etc")
    set_license("0-bsd")

    add_deps("meson")

    add_urls("https://github.com/velipso/sndfilter.git")
    add_versions("2024.06.23", "1e45029cc5eae2ad12dc7ca1e06c59693482ff90")

    add_includedirs("include", "include/sndfilter")

    on_install(function(package)
        io.writefile("xmake.lua", [[
            add_rules("mode.debug", "mode.release")
            set_languages("c89")

            target("sndfilter")
                set_kind("$(kind)")
                set_encodings("utf-8")

                if is_kind("shared") and is_plat("windows") then
                    add_rules("utils.symbols.export_all")
                end

                if not is_plat("windows") then
                    add_syslinks("m")
                else
                    add_defines("_USE_MATH_DEFINES", {public = true})
                end

                add_includedirs("src")
                add_headerfiles("src/(*.h)", {prefixdir = "sndfilter"})
                add_files("src/*.c")
                remove_files("src/main.c")
        ]])
        import("package.tools.xmake").install(package)
    end)

    on_test(function(package)
        assert(package:has_cfuncs("sf_snd_new", {includes = "snd.h"}))
    end)

-- doesn't work
package("kfr")
    set_homepage("https://www.kfrlib.com/")
    set_description("a library for audio and music analysis")
    set_license("GPL-2.0")

    add_urls("https://github.com/kfrlib/kfr.git")
    add_urls("https://github.com/kfrlib/kfr/archive/refs/tags/$(version).tar.gz")
    add_versions("6.2.0", "bc9507e1dde17a86b68fb045404b66c5c486e61e324d9209468ea1e6cac7173c")

    add_deps("cmake")
    add_deps("python")

    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DKFR_WITH_CLANG=OFF")
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        table.insert(configs, "-DKFR_BUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        if package:config("shared") and package:is_plat("windows") then
            table.insert(configs, "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON")
        end

        import("package.tools.cmake").build(package, configs)
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            void test() {
                kfr::expression_scalar<int> test;
                test.value = 0;
            }
        ]]}, {configs = {languages = "c++17"}, includes = "kfr/all.hpp"}))
    end)
