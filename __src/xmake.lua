set_project( "Video-Frame-Generator" )

set_version( "0.0.1", { build = "%Y%m%d", soname = true } )

set_warnings( "allextra" )

add_rules( "mode.debug", "mode.release", "mode.releasedbg", "mode.minsizerel" )

set_languages( "c++20" )

if is_plat( "windows" ) then
  add_cxflags( "/Zc:__cplusplus" )
  add_cxflags( "/Zc:preprocessor" )

  add_cxflags( "/permissive-" )
else
end

--add_requireconfs( "*", "**", "*.**", "**.*", "**.**", { system = false } )
--add_requireconfs( "*", "**", "*.**", "**.*", "**.**", { configs = { shared = get_config( "kind" ) == "shared" } } )
add_requireconfs( "*", { configs = { shared = get_config( "kind" ) == "shared" } } )

add_requires( "cairo" )
add_requires( "dr_wav" )
add_requires( "vcpkg::iir1", { alias = "iir1" } )
add_requires( "vcpkg::fftw3", { alias = "fftw3" } )
add_requires( "spdlog" )

add_requireconfs( "fftw3", { configs = { features = { "threads" } } } )
add_requireconfs( "spdlog", { configs = { header_only = true, std_format = false, fmt_external = false, fmt_external_ho = true, noexcept = false } } )

if is_plat( "windows" ) then
  add_cxxflags( "/utf-8", { public = true } )
end

target( "Video-Frame-Generator" )
  set_kind( "binary" )
  set_encodings( "utf-8" )

  set_default( true )
  set_group( "EXES" )

  add_packages( "cairo", { public = true } )
  add_packages( "dr_wav", { public = true } )
  add_packages( "iir1", { public = true } )
  add_packages( "fftw3", { public = true } )
  add_packages( "spdlog", { public = true } )

  add_includedirs( "include", { public = true } )

  add_headerfiles( "include/(*.h)" )

  add_files( "src/*.cpp" )
