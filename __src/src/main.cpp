#include <Iir.h>
#include <cairo.h>
#include <cmath>
#include <cstdint>
#include <fftw3.h>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "_dr_wav.h"
#include "fontManager.h"
#include "loggerFactory.h"
#include "regularVideoGenerator.h"
#include "surface.h"
#include "utils.h"
#include "window_functions.h"

#define FPS 60.0
#define VIDEO_WIDTH 1920
#define VIDEO_HEIGHT 1080
#define THREAD_COUNT 16
#define FILTER_ORDER 16
// #define DO_SAVE_LOWPASS_AUDIO

#define SCALE_WIDTH_FROM_FULLHD( x ) ( double( VIDEO_WIDTH ) * ( double( x ) / 1920.0 ) )
#define SCALE_HEIGHT_FROM_FULLHD( x ) ( double( VIDEO_HEIGHT ) * ( double( x ) / 1080.0 ) )

std::vector< std::string > parse_args( int const argc, char const* const* const argv ) {
  spdlogger logger = LoggerFactory::get_logger( "parse_args" );
  logger->trace( "enter: argc: {}, argv: {}", argc, static_cast< void const* >( argv ) );

  std::vector< std::string > args;
  std::string tmp_arg;
  std::string tmp_multi_arg;
  char multi_arg = '\0';
  for( int i = 0; i < argc; i++ ) {
    tmp_arg = std::string( argv[i] );
    if( ( !multi_arg ) && ( tmp_arg.size() > 0 ) && ( tmp_arg.front() == '\"' || tmp_arg.front() == '\'' ) ) {
      multi_arg = tmp_arg.front();
      tmp_multi_arg += tmp_arg;
    } else if( multi_arg ) {
      tmp_multi_arg += " " + tmp_arg;
      if( ( tmp_arg.size() > 0 ) && ( tmp_arg.back() == multi_arg ) ) {
        multi_arg = '\0';
        // skip first "/' and go until before last of them
        args.push_back( tmp_multi_arg.substr( 1, tmp_multi_arg.size() - 2 ) );
        tmp_multi_arg = "";
      }
    } else {
      args.push_back( tmp_arg );
    }
  }

  logger->trace( "exit" );
  return args;
}

int main( int argc, char** argv ) {
  LoggerFactory::init( "main.log", false );

  std::vector< std::string > args = parse_args( argc, argv );
  for( size_t i = 0; i < args.size(); i++ ) {
    spdlog::debug( "arg {}: {:?}", i, args[i] );
  }
  if( args.size() <= 1 ) {
    // spdlog::error( "usage: program 'folder/path/of/video/project'" );
    // LoggerFactory::deinit();
    // return 1;
    args.push_back(R"(C:\Users\SFG\Documents\Video Project\_TEST)");
  }

  std::filesystem::path project_path( args[1] );
  project_path = std::filesystem::absolute( project_path );
  spdlog::debug( "project_path: {:?}", project_path.string() );
  std::filesystem::path common_path( project_path / ".." );
  common_path = std::filesystem::absolute( common_path );
  spdlog::debug( "common_path: {:?}", common_path.string() );

  FontManager::init( common_path / "__fonts" );

  RegularVideoGenerator::init( project_path, common_path );

  RegularVideoGenerator::render();

  RegularVideoGenerator::deinit();

  FontManager::deinit();

  LoggerFactory::deinit();
  return 0;
}
