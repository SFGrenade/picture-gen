#define DR_WAV_IMPLEMENTATION
#include <Iir.h>
#include <cairo.h>
#include <dr_wav.h>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "surface.h"
#include "utils.h"

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

struct AudioData {
  uint32_t channels;
  uint32_t sample_rate;
  size_t total_pcm_frame_count;
  float* sample_data = nullptr;
  float* processed_sample_data = nullptr;
  double duration;

  ~AudioData() {
    if( sample_data )
      drwav_free( sample_data, NULL );
    if( processed_sample_data )
      delete[] processed_sample_data;
  }
};

struct ThreadInputData {
  size_t i;
  size_t amount_output_frames;
  std::filesystem::path project_temp_pictureset_picture_path;
  size_t pcm_frame_offset_from;
  size_t pcm_frame_offset_to;
  AudioData* audio_data_ptr = nullptr;
  cairo_surface_t* project_common_bg_surface = nullptr;
  cairo_surface_t* project_common_circle_surface = nullptr;
  cairo_surface_t* project_art_surface = nullptr;
  cairo_surface_t* static_text_surface = nullptr;
};

void create_lowpass_for_audio_data( AudioData* audio_data_ptr,
                                    std::filesystem::path const& project_folder,
                                    float lowpass_cutoff = 100.0,
                                    float highpass_cutoff = 20.0 ) {
  size_t sample_amount = audio_data_ptr->total_pcm_frame_count * audio_data_ptr->channels;

  if( audio_data_ptr->processed_sample_data ) {
    delete[] audio_data_ptr->processed_sample_data;
  }
  audio_data_ptr->processed_sample_data = new float[sample_amount];
  memset( audio_data_ptr->processed_sample_data, 0, sample_amount * sizeof( audio_data_ptr->processed_sample_data[0] ) );

  const int lowpass_order = 4;
  const int highpass_order = 4;

  Iir::Butterworth::LowPass< lowpass_order > lowpass;
  lowpass.setup( audio_data_ptr->sample_rate, lowpass_cutoff );
  Iir::Butterworth::HighPass< highpass_order > highpass;
  highpass.setup( audio_data_ptr->sample_rate, highpass_cutoff );

  // fill buf1 into audio_data_ptr
  for( size_t i = 0; i < sample_amount; i++ ) {
    float lowpassed_sample = lowpass.filter( audio_data_ptr->sample_data[i] );
    float highpassed_sample = highpass.filter( lowpassed_sample );
    audio_data_ptr->processed_sample_data[i] = highpassed_sample;
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
      drwav_uint64 framesWritten = drwav_write_pcm_frames( &dw_obj, audio_data_ptr->total_pcm_frame_count, audio_data_ptr->processed_sample_data );
      std::cout << fmt::format( "framesWritten: {}", framesWritten ) << std::endl;
      drwav_uninit( &dw_obj );
    }
  }
}

void save_surface( cairo_surface_t* surface, std::filesystem::path const& filepath ) {
  cairo_status_t status = cairo_surface_write_to_png( surface, filepath.string().c_str() );
  if( status != cairo_status_t::CAIRO_STATUS_SUCCESS ) {
    std::cerr << fmt::format( "error in save_surface: {}", cairo_status_to_string( status ) ) << std::endl;
  }
}

void draw_samples_on_surface( cairo_surface_t* surface, AudioData* audio_data_ptr, size_t pcm_frame_offset_from, size_t pcm_frame_offset_to ) {
  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  cairo_t* cr = cairo_create( surface );
  cairo_save( cr );

  double surface_w = cairo_image_surface_get_width( surface );
  double surface_h = cairo_image_surface_get_height( surface );

  double middle_y = surface_h / 2;
  double frame_duration = pcm_frame_offset_to - pcm_frame_offset_from + 1;
  double prev_x = 0;
  double prev_y = middle_y;
  double x = 0;
  double y = middle_y;
  // std::cout << fmt::format( "draw_samples_on_surface - {} frames for {} pixels", frame_duration, surface_w ) << std::endl;

  cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
  // default is 2
  cairo_set_line_width( cr, 3 );

  for( int c = audio_data_ptr->channels - 1; c >= 0; c-- ) {
    prev_x = 0;
    prev_y = middle_y;

    double red = std::pow( 0.5, static_cast< double >( c ) );
    cairo_set_source_rgb( cr, red, 0.0, 0.0 );

    for( size_t i = 0; i < frame_duration; i++ ) {
      x = std::round( static_cast< float >( surface_w ) * ( static_cast< float >( i ) / static_cast< float >( frame_duration - 1 ) ) );

      // range: -1.0 to 1.0
      float sample = audio_data_ptr->sample_data[( ( pcm_frame_offset_from + i ) * audio_data_ptr->channels ) + c];

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
}

void draw_freqs_on_surface( cairo_surface_t* surface, AudioData* audio_data_ptr, size_t pcm_frame_offset_from, size_t pcm_frame_offset_to ) {
  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );
  // todo: fixme: add content
}

void thread_run( std::vector< ThreadInputData > const& inputs ) {
  int32_t render_frame_width = 1920;
  int32_t render_frame_height = 1080;

  std::vector< ThreadInputData > inputs_copy = inputs;
  cairo_surface_t* dynamic_waves_surface = surface_create_size( 917, 387 );
  cairo_surface_t* dynamic_freqs_surface = surface_create_size( 917, 387 );
  std::cout << fmt::format( "dynamic_waves_surface: {}", static_cast< void* >( dynamic_waves_surface ) ) << std::endl;
  std::cout << fmt::format( "dynamic_freqs_surface: {}", static_cast< void* >( dynamic_freqs_surface ) ) << std::endl;

  cairo_rectangle_t project_common_circle_dest_rect;
  cairo_rectangle_t dynamic_waves_dest_rect;
  cairo_rectangle_t dynamic_freqs_dest_rect;

  dynamic_waves_dest_rect.x = 979;
  dynamic_waves_dest_rect.y = 449;
  dynamic_waves_dest_rect.width = cairo_image_surface_get_width( dynamic_waves_surface );
  dynamic_waves_dest_rect.height = cairo_image_surface_get_height( dynamic_waves_surface );
  dynamic_freqs_dest_rect.x = 979;
  dynamic_freqs_dest_rect.y = 24;
  dynamic_freqs_dest_rect.width = cairo_image_surface_get_width( dynamic_freqs_surface );
  dynamic_freqs_dest_rect.height = cairo_image_surface_get_height( dynamic_freqs_surface );

  for( auto const& input_data : inputs_copy ) {
    cairo_surface_t* frame_surface_to_save = surface_create_size( render_frame_width, render_frame_height );
    surface_fill( frame_surface_to_save, 0.0, 0.0, 0.0, 1.0 );

    // range: 0.0 - 1.0
    float min_sample_value = 0.0;
    float max_sample_value = 0.0;
    for( size_t i = input_data.pcm_frame_offset_from; i <= input_data.pcm_frame_offset_to; i++ ) {
      for( size_t c = 0; c <= input_data.audio_data_ptr->channels; c++ ) {
        size_t sample_index = ( i * input_data.audio_data_ptr->channels ) + c;
        float sample = input_data.audio_data_ptr->processed_sample_data[sample_index];
        min_sample_value = std::min( min_sample_value, sample );
        max_sample_value = std::max( max_sample_value, sample );
      }
    }

    double sound_intensity = ( std::abs( min_sample_value ) + std::abs( max_sample_value ) ) / 2.0f;
    double circle_intensity_scale = 0.5;
    double colour_displace_intensity_scale = 0.2;

    project_common_circle_dest_rect.width = static_cast< double >( cairo_image_surface_get_width( input_data.project_common_circle_surface ) )
                                            * ( 0.5 + ( sound_intensity * circle_intensity_scale ) );
    project_common_circle_dest_rect.height = project_common_circle_dest_rect.width;
    project_common_circle_dest_rect.x
        = 114.5 + ( ( 1804.5 - 114.5 ) * ( static_cast< double >( input_data.i ) / static_cast< double >( input_data.amount_output_frames ) ) )
          - ( project_common_circle_dest_rect.width / 2.0 );
    project_common_circle_dest_rect.y = 964.5 - ( project_common_circle_dest_rect.height / 2.0 );

    draw_samples_on_surface( dynamic_waves_surface, input_data.audio_data_ptr, input_data.pcm_frame_offset_from, input_data.pcm_frame_offset_to );
    draw_freqs_on_surface( dynamic_freqs_surface, input_data.audio_data_ptr, input_data.pcm_frame_offset_from, input_data.pcm_frame_offset_to );

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

    // todo: fixme: use surface_shake_and_blit
    surface_shake_and_blit( copied_bg_surface, frame_surface_to_save, ( colour_displace_intensity_scale * sound_intensity ), true );
    // surface_blit( copied_bg_surface, frame_surface_to_save, 0, 0, render_frame_width, render_frame_height );
    cairo_surface_destroy( copied_bg_surface );

    // // put art on canvas, shakily
    // todo: fixme: use surface_shake_and_blit
    surface_shake_and_blit( input_data.project_art_surface, frame_surface_to_save, ( colour_displace_intensity_scale * sound_intensity ) );
    // surface_blit( input_data.project_art_surface, frame_surface_to_save, 0, 0, render_frame_width, render_frame_height );

    // // put title on canvas, shakily
    // todo: fixme: use surface_shake_and_blit
    surface_shake_and_blit( input_data.static_text_surface, frame_surface_to_save, ( colour_displace_intensity_scale * sound_intensity ) );
    // surface_blit( input_data.static_text_surface, frame_surface_to_save, 0, 0, render_frame_width, render_frame_height );

    // // save canvas
    save_surface( frame_surface_to_save, input_data.project_temp_pictureset_picture_path );
    cairo_surface_destroy( frame_surface_to_save );
  }

  cairo_surface_destroy( dynamic_freqs_surface );
  cairo_surface_destroy( dynamic_waves_surface );
}

int main( int argc, char** argv ) {
  std::vector< std::string > args = parse_args( argc, argv );
  for( size_t i = 0; i < args.size(); i++ ) {
    std::cout << fmt::format( "arg {}: {}", i, args[i] ) << std::endl;
  }
  if( args.size() <= 1 ) {
    std::cerr << fmt::format( "usage: program 'folder/path/of/video/project'" ) << std::endl;
    args.push_back( "C:\\Users\\SFG\\Documents\\Video Project\\_SFGrenade - Decimation\\" );
    // return 1;
  }
  std::filesystem::path project_folder( args[1] );
  std::cout << fmt::format( "project_folder: {}", project_folder.string() ) << std::endl;

  std::filesystem::path project_common_bg_path( project_folder / ".." / "bg.old.png" );
  std::filesystem::path project_common_circle_path( project_folder / ".." / "circle.png" );
  std::filesystem::path project_art_path( project_folder / "art.png" );
  std::filesystem::path project_audio_path( project_folder / "audio.wav" );
  std::filesystem::path project_title_path( project_folder / "title.txt" );
  if( !std::filesystem::is_regular_file( project_common_bg_path ) ) {
    std::cerr << fmt::format( "error: file '{}' doesn't exist!", project_common_bg_path.string() ) << std::endl;
    return 1;
  }
  if( !std::filesystem::is_regular_file( project_common_circle_path ) ) {
    std::cerr << fmt::format( "error: file '{}' doesn't exist!", project_common_circle_path.string() ) << std::endl;
    return 1;
  }
  if( !std::filesystem::is_regular_file( project_art_path ) ) {
    std::cerr << fmt::format( "error: file '{}' doesn't exist!", project_art_path.string() ) << std::endl;
    return 1;
  }
  if( !std::filesystem::is_regular_file( project_audio_path ) ) {
    std::cerr << fmt::format( "error: file '{}' doesn't exist!", project_audio_path.string() ) << std::endl;
    return 1;
  }
  if( !std::filesystem::is_regular_file( project_title_path ) ) {
    std::cerr << fmt::format( "error: file '{}' doesn't exist!", project_title_path.string() ) << std::endl;
    return 1;
  }

  std::filesystem::path project_temp_pictureset_folder( project_folder / "__pictures" );
  if( !std::filesystem::is_directory( project_temp_pictureset_folder ) ) {
    std::filesystem::create_directory( project_temp_pictureset_folder );
  }

  double fps = 60.0;
  int32_t width = 1920;
  int32_t height = 1080;

  size_t wanted_threads = 16;

  AudioData project_audio_data;
  project_audio_data.sample_data = drwav_open_file_and_read_pcm_frames_f32( project_audio_path.string().c_str(),
                                                                            &project_audio_data.channels,
                                                                            &project_audio_data.sample_rate,
                                                                            &project_audio_data.total_pcm_frame_count,
                                                                            nullptr );
  std::cout << fmt::format( "project_audio_data.channels: {}", project_audio_data.channels ) << std::endl;
  std::cout << fmt::format( "project_audio_data.sample_rate: {}", project_audio_data.sample_rate ) << std::endl;
  std::cout << fmt::format( "project_audio_data.total_pcm_frame_count: {}", project_audio_data.total_pcm_frame_count ) << std::endl;
  project_audio_data.duration = static_cast< double >( project_audio_data.total_pcm_frame_count ) / static_cast< double >( project_audio_data.sample_rate );
  std::cout << fmt::format( "project_audio_data.duration: {}", project_audio_data.duration ) << std::endl;

  std::cout << fmt::format( "create_lowpass_for_audio_data..." ) << std::endl;
  create_lowpass_for_audio_data( &project_audio_data, project_folder, 100.0, 20.0 );

  size_t amount_output_frames = static_cast< size_t >( std::ceil( project_audio_data.duration * fps ) );
  std::cout << fmt::format( "amount_output_frames: {}", amount_output_frames ) << std::endl;
  double pcmFramesPerOutputFrame = static_cast< double >( project_audio_data.total_pcm_frame_count ) / static_cast< double >( amount_output_frames );
  std::cout << fmt::format( "pcmFramesPerOutputFrame: {}", pcmFramesPerOutputFrame ) << std::endl;

  cairo_surface_t* project_common_bg_surface = surface_load_file( project_common_bg_path );
  cairo_surface_t* project_common_circle_surface = surface_load_file( project_common_circle_path );
  cairo_surface_t* project_art_surface = surface_load_file_into_overlay( project_art_path, width, height, 24, 24, 917, 812 );
  std::cout << fmt::format( "project_common_bg_surface: {}", static_cast< void* >( project_common_bg_surface ) ) << std::endl;
  std::cout << fmt::format( "project_common_circle_surface: {}", static_cast< void* >( project_common_circle_surface ) ) << std::endl;
  std::cout << fmt::format( "project_art_surface: {}", static_cast< void* >( project_art_surface ) ) << std::endl;
  cairo_surface_t* static_text_surface = surface_render_text_into_overlay( project_title_path, width, height, 979, 24, 917, 387 );
  std::cout << fmt::format( "static_text_surface: {}", static_cast< void* >( static_text_surface ) ) << std::endl;

  std::vector< std::vector< ThreadInputData > > thread_input_lists;
  thread_input_lists.reserve( wanted_threads );
  for( int i = 0; i < wanted_threads; i++ ) {
    std::vector< ThreadInputData > vec;
    vec.reserve( ( amount_output_frames / wanted_threads ) + 1 );
    thread_input_lists.push_back( vec );
  }
  double pcm_frame_offset = 0.0;
  for( size_t i = 0; i < amount_output_frames; i++ ) {
    ThreadInputData input_data;
    input_data.i = i;
    input_data.amount_output_frames = amount_output_frames;
    input_data.project_temp_pictureset_picture_path = project_temp_pictureset_folder / fmt::format( "{:0>6}.png", i );
    input_data.pcm_frame_offset_from = std::max< size_t >( 0, static_cast< size_t >( std::floor( pcm_frame_offset ) ) );
    input_data.pcm_frame_offset_to
        = std::min< size_t >( project_audio_data.total_pcm_frame_count - 1, static_cast< size_t >( std::ceil( pcm_frame_offset + pcmFramesPerOutputFrame ) ) );
    input_data.audio_data_ptr = &project_audio_data;
    input_data.project_common_bg_surface = project_common_bg_surface;
    input_data.project_common_circle_surface = project_common_circle_surface;
    input_data.project_art_surface = project_art_surface;
    input_data.static_text_surface = static_text_surface;
    // std::cout << fmt::format( "output frame from sample {} to {}", input_data.pcm_frame_offset_from, input_data.pcm_frame_offset_to ) << std::endl;

    size_t thread_index = i % wanted_threads;
    thread_input_lists[thread_index].push_back( input_data );

    pcm_frame_offset += pcmFramesPerOutputFrame;
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

  return 0;
}
