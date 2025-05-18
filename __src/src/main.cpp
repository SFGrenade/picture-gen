#define DR_WAV_IMPLEMENTATION
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <atomic>
#include <chrono>
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
  float* sample_data;
  double duration;
};

struct ThreadInputData {
  size_t i;
  size_t amount_output_frames;
  std::filesystem::path project_temp_pictureset_picture_path;
  size_t pcm_frame_offset_from;
  size_t pcm_frame_offset_to;
  AudioData* audio_data_ptr;
  SDL_Surface* project_common_bg_surface;
  SDL_Surface* project_common_circle_surface;
  SDL_Surface* project_art_surface;
  SDL_Surface* static_text_surface;
};

// #define SAVE_LOCKS_MAX 16
// std::atomic_int save_locks = 0;
void save_surface( SDL_Surface* surface, std::filesystem::path const& filepath ) {
  // while (save_locks >= SAVE_LOCKS_MAX) {
  //   std::this_thread::sleep_for(std::chrono::microseconds(1));
  // }
  // save_locks++;
  if( 0 != IMG_SavePNG( surface, filepath.string().c_str() ) ) {
    std::cerr << fmt::format( "error: IMG_SavePNG: {}", SDL_GetError() ) << std::endl;
  }
  // save_locks--;
}

void draw_samples_on_surface( SDL_Surface* surface, AudioData* audio_data_ptr, size_t pcm_frame_offset_from, size_t pcm_frame_offset_to ) {
  set_colour_on_surface( surface, 0x000000FF );
  size_t middle_y = surface->h / 2;
  size_t frame_duration = pcm_frame_offset_to - pcm_frame_offset_from + 1;
  size_t prev_x = 0;
  size_t prev_y = middle_y;
  size_t x = 0;
  size_t y = middle_y;
  std::cout << fmt::format( "draw_samples_on_surface - {} frames for {} pixels", frame_duration, surface->w ) << std::endl;
  for( int c = audio_data_ptr->channels - 1; c >= 0; c-- ) {
    prev_x = 0;
    prev_y = middle_y;
    for( size_t i = 0; i <= ( pcm_frame_offset_to - pcm_frame_offset_from ); i++ ) {
      x = static_cast< size_t >( std::round( static_cast< float >( surface->w ) * ( static_cast< float >( i ) / static_cast< float >( frame_duration ) ) ) );

      uint8_t red = 0xFF >> c;
      uint32_t colour = ( static_cast< uint32_t >( red ) << ( 3 * 8 ) ) | ( static_cast< uint32_t >( 0 ) << ( 2 * 8 ) )
                        | ( static_cast< uint32_t >( 0 ) << ( 1 * 8 ) ) | ( static_cast< uint32_t >( 0xFF ) << ( 0 * 8 ) );

      // range: -1.0 to 1.0
      float sample = audio_data_ptr->sample_data[( ( pcm_frame_offset_from + i ) * audio_data_ptr->channels ) + c];

      y = static_cast< size_t >( std::round( static_cast< float >( middle_y ) + ( static_cast< float >( middle_y ) * sample ) ) );

      set_pixel(surface, x, y, colour);

      prev_x = x;
      prev_y = y;
    }
  }
}

void draw_freqs_on_surface( SDL_Surface* surface, AudioData* audio_data_ptr, size_t pcm_frame_offset_from, size_t pcm_frame_offset_to ) {
  set_colour_on_surface( surface, 0x000000FF );
}

void thread_run( std::vector< ThreadInputData > const& inputs ) {
  std::vector< ThreadInputData > inputs_copy = inputs;
  SDL_Surface* dynamic_waves_surface = create_rgb_surface( 917, 387 );
  SDL_Surface* dynamic_freqs_surface = create_rgb_surface( 917, 387 );
  std::cout << fmt::format( "dynamic_waves_surface: {}", static_cast< void* >( dynamic_waves_surface ) ) << std::endl;
  std::cout << fmt::format( "dynamic_freqs_surface: {}", static_cast< void* >( dynamic_freqs_surface ) ) << std::endl;

  SDL_Rect project_common_circle_dest_rect;
  SDL_Rect dynamic_waves_dest_rect;
  SDL_Rect dynamic_freqs_dest_rect;

  dynamic_waves_dest_rect.x = 979;
  dynamic_waves_dest_rect.y = 449;
  dynamic_waves_dest_rect.w = dynamic_waves_surface->w;
  dynamic_waves_dest_rect.h = dynamic_waves_surface->h;
  dynamic_freqs_dest_rect.x = 979;
  dynamic_freqs_dest_rect.y = 24;
  dynamic_freqs_dest_rect.w = dynamic_freqs_surface->w;
  dynamic_freqs_dest_rect.h = dynamic_freqs_surface->h;

  for( auto const& input_data : inputs_copy ) {
    SDL_Surface* frame_surface_to_save = create_rgb_surface( 1920, 1080 );
    set_colour_on_surface( frame_surface_to_save, 0x000000FF );

    // range: 0.0 - 1.0
    double sound_intensity = 0.0;
    double circle_intensity_scale = 0.5;
    double colour_displace_intensity_scale = 0.2;

    project_common_circle_dest_rect.w = static_cast< int32_t >( static_cast< double >( input_data.project_common_circle_surface->w )
                                                                * ( 0.5 + ( sound_intensity * circle_intensity_scale ) ) );
    project_common_circle_dest_rect.h = project_common_circle_dest_rect.w;
    project_common_circle_dest_rect.x = static_cast< int32_t >(
        114.5 + ( ( 1804.5 - 114.5 ) * ( static_cast< double >( input_data.i ) / static_cast< double >( input_data.amount_output_frames ) ) )
        - ( static_cast< double >( project_common_circle_dest_rect.w ) / 2.0 ) );
    project_common_circle_dest_rect.y = static_cast< int32_t >( 964.5 - ( static_cast< double >( project_common_circle_dest_rect.h ) / 2.0 ) );

    draw_samples_on_surface( dynamic_waves_surface, input_data.audio_data_ptr, input_data.pcm_frame_offset_from, input_data.pcm_frame_offset_to );
    draw_freqs_on_surface( dynamic_freqs_surface, input_data.audio_data_ptr, input_data.pcm_frame_offset_from, input_data.pcm_frame_offset_to );

    // put art on canvas, shakily
    shake_and_blit_surface( input_data.project_art_surface, frame_surface_to_save, ( colour_displace_intensity_scale * sound_intensity ) );

    // make copy of bg, put waves and freqs and circle on copy, put copy on canvas, shakily
    SDL_Surface* copied_bg_surface = copy_surface( input_data.project_common_bg_surface );
    if( 0 != SDL_BlitScaled( dynamic_waves_surface, nullptr, copied_bg_surface, &dynamic_waves_dest_rect ) ) {
      std::cerr << fmt::format( "error: dynamic_waves_surface SDL_BlitScaled: {}", SDL_GetError() ) << std::endl;
    }
    if( 0 != SDL_BlitScaled( dynamic_freqs_surface, nullptr, copied_bg_surface, &dynamic_freqs_dest_rect ) ) {
      std::cerr << fmt::format( "error: dynamic_freqs_surface SDL_BlitScaled: {}", SDL_GetError() ) << std::endl;
    }
    if( 0 != SDL_BlitScaled( input_data.project_common_circle_surface, nullptr, copied_bg_surface, &project_common_circle_dest_rect ) ) {
      std::cerr << fmt::format( "error: project_common_circle_surface SDL_BlitScaled: {}", SDL_GetError() ) << std::endl;
    }
    shake_and_blit_surface( copied_bg_surface, frame_surface_to_save, ( colour_displace_intensity_scale * sound_intensity ), true );
    SDL_FreeSurface( copied_bg_surface );

    // save canvas
    save_surface( frame_surface_to_save, input_data.project_temp_pictureset_picture_path );
    SDL_FreeSurface( frame_surface_to_save );
  }

  SDL_FreeSurface( dynamic_freqs_surface );
  SDL_FreeSurface( dynamic_waves_surface );
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
  wanted_threads = 1;

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

  size_t amount_output_frames = static_cast< size_t >( std::ceil( project_audio_data.duration * fps ) );
  std::cout << fmt::format( "amount_output_frames: {}", amount_output_frames ) << std::endl;
  double pcmFramesPerOutputFrame = static_cast< double >( project_audio_data.total_pcm_frame_count ) / static_cast< double >( amount_output_frames );
  std::cout << fmt::format( "pcmFramesPerOutputFrame: {}", pcmFramesPerOutputFrame ) << std::endl;

  // 1920x1080x4
  SDL_Surface* project_common_bg_surface = load_image_and_check_surface( project_common_bg_path );
  SDL_Surface* project_common_circle_surface = load_image_and_check_surface( project_common_circle_path );
  SDL_Surface* project_art_surface = load_image_into_overlay_at( project_art_path, width, height, 24, 24, 917, 812 );
  std::cout << fmt::format( "project_common_bg_surface: {}", static_cast< void* >( project_common_bg_surface ) ) << std::endl;
  std::cout << fmt::format( "project_common_circle_surface: {}", static_cast< void* >( project_common_circle_surface ) ) << std::endl;
  std::cout << fmt::format( "project_art_surface: {}", static_cast< void* >( project_art_surface ) ) << std::endl;
  SDL_Surface* static_text_surface = create_rgb_surface( 917, 387 );
  // todo: fixme: make this text have the text on it
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

  SDL_FreeSurface( static_text_surface );
  SDL_FreeSurface( project_art_surface );
  SDL_FreeSurface( project_common_circle_surface );
  SDL_FreeSurface( project_common_bg_surface );

  return 0;
}
