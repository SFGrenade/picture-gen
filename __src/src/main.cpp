#define DR_WAV_IMPLEMENTATION
#include <Iir.h>
#include <cairo.h>
#include <cmath>
#include <cstdint>
#include <dr_wav.h>
#include <fftw3.h>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <memory>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

#include "fontManager.h"
#include "loggerFactory.h"
#include "surface.h"
#include "utils.h"
#include "window_functions.h"

#define FPS 60.0
#define VIDEO_WIDTH 1920
#define VIDEO_HEIGHT 1080
#define THREAD_COUNT 16
#define FILTER_ORDER 16

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

struct AudioData {
  uint32_t channels;
  uint32_t sample_rate;
  size_t total_pcm_frame_count;
  float* sample_data = nullptr;
  float sample_min = 0.0;
  float sample_max = 0.0;
  float* processed_sample_data = nullptr;
  float processed_sample_min = 0.0;
  float processed_sample_max = 0.0;
  double duration;

  ~AudioData() {
    if( sample_data )
      drwav_free( sample_data, nullptr );
    if( processed_sample_data )
      delete[] processed_sample_data;
  }
};

struct ThreadInputData {
  size_t i;
  size_t amount_output_frames;
  std::filesystem::path project_temp_pictureset_picture_path;
  size_t pcm_frame_offset;
  size_t pcm_frame_count;
  AudioData* audio_data_ptr = nullptr;
  cairo_surface_t* project_common_epilepsy_warning_surface = nullptr;
  cairo_surface_t* project_common_bg_surface = nullptr;
  cairo_surface_t* project_common_circle_surface = nullptr;
  cairo_surface_t* project_art_surface = nullptr;
  cairo_surface_t* static_text_surface = nullptr;
  std::shared_ptr< fftwf_plan_s > fft_plan = nullptr;
  size_t fft_input_size;
  std::shared_ptr< float[] > fft_input = nullptr;
  std::shared_ptr< double[] > fft_window = nullptr;
  size_t fft_output_size;
  std::shared_ptr< fftwf_complex[] > fft_output = nullptr;
};

void create_lowpass_for_audio_data( AudioData* audio_data_ptr,
                                    std::filesystem::path const& project_folder,
                                    float const lowpass_cutoff = 80.0,
                                    float const highpass_cutoff = 20.0 ) {
  spdlogger logger = LoggerFactory::get_logger( "create_lowpass_for_audio_data" );
  logger->trace( "enter: audio_data_ptr: {}, project_folder: {:?}, lowpass_cutoff: {}, highpass_cutoff: {}",
                 static_cast< void* >( audio_data_ptr ),
                 project_folder.string(),
                 lowpass_cutoff,
                 highpass_cutoff );

  size_t sample_amount = audio_data_ptr->total_pcm_frame_count * audio_data_ptr->channels;

  if( audio_data_ptr->processed_sample_data ) {
    delete[] audio_data_ptr->processed_sample_data;
  }
  audio_data_ptr->processed_sample_data = new float[sample_amount];
  memset( audio_data_ptr->processed_sample_data, 0, sample_amount * sizeof( audio_data_ptr->processed_sample_data[0] ) );

  Iir::Butterworth::LowPass< FILTER_ORDER > lowpass;
  Iir::Butterworth::HighPass< FILTER_ORDER > highpass;

  // fill buf1 into audio_data_ptr
  for( size_t c = 0; c < audio_data_ptr->channels; c++ ) {
    lowpass.setup( audio_data_ptr->sample_rate, lowpass_cutoff );
    highpass.setup( audio_data_ptr->sample_rate, highpass_cutoff );
    for( size_t i = 0; i < audio_data_ptr->total_pcm_frame_count; i++ ) {
      size_t sample_index = ( i * audio_data_ptr->channels ) + c;

      audio_data_ptr->sample_min = std::min( audio_data_ptr->sample_min, audio_data_ptr->sample_data[sample_index] );
      audio_data_ptr->sample_max = std::max( audio_data_ptr->sample_max, audio_data_ptr->sample_data[sample_index] );
      audio_data_ptr->sample_min = std::clamp( audio_data_ptr->sample_min, -1.0f, 1.0f );
      audio_data_ptr->sample_max = std::clamp( audio_data_ptr->sample_max, -1.0f, 1.0f );

      float lowpassed_sample = lowpass.filter( audio_data_ptr->sample_data[sample_index] );
      float highpassed_sample = highpass.filter( lowpassed_sample );
      audio_data_ptr->processed_sample_data[sample_index] = highpassed_sample;
      audio_data_ptr->processed_sample_min = std::min( audio_data_ptr->processed_sample_min, audio_data_ptr->processed_sample_data[sample_index] );
      audio_data_ptr->processed_sample_max = std::max( audio_data_ptr->processed_sample_max, audio_data_ptr->processed_sample_data[sample_index] );
      audio_data_ptr->processed_sample_min = std::clamp( audio_data_ptr->processed_sample_min, -1.0f, 1.0f );
      audio_data_ptr->processed_sample_max = std::clamp( audio_data_ptr->processed_sample_max, -1.0f, 1.0f );
    }
  }

  drwav dw_obj;
  drwav_data_format dw_fmt;
  dw_fmt.container = drwav_container::drwav_container_riff;
  dw_fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  dw_fmt.channels = audio_data_ptr->channels;
  dw_fmt.sampleRate = audio_data_ptr->sample_rate;
  dw_fmt.bitsPerSample = 32;
  {
    std::filesystem::path tmp_audio_path( project_folder / fmt::format( "__filtered_{}_{}.wav", lowpass_cutoff, highpass_cutoff ) );
    if( std::filesystem::is_regular_file( tmp_audio_path ) ) {
      std::filesystem::remove( tmp_audio_path );
    }
    if( drwav_init_file_write( &dw_obj, tmp_audio_path.string().c_str(), &dw_fmt, nullptr ) ) {
      size_t framesWritten = drwav_write_pcm_frames( &dw_obj, audio_data_ptr->total_pcm_frame_count, audio_data_ptr->processed_sample_data );
      logger->debug( "framesWritten: {}", framesWritten );
      drwav_uninit( &dw_obj );
    }
  }

  logger->trace( "exit" );
}

void save_surface( cairo_surface_t* surface, std::filesystem::path const& filepath ) {
  spdlogger logger = LoggerFactory::get_logger( "save_surface" );
  logger->trace( "enter: surface: {}, filepath: {:?}", static_cast< void* >( surface ), filepath.string() );

  cairo_status_t status = cairo_surface_write_to_png( surface, filepath.string().c_str() );
  if( status != cairo_status_t::CAIRO_STATUS_SUCCESS ) {
    logger->error( "error in save_surface: {}", cairo_status_to_string( status ) );
  }

  logger->trace( "exit" );
}

void draw_samples_on_surface( cairo_surface_t* surface, AudioData const* audio_data_ptr, size_t const pcm_frame_offset, size_t const pcm_frame_count ) {
  spdlogger logger = LoggerFactory::get_logger( "draw_samples_on_surface" );
  logger->trace( "enter: surface: {}, audio_data_ptr: {}, pcm_frame_offset: {}, pcm_frame_count: {}",
                 static_cast< void* >( surface ),
                 static_cast< void const* >( audio_data_ptr ),
                 pcm_frame_offset,
                 pcm_frame_count );

  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  cairo_t* cr = cairo_create( surface );
  cairo_save( cr );

  double const surface_w = cairo_image_surface_get_width( surface );
  double const surface_h = cairo_image_surface_get_height( surface );

  double const middle_y = surface_h / 2.0;
  double const frame_duration = static_cast< double >( pcm_frame_count );
  double prev_x = 0.0;
  double prev_y = middle_y;
  double x = 0.0;
  double y = middle_y;
  // logger->debug( "draw_samples_on_surface - {} frames for {} pixels", frame_duration, surface_w );

  cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
  // default is 2.0
  cairo_set_line_width( cr, 3.0 );

  for( int c = audio_data_ptr->channels - 1; c >= 0; c-- ) {
    prev_x = 0.0;
    prev_y = middle_y;

    double red = std::pow( 0.5, static_cast< double >( c ) );
    cairo_set_source_rgb( cr, red, 0.0, 0.0 );

    for( size_t i = 0; i < pcm_frame_count; i++ ) {
      x = std::round( static_cast< float >( surface_w ) * ( static_cast< float >( i ) / static_cast< float >( frame_duration - 1 ) ) );

      // range: -1.0 to 1.0
      float sample = 0.0;
      size_t sample_index = ( ( pcm_frame_offset + i ) * audio_data_ptr->channels ) + c;
      if( sample_index < ( audio_data_ptr->total_pcm_frame_count * audio_data_ptr->channels ) ) {
        sample = audio_data_ptr->sample_data[sample_index];
      }

      y = std::round( static_cast< double >( middle_y ) + ( static_cast< double >( middle_y ) * sample ) );

      if( i == 0 ) {
        prev_x = x;
        prev_y = y;
      }
      cairo_move_to( cr, prev_x, prev_y );
      cairo_line_to( cr, x, y );

      prev_x = x;
      prev_y = y;
    }
    cairo_stroke( cr );
  }

  cairo_restore( cr );
  cairo_destroy( cr );

  logger->trace( "exit" );
}

void draw_freqs_on_surface( cairo_surface_t* surface,
                            ThreadInputData const& input_data,
                            AudioData const* audio_data_ptr,
                            size_t const pcm_frame_offset,
                            size_t const pcm_frame_count ) {
  spdlogger logger = LoggerFactory::get_logger( "draw_freqs_on_surface" );
  logger->trace( "enter: surface: {}, audio_data_ptr: {}, pcm_frame_offset: {}, pcm_frame_count: {}",
                 static_cast< void* >( surface ),
                 static_cast< void const* >( audio_data_ptr ),
                 pcm_frame_offset,
                 pcm_frame_count );

  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  if( !surface || !audio_data_ptr || !audio_data_ptr->sample_data ) {
    logger->error( "either of surface, audio_data_ptr, audio_data_ptr->sample_data is nullptr!" );
    return;
  }

  cairo_t* cr = cairo_create( surface );
  cairo_save( cr );

  double const width = cairo_image_surface_get_width( surface );
  double const height = cairo_image_surface_get_height( surface );

  const uint32_t channels = audio_data_ptr->channels;
  const uint32_t sample_rate = audio_data_ptr->sample_rate;
  const float* samples = audio_data_ptr->sample_data;
  const size_t total_frames = audio_data_ptr->total_pcm_frame_count;

  double const min_freq = 20.0;
  double const max_freq = static_cast< double >( sample_rate ) / 2.0;
  double const min_mag_db = -120.0;
  double const max_mag_db = 0.0;

  cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_BUTT );
  // cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
  // // default is 2.0
  // cairo_set_line_width( cr, 3.0 );

  // per channel
  for( int c = ( channels - 1 ); c >= 0; c-- ) {
    double red = std::pow( 0.5, static_cast< double >( c + 1 ) );
    cairo_set_source_rgb( cr, red, 0.0, 0.0 );

    std::memset( input_data.fft_input.get(), 0, input_data.fft_input_size * sizeof( input_data.fft_input[0] ) );

    for( size_t i = 0; i < input_data.fft_input_size; i++ ) {
      float w = input_data.fft_window[i];
      if( ( i < pcm_frame_count ) && ( ( pcm_frame_offset + i ) < total_frames ) ) {
        float sample = samples[( ( pcm_frame_offset + i ) * channels ) + c];
        input_data.fft_input[i] = sample * w;
      }
    }

    fftwf_execute( input_data.fft_plan.get() );

    size_t log_bin_count = 20;
    std::vector< double > log_bin_magnitude_sum( log_bin_count, 0.0 );
    std::vector< size_t > log_bin_counts( log_bin_count, 0 );
    {
      std::function< double( double ) > log_func = []( double f ) { return std::log( f ); };
      // std::function< double( double ) > log_func = []( double f ) { return std::log10( f ); };
      // std::function< double( double ) > log_func = []( double f ) { return f; };

      for( size_t i = 0; i < input_data.fft_output_size; i++ ) {
        double freq = static_cast< double >( i ) * max_freq / double( input_data.fft_output_size );

        double real = input_data.fft_output[i][0];
        double imag = input_data.fft_output[i][1];
        double mag = std::sqrt( real * real + imag * imag ) / double( input_data.fft_input_size );
        double mag_db = 20.0 * std::log10( mag + 1e-6 );

        int32_t bin = static_cast< int32_t >( ( ( log_func( freq ) - log_func( min_freq ) ) / ( log_func( max_freq ) - log_func( min_freq ) ) )
                                              * double( log_bin_count ) );
        // if( bin < 0 ) {
        //   bin = 0;
        // }
        if( bin >= log_bin_count ) {
          bin = log_bin_count - 1;
        }
        if( ( bin < 0 ) || ( bin >= log_bin_count ) ) {
          continue;
        }

        log_bin_magnitude_sum[bin] += mag_db;
        log_bin_counts[bin]++;
      }
    }

    double x = 0.0;
    double y = 0.0;
    double px = x;
    double py = y;
    double line_width = width / double( log_bin_count );
    cairo_set_line_width( cr, line_width * 0.9 );
    for( size_t bin = 0; bin < log_bin_count; bin++ ) {
      if( log_bin_counts[bin] == 0 )
        continue;

      double avg_db = log_bin_magnitude_sum[bin] / double( log_bin_counts[bin] );
      double norm_mag = ( avg_db - min_mag_db ) / ( max_mag_db - min_mag_db );
      norm_mag = std::clamp( norm_mag, 0.0, 1.0 );

      x = double( bin ) / double( log_bin_count ) * width + ( line_width / 2.0 );
      y = height * ( 1.0 - norm_mag );

      cairo_move_to( cr, x, height );
      cairo_line_to( cr, x, y );
      cairo_stroke( cr );
      px = x;
      py = y;
    }
  }

  cairo_restore( cr );
  cairo_destroy( cr );

  logger->trace( "exit" );
}

void thread_run( std::vector< ThreadInputData > inputs ) {
  spdlogger logger = LoggerFactory::get_logger( "thread_run" );
  logger->trace( "enter: inputs: {} items", inputs.size() );

  std::vector< ThreadInputData > inputs_copy = inputs;
  cairo_surface_t* dynamic_waves_surface = surface_create_size( SCALE_WIDTH_FROM_FULLHD( 917 ), SCALE_HEIGHT_FROM_FULLHD( 387 ) );
  cairo_surface_t* dynamic_freqs_surface = surface_create_size( SCALE_WIDTH_FROM_FULLHD( 917 ), SCALE_HEIGHT_FROM_FULLHD( 387 ) );
  logger->debug( "dynamic_waves_surface: {}", static_cast< void* >( dynamic_waves_surface ) );
  logger->debug( "dynamic_freqs_surface: {}", static_cast< void* >( dynamic_freqs_surface ) );

  cairo_rectangle_t project_common_circle_dest_rect;
  cairo_rectangle_t dynamic_waves_dest_rect;
  cairo_rectangle_t dynamic_freqs_dest_rect;

  dynamic_waves_dest_rect.x = SCALE_WIDTH_FROM_FULLHD( 979 );
  dynamic_waves_dest_rect.y = SCALE_HEIGHT_FROM_FULLHD( 449 );
  dynamic_waves_dest_rect.width = cairo_image_surface_get_width( dynamic_waves_surface );
  dynamic_waves_dest_rect.height = cairo_image_surface_get_height( dynamic_waves_surface );
  dynamic_freqs_dest_rect.x = SCALE_WIDTH_FROM_FULLHD( 979 );
  dynamic_freqs_dest_rect.y = SCALE_HEIGHT_FROM_FULLHD( 24 );
  dynamic_freqs_dest_rect.width = cairo_image_surface_get_width( dynamic_freqs_surface );
  dynamic_freqs_dest_rect.height = cairo_image_surface_get_height( dynamic_freqs_surface );

  for( ThreadInputData input_data : inputs_copy ) {
    logger->trace( "computing input {}", input_data.i );
    cairo_surface_t* frame_surface_to_save = surface_create_size( VIDEO_WIDTH, VIDEO_HEIGHT );
    surface_fill( frame_surface_to_save, 0.0, 0.0, 0.0, 1.0 );

    double const epilepsy_warning_visible_seconds = 3.0;
    double const epilepsy_warning_fadeout_seconds = 2.0;
    double epilepsy_warning_alpha = 0.0;
    if( input_data.i < size_t( epilepsy_warning_visible_seconds * FPS ) ) {
      epilepsy_warning_alpha = 1.0;
    } else if( input_data.i >= size_t( ( epilepsy_warning_visible_seconds + epilepsy_warning_fadeout_seconds ) * FPS ) ) {
      epilepsy_warning_alpha = 0.0;
    } else {
      epilepsy_warning_alpha = 1.0 - ( ( ( double( input_data.i ) / FPS ) - epilepsy_warning_visible_seconds ) / epilepsy_warning_fadeout_seconds );
      epilepsy_warning_alpha = std::clamp( epilepsy_warning_alpha, 0.0, 1.0 );
    }

    // range: 0.0 - 1.0
    float min_bass_sample_value = 0.0;
    float max_bass_sample_value = 0.0;
    float min_sound_sample_value = 0.0;
    float max_sound_sample_value = 0.0;
    for( size_t i = 0; i <= input_data.pcm_frame_count; i++ ) {
      for( size_t c = 0; c <= input_data.audio_data_ptr->channels; c++ ) {
        size_t sample_index = ( ( input_data.pcm_frame_offset + i ) * input_data.audio_data_ptr->channels ) + c;
        float bass_sample = 0.0;
        float sound_sample = 0.0;
        if( sample_index < ( input_data.audio_data_ptr->total_pcm_frame_count * input_data.audio_data_ptr->channels ) ) {
          bass_sample = input_data.audio_data_ptr->processed_sample_data[sample_index];
          sound_sample = input_data.audio_data_ptr->sample_data[sample_index];
        }
        bass_sample = std::clamp( bass_sample, -1.0f, 1.0f );
        sound_sample = std::clamp( sound_sample, -1.0f, 1.0f );
        min_bass_sample_value = std::min( min_bass_sample_value, bass_sample );
        max_bass_sample_value = std::max( max_bass_sample_value, bass_sample );
        min_sound_sample_value = std::min( min_sound_sample_value, sound_sample );
        max_sound_sample_value = std::max( max_sound_sample_value, sound_sample );
      }
    }

    double const bass_intensity = ( ( std::abs( min_bass_sample_value ) / std::abs( input_data.audio_data_ptr->processed_sample_min ) )
                                    + ( std::abs( max_bass_sample_value ) / std::abs( input_data.audio_data_ptr->processed_sample_max ) ) )
                                  / 2.0f;
    double const sound_intensity = ( ( std::abs( min_sound_sample_value ) / std::abs( input_data.audio_data_ptr->sample_min ) )
                                     + ( std::abs( max_sound_sample_value ) / std::abs( input_data.audio_data_ptr->sample_max ) ) )
                                   / 2.0f;
    double const circle_intensity_scale = 0.5;
    double const colour_displace_intensity_scale = 0.15;

    project_common_circle_dest_rect.width
        = static_cast< double >( cairo_image_surface_get_width( input_data.project_common_circle_surface ) )
          * ( ( 1.0 - circle_intensity_scale ) + ( ( ( bass_intensity + sound_intensity ) / 2.0 ) * circle_intensity_scale ) );
    project_common_circle_dest_rect.width = project_common_circle_dest_rect.width * ( double( VIDEO_WIDTH ) / 1920.0 );
    project_common_circle_dest_rect.height = project_common_circle_dest_rect.width;
    project_common_circle_dest_rect.x = SCALE_WIDTH_FROM_FULLHD( 114.5 )
                                        + ( ( SCALE_WIDTH_FROM_FULLHD( 1804.5 ) - SCALE_WIDTH_FROM_FULLHD( 114.5 ) )
                                            * ( static_cast< double >( input_data.i ) / static_cast< double >( input_data.amount_output_frames ) ) )
                                        - ( project_common_circle_dest_rect.width / 2.0 );
    project_common_circle_dest_rect.y = SCALE_HEIGHT_FROM_FULLHD( 964.5 ) - ( project_common_circle_dest_rect.height / 2.0 );

    try {
      draw_samples_on_surface( dynamic_waves_surface, input_data.audio_data_ptr, input_data.pcm_frame_offset, input_data.pcm_frame_count );
    } catch( std::exception const& e ) {
      logger->error( "error in draw_samples_on_surface: {}", e.what() );
    }
    try {
      draw_freqs_on_surface( dynamic_freqs_surface, input_data, input_data.audio_data_ptr, input_data.pcm_frame_offset, input_data.pcm_frame_count );
    } catch( std::exception const& e ) {
      logger->error( "error in draw_freqs_on_surface: {}", e.what() );
    }

    // make copy of bg, put waves and freqs and circle on copy, put copy on canvas, shakily
    cairo_surface_t* copied_bg_surface = surface_copy( input_data.project_common_bg_surface );
    surface_blit( dynamic_waves_surface,
                  copied_bg_surface,
                  dynamic_waves_dest_rect.x,
                  dynamic_waves_dest_rect.y,
                  dynamic_waves_dest_rect.width,
                  dynamic_waves_dest_rect.height );
    surface_blit( dynamic_freqs_surface,
                  copied_bg_surface,
                  dynamic_freqs_dest_rect.x,
                  dynamic_freqs_dest_rect.y,
                  dynamic_freqs_dest_rect.width,
                  dynamic_freqs_dest_rect.height );
    surface_blit( input_data.project_common_circle_surface,
                  copied_bg_surface,
                  project_common_circle_dest_rect.x,
                  project_common_circle_dest_rect.y,
                  project_common_circle_dest_rect.width,
                  project_common_circle_dest_rect.height );

    surface_shake_and_blit( copied_bg_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ), true );
    cairo_surface_destroy( copied_bg_surface );

    // put art on canvas, shakily
    surface_shake_and_blit( input_data.project_art_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // put title on canvas, shakily
    surface_shake_and_blit( input_data.static_text_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // put warning on top, with alpha
    surface_blit( input_data.project_common_epilepsy_warning_surface, frame_surface_to_save, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, epilepsy_warning_alpha );

    // save canvas
    save_surface( frame_surface_to_save, input_data.project_temp_pictureset_picture_path );
    cairo_surface_destroy( frame_surface_to_save );
  }

  cairo_surface_destroy( dynamic_freqs_surface );
  cairo_surface_destroy( dynamic_waves_surface );

  logger->trace( "exit" );
}

cairo_surface_t* create_epilepsy_warning( cairo_font_face_t* header_font_face,
                                          cairo_font_face_t* content_font_face,
                                          std::filesystem::path const& epilepsy_warning_path ) {
  spdlogger logger = LoggerFactory::get_logger( "create_epilepsy_warning" );
  logger->trace( "enter: header_font_face: {}, content_font_face: {}, epilepsy_warning_path: {:?}",
                 static_cast< void* >( header_font_face ),
                 static_cast< void* >( content_font_face ),
                 epilepsy_warning_path.string() );

  const size_t width = VIDEO_WIDTH;
  const size_t height = VIDEO_HEIGHT;

  cairo_surface_t* surface = surface_create_size( width, height );

  // drawing the epilepsy warning
  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  cairo_surface_t* text_surface
      = surface_render_text_advanced_into_overlay( header_font_face, content_font_face, epilepsy_warning_path, width, height, 0, 0, width, height );
  surface_blit( text_surface, surface, 0, 0, width, height );

  std::string tmp_str = epilepsy_warning_path.string();
  std::filesystem::path epilepsy_warning_picture_path( replace( tmp_str, ".txt", ".png" ) );
  save_surface( surface, epilepsy_warning_picture_path );

  logger->trace( "exit" );
  return surface;
}

int main( int argc, char** argv ) {
  int32_t const width = VIDEO_WIDTH;
  int32_t const height = VIDEO_HEIGHT;

  LoggerFactory::init( "main.log", false );

  std::vector< std::string > args = parse_args( argc, argv );
  for( size_t i = 0; i < args.size(); i++ ) {
    spdlog::debug( "arg {}: {:?}", i, args[i] );
  }
  if( args.size() <= 1 ) {
    spdlog::error( "usage: program 'folder/path/of/video/project'" );
    LoggerFactory::deinit();
    return 1;
  }

  std::filesystem::path project_folder( args[1] );
  spdlog::debug( "project_folder: {:?}", project_folder.string() );

  FontManager::init( project_folder / ".." / "__fonts" );

  std::filesystem::path project_common_epilepsy_warning_path( project_folder / ".." / "epileptic_warning.txt" );
  std::filesystem::path project_common_bg_path( project_folder / ".." / "bg.old.png" );
  std::filesystem::path project_common_circle_path( project_folder / ".." / "circle.png" );
  std::filesystem::path project_art_path( project_folder / "art.png" );
  std::filesystem::path project_audio_path( project_folder / "audio.wav" );
  std::filesystem::path project_title_path( project_folder / "title.txt" );

  for( auto const& file : std::vector< std::filesystem::path >( { project_common_epilepsy_warning_path,
                                                                  project_common_bg_path,
                                                                  project_common_circle_path,
                                                                  project_art_path,
                                                                  project_audio_path,
                                                                  project_title_path } ) ) {
    if( !std::filesystem::is_regular_file( file ) ) {
      spdlog::error( "error: file {:?} doesn't exist!", file.string() );
      LoggerFactory::deinit();
      return 1;
    }
  }

  std::filesystem::path project_temp_pictureset_folder( project_folder / "__pictures" );
  if( std::filesystem::is_directory( project_temp_pictureset_folder ) ) {
    std::filesystem::remove_all( project_temp_pictureset_folder );
  }
  std::filesystem::create_directory( project_temp_pictureset_folder );

  AudioData project_audio_data;
  project_audio_data.sample_data = drwav_open_file_and_read_pcm_frames_f32( project_audio_path.string().c_str(),
                                                                            &project_audio_data.channels,
                                                                            &project_audio_data.sample_rate,
                                                                            &project_audio_data.total_pcm_frame_count,
                                                                            nullptr );
  spdlog::debug( "project_audio_data.channels: {}", project_audio_data.channels );
  spdlog::debug( "project_audio_data.sample_rate: {}", project_audio_data.sample_rate );
  spdlog::debug( "project_audio_data.total_pcm_frame_count: {}", project_audio_data.total_pcm_frame_count );
  project_audio_data.duration = static_cast< double >( project_audio_data.total_pcm_frame_count ) / static_cast< double >( project_audio_data.sample_rate );
  spdlog::debug( "project_audio_data.duration: {}", project_audio_data.duration );

  spdlog::debug( "create_lowpass_for_audio_data..." );
  create_lowpass_for_audio_data( &project_audio_data, project_folder, 80.0, 20.0 );

  size_t amount_output_frames = static_cast< size_t >( std::ceil( project_audio_data.duration * FPS ) );
  spdlog::debug( "amount_output_frames: {}", amount_output_frames );
  double pcm_frames_per_output_frame = static_cast< double >( project_audio_data.total_pcm_frame_count ) / static_cast< double >( amount_output_frames );
  spdlog::debug( "pcm_frames_per_output_frame: {}", pcm_frames_per_output_frame );

  cairo_surface_t* project_common_epilepsy_warning_surface = create_epilepsy_warning( FontManager::get_font_face( "BarberChop.otf" ),
                                                                                      FontManager::get_font_face( "arial_narrow_7.ttf" ),
                                                                                      project_common_epilepsy_warning_path );
  cairo_surface_t* project_common_bg_surface = surface_load_file( project_common_bg_path );
  cairo_surface_t* project_common_circle_surface = surface_load_file( project_common_circle_path );
  cairo_surface_t* project_art_surface = surface_load_file_into_overlay( project_art_path,
                                                                         width,
                                                                         height,
                                                                         SCALE_WIDTH_FROM_FULLHD( 24 ),
                                                                         SCALE_HEIGHT_FROM_FULLHD( 24 ),
                                                                         SCALE_WIDTH_FROM_FULLHD( 917 ),
                                                                         SCALE_HEIGHT_FROM_FULLHD( 812 ) );
  spdlog::debug( "project_common_epilepsy_warning_surface: {}", static_cast< void* >( project_common_epilepsy_warning_surface ) );
  spdlog::debug( "project_common_bg_surface: {}", static_cast< void* >( project_common_bg_surface ) );
  spdlog::debug( "project_common_circle_surface: {}", static_cast< void* >( project_common_circle_surface ) );
  spdlog::debug( "project_art_surface: {}", static_cast< void* >( project_art_surface ) );
  cairo_surface_t* static_text_surface = surface_render_text_into_overlay( FontManager::get_font_face( "Roboto-Regular.ttf" ),
                                                                           project_title_path,
                                                                           width,
                                                                           height,
                                                                           SCALE_WIDTH_FROM_FULLHD( 979 ),
                                                                           SCALE_HEIGHT_FROM_FULLHD( 24 ),
                                                                           SCALE_WIDTH_FROM_FULLHD( 917 ),
                                                                           SCALE_HEIGHT_FROM_FULLHD( 387 ) );
  spdlog::debug( "static_text_surface: {}", static_cast< void* >( static_text_surface ) );

  std::vector< std::vector< ThreadInputData > > thread_input_lists;
  thread_input_lists.reserve( THREAD_COUNT );
  for( int i = 0; i < THREAD_COUNT; i++ ) {
    std::vector< ThreadInputData > vec;
    vec.reserve( ( amount_output_frames / THREAD_COUNT ) + 1 );
    thread_input_lists.push_back( vec );
  }
  double pcm_frame_offset = 0.0;
  for( size_t i = 0; i < amount_output_frames; i++ ) {
    ThreadInputData input_data;
    input_data.i = i;
    input_data.amount_output_frames = amount_output_frames;
    input_data.project_temp_pictureset_picture_path = project_temp_pictureset_folder / fmt::format( "{}.png", i );
    input_data.pcm_frame_offset = std::max< size_t >( 0, static_cast< size_t >( std::floor( pcm_frame_offset ) ) );
    input_data.pcm_frame_count = static_cast< size_t >( std::ceil( pcm_frames_per_output_frame * 4.0 ) );
    input_data.audio_data_ptr = &project_audio_data;
    input_data.project_common_epilepsy_warning_surface = project_common_epilepsy_warning_surface;
    input_data.project_common_bg_surface = project_common_bg_surface;
    input_data.project_common_circle_surface = project_common_circle_surface;
    input_data.project_art_surface = project_art_surface;
    input_data.static_text_surface = static_text_surface;
    spdlog::debug( "output frame {} from sample {} to {}",
                   input_data.i,
                   input_data.pcm_frame_offset,
                   input_data.pcm_frame_offset + ( input_data.pcm_frame_count - 1 ) );

    size_t thread_index = i % THREAD_COUNT;
    thread_input_lists[thread_index].push_back( input_data );

    pcm_frame_offset += pcm_frames_per_output_frame;
  }

  size_t fft_size = 1;
  while( fft_size < pcm_frames_per_output_frame ) {
    fft_size = fft_size << 1;
  }
  // for( int e = 0; e < 3; e++ ) {
  //   // extra passes of fft size
  //   fft_size = fft_size << 1;
  // }

  for( auto& input_list : thread_input_lists ) {
    size_t fft_input_size = fft_size;
    std::shared_ptr< float[] > fft_input = std::make_shared< float[] >( fft_input_size );

    std::shared_ptr< double[] > fft_window = std::make_shared< double[] >( fft_input_size );
    blackmanharris( fft_window.get(), fft_input_size, false );  // create values for window function

    size_t fft_output_size = fft_size / 2 + 1;
    std::shared_ptr< fftwf_complex[] > fft_output = std::make_shared< fftwf_complex[] >( fft_output_size );
    std::shared_ptr< fftwf_plan_s > fft_plan
        = std::shared_ptr< fftwf_plan_s >( fftwf_plan_dft_r2c_1d( fft_size, fft_input.get(), fft_output.get(), FFTW_EXHAUSTIVE ),
                                           []( fftwf_plan p ) { fftwf_destroy_plan( p ); } );
    if( !fft_plan ) {
      spdlog::error( "failed fftwf_plan_dft_r2c_1d!" );
    }

    for( auto& input_data : input_list ) {
      input_data.fft_input_size = fft_input_size;
      input_data.fft_input = fft_input;
      input_data.fft_window = fft_window;
      input_data.fft_output_size = fft_output_size;
      input_data.fft_output = fft_output;
      input_data.fft_plan = fft_plan;
    }
  }

  std::vector< std::thread > thread_list;
  thread_input_lists.reserve( thread_input_lists.size() );
  for( auto const& input_list : thread_input_lists ) {
    thread_list.emplace_back( thread_run, input_list );
  }
  for( auto& thread : thread_list ) {
    thread.join();
  }

  cairo_surface_destroy( static_text_surface );
  cairo_surface_destroy( project_art_surface );
  cairo_surface_destroy( project_common_circle_surface );
  cairo_surface_destroy( project_common_bg_surface );
  cairo_surface_destroy( project_common_epilepsy_warning_surface );

  FontManager::deinit();

  LoggerFactory::deinit();
  return 0;
}
