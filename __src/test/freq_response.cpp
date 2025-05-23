#include <numbers>

#include "spdlog/common.h"

#define DR_WAV_IMPLEMENTATION
#include <Iir.h>
#include <cairo.h>
#include <cmath>
#include <dr_wav.h>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <string>
#include <vector>

#include "_spdlog.h"

std::vector< std::string > parse_args( int argc, char** argv ) {
  std::vector< std::string > args;
  std::string tmp_arg;
  std::string tmp_multi_arg;
  char multi_arg = '\0';
  for( int i = 0; i < argc; i++ ) {
    std::string tmp_arg = std::string( argv[i] );
    if( ( !multi_arg ) && ( tmp_arg.size() > 0 ) && ( tmp_arg.front() == '\"' || tmp_arg.front() == '\'' ) ) {
      multi_arg = tmp_arg.front();
      tmp_multi_arg += tmp_arg;
    } else if( multi_arg ) {
      tmp_multi_arg += " " + tmp_arg;
      if( ( tmp_arg.size() > 0 ) && ( tmp_arg.back() == multi_arg ) ) {
        multi_arg = '\0';
        args.push_back( tmp_multi_arg.substr( 1, tmp_multi_arg.size() - 2 ) );
        tmp_multi_arg = "";
      }
    } else {
      args.push_back( tmp_arg );
    }
  }
  return args;
}

void draw_frequency_response( std::vector< double > const& freq, std::vector< double > const& magnitude, std::filesystem::path const& output_path ) {
  const int WIDTH = 800, HEIGHT = 600;
  const int TOP_MARGIN = 20, LEFT_MARGIN = 20, RIGHT_MARGIN = 50, BOTTOM_MARGIN = 50;
  cairo_surface_t* surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT );
  cairo_t* cr = cairo_create( surface );

  // Background
  cairo_set_source_rgb( cr, 1, 1, 1 );
  cairo_paint( cr );

  // Axes
  cairo_set_source_rgb( cr, 0, 0, 0 );
  cairo_set_line_width( cr, 1 );
  cairo_move_to( cr, LEFT_MARGIN, HEIGHT - BOTTOM_MARGIN );
  cairo_line_to( cr, WIDTH - RIGHT_MARGIN, HEIGHT - BOTTOM_MARGIN );
  cairo_line_to( cr, WIDTH - RIGHT_MARGIN, TOP_MARGIN );
  cairo_stroke( cr );

  // get y axis and print min val bottom right
  double mag_min = -120.0;
  double mag_max = +20.0;
  // for( double mag : magnitude ) {
  //   mag_min = std::min( mag_min, mag );
  //   mag_max = std::max( mag_max, mag );
  // }
  cairo_move_to( cr, WIDTH - RIGHT_MARGIN, HEIGHT - BOTTOM_MARGIN );
  cairo_text_path( cr, fmt::format( "{}", mag_min ).c_str() );
  cairo_fill( cr );
  cairo_move_to( cr, WIDTH - RIGHT_MARGIN, TOP_MARGIN );
  cairo_text_path( cr, fmt::format( "{}", mag_max ).c_str() );
  cairo_fill( cr );

  // Plot magnitude data (example)
  cairo_set_source_rgb( cr, 0, 0, 1 );
  cairo_set_line_width( cr, 2 );
  bool skipped = true;
  double x, y;
  for( size_t i = 0; i < freq.size(); ++i ) {
    x = LEFT_MARGIN + ( std::log10( freq[i] ) - std::log10( 10 ) ) / ( std::log10( 22000 ) - std::log10( 10 ) ) * ( WIDTH - ( LEFT_MARGIN + RIGHT_MARGIN ) );
    y = HEIGHT - BOTTOM_MARGIN - ( ( ( magnitude[i] - mag_min ) / ( mag_max - mag_min ) ) * ( HEIGHT - ( TOP_MARGIN + BOTTOM_MARGIN ) ) );
    if( ( x < 0 ) || ( x >= WIDTH ) || ( y < 0 ) || ( y >= HEIGHT ) ) {
      skipped = true;
      continue;
    }
    if( skipped ) {
      cairo_move_to( cr, x, y );
      skipped = false;
    }
    cairo_line_to( cr, x, y );
  }
  cairo_stroke( cr );

  // Save image
  cairo_surface_write_to_png( surface, output_path.string().c_str() );
  cairo_destroy( cr );
  cairo_surface_destroy( surface );
}

#define TestFilter( class, ARGS_MACRO, id )                                               \
  {                                                                                       \
    std::vector< double > freq, magnitude;                                                \
    class filter;                                                                         \
    filter.setup( sampleRate, ARGS_MACRO );                                               \
    for( double f = 10; f <= sampleRate / 2.0; f += 1.0 ) {                               \
      double omega = f / sampleRate;                                                      \
      auto h = filter.response( omega );                                                  \
      freq.push_back( f );                                                                \
      magnitude.push_back( 20 * log10( std::abs( h ) ) );                                 \
    }                                                                                     \
    draw_frequency_response( freq, magnitude, base_path / fmt::format( "{}.png", #id ) ); \
  }

void filter_response_test( std::filesystem::path const& project_folder ) {
  std::filesystem::path base_path( project_folder / "__testing" );
  if( !std::filesystem::is_directory( base_path ) ) {
    std::filesystem::create_directory( base_path );
  }

  double const sampleRate = 44100.0;
  double const cutoffFrequency = 2000.0;
  double const widthFrequency = 10.0;
  double const widthOctaves = 2.0;
  double const gain = 20.0;
  double const passband_ripple_in_dB = 5.0;
  double const stopband_ripple_in_dB = 20.0;
  double const q_factor = 0.707;
  double const slope = 1.0;

#define SETUP_ARGS cutoffFrequency
  TestFilter( Iir::Butterworth::LowPass< 1 >, SETUP_ARGS, Butterworth_LowPass_1 );
  TestFilter( Iir::Butterworth::LowPass< 2 >, SETUP_ARGS, Butterworth_LowPass_2 );
  TestFilter( Iir::Butterworth::LowPass< 3 >, SETUP_ARGS, Butterworth_LowPass_3 );
  TestFilter( Iir::Butterworth::LowPass< 4 >, SETUP_ARGS, Butterworth_LowPass_4 );
  TestFilter( Iir::Butterworth::LowPass< 5 >, SETUP_ARGS, Butterworth_LowPass_5 );
  TestFilter( Iir::Butterworth::LowPass< 6 >, SETUP_ARGS, Butterworth_LowPass_6 );
  TestFilter( Iir::Butterworth::LowPass< 7 >, SETUP_ARGS, Butterworth_LowPass_7 );
  TestFilter( Iir::Butterworth::LowPass< 8 >, SETUP_ARGS, Butterworth_LowPass_8 );
  TestFilter( Iir::Butterworth::LowPass< 9 >, SETUP_ARGS, Butterworth_LowPass_9 );
  TestFilter( Iir::Butterworth::LowPass< 10 >, SETUP_ARGS, Butterworth_LowPass_10 );
  TestFilter( Iir::Butterworth::LowPass< 11 >, SETUP_ARGS, Butterworth_LowPass_11 );
  TestFilter( Iir::Butterworth::LowPass< 12 >, SETUP_ARGS, Butterworth_LowPass_12 );
  TestFilter( Iir::Butterworth::LowPass< 13 >, SETUP_ARGS, Butterworth_LowPass_13 );
  TestFilter( Iir::Butterworth::LowPass< 14 >, SETUP_ARGS, Butterworth_LowPass_14 );
  TestFilter( Iir::Butterworth::LowPass< 15 >, SETUP_ARGS, Butterworth_LowPass_15 );
  TestFilter( Iir::Butterworth::LowPass< 16 >, SETUP_ARGS, Butterworth_LowPass_16 );
  TestFilter( Iir::Butterworth::HighPass< 1 >, SETUP_ARGS, Butterworth_HighPass_1 );
  TestFilter( Iir::Butterworth::HighPass< 2 >, SETUP_ARGS, Butterworth_HighPass_2 );
  TestFilter( Iir::Butterworth::HighPass< 3 >, SETUP_ARGS, Butterworth_HighPass_3 );
  TestFilter( Iir::Butterworth::HighPass< 4 >, SETUP_ARGS, Butterworth_HighPass_4 );
  TestFilter( Iir::Butterworth::HighPass< 5 >, SETUP_ARGS, Butterworth_HighPass_5 );
  TestFilter( Iir::Butterworth::HighPass< 6 >, SETUP_ARGS, Butterworth_HighPass_6 );
  TestFilter( Iir::Butterworth::HighPass< 7 >, SETUP_ARGS, Butterworth_HighPass_7 );
  TestFilter( Iir::Butterworth::HighPass< 8 >, SETUP_ARGS, Butterworth_HighPass_8 );
  TestFilter( Iir::Butterworth::HighPass< 9 >, SETUP_ARGS, Butterworth_HighPass_9 );
  TestFilter( Iir::Butterworth::HighPass< 10 >, SETUP_ARGS, Butterworth_HighPass_10 );
  TestFilter( Iir::Butterworth::HighPass< 11 >, SETUP_ARGS, Butterworth_HighPass_11 );
  TestFilter( Iir::Butterworth::HighPass< 12 >, SETUP_ARGS, Butterworth_HighPass_12 );
  TestFilter( Iir::Butterworth::HighPass< 13 >, SETUP_ARGS, Butterworth_HighPass_13 );
  TestFilter( Iir::Butterworth::HighPass< 14 >, SETUP_ARGS, Butterworth_HighPass_14 );
  TestFilter( Iir::Butterworth::HighPass< 15 >, SETUP_ARGS, Butterworth_HighPass_15 );
  TestFilter( Iir::Butterworth::HighPass< 16 >, SETUP_ARGS, Butterworth_HighPass_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency
  TestFilter( Iir::Butterworth::BandPass< 1 >, SETUP_ARGS, Butterworth_BandPass_1 );
  TestFilter( Iir::Butterworth::BandPass< 2 >, SETUP_ARGS, Butterworth_BandPass_2 );
  TestFilter( Iir::Butterworth::BandPass< 3 >, SETUP_ARGS, Butterworth_BandPass_3 );
  TestFilter( Iir::Butterworth::BandPass< 4 >, SETUP_ARGS, Butterworth_BandPass_4 );
  TestFilter( Iir::Butterworth::BandPass< 5 >, SETUP_ARGS, Butterworth_BandPass_5 );
  TestFilter( Iir::Butterworth::BandPass< 6 >, SETUP_ARGS, Butterworth_BandPass_6 );
  TestFilter( Iir::Butterworth::BandPass< 7 >, SETUP_ARGS, Butterworth_BandPass_7 );
  TestFilter( Iir::Butterworth::BandPass< 8 >, SETUP_ARGS, Butterworth_BandPass_8 );
  TestFilter( Iir::Butterworth::BandPass< 9 >, SETUP_ARGS, Butterworth_BandPass_9 );
  TestFilter( Iir::Butterworth::BandPass< 10 >, SETUP_ARGS, Butterworth_BandPass_10 );
  TestFilter( Iir::Butterworth::BandPass< 11 >, SETUP_ARGS, Butterworth_BandPass_11 );
  TestFilter( Iir::Butterworth::BandPass< 12 >, SETUP_ARGS, Butterworth_BandPass_12 );
  TestFilter( Iir::Butterworth::BandPass< 13 >, SETUP_ARGS, Butterworth_BandPass_13 );
  TestFilter( Iir::Butterworth::BandPass< 14 >, SETUP_ARGS, Butterworth_BandPass_14 );
  TestFilter( Iir::Butterworth::BandPass< 15 >, SETUP_ARGS, Butterworth_BandPass_15 );
  TestFilter( Iir::Butterworth::BandPass< 16 >, SETUP_ARGS, Butterworth_BandPass_16 );
  TestFilter( Iir::Butterworth::BandStop< 1 >, SETUP_ARGS, Butterworth_BandStop_1 );
  TestFilter( Iir::Butterworth::BandStop< 2 >, SETUP_ARGS, Butterworth_BandStop_2 );
  TestFilter( Iir::Butterworth::BandStop< 3 >, SETUP_ARGS, Butterworth_BandStop_3 );
  TestFilter( Iir::Butterworth::BandStop< 4 >, SETUP_ARGS, Butterworth_BandStop_4 );
  TestFilter( Iir::Butterworth::BandStop< 5 >, SETUP_ARGS, Butterworth_BandStop_5 );
  TestFilter( Iir::Butterworth::BandStop< 6 >, SETUP_ARGS, Butterworth_BandStop_6 );
  TestFilter( Iir::Butterworth::BandStop< 7 >, SETUP_ARGS, Butterworth_BandStop_7 );
  TestFilter( Iir::Butterworth::BandStop< 8 >, SETUP_ARGS, Butterworth_BandStop_8 );
  TestFilter( Iir::Butterworth::BandStop< 9 >, SETUP_ARGS, Butterworth_BandStop_9 );
  TestFilter( Iir::Butterworth::BandStop< 10 >, SETUP_ARGS, Butterworth_BandStop_10 );
  TestFilter( Iir::Butterworth::BandStop< 11 >, SETUP_ARGS, Butterworth_BandStop_11 );
  TestFilter( Iir::Butterworth::BandStop< 12 >, SETUP_ARGS, Butterworth_BandStop_12 );
  TestFilter( Iir::Butterworth::BandStop< 13 >, SETUP_ARGS, Butterworth_BandStop_13 );
  TestFilter( Iir::Butterworth::BandStop< 14 >, SETUP_ARGS, Butterworth_BandStop_14 );
  TestFilter( Iir::Butterworth::BandStop< 15 >, SETUP_ARGS, Butterworth_BandStop_15 );
  TestFilter( Iir::Butterworth::BandStop< 16 >, SETUP_ARGS, Butterworth_BandStop_16 );
#define SETUP_ARGS cutoffFrequency, gain
  TestFilter( Iir::Butterworth::LowShelf< 1 >, SETUP_ARGS, Butterworth_LowShelf_1 );
  TestFilter( Iir::Butterworth::LowShelf< 2 >, SETUP_ARGS, Butterworth_LowShelf_2 );
  TestFilter( Iir::Butterworth::LowShelf< 3 >, SETUP_ARGS, Butterworth_LowShelf_3 );
  TestFilter( Iir::Butterworth::LowShelf< 4 >, SETUP_ARGS, Butterworth_LowShelf_4 );
  TestFilter( Iir::Butterworth::LowShelf< 5 >, SETUP_ARGS, Butterworth_LowShelf_5 );
  TestFilter( Iir::Butterworth::LowShelf< 6 >, SETUP_ARGS, Butterworth_LowShelf_6 );
  TestFilter( Iir::Butterworth::LowShelf< 7 >, SETUP_ARGS, Butterworth_LowShelf_7 );
  TestFilter( Iir::Butterworth::LowShelf< 8 >, SETUP_ARGS, Butterworth_LowShelf_8 );
  TestFilter( Iir::Butterworth::LowShelf< 9 >, SETUP_ARGS, Butterworth_LowShelf_9 );
  TestFilter( Iir::Butterworth::LowShelf< 10 >, SETUP_ARGS, Butterworth_LowShelf_10 );
  TestFilter( Iir::Butterworth::LowShelf< 11 >, SETUP_ARGS, Butterworth_LowShelf_11 );
  TestFilter( Iir::Butterworth::LowShelf< 12 >, SETUP_ARGS, Butterworth_LowShelf_12 );
  TestFilter( Iir::Butterworth::LowShelf< 13 >, SETUP_ARGS, Butterworth_LowShelf_13 );
  TestFilter( Iir::Butterworth::LowShelf< 14 >, SETUP_ARGS, Butterworth_LowShelf_14 );
  TestFilter( Iir::Butterworth::LowShelf< 15 >, SETUP_ARGS, Butterworth_LowShelf_15 );
  TestFilter( Iir::Butterworth::LowShelf< 16 >, SETUP_ARGS, Butterworth_LowShelf_16 );
  TestFilter( Iir::Butterworth::HighShelf< 1 >, SETUP_ARGS, Butterworth_HighShelf_1 );
  TestFilter( Iir::Butterworth::HighShelf< 2 >, SETUP_ARGS, Butterworth_HighShelf_2 );
  TestFilter( Iir::Butterworth::HighShelf< 3 >, SETUP_ARGS, Butterworth_HighShelf_3 );
  TestFilter( Iir::Butterworth::HighShelf< 4 >, SETUP_ARGS, Butterworth_HighShelf_4 );
  TestFilter( Iir::Butterworth::HighShelf< 5 >, SETUP_ARGS, Butterworth_HighShelf_5 );
  TestFilter( Iir::Butterworth::HighShelf< 6 >, SETUP_ARGS, Butterworth_HighShelf_6 );
  TestFilter( Iir::Butterworth::HighShelf< 7 >, SETUP_ARGS, Butterworth_HighShelf_7 );
  TestFilter( Iir::Butterworth::HighShelf< 8 >, SETUP_ARGS, Butterworth_HighShelf_8 );
  TestFilter( Iir::Butterworth::HighShelf< 9 >, SETUP_ARGS, Butterworth_HighShelf_9 );
  TestFilter( Iir::Butterworth::HighShelf< 10 >, SETUP_ARGS, Butterworth_HighShelf_10 );
  TestFilter( Iir::Butterworth::HighShelf< 11 >, SETUP_ARGS, Butterworth_HighShelf_11 );
  TestFilter( Iir::Butterworth::HighShelf< 12 >, SETUP_ARGS, Butterworth_HighShelf_12 );
  TestFilter( Iir::Butterworth::HighShelf< 13 >, SETUP_ARGS, Butterworth_HighShelf_13 );
  TestFilter( Iir::Butterworth::HighShelf< 14 >, SETUP_ARGS, Butterworth_HighShelf_14 );
  TestFilter( Iir::Butterworth::HighShelf< 15 >, SETUP_ARGS, Butterworth_HighShelf_15 );
  TestFilter( Iir::Butterworth::HighShelf< 16 >, SETUP_ARGS, Butterworth_HighShelf_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency, gain
  TestFilter( Iir::Butterworth::BandShelf< 1 >, SETUP_ARGS, Butterworth_BandShelf_1 );
  TestFilter( Iir::Butterworth::BandShelf< 2 >, SETUP_ARGS, Butterworth_BandShelf_2 );
  TestFilter( Iir::Butterworth::BandShelf< 3 >, SETUP_ARGS, Butterworth_BandShelf_3 );
  TestFilter( Iir::Butterworth::BandShelf< 4 >, SETUP_ARGS, Butterworth_BandShelf_4 );
  TestFilter( Iir::Butterworth::BandShelf< 5 >, SETUP_ARGS, Butterworth_BandShelf_5 );
  TestFilter( Iir::Butterworth::BandShelf< 6 >, SETUP_ARGS, Butterworth_BandShelf_6 );
  TestFilter( Iir::Butterworth::BandShelf< 7 >, SETUP_ARGS, Butterworth_BandShelf_7 );
  TestFilter( Iir::Butterworth::BandShelf< 8 >, SETUP_ARGS, Butterworth_BandShelf_8 );
  TestFilter( Iir::Butterworth::BandShelf< 9 >, SETUP_ARGS, Butterworth_BandShelf_9 );
  TestFilter( Iir::Butterworth::BandShelf< 10 >, SETUP_ARGS, Butterworth_BandShelf_10 );
  TestFilter( Iir::Butterworth::BandShelf< 11 >, SETUP_ARGS, Butterworth_BandShelf_11 );
  TestFilter( Iir::Butterworth::BandShelf< 12 >, SETUP_ARGS, Butterworth_BandShelf_12 );
  TestFilter( Iir::Butterworth::BandShelf< 13 >, SETUP_ARGS, Butterworth_BandShelf_13 );
  TestFilter( Iir::Butterworth::BandShelf< 14 >, SETUP_ARGS, Butterworth_BandShelf_14 );
  TestFilter( Iir::Butterworth::BandShelf< 15 >, SETUP_ARGS, Butterworth_BandShelf_15 );
  TestFilter( Iir::Butterworth::BandShelf< 16 >, SETUP_ARGS, Butterworth_BandShelf_16 );

#define SETUP_ARGS cutoffFrequency, passband_ripple_in_dB
  TestFilter( Iir::ChebyshevI::LowPass< 1 >, SETUP_ARGS, ChebyshevI_LowPass_1 );
  TestFilter( Iir::ChebyshevI::LowPass< 2 >, SETUP_ARGS, ChebyshevI_LowPass_2 );
  TestFilter( Iir::ChebyshevI::LowPass< 3 >, SETUP_ARGS, ChebyshevI_LowPass_3 );
  TestFilter( Iir::ChebyshevI::LowPass< 4 >, SETUP_ARGS, ChebyshevI_LowPass_4 );
  TestFilter( Iir::ChebyshevI::LowPass< 5 >, SETUP_ARGS, ChebyshevI_LowPass_5 );
  TestFilter( Iir::ChebyshevI::LowPass< 6 >, SETUP_ARGS, ChebyshevI_LowPass_6 );
  TestFilter( Iir::ChebyshevI::LowPass< 7 >, SETUP_ARGS, ChebyshevI_LowPass_7 );
  TestFilter( Iir::ChebyshevI::LowPass< 8 >, SETUP_ARGS, ChebyshevI_LowPass_8 );
  TestFilter( Iir::ChebyshevI::LowPass< 9 >, SETUP_ARGS, ChebyshevI_LowPass_9 );
  TestFilter( Iir::ChebyshevI::LowPass< 10 >, SETUP_ARGS, ChebyshevI_LowPass_10 );
  TestFilter( Iir::ChebyshevI::LowPass< 11 >, SETUP_ARGS, ChebyshevI_LowPass_11 );
  TestFilter( Iir::ChebyshevI::LowPass< 12 >, SETUP_ARGS, ChebyshevI_LowPass_12 );
  TestFilter( Iir::ChebyshevI::LowPass< 13 >, SETUP_ARGS, ChebyshevI_LowPass_13 );
  TestFilter( Iir::ChebyshevI::LowPass< 14 >, SETUP_ARGS, ChebyshevI_LowPass_14 );
  TestFilter( Iir::ChebyshevI::LowPass< 15 >, SETUP_ARGS, ChebyshevI_LowPass_15 );
  TestFilter( Iir::ChebyshevI::LowPass< 16 >, SETUP_ARGS, ChebyshevI_LowPass_16 );
  TestFilter( Iir::ChebyshevI::HighPass< 1 >, SETUP_ARGS, ChebyshevI_HighPass_1 );
  TestFilter( Iir::ChebyshevI::HighPass< 2 >, SETUP_ARGS, ChebyshevI_HighPass_2 );
  TestFilter( Iir::ChebyshevI::HighPass< 3 >, SETUP_ARGS, ChebyshevI_HighPass_3 );
  TestFilter( Iir::ChebyshevI::HighPass< 4 >, SETUP_ARGS, ChebyshevI_HighPass_4 );
  TestFilter( Iir::ChebyshevI::HighPass< 5 >, SETUP_ARGS, ChebyshevI_HighPass_5 );
  TestFilter( Iir::ChebyshevI::HighPass< 6 >, SETUP_ARGS, ChebyshevI_HighPass_6 );
  TestFilter( Iir::ChebyshevI::HighPass< 7 >, SETUP_ARGS, ChebyshevI_HighPass_7 );
  TestFilter( Iir::ChebyshevI::HighPass< 8 >, SETUP_ARGS, ChebyshevI_HighPass_8 );
  TestFilter( Iir::ChebyshevI::HighPass< 9 >, SETUP_ARGS, ChebyshevI_HighPass_9 );
  TestFilter( Iir::ChebyshevI::HighPass< 10 >, SETUP_ARGS, ChebyshevI_HighPass_10 );
  TestFilter( Iir::ChebyshevI::HighPass< 11 >, SETUP_ARGS, ChebyshevI_HighPass_11 );
  TestFilter( Iir::ChebyshevI::HighPass< 12 >, SETUP_ARGS, ChebyshevI_HighPass_12 );
  TestFilter( Iir::ChebyshevI::HighPass< 13 >, SETUP_ARGS, ChebyshevI_HighPass_13 );
  TestFilter( Iir::ChebyshevI::HighPass< 14 >, SETUP_ARGS, ChebyshevI_HighPass_14 );
  TestFilter( Iir::ChebyshevI::HighPass< 15 >, SETUP_ARGS, ChebyshevI_HighPass_15 );
  TestFilter( Iir::ChebyshevI::HighPass< 16 >, SETUP_ARGS, ChebyshevI_HighPass_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency, passband_ripple_in_dB
  TestFilter( Iir::ChebyshevI::BandPass< 1 >, SETUP_ARGS, ChebyshevI_BandPass_1 );
  TestFilter( Iir::ChebyshevI::BandPass< 2 >, SETUP_ARGS, ChebyshevI_BandPass_2 );
  TestFilter( Iir::ChebyshevI::BandPass< 3 >, SETUP_ARGS, ChebyshevI_BandPass_3 );
  TestFilter( Iir::ChebyshevI::BandPass< 4 >, SETUP_ARGS, ChebyshevI_BandPass_4 );
  TestFilter( Iir::ChebyshevI::BandPass< 5 >, SETUP_ARGS, ChebyshevI_BandPass_5 );
  TestFilter( Iir::ChebyshevI::BandPass< 6 >, SETUP_ARGS, ChebyshevI_BandPass_6 );
  TestFilter( Iir::ChebyshevI::BandPass< 7 >, SETUP_ARGS, ChebyshevI_BandPass_7 );
  TestFilter( Iir::ChebyshevI::BandPass< 8 >, SETUP_ARGS, ChebyshevI_BandPass_8 );
  TestFilter( Iir::ChebyshevI::BandPass< 9 >, SETUP_ARGS, ChebyshevI_BandPass_9 );
  TestFilter( Iir::ChebyshevI::BandPass< 10 >, SETUP_ARGS, ChebyshevI_BandPass_10 );
  TestFilter( Iir::ChebyshevI::BandPass< 11 >, SETUP_ARGS, ChebyshevI_BandPass_11 );
  TestFilter( Iir::ChebyshevI::BandPass< 12 >, SETUP_ARGS, ChebyshevI_BandPass_12 );
  TestFilter( Iir::ChebyshevI::BandPass< 13 >, SETUP_ARGS, ChebyshevI_BandPass_13 );
  TestFilter( Iir::ChebyshevI::BandPass< 14 >, SETUP_ARGS, ChebyshevI_BandPass_14 );
  TestFilter( Iir::ChebyshevI::BandPass< 15 >, SETUP_ARGS, ChebyshevI_BandPass_15 );
  TestFilter( Iir::ChebyshevI::BandPass< 16 >, SETUP_ARGS, ChebyshevI_BandPass_16 );
  TestFilter( Iir::ChebyshevI::BandStop< 1 >, SETUP_ARGS, ChebyshevI_BandStop_1 );
  TestFilter( Iir::ChebyshevI::BandStop< 2 >, SETUP_ARGS, ChebyshevI_BandStop_2 );
  TestFilter( Iir::ChebyshevI::BandStop< 3 >, SETUP_ARGS, ChebyshevI_BandStop_3 );
  TestFilter( Iir::ChebyshevI::BandStop< 4 >, SETUP_ARGS, ChebyshevI_BandStop_4 );
  TestFilter( Iir::ChebyshevI::BandStop< 5 >, SETUP_ARGS, ChebyshevI_BandStop_5 );
  TestFilter( Iir::ChebyshevI::BandStop< 6 >, SETUP_ARGS, ChebyshevI_BandStop_6 );
  TestFilter( Iir::ChebyshevI::BandStop< 7 >, SETUP_ARGS, ChebyshevI_BandStop_7 );
  TestFilter( Iir::ChebyshevI::BandStop< 8 >, SETUP_ARGS, ChebyshevI_BandStop_8 );
  TestFilter( Iir::ChebyshevI::BandStop< 9 >, SETUP_ARGS, ChebyshevI_BandStop_9 );
  TestFilter( Iir::ChebyshevI::BandStop< 10 >, SETUP_ARGS, ChebyshevI_BandStop_10 );
  TestFilter( Iir::ChebyshevI::BandStop< 11 >, SETUP_ARGS, ChebyshevI_BandStop_11 );
  TestFilter( Iir::ChebyshevI::BandStop< 12 >, SETUP_ARGS, ChebyshevI_BandStop_12 );
  TestFilter( Iir::ChebyshevI::BandStop< 13 >, SETUP_ARGS, ChebyshevI_BandStop_13 );
  TestFilter( Iir::ChebyshevI::BandStop< 14 >, SETUP_ARGS, ChebyshevI_BandStop_14 );
  TestFilter( Iir::ChebyshevI::BandStop< 15 >, SETUP_ARGS, ChebyshevI_BandStop_15 );
  TestFilter( Iir::ChebyshevI::BandStop< 16 >, SETUP_ARGS, ChebyshevI_BandStop_16 );
#define SETUP_ARGS cutoffFrequency, gain, passband_ripple_in_dB
  TestFilter( Iir::ChebyshevI::LowShelf< 1 >, SETUP_ARGS, ChebyshevI_LowShelf_1 );
  TestFilter( Iir::ChebyshevI::LowShelf< 2 >, SETUP_ARGS, ChebyshevI_LowShelf_2 );
  TestFilter( Iir::ChebyshevI::LowShelf< 3 >, SETUP_ARGS, ChebyshevI_LowShelf_3 );
  TestFilter( Iir::ChebyshevI::LowShelf< 4 >, SETUP_ARGS, ChebyshevI_LowShelf_4 );
  TestFilter( Iir::ChebyshevI::LowShelf< 5 >, SETUP_ARGS, ChebyshevI_LowShelf_5 );
  TestFilter( Iir::ChebyshevI::LowShelf< 6 >, SETUP_ARGS, ChebyshevI_LowShelf_6 );
  TestFilter( Iir::ChebyshevI::LowShelf< 7 >, SETUP_ARGS, ChebyshevI_LowShelf_7 );
  TestFilter( Iir::ChebyshevI::LowShelf< 8 >, SETUP_ARGS, ChebyshevI_LowShelf_8 );
  TestFilter( Iir::ChebyshevI::LowShelf< 9 >, SETUP_ARGS, ChebyshevI_LowShelf_9 );
  TestFilter( Iir::ChebyshevI::LowShelf< 10 >, SETUP_ARGS, ChebyshevI_LowShelf_10 );
  TestFilter( Iir::ChebyshevI::LowShelf< 11 >, SETUP_ARGS, ChebyshevI_LowShelf_11 );
  TestFilter( Iir::ChebyshevI::LowShelf< 12 >, SETUP_ARGS, ChebyshevI_LowShelf_12 );
  TestFilter( Iir::ChebyshevI::LowShelf< 13 >, SETUP_ARGS, ChebyshevI_LowShelf_13 );
  TestFilter( Iir::ChebyshevI::LowShelf< 14 >, SETUP_ARGS, ChebyshevI_LowShelf_14 );
  TestFilter( Iir::ChebyshevI::LowShelf< 15 >, SETUP_ARGS, ChebyshevI_LowShelf_15 );
  TestFilter( Iir::ChebyshevI::LowShelf< 16 >, SETUP_ARGS, ChebyshevI_LowShelf_16 );
  TestFilter( Iir::ChebyshevI::HighShelf< 1 >, SETUP_ARGS, ChebyshevI_HighShelf_1 );
  TestFilter( Iir::ChebyshevI::HighShelf< 2 >, SETUP_ARGS, ChebyshevI_HighShelf_2 );
  TestFilter( Iir::ChebyshevI::HighShelf< 3 >, SETUP_ARGS, ChebyshevI_HighShelf_3 );
  TestFilter( Iir::ChebyshevI::HighShelf< 4 >, SETUP_ARGS, ChebyshevI_HighShelf_4 );
  TestFilter( Iir::ChebyshevI::HighShelf< 5 >, SETUP_ARGS, ChebyshevI_HighShelf_5 );
  TestFilter( Iir::ChebyshevI::HighShelf< 6 >, SETUP_ARGS, ChebyshevI_HighShelf_6 );
  TestFilter( Iir::ChebyshevI::HighShelf< 7 >, SETUP_ARGS, ChebyshevI_HighShelf_7 );
  TestFilter( Iir::ChebyshevI::HighShelf< 8 >, SETUP_ARGS, ChebyshevI_HighShelf_8 );
  TestFilter( Iir::ChebyshevI::HighShelf< 9 >, SETUP_ARGS, ChebyshevI_HighShelf_9 );
  TestFilter( Iir::ChebyshevI::HighShelf< 10 >, SETUP_ARGS, ChebyshevI_HighShelf_10 );
  TestFilter( Iir::ChebyshevI::HighShelf< 11 >, SETUP_ARGS, ChebyshevI_HighShelf_11 );
  TestFilter( Iir::ChebyshevI::HighShelf< 12 >, SETUP_ARGS, ChebyshevI_HighShelf_12 );
  TestFilter( Iir::ChebyshevI::HighShelf< 13 >, SETUP_ARGS, ChebyshevI_HighShelf_13 );
  TestFilter( Iir::ChebyshevI::HighShelf< 14 >, SETUP_ARGS, ChebyshevI_HighShelf_14 );
  TestFilter( Iir::ChebyshevI::HighShelf< 15 >, SETUP_ARGS, ChebyshevI_HighShelf_15 );
  TestFilter( Iir::ChebyshevI::HighShelf< 16 >, SETUP_ARGS, ChebyshevI_HighShelf_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency, gain, passband_ripple_in_dB
  TestFilter( Iir::ChebyshevI::BandShelf< 1 >, SETUP_ARGS, ChebyshevI_BandShelf_1 );
  TestFilter( Iir::ChebyshevI::BandShelf< 2 >, SETUP_ARGS, ChebyshevI_BandShelf_2 );
  TestFilter( Iir::ChebyshevI::BandShelf< 3 >, SETUP_ARGS, ChebyshevI_BandShelf_3 );
  TestFilter( Iir::ChebyshevI::BandShelf< 4 >, SETUP_ARGS, ChebyshevI_BandShelf_4 );
  TestFilter( Iir::ChebyshevI::BandShelf< 5 >, SETUP_ARGS, ChebyshevI_BandShelf_5 );
  TestFilter( Iir::ChebyshevI::BandShelf< 6 >, SETUP_ARGS, ChebyshevI_BandShelf_6 );
  TestFilter( Iir::ChebyshevI::BandShelf< 7 >, SETUP_ARGS, ChebyshevI_BandShelf_7 );
  TestFilter( Iir::ChebyshevI::BandShelf< 8 >, SETUP_ARGS, ChebyshevI_BandShelf_8 );
  TestFilter( Iir::ChebyshevI::BandShelf< 9 >, SETUP_ARGS, ChebyshevI_BandShelf_9 );
  TestFilter( Iir::ChebyshevI::BandShelf< 10 >, SETUP_ARGS, ChebyshevI_BandShelf_10 );
  TestFilter( Iir::ChebyshevI::BandShelf< 11 >, SETUP_ARGS, ChebyshevI_BandShelf_11 );
  TestFilter( Iir::ChebyshevI::BandShelf< 12 >, SETUP_ARGS, ChebyshevI_BandShelf_12 );
  TestFilter( Iir::ChebyshevI::BandShelf< 13 >, SETUP_ARGS, ChebyshevI_BandShelf_13 );
  TestFilter( Iir::ChebyshevI::BandShelf< 14 >, SETUP_ARGS, ChebyshevI_BandShelf_14 );
  TestFilter( Iir::ChebyshevI::BandShelf< 15 >, SETUP_ARGS, ChebyshevI_BandShelf_15 );
  TestFilter( Iir::ChebyshevI::BandShelf< 16 >, SETUP_ARGS, ChebyshevI_BandShelf_16 );

#define SETUP_ARGS cutoffFrequency, stopband_ripple_in_dB
  TestFilter( Iir::ChebyshevII::LowPass< 1 >, SETUP_ARGS, ChebyshevII_LowPass_1 );
  TestFilter( Iir::ChebyshevII::LowPass< 2 >, SETUP_ARGS, ChebyshevII_LowPass_2 );
  TestFilter( Iir::ChebyshevII::LowPass< 3 >, SETUP_ARGS, ChebyshevII_LowPass_3 );
  TestFilter( Iir::ChebyshevII::LowPass< 4 >, SETUP_ARGS, ChebyshevII_LowPass_4 );
  TestFilter( Iir::ChebyshevII::LowPass< 5 >, SETUP_ARGS, ChebyshevII_LowPass_5 );
  TestFilter( Iir::ChebyshevII::LowPass< 6 >, SETUP_ARGS, ChebyshevII_LowPass_6 );
  TestFilter( Iir::ChebyshevII::LowPass< 7 >, SETUP_ARGS, ChebyshevII_LowPass_7 );
  TestFilter( Iir::ChebyshevII::LowPass< 8 >, SETUP_ARGS, ChebyshevII_LowPass_8 );
  TestFilter( Iir::ChebyshevII::LowPass< 9 >, SETUP_ARGS, ChebyshevII_LowPass_9 );
  TestFilter( Iir::ChebyshevII::LowPass< 10 >, SETUP_ARGS, ChebyshevII_LowPass_10 );
  TestFilter( Iir::ChebyshevII::LowPass< 11 >, SETUP_ARGS, ChebyshevII_LowPass_11 );
  TestFilter( Iir::ChebyshevII::LowPass< 12 >, SETUP_ARGS, ChebyshevII_LowPass_12 );
  TestFilter( Iir::ChebyshevII::LowPass< 13 >, SETUP_ARGS, ChebyshevII_LowPass_13 );
  TestFilter( Iir::ChebyshevII::LowPass< 14 >, SETUP_ARGS, ChebyshevII_LowPass_14 );
  TestFilter( Iir::ChebyshevII::LowPass< 15 >, SETUP_ARGS, ChebyshevII_LowPass_15 );
  TestFilter( Iir::ChebyshevII::LowPass< 16 >, SETUP_ARGS, ChebyshevII_LowPass_16 );
  TestFilter( Iir::ChebyshevII::HighPass< 1 >, SETUP_ARGS, ChebyshevII_HighPass_1 );
  TestFilter( Iir::ChebyshevII::HighPass< 2 >, SETUP_ARGS, ChebyshevII_HighPass_2 );
  TestFilter( Iir::ChebyshevII::HighPass< 3 >, SETUP_ARGS, ChebyshevII_HighPass_3 );
  TestFilter( Iir::ChebyshevII::HighPass< 4 >, SETUP_ARGS, ChebyshevII_HighPass_4 );
  TestFilter( Iir::ChebyshevII::HighPass< 5 >, SETUP_ARGS, ChebyshevII_HighPass_5 );
  TestFilter( Iir::ChebyshevII::HighPass< 6 >, SETUP_ARGS, ChebyshevII_HighPass_6 );
  TestFilter( Iir::ChebyshevII::HighPass< 7 >, SETUP_ARGS, ChebyshevII_HighPass_7 );
  TestFilter( Iir::ChebyshevII::HighPass< 8 >, SETUP_ARGS, ChebyshevII_HighPass_8 );
  TestFilter( Iir::ChebyshevII::HighPass< 9 >, SETUP_ARGS, ChebyshevII_HighPass_9 );
  TestFilter( Iir::ChebyshevII::HighPass< 10 >, SETUP_ARGS, ChebyshevII_HighPass_10 );
  TestFilter( Iir::ChebyshevII::HighPass< 11 >, SETUP_ARGS, ChebyshevII_HighPass_11 );
  TestFilter( Iir::ChebyshevII::HighPass< 12 >, SETUP_ARGS, ChebyshevII_HighPass_12 );
  TestFilter( Iir::ChebyshevII::HighPass< 13 >, SETUP_ARGS, ChebyshevII_HighPass_13 );
  TestFilter( Iir::ChebyshevII::HighPass< 14 >, SETUP_ARGS, ChebyshevII_HighPass_14 );
  TestFilter( Iir::ChebyshevII::HighPass< 15 >, SETUP_ARGS, ChebyshevII_HighPass_15 );
  TestFilter( Iir::ChebyshevII::HighPass< 16 >, SETUP_ARGS, ChebyshevII_HighPass_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency, stopband_ripple_in_dB
  TestFilter( Iir::ChebyshevII::BandPass< 1 >, SETUP_ARGS, ChebyshevII_BandPass_1 );
  TestFilter( Iir::ChebyshevII::BandPass< 2 >, SETUP_ARGS, ChebyshevII_BandPass_2 );
  TestFilter( Iir::ChebyshevII::BandPass< 3 >, SETUP_ARGS, ChebyshevII_BandPass_3 );
  TestFilter( Iir::ChebyshevII::BandPass< 4 >, SETUP_ARGS, ChebyshevII_BandPass_4 );
  TestFilter( Iir::ChebyshevII::BandPass< 5 >, SETUP_ARGS, ChebyshevII_BandPass_5 );
  TestFilter( Iir::ChebyshevII::BandPass< 6 >, SETUP_ARGS, ChebyshevII_BandPass_6 );
  TestFilter( Iir::ChebyshevII::BandPass< 7 >, SETUP_ARGS, ChebyshevII_BandPass_7 );
  TestFilter( Iir::ChebyshevII::BandPass< 8 >, SETUP_ARGS, ChebyshevII_BandPass_8 );
  TestFilter( Iir::ChebyshevII::BandPass< 9 >, SETUP_ARGS, ChebyshevII_BandPass_9 );
  TestFilter( Iir::ChebyshevII::BandPass< 10 >, SETUP_ARGS, ChebyshevII_BandPass_10 );
  TestFilter( Iir::ChebyshevII::BandPass< 11 >, SETUP_ARGS, ChebyshevII_BandPass_11 );
  TestFilter( Iir::ChebyshevII::BandPass< 12 >, SETUP_ARGS, ChebyshevII_BandPass_12 );
  TestFilter( Iir::ChebyshevII::BandPass< 13 >, SETUP_ARGS, ChebyshevII_BandPass_13 );
  TestFilter( Iir::ChebyshevII::BandPass< 14 >, SETUP_ARGS, ChebyshevII_BandPass_14 );
  TestFilter( Iir::ChebyshevII::BandPass< 15 >, SETUP_ARGS, ChebyshevII_BandPass_15 );
  TestFilter( Iir::ChebyshevII::BandPass< 16 >, SETUP_ARGS, ChebyshevII_BandPass_16 );
  TestFilter( Iir::ChebyshevII::BandStop< 1 >, SETUP_ARGS, ChebyshevII_BandStop_1 );
  TestFilter( Iir::ChebyshevII::BandStop< 2 >, SETUP_ARGS, ChebyshevII_BandStop_2 );
  TestFilter( Iir::ChebyshevII::BandStop< 3 >, SETUP_ARGS, ChebyshevII_BandStop_3 );
  TestFilter( Iir::ChebyshevII::BandStop< 4 >, SETUP_ARGS, ChebyshevII_BandStop_4 );
  TestFilter( Iir::ChebyshevII::BandStop< 5 >, SETUP_ARGS, ChebyshevII_BandStop_5 );
  TestFilter( Iir::ChebyshevII::BandStop< 6 >, SETUP_ARGS, ChebyshevII_BandStop_6 );
  TestFilter( Iir::ChebyshevII::BandStop< 7 >, SETUP_ARGS, ChebyshevII_BandStop_7 );
  TestFilter( Iir::ChebyshevII::BandStop< 8 >, SETUP_ARGS, ChebyshevII_BandStop_8 );
  TestFilter( Iir::ChebyshevII::BandStop< 9 >, SETUP_ARGS, ChebyshevII_BandStop_9 );
  TestFilter( Iir::ChebyshevII::BandStop< 10 >, SETUP_ARGS, ChebyshevII_BandStop_10 );
  TestFilter( Iir::ChebyshevII::BandStop< 11 >, SETUP_ARGS, ChebyshevII_BandStop_11 );
  TestFilter( Iir::ChebyshevII::BandStop< 12 >, SETUP_ARGS, ChebyshevII_BandStop_12 );
  TestFilter( Iir::ChebyshevII::BandStop< 13 >, SETUP_ARGS, ChebyshevII_BandStop_13 );
  TestFilter( Iir::ChebyshevII::BandStop< 14 >, SETUP_ARGS, ChebyshevII_BandStop_14 );
  TestFilter( Iir::ChebyshevII::BandStop< 15 >, SETUP_ARGS, ChebyshevII_BandStop_15 );
  TestFilter( Iir::ChebyshevII::BandStop< 16 >, SETUP_ARGS, ChebyshevII_BandStop_16 );
#define SETUP_ARGS cutoffFrequency, gain, stopband_ripple_in_dB
  TestFilter( Iir::ChebyshevII::LowShelf< 1 >, SETUP_ARGS, ChebyshevII_LowShelf_1 );
  TestFilter( Iir::ChebyshevII::LowShelf< 2 >, SETUP_ARGS, ChebyshevII_LowShelf_2 );
  TestFilter( Iir::ChebyshevII::LowShelf< 3 >, SETUP_ARGS, ChebyshevII_LowShelf_3 );
  TestFilter( Iir::ChebyshevII::LowShelf< 4 >, SETUP_ARGS, ChebyshevII_LowShelf_4 );
  TestFilter( Iir::ChebyshevII::LowShelf< 5 >, SETUP_ARGS, ChebyshevII_LowShelf_5 );
  TestFilter( Iir::ChebyshevII::LowShelf< 6 >, SETUP_ARGS, ChebyshevII_LowShelf_6 );
  TestFilter( Iir::ChebyshevII::LowShelf< 7 >, SETUP_ARGS, ChebyshevII_LowShelf_7 );
  TestFilter( Iir::ChebyshevII::LowShelf< 8 >, SETUP_ARGS, ChebyshevII_LowShelf_8 );
  TestFilter( Iir::ChebyshevII::LowShelf< 9 >, SETUP_ARGS, ChebyshevII_LowShelf_9 );
  TestFilter( Iir::ChebyshevII::LowShelf< 10 >, SETUP_ARGS, ChebyshevII_LowShelf_10 );
  TestFilter( Iir::ChebyshevII::LowShelf< 11 >, SETUP_ARGS, ChebyshevII_LowShelf_11 );
  TestFilter( Iir::ChebyshevII::LowShelf< 12 >, SETUP_ARGS, ChebyshevII_LowShelf_12 );
  TestFilter( Iir::ChebyshevII::LowShelf< 13 >, SETUP_ARGS, ChebyshevII_LowShelf_13 );
  TestFilter( Iir::ChebyshevII::LowShelf< 14 >, SETUP_ARGS, ChebyshevII_LowShelf_14 );
  TestFilter( Iir::ChebyshevII::LowShelf< 15 >, SETUP_ARGS, ChebyshevII_LowShelf_15 );
  TestFilter( Iir::ChebyshevII::LowShelf< 16 >, SETUP_ARGS, ChebyshevII_LowShelf_16 );
  TestFilter( Iir::ChebyshevII::HighShelf< 1 >, SETUP_ARGS, ChebyshevII_HighShelf_1 );
  TestFilter( Iir::ChebyshevII::HighShelf< 2 >, SETUP_ARGS, ChebyshevII_HighShelf_2 );
  TestFilter( Iir::ChebyshevII::HighShelf< 3 >, SETUP_ARGS, ChebyshevII_HighShelf_3 );
  TestFilter( Iir::ChebyshevII::HighShelf< 4 >, SETUP_ARGS, ChebyshevII_HighShelf_4 );
  TestFilter( Iir::ChebyshevII::HighShelf< 5 >, SETUP_ARGS, ChebyshevII_HighShelf_5 );
  TestFilter( Iir::ChebyshevII::HighShelf< 6 >, SETUP_ARGS, ChebyshevII_HighShelf_6 );
  TestFilter( Iir::ChebyshevII::HighShelf< 7 >, SETUP_ARGS, ChebyshevII_HighShelf_7 );
  TestFilter( Iir::ChebyshevII::HighShelf< 8 >, SETUP_ARGS, ChebyshevII_HighShelf_8 );
  TestFilter( Iir::ChebyshevII::HighShelf< 9 >, SETUP_ARGS, ChebyshevII_HighShelf_9 );
  TestFilter( Iir::ChebyshevII::HighShelf< 10 >, SETUP_ARGS, ChebyshevII_HighShelf_10 );
  TestFilter( Iir::ChebyshevII::HighShelf< 11 >, SETUP_ARGS, ChebyshevII_HighShelf_11 );
  TestFilter( Iir::ChebyshevII::HighShelf< 12 >, SETUP_ARGS, ChebyshevII_HighShelf_12 );
  TestFilter( Iir::ChebyshevII::HighShelf< 13 >, SETUP_ARGS, ChebyshevII_HighShelf_13 );
  TestFilter( Iir::ChebyshevII::HighShelf< 14 >, SETUP_ARGS, ChebyshevII_HighShelf_14 );
  TestFilter( Iir::ChebyshevII::HighShelf< 15 >, SETUP_ARGS, ChebyshevII_HighShelf_15 );
  TestFilter( Iir::ChebyshevII::HighShelf< 16 >, SETUP_ARGS, ChebyshevII_HighShelf_16 );
#define SETUP_ARGS cutoffFrequency, widthFrequency, gain, stopband_ripple_in_dB
  TestFilter( Iir::ChebyshevII::BandShelf< 1 >, SETUP_ARGS, ChebyshevII_BandShelf_1 );
  TestFilter( Iir::ChebyshevII::BandShelf< 2 >, SETUP_ARGS, ChebyshevII_BandShelf_2 );
  TestFilter( Iir::ChebyshevII::BandShelf< 3 >, SETUP_ARGS, ChebyshevII_BandShelf_3 );
  TestFilter( Iir::ChebyshevII::BandShelf< 4 >, SETUP_ARGS, ChebyshevII_BandShelf_4 );
  TestFilter( Iir::ChebyshevII::BandShelf< 5 >, SETUP_ARGS, ChebyshevII_BandShelf_5 );
  TestFilter( Iir::ChebyshevII::BandShelf< 6 >, SETUP_ARGS, ChebyshevII_BandShelf_6 );
  TestFilter( Iir::ChebyshevII::BandShelf< 7 >, SETUP_ARGS, ChebyshevII_BandShelf_7 );
  TestFilter( Iir::ChebyshevII::BandShelf< 8 >, SETUP_ARGS, ChebyshevII_BandShelf_8 );
  TestFilter( Iir::ChebyshevII::BandShelf< 9 >, SETUP_ARGS, ChebyshevII_BandShelf_9 );
  TestFilter( Iir::ChebyshevII::BandShelf< 10 >, SETUP_ARGS, ChebyshevII_BandShelf_10 );
  TestFilter( Iir::ChebyshevII::BandShelf< 11 >, SETUP_ARGS, ChebyshevII_BandShelf_11 );
  TestFilter( Iir::ChebyshevII::BandShelf< 12 >, SETUP_ARGS, ChebyshevII_BandShelf_12 );
  TestFilter( Iir::ChebyshevII::BandShelf< 13 >, SETUP_ARGS, ChebyshevII_BandShelf_13 );
  TestFilter( Iir::ChebyshevII::BandShelf< 14 >, SETUP_ARGS, ChebyshevII_BandShelf_14 );
  TestFilter( Iir::ChebyshevII::BandShelf< 15 >, SETUP_ARGS, ChebyshevII_BandShelf_15 );
  TestFilter( Iir::ChebyshevII::BandShelf< 16 >, SETUP_ARGS, ChebyshevII_BandShelf_16 );

#define SETUP_ARGS cutoffFrequency, q_factor
  TestFilter( Iir::RBJ::LowPass, SETUP_ARGS, RBJ_LowPass );
  TestFilter( Iir::RBJ::HighPass, SETUP_ARGS, RBJ_HighPass );
#define SETUP_ARGS cutoffFrequency, widthOctaves
  TestFilter( Iir::RBJ::BandPass1, SETUP_ARGS, RBJ_BandPass1 );
  TestFilter( Iir::RBJ::BandPass2, SETUP_ARGS, RBJ_BandPass2 );
  TestFilter( Iir::RBJ::BandStop, SETUP_ARGS, RBJ_BandStop );
  #define SETUP_ARGS cutoffFrequency, (1.0 + (q_factor * 19))
  TestFilter( Iir::RBJ::IIRNotch, SETUP_ARGS, RBJ_BandStop );
  #define SETUP_ARGS cutoffFrequency, gain, slope
  TestFilter( Iir::RBJ::LowShelf, SETUP_ARGS, RBJ_LowShelf );
  TestFilter( Iir::RBJ::HighShelf, SETUP_ARGS, RBJ_HighShelf );
#define SETUP_ARGS cutoffFrequency, gain, widthOctaves
  TestFilter( Iir::RBJ::BandShelf, SETUP_ARGS, RBJ_BandShelf );
#define SETUP_ARGS cutoffFrequency, q_factor
  TestFilter( Iir::RBJ::AllPass, SETUP_ARGS, RBJ_AllPass );

}

void set_up_logger( bool print_on_std_out ) {
  std::string const logger_file = "spdlog.log";
  std::string const logger_pattern = "[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%n] [%l] %v";

  std::shared_ptr< spdlog::sinks::stdout_color_sink_mt > console_sink = std::make_shared< spdlog::sinks::stdout_color_sink_mt >();
  console_sink->set_level( print_on_std_out ? spdlog::level::level_enum::trace : spdlog::level::level_enum::off );
  std::shared_ptr< spdlog::sinks::basic_file_sink_mt > file_sink = std::make_shared< spdlog::sinks::basic_file_sink_mt >( logger_file, true );
  file_sink->set_level( spdlog::level::level_enum::trace );

  spdlog::sinks_init_list truncated_sink_list = { file_sink, console_sink };
  spdlogger main_logger = std::make_shared< spdlog::logger >( "main", truncated_sink_list.begin(), truncated_sink_list.end() );
  main_logger->set_level( spdlog::level::level_enum::trace );
  main_logger->flush_on( spdlog::level::level_enum::trace );
  main_logger->set_pattern( logger_pattern );
  spdlog::register_logger( main_logger );
  spdlog::set_default_logger( main_logger );
}

int main( int argc, char** argv ) {
  set_up_logger( true );

  std::vector< std::string > args = parse_args( argc, argv );
  for( size_t i = 0; i < args.size(); i++ ) {
    spdlog::debug( "arg {}: {}", i, args[i] );
  }
  if( args.size() <= 1 ) {
    spdlog::error( "usage: program 'folder/path/of/video/project'" );
    return 1;
  }
  std::filesystem::path project_folder( args[1] );
  spdlog::debug( "project_folder: {}", project_folder.string() );

  filter_response_test( project_folder );
  return 0;
}
