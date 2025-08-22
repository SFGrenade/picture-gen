#include "regularVideoGenerator.h"

#include <Iir.h>
#include <memory>

#include "_dr_wav.h"
#include "_fftw.h"
#include "cairo.h"
#include "fontManager.h"
#include "loggerFactory.h"
#include "surface.h"
#include "utils.h"
#include "window_functions.h"

// #define DO_SAVE_LOWPASS_AUDIO
// #define DO_SAVE_EPILEPSY_WARNING

double const RegularVideoGenerator::FPS = 60.0;
int32_t const RegularVideoGenerator::VIDEO_WIDTH = 1920;
int32_t const RegularVideoGenerator::VIDEO_HEIGHT = 1080;
int32_t const RegularVideoGenerator::IIR_FILTER_ORDER = 16;
double const RegularVideoGenerator::BASS_LP_CUTOFF = 80.0;
double const RegularVideoGenerator::BASS_HP_CUTOFF = 20.0;
/*
  0 => 1024
  1 => 2048
  2 => 4096
  3 => 8192
  4 => 16384
  5 => 32768
*/
uint16_t const RegularVideoGenerator::EXTRA_FFT_SIZE = 3;
/*
  to keep 4096 samples:
  0 => 4.0
  1 => 2.0
  2 => 1.0
  3 => 0.5
  4 => 0.25
  5 => 0.125
*/
double const RegularVideoGenerator::FFT_INPUT_SIZE_MULT = 4.0 * std::pow< double >( 0.5, RegularVideoGenerator::EXTRA_FFT_SIZE );
double const RegularVideoGenerator::EPILEPSY_WARNING_VISIBLE_SECONDS = 3.0;
double const RegularVideoGenerator::EPILEPSY_WARNING_FADEOUT_SECONDS = 2.0;
// uint32_t const RegularVideoGenerator::FFTW_PLAN_FLAGS = FFTW_EXHAUSTIVE;
uint32_t const RegularVideoGenerator::FFTW_PLAN_FLAGS = FFTW_ESTIMATE;
std::string const RegularVideoGenerator::EPILEPSY_WARNING_HEADER_FONT = "BarberChop.otf";
std::string const RegularVideoGenerator::EPILEPSY_WARNING_CONTENT_FONT = "arial_narrow_7.ttf";
double const RegularVideoGenerator::FFT_DISPLAY_MIN_FREQ = 20.0;
double const RegularVideoGenerator::FFT_DISPLAY_MAX_FREQ = 44100.0;
double const RegularVideoGenerator::FFT_DISPLAY_MIN_MAG = -96.0;
double const RegularVideoGenerator::FFT_DISPLAY_MAX_MAG = -10.0;

spdlogger RegularVideoGenerator::logger_ = nullptr;
bool RegularVideoGenerator::is_ready_ = false;
std::filesystem::path RegularVideoGenerator::project_path_;
std::filesystem::path RegularVideoGenerator::common_path_;
std::filesystem::path RegularVideoGenerator::common_epilepsy_warning_path_;
std::filesystem::path RegularVideoGenerator::common_bg_path_;
std::filesystem::path RegularVideoGenerator::common_circle_path_;
std::filesystem::path RegularVideoGenerator::project_art_path_;
std::filesystem::path RegularVideoGenerator::project_audio_path_;
std::filesystem::path RegularVideoGenerator::project_title_path_;
std::filesystem::path RegularVideoGenerator::project_temp_pictureset_path_;
std::shared_ptr< RegularVideoGenerator::AudioData > RegularVideoGenerator::audio_data_ = nullptr;
std::shared_ptr< RegularVideoGenerator::FrameInformation > RegularVideoGenerator::frame_information_ = nullptr;

void RegularVideoGenerator::init( std::filesystem::path const& project_path, std::filesystem::path const& common_path ) {
  logger_ = LoggerFactory::get_logger( "RegularVideoGenerator" );
  logger_->trace( "[init] enter: project_path: {:?}, common_path: {:?}", project_path.string(), common_path.string() );

  bool ready_val = true;

  project_path_ = project_path;
  common_path_ = common_path;

  common_epilepsy_warning_path_ = common_path_ / "epileptic_warning.txt";
  common_bg_path_ = common_path_ / "bg.old.png";
  common_circle_path_ = common_path_ / "circle.png";
  project_art_path_ = project_path_ / "art.png";
  project_audio_path_ = project_path_ / "audio.wav";
  project_title_path_ = project_path_ / "title.txt";

  for( auto const& file : std::vector< std::filesystem::path >(
           { common_epilepsy_warning_path_, common_bg_path_, common_circle_path_, project_art_path_, project_audio_path_, project_title_path_ } ) ) {
    if( !std::filesystem::is_regular_file( file ) ) {
      logger_->error( "[init] file {:?} doesn't exist!", file.string() );
      ready_val = false;
    }
  }

  project_temp_pictureset_path_ = project_path_ / "__pictures";
  if( std::filesystem::is_directory( project_temp_pictureset_path_ ) ) {
    std::filesystem::remove_all( project_temp_pictureset_path_ );
  }
  std::filesystem::create_directory( project_temp_pictureset_path_ );

  is_ready_ = ready_val;
  logger_->trace( "[init] exit" );
}

void RegularVideoGenerator::deinit() {
  logger_->trace( "[deinit] enter" );

  logger_->trace( "[deinit] exit" );
}

void RegularVideoGenerator::render() {
  logger_->trace( "[render] enter" );

  if( !is_ready_ ) {
    logger_->error( "[render] generator is not ready!" );
    return;
  }

  prepare_audio();

  calculate_frames();

  prepare_surfaces();

  prepare_threads();

  prepare_fft();

  start_threads();

  join_threads();

  clean_up();

  logger_->trace( "[render] exit" );
}

void RegularVideoGenerator::prepare_audio() {
  logger_->trace( "[prepare_audio] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_audio] generator is not ready!" );
    return;
  }

  audio_data_ = std::make_shared< RegularVideoGenerator::AudioData >();

  audio_data_->sample_data = std::shared_ptr< float[] >( drwav_open_file_and_read_pcm_frames_f32( project_audio_path_.string().c_str(),
                                                                                                  &audio_data_->channels,
                                                                                                  &audio_data_->sample_rate,
                                                                                                  &audio_data_->total_pcm_frame_count,
                                                                                                  nullptr ),
                                                         []( float* p ) { drwav_free( p, nullptr ); } );
  logger_->debug( "[prepare_audio] audio_data_->channels: {}", audio_data_->channels );
  logger_->debug( "[prepare_audio] audio_data_->sample_rate: {}", audio_data_->sample_rate );
  logger_->debug( "[prepare_audio] audio_data_->total_pcm_frame_count: {}", audio_data_->total_pcm_frame_count );
  audio_data_->duration = double( audio_data_->total_pcm_frame_count ) / double( audio_data_->sample_rate );
  logger_->debug( "[prepare_audio] audio_data_->duration: {}", audio_data_->duration );

  create_lowpass_for_audio_data();

  logger_->trace( "[prepare_audio] exit" );
}

void RegularVideoGenerator::calculate_frames() {
  logger_->trace( "[calculate_frames] enter" );

  if( !is_ready_ ) {
    logger_->error( "[calculate_frames] generator is not ready!" );
    return;
  }

  frame_information_ = std::make_shared< RegularVideoGenerator::FrameInformation >();

  frame_information_->amount_output_frames = size_t( std::ceil( audio_data_->duration * FPS ) );
  logger_->debug( "[calculate_frames] frame_information_->amount_output_frames: {}", frame_information_->amount_output_frames );
  frame_information_->pcm_frames_per_output_frame = double( audio_data_->total_pcm_frame_count ) / double( frame_information_->amount_output_frames );
  logger_->debug( "[calculate_frames] frame_information_->pcm_frames_per_output_frame: {}", frame_information_->pcm_frames_per_output_frame );

  frame_information_->fft_size = 1;
  while( frame_information_->fft_size < frame_information_->pcm_frames_per_output_frame ) {
    frame_information_->fft_size = frame_information_->fft_size << 1;
  }
  for( int e = 0; e < EXTRA_FFT_SIZE; e++ ) {
    // extra passes of fft size
    frame_information_->fft_size = frame_information_->fft_size << 1;
  }
  logger_->debug( "[calculate_frames] frame_information_->fft_size: {}", frame_information_->fft_size );

  logger_->trace( "[calculate_frames] exit" );
}

void RegularVideoGenerator::prepare_surfaces() {
  logger_->trace( "[prepare_surfaces] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_surfaces] generator is not ready!" );
    return;
  }

  create_epilepsy_warning();
  frame_information_->common_bg_surface = surface_load_file( common_bg_path_ );
  frame_information_->common_circle_surface = surface_load_file( common_circle_path_ );
  frame_information_->project_art_surface = surface_load_file_into_overlay( project_art_path_, VIDEO_WIDTH, VIDEO_HEIGHT, 24, 24, 917, 812 );
  logger_->debug( "[prepare_surfaces] frame_information_->common_epilepsy_warning_surface: {}",
                  static_cast< void* >( frame_information_->common_epilepsy_warning_surface.get() ) );
  logger_->debug( "[prepare_surfaces] frame_information_->common_bg_surface: {}", static_cast< void* >( frame_information_->common_bg_surface.get() ) );
  logger_->debug( "[prepare_surfaces] frame_information_->common_circle_surface: {}", static_cast< void* >( frame_information_->common_circle_surface.get() ) );
  logger_->debug( "[prepare_surfaces] frame_information_->project_art_surface: {}", static_cast< void* >( frame_information_->project_art_surface.get() ) );
  frame_information_->static_text_surface = surface_render_text_into_overlay( FontManager::get_font_face( "Roboto-Regular.ttf" ),
                                                                              project_title_path_,
                                                                              VIDEO_WIDTH,
                                                                              VIDEO_HEIGHT,
                                                                              979,
                                                                              24,
                                                                              917,
                                                                              387 );
  logger_->debug( "[prepare_surfaces] frame_information_->static_text_surface: {}", static_cast< void* >( frame_information_->static_text_surface.get() ) );

  logger_->trace( "[prepare_surfaces] exit" );
}

void RegularVideoGenerator::prepare_threads() {
  logger_->trace( "[prepare_threads] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_threads] generator is not ready!" );
    return;
  }

  uint32_t thread_count = std::thread::hardware_concurrency();
  logger_->debug( "[prepare_threads] thread_count: {}", thread_count );
  frame_information_->thread_input_lists.reserve( std::thread::hardware_concurrency() );
  for( int i = 0; i < frame_information_->thread_input_lists.capacity(); i++ ) {
    std::vector< RegularVideoGenerator::ThreadInputData > vec;
    vec.reserve( ( frame_information_->amount_output_frames / frame_information_->thread_input_lists.capacity() ) + 1 );
    frame_information_->thread_input_lists.push_back( vec );
  }

  double pcm_frame_offset = 0.0;
  for( size_t i = 0; i < frame_information_->amount_output_frames; i++ ) {
    RegularVideoGenerator::ThreadInputData input_data;
    input_data.i = i;
    input_data.amount_output_frames = frame_information_->amount_output_frames;
    input_data.project_temp_pictureset_picture_path = project_temp_pictureset_path_ / fmt::format( "{}.png", i );
    input_data.pcm_frame_count = uint64_t( double( frame_information_->fft_size ) * FFT_INPUT_SIZE_MULT );
    input_data.pcm_frame_offset
        = std::min< int64_t >( audio_data_->total_pcm_frame_count, int64_t( pcm_frame_offset ) - int64_t( input_data.pcm_frame_count / 2 ) );
    input_data.audio_data_ptr = audio_data_;
    input_data.common_epilepsy_warning_surface = frame_information_->common_epilepsy_warning_surface;
    input_data.common_bg_surface = frame_information_->common_bg_surface;
    input_data.common_circle_surface = frame_information_->common_circle_surface;
    input_data.project_art_surface = frame_information_->project_art_surface;
    input_data.static_text_surface = frame_information_->static_text_surface;
    logger_->debug( "[prepare_threads] output frame {} from sample {} to {}",
                    input_data.i,
                    input_data.pcm_frame_offset,
                    input_data.pcm_frame_offset + ( input_data.pcm_frame_count - 1 ) );

    size_t thread_index = i % frame_information_->thread_input_lists.capacity();
    frame_information_->thread_input_lists[thread_index].push_back( input_data );

    pcm_frame_offset += frame_information_->pcm_frames_per_output_frame;
  }

  logger_->trace( "[prepare_threads] exit" );
}

void RegularVideoGenerator::prepare_fft() {
  logger_->trace( "[prepare_fft] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_fft] generator is not ready!" );
    return;
  }

  for( auto& input_list : frame_information_->thread_input_lists ) {
    size_t fft_input_size = frame_information_->fft_size;
    std::shared_ptr< float[] > fft_input = std::make_shared< float[] >( fft_input_size );

    std::shared_ptr< double[] > fft_window = std::make_shared< double[] >( fft_input_size );
    blackmanharris( fft_window.get(), fft_input_size, false );  // create values for window function

    size_t fft_output_size = frame_information_->fft_size / 2 + 1;
    std::shared_ptr< fftwf_complex[] > fft_output = std::make_shared< fftwf_complex[] >( fft_output_size );
    std::shared_ptr< fftwf_plan_s > fft_plan
        = make_fftw_shared_ptr( fftwf_plan_dft_r2c_1d( frame_information_->fft_size, fft_input.get(), fft_output.get(), FFTW_PLAN_FLAGS ) );
    if( !fft_plan ) {
      logger_->error( "[prepare_fft] failed fftwf_plan_dft_r2c_1d!" );
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

  logger_->trace( "[prepare_fft] exit" );
}

void RegularVideoGenerator::start_threads() {
  logger_->trace( "[start_threads] enter" );

  if( !is_ready_ ) {
    logger_->error( "[start_threads] generator is not ready!" );
    return;
  }

  frame_information_->thread_input_lists.reserve( frame_information_->thread_input_lists.size() );
  for( auto const& input_list : frame_information_->thread_input_lists ) {
    frame_information_->thread_list.emplace_back( RegularVideoGenerator::thread_run, input_list );
  }

  logger_->trace( "[start_threads] exit" );
}

void RegularVideoGenerator::join_threads() {
  logger_->trace( "[join_threads] enter" );

  if( !is_ready_ ) {
    logger_->error( "[join_threads] generator is not ready!" );
    return;
  }

  for( auto& thread : frame_information_->thread_list ) {
    thread.join();
  }

  logger_->trace( "[join_threads] exit" );
}

void RegularVideoGenerator::clean_up() {
  logger_->trace( "[clean_up] enter" );

  if( !is_ready_ ) {
    logger_->error( "[clean_up] generator is not ready!" );
    return;
  }

  // from prepare_surfaces
  // keep epilepsy_warning_surface

  // from calculate_frames
  frame_information_.reset();

  // from prepare_audio
  // keep __filtered_#_#.wav
  audio_data_.reset();

  // don't delete things other programs still need
  // // from init
  // if( std::filesystem::is_directory( project_temp_pictureset_path_ ) ) {
  //   std::filesystem::remove_all( project_temp_pictureset_path_ );
  // }

  logger_->trace( "[clean_up] exit" );
}

void RegularVideoGenerator::save_surface( std::shared_ptr< cairo_surface_t > surface, std::filesystem::path const& file_path ) {
  logger_->trace( "[save_surface] enter: surface: {}, file_path: {:?}", static_cast< void* >( surface.get() ), file_path.string() );

  cairo_status_t status = cairo_surface_write_to_png( surface.get(), file_path.string().c_str() );
  if( status != cairo_status_t::CAIRO_STATUS_SUCCESS ) {
    logger_->error( "[save_surface] error in save_surface: {}", cairo_status_to_string( status ) );
  }

  logger_->trace( "[save_surface] exit" );
}

void RegularVideoGenerator::create_lowpass_for_audio_data() {
  logger_->trace( "[create_lowpass_for_audio_data] enter" );

  if( !is_ready_ ) {
    logger_->error( "[create_lowpass_for_audio_data] generator is not ready!" );
    return;
  }

  uint64_t sample_amount = audio_data_->total_pcm_frame_count * audio_data_->channels;

  audio_data_->processed_sample_data = std::make_shared< float[] >( sample_amount );
  memset( audio_data_->processed_sample_data.get(), 0, sample_amount * sizeof( audio_data_->processed_sample_data[0] ) );

  Iir::Butterworth::LowPass< IIR_FILTER_ORDER > lowpass;
  Iir::Butterworth::HighPass< IIR_FILTER_ORDER > highpass;

  // fill buf1 into audio_data_
  for( uint64_t c = 0; c < audio_data_->channels; c++ ) {
    lowpass.setup( audio_data_->sample_rate, BASS_LP_CUTOFF );
    highpass.setup( audio_data_->sample_rate, BASS_HP_CUTOFF );
    for( uint64_t i = 0; i < audio_data_->total_pcm_frame_count; i++ ) {
      uint64_t sample_index = ( i * audio_data_->channels ) + c;

      audio_data_->sample_min = std::min( audio_data_->sample_min, audio_data_->sample_data[sample_index] );
      audio_data_->sample_max = std::max( audio_data_->sample_max, audio_data_->sample_data[sample_index] );
      audio_data_->sample_min = std::clamp( audio_data_->sample_min, -1.0f, 1.0f );
      audio_data_->sample_max = std::clamp( audio_data_->sample_max, -1.0f, 1.0f );

      float lowpassed_sample = lowpass.filter( audio_data_->sample_data[sample_index] );
      float highpassed_sample = highpass.filter( lowpassed_sample );
      audio_data_->processed_sample_data[sample_index] = highpassed_sample;
      audio_data_->processed_sample_min = std::min( audio_data_->processed_sample_min, audio_data_->processed_sample_data[sample_index] );
      audio_data_->processed_sample_max = std::max( audio_data_->processed_sample_max, audio_data_->processed_sample_data[sample_index] );
      audio_data_->processed_sample_min = std::clamp( audio_data_->processed_sample_min, -1.0f, 1.0f );
      audio_data_->processed_sample_max = std::clamp( audio_data_->processed_sample_max, -1.0f, 1.0f );
    }
  }

#if defined( DO_SAVE_LOWPASS_AUDIO )
  {
    drwav dw_obj;
    drwav_data_format dw_fmt;
    dw_fmt.container = drwav_container::drwav_container_riff;
    dw_fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    dw_fmt.channels = audio_data_->channels;
    dw_fmt.sampleRate = audio_data_->sample_rate;
    dw_fmt.bitsPerSample = 32;
    std::filesystem::path tmp_audio_path( project_path_ / fmt::format( "__filtered_{}_{}.wav", BASS_LP_CUTOFF, BASS_HP_CUTOFF ) );
    if( std::filesystem::is_regular_file( tmp_audio_path ) ) {
      std::filesystem::remove( tmp_audio_path );
    }
    if( drwav_init_file_write( &dw_obj, tmp_audio_path.string().c_str(), &dw_fmt, nullptr ) ) {
      size_t framesWritten = drwav_write_pcm_frames( &dw_obj, audio_data_->total_pcm_frame_count, audio_data_->processed_sample_data.get() );
      logger_->debug( "[create_lowpass_for_audio_data] framesWritten: {}", framesWritten );
      drwav_uninit( &dw_obj );
    }
  }
#endif

  logger_->trace( "[create_lowpass_for_audio_data] exit" );
}

void RegularVideoGenerator::create_epilepsy_warning() {
  logger_->trace( "[create_epilepsy_warning] enter" );

  if( !is_ready_ ) {
    logger_->error( "[create_epilepsy_warning] generator is not ready!" );
    return;
  }

  frame_information_->common_epilepsy_warning_surface = surface_create_size( VIDEO_WIDTH, VIDEO_HEIGHT );

  // drawing the epilepsy warning
  surface_fill( frame_information_->common_epilepsy_warning_surface, 0.0, 0.0, 0.0, 1.0 );

  std::shared_ptr< cairo_surface_t > text_surface = surface_render_text_advanced_into_overlay( FontManager::get_font_face( EPILEPSY_WARNING_HEADER_FONT ),
                                                                                               FontManager::get_font_face( EPILEPSY_WARNING_CONTENT_FONT ),
                                                                                               common_epilepsy_warning_path_,
                                                                                               VIDEO_WIDTH,
                                                                                               VIDEO_HEIGHT,
                                                                                               0,
                                                                                               0,
                                                                                               VIDEO_WIDTH,
                                                                                               VIDEO_HEIGHT );
  surface_blit( text_surface, frame_information_->common_epilepsy_warning_surface, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT );

#if defined( DO_SAVE_EPILEPSY_WARNING )
  std::string tmp_str = common_epilepsy_warning_path_.string();
  std::filesystem::path epilepsy_warning_picture_path( replace( tmp_str, ".txt", ".png" ) );

  save_surface( frame_information_->common_epilepsy_warning_surface, epilepsy_warning_picture_path );
#endif

  logger_->trace( "[create_epilepsy_warning] exit" );
}

void RegularVideoGenerator::draw_samples_on_surface( std::shared_ptr< cairo_surface_t > surface, RegularVideoGenerator::ThreadInputData const& input_data ) {
  logger_->trace( "[draw_samples_on_surface] enter: surface: {}", static_cast< void* >( surface.get() ) );

  if( !is_ready_ ) {
    logger_->error( "[draw_samples_on_surface] generator is not ready!" );
    return;
  }

  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  cairo_t* cr = cairo_create( surface.get() );
  cairo_save( cr );

  double const surface_w = cairo_image_surface_get_width( surface.get() );
  double const surface_h = cairo_image_surface_get_height( surface.get() );

  double const middle_y = surface_h / 2.0;
  double const frame_duration = static_cast< double >( input_data.pcm_frame_count );
  double prev_x = 0.0;
  double prev_y = middle_y;
  double x = 0.0;
  double y = middle_y;
  // logger->debug( "draw_samples_on_surface - {} frames for {} pixels", frame_duration, surface_w );

  cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
  // default is 2.0
  cairo_set_line_width( cr, 3.0 );

  for( int c = audio_data_->channels - 1; c >= 0; c-- ) {
    prev_x = 0.0;
    prev_y = middle_y;

    double red = std::pow( 0.5, static_cast< double >( c ) );
    cairo_set_source_rgb( cr, red, 0.0, 0.0 );

    for( int64_t i = 0; i < input_data.pcm_frame_count; i++ ) {
      x = std::round( static_cast< float >( surface_w ) * ( static_cast< float >( i ) / static_cast< float >( frame_duration - 1 ) ) );

      // range: -1.0 to 1.0
      float sample = 0.0;
      int64_t sample_index = ( ( input_data.pcm_frame_offset + i ) * audio_data_->channels ) + c;
      if( ( sample_index >= 0 ) && ( sample_index < ( audio_data_->total_pcm_frame_count * audio_data_->channels ) ) ) {
        sample = audio_data_->sample_data[sample_index];
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

  logger_->trace( "[draw_samples_on_surface] exit" );
}

void RegularVideoGenerator::draw_freqs_on_surface( std::shared_ptr< cairo_surface_t > surface, RegularVideoGenerator::ThreadInputData const& input_data ) {
  logger_->trace( "[draw_freqs_on_surface] enter: surface: {}", static_cast< void* >( surface.get() ) );

  if( !is_ready_ ) {
    logger_->error( "[draw_freqs_on_surface] generator is not ready!" );
    return;
  }

  surface_fill( surface, 0.0, 0.0, 0.0, 1.0 );

  cairo_t* cr = cairo_create( surface.get() );
  cairo_save( cr );

  double const width = cairo_image_surface_get_width( surface.get() );
  double const height = cairo_image_surface_get_height( surface.get() );

  double const min_freq = std::max( 20.0, FFT_DISPLAY_MIN_FREQ );
  double const max_freq = std::min( double( audio_data_->sample_rate ) / 2.0, FFT_DISPLAY_MAX_FREQ );

  // cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_BUTT );
  cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
  // // default is 2.0
  // cairo_set_line_width( cr, 3.0 );

  // per channel
  for( int c = ( audio_data_->channels - 1 ); c >= 0; c-- ) {
    double red = std::pow( 0.5, double( c + 1 ) );
    cairo_set_source_rgb( cr, red, 0.0, 0.0 );

    std::memset( input_data.fft_input.get(), 0, input_data.fft_input_size * sizeof( input_data.fft_input[0] ) );

    for( int64_t i = 0; i < input_data.pcm_frame_count; i++ ) {
      float w = input_data.fft_window[i];
      if( ( ( input_data.pcm_frame_offset + i ) >= 0 ) && ( ( input_data.pcm_frame_offset + i ) < audio_data_->total_pcm_frame_count ) ) {
        float sample = audio_data_->sample_data[( ( input_data.pcm_frame_offset + i ) * audio_data_->channels ) + c];
        input_data.fft_input[i] = sample * w;
      }
    }

    fftwf_execute( input_data.fft_plan.get() );

    double x = 0.0;
    double y = 0.0;

    cairo_new_path( cr );
    // start at bottom left corner
    cairo_move_to( cr, 0.0, height );

    for( size_t i = 1; i < input_data.fft_output_size; i++ ) {
      double freq = static_cast< double >( i ) * audio_data_->sample_rate / static_cast< double >( input_data.fft_input_size );
      // no need to continue if we draw a big polygon of a path
      // if( freq < min_freq || freq > max_freq )
      //   continue;

      // either this
      double compensation = std::sqrt( freq / min_freq );

      double real = input_data.fft_output[i][0];
      double imaginary = input_data.fft_output[i][1];
      double mag = std::sqrt( real * real + imaginary * imaginary ) / input_data.fft_input_size;
      // either this
      mag *= compensation;
      double mag_db = 20.0 * std::log10( mag + 1e-6 );  // Avoid log(0)
      // or this
      // mag_db += A_weighting_db( freq );  // Apply perceptual boost

      // Normalize
      double norm_freq = ( freq - min_freq ) / ( max_freq - min_freq );
      double norm_freq_log = ( std::log( freq ) - std::log( min_freq ) ) / ( std::log( max_freq ) - std::log( min_freq ) );
      double norm_mag = ( mag_db - FFT_DISPLAY_MIN_MAG ) / ( FFT_DISPLAY_MAX_MAG - FFT_DISPLAY_MIN_MAG );
      norm_mag = std::clamp( norm_mag, 0.0, 1.0 );

      x = norm_freq_log * width;
      y = height * ( 1.0 - norm_mag );

      cairo_line_to( cr, x, y );
    }
    // end at bottom right corner
    cairo_line_to( cr, width, height );
    cairo_close_path( cr );

    // cairo_stroke( cr );
    cairo_fill( cr );
  }

  cairo_restore( cr );
  cairo_destroy( cr );

  logger_->trace( "[draw_freqs_on_surface] exit" );
}

void RegularVideoGenerator::thread_run( std::vector< RegularVideoGenerator::ThreadInputData > inputs ) {
  logger_->trace( "[thread_run] enter: inputs: [{} items]", inputs.size() );

  if( !is_ready_ ) {
    logger_->error( "[thread_run] generator is not ready!" );
    return;
  }

  std::shared_ptr< cairo_surface_t > dynamic_waves_surface = surface_create_size( 917, 387 );
  std::shared_ptr< cairo_surface_t > dynamic_freqs_surface = surface_create_size( 917, 387 );
  logger_->debug( "[thread_run] dynamic_waves_surface: {}", static_cast< void* >( dynamic_waves_surface.get() ) );
  logger_->debug( "[thread_run] dynamic_freqs_surface: {}", static_cast< void* >( dynamic_freqs_surface.get() ) );

  cairo_rectangle_t project_common_circle_dest_rect;
  cairo_rectangle_t dynamic_waves_dest_rect;
  cairo_rectangle_t dynamic_freqs_dest_rect;

  dynamic_waves_dest_rect.x = 979;
  dynamic_waves_dest_rect.y = 449;
  dynamic_waves_dest_rect.width = cairo_image_surface_get_width( dynamic_waves_surface.get() );
  dynamic_waves_dest_rect.height = cairo_image_surface_get_height( dynamic_waves_surface.get() );
  dynamic_freqs_dest_rect.x = 979;
  dynamic_freqs_dest_rect.y = 24;
  dynamic_freqs_dest_rect.width = cairo_image_surface_get_width( dynamic_freqs_surface.get() );
  dynamic_freqs_dest_rect.height = cairo_image_surface_get_height( dynamic_freqs_surface.get() );

  for( ThreadInputData input_data : inputs ) {
    logger_->trace( "[thread_run] computing input {}", input_data.i );
    std::shared_ptr< cairo_surface_t > frame_surface_to_save = surface_create_size( VIDEO_WIDTH, VIDEO_HEIGHT );
    surface_fill( frame_surface_to_save, 0.0, 0.0, 0.0, 1.0 );

    double epilepsy_warning_alpha = 0.0;
    if( input_data.i < size_t( EPILEPSY_WARNING_VISIBLE_SECONDS * FPS ) ) {
      epilepsy_warning_alpha = 1.0;
    } else if( input_data.i >= size_t( ( EPILEPSY_WARNING_VISIBLE_SECONDS + EPILEPSY_WARNING_FADEOUT_SECONDS ) * FPS ) ) {
      epilepsy_warning_alpha = 0.0;
    } else {
      epilepsy_warning_alpha = 1.0 - ( ( ( double( input_data.i ) / FPS ) - EPILEPSY_WARNING_VISIBLE_SECONDS ) / EPILEPSY_WARNING_FADEOUT_SECONDS );
      epilepsy_warning_alpha = std::clamp( epilepsy_warning_alpha, 0.0, 1.0 );
    }

    double bass_rms_sum_value = 0.0;
    double rms_sum_value = 0.0;
    for( int64_t i = 0; i <= input_data.pcm_frame_count; i++ ) {
      for( int64_t c = 0; c <= input_data.audio_data_ptr->channels; c++ ) {
        int64_t sample_index = ( ( input_data.pcm_frame_offset + i ) * input_data.audio_data_ptr->channels ) + c;
        double bass_sample = 0.0;
        double sound_sample = 0.0;
        if( ( sample_index >= 0 ) && ( sample_index < ( input_data.audio_data_ptr->total_pcm_frame_count * input_data.audio_data_ptr->channels ) ) ) {
          bass_sample = input_data.audio_data_ptr->processed_sample_data[sample_index];
          sound_sample = input_data.audio_data_ptr->sample_data[sample_index];
        }
        bass_sample = std::clamp( bass_sample, -1.0, 1.0 );
        sound_sample = std::clamp( sound_sample, -1.0, 1.0 );

        bass_rms_sum_value += std::pow( bass_sample, 2.0 );
        rms_sum_value += std::pow( sound_sample, 2.0 );
      }
    }

    double const bass_intensity = std::sqrt( bass_rms_sum_value / (static_cast< double >( static_cast< double >( input_data.pcm_frame_count ) ) * static_cast< double >( input_data.pcm_frame_count )) );
    double const sound_intensity = std::sqrt( rms_sum_value / (static_cast< double >( static_cast< double >( input_data.pcm_frame_count ) ) * static_cast< double >( input_data.pcm_frame_count )) );
    double const circle_intensity_scale = 0.5;
    double const colour_displace_intensity_scale = 0.15;

    project_common_circle_dest_rect.width = double( cairo_image_surface_get_width( input_data.common_circle_surface.get() ) )
                                            * ( ( 1.0 - circle_intensity_scale ) + ( sound_intensity * circle_intensity_scale ) );
    project_common_circle_dest_rect.width = project_common_circle_dest_rect.width * ( double( VIDEO_WIDTH ) / 1920.0 );
    project_common_circle_dest_rect.height = project_common_circle_dest_rect.width;
    project_common_circle_dest_rect.x = 114.5 + ( ( 1804.5 - 114.5 ) * ( double( input_data.i ) / double( input_data.amount_output_frames ) ) )
                                        - ( project_common_circle_dest_rect.width / 2.0 );
    project_common_circle_dest_rect.y = 964.5 - ( project_common_circle_dest_rect.height / 2.0 );

    try {
      draw_samples_on_surface( dynamic_waves_surface, input_data );
    } catch( std::exception const& e ) {
      logger_->error( "[thread_run] error in draw_samples_on_surface: {}", e.what() );
    }
    try {
      draw_freqs_on_surface( dynamic_freqs_surface, input_data );
    } catch( std::exception const& e ) {
      logger_->error( "[thread_run] error in draw_freqs_on_surface: {}", e.what() );
    }

    // make copy of bg, put waves and freqs and circle on copy, put copy on canvas, shakily
    std::shared_ptr< cairo_surface_t > copied_bg_surface = surface_copy( input_data.common_bg_surface );
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
    surface_blit( input_data.common_circle_surface,
                  copied_bg_surface,
                  project_common_circle_dest_rect.x,
                  project_common_circle_dest_rect.y,
                  project_common_circle_dest_rect.width,
                  project_common_circle_dest_rect.height );

    surface_shake_and_blit( copied_bg_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ), true );
    copied_bg_surface.reset();

    // put art on canvas, shakily
    surface_shake_and_blit( input_data.project_art_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // put title on canvas, shakily
    surface_shake_and_blit( input_data.static_text_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // put warning on top, with alpha
    surface_blit( input_data.common_epilepsy_warning_surface, frame_surface_to_save, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, epilepsy_warning_alpha );

    // save canvas
    save_surface( frame_surface_to_save, input_data.project_temp_pictureset_picture_path );
    frame_surface_to_save.reset();
  }

  dynamic_freqs_surface.reset();
  dynamic_waves_surface.reset();

  logger_->trace( "[thread_run] exit" );
}
