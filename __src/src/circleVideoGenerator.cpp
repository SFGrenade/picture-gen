#include "circleVideoGenerator.h"

#include <Iir.h>
#include <limits>
#include <memory>
#include <numbers>

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

double const CircleVideoGenerator::FPS = 60.0;
int32_t const CircleVideoGenerator::VIDEO_WIDTH = 1920;
int32_t const CircleVideoGenerator::VIDEO_HEIGHT = 1080;
int32_t const CircleVideoGenerator::IIR_FILTER_ORDER = 16;
double const CircleVideoGenerator::BASS_LP_CUTOFF = 80.0;
double const CircleVideoGenerator::BASS_HP_CUTOFF = 20.0;
// amount of pcm samples played per frame * this = amount of pcm samples shown per frame
double const CircleVideoGenerator::PCM_FRAME_COUNT_MULT = CircleVideoGenerator::FPS / 10.0;
// /*
//   0 => 1024
//   1 => 2048
//   2 => 4096
//   3 => 8192
//   4 => 16384
//   5 => 32768
// */
// uint16_t const CircleVideoGenerator::EXTRA_FFT_SIZE = 3;
// // 2^N times the resolution of the regular fft
// uint16_t const CircleVideoGenerator::FANCY_EXTRA_FFT_SIZE = 4;
// /*
//   to keep 4096 samples:
//   0 => 4.0
//   1 => 2.0
//   2 => 1.0
//   3 => 0.5
//   4 => 0.25
//   5 => 0.125
// */
// double const CircleVideoGenerator::FFT_INPUT_SIZE_MULT = 0.5;
double const CircleVideoGenerator::EPILEPSY_WARNING_VISIBLE_SECONDS = 3.0;
double const CircleVideoGenerator::EPILEPSY_WARNING_FADEOUT_SECONDS = 2.0;
// uint32_t const CircleVideoGenerator::FFTW_PLAN_FLAGS = FFTW_EXHAUSTIVE;
uint32_t const CircleVideoGenerator::FFTW_PLAN_FLAGS = FFTW_ESTIMATE;
std::string const CircleVideoGenerator::EPILEPSY_WARNING_HEADER_FONT = "BarberChop.otf";
std::string const CircleVideoGenerator::EPILEPSY_WARNING_CONTENT_FONT = "arial_narrow_7.ttf";
uint32_t const CircleVideoGenerator::FFT_DISPLAY_BIN_AMOUNT = 512;
// 0.0 = max smooth, 1.0 = no smooth
double const CircleVideoGenerator::FFT_COMPUTE_ALPHA = 0.7;
double const CircleVideoGenerator::FFT_DISPLAY_MIN_FREQ = 20.0;
double const CircleVideoGenerator::FFT_DISPLAY_MAX_FREQ = 200.0;
// NONE: { 30.0, 71.0 }
// rectwin: { ???, ??? }
// hann: { -60.0, -18.0 }
// hamming: { ???, ??? }
// blackman: { ???, ??? }
// blackman-harris: { ???, ??? }
// nuttall: { -30.0, 3.0 }
// nuttall octave: { 40.0, 80.0 }
// flattop: { ???, ??? }
// flattop octave: { ???, ??? }
double const CircleVideoGenerator::FFT_DISPLAY_MIN_MAG_DB = -30.0;
double const CircleVideoGenerator::FFT_DISPLAY_MAX_MAG_DB = -5.0;

spdlogger CircleVideoGenerator::logger_ = nullptr;
bool CircleVideoGenerator::is_ready_ = false;
std::filesystem::path CircleVideoGenerator::project_path_;
std::filesystem::path CircleVideoGenerator::common_path_;
std::filesystem::path CircleVideoGenerator::common_epilepsy_warning_path_;
std::filesystem::path CircleVideoGenerator::common_bg_path_;
std::filesystem::path CircleVideoGenerator::common_circle_path_;
std::filesystem::path CircleVideoGenerator::project_art_path_;
std::filesystem::path CircleVideoGenerator::project_audio_path_;
std::filesystem::path CircleVideoGenerator::project_title_path_;
std::filesystem::path CircleVideoGenerator::project_temp_pictureset_path_;
std::shared_ptr< CircleVideoGenerator::AudioData > CircleVideoGenerator::audio_data_ = nullptr;
std::shared_ptr< CircleVideoGenerator::FrameInformation > CircleVideoGenerator::frame_information_ = nullptr;

typedef std::pair< double, double > Point;

void CircleVideoGenerator::init( std::filesystem::path const& project_path, std::filesystem::path const& common_path ) {
  logger_ = LoggerFactory::get_logger( "CircleVideoGenerator" );
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
    logger_->trace( "[init] deleting directory {:?}", project_temp_pictureset_path_.string() );
    std::filesystem::remove_all( project_temp_pictureset_path_ );
  }
  logger_->trace( "[init] creating directory {:?}", project_temp_pictureset_path_.string() );
  std::filesystem::create_directory( project_temp_pictureset_path_ );

  is_ready_ = ready_val;
  logger_->trace( "[init] exit" );
}

void CircleVideoGenerator::deinit() {
  logger_->trace( "[deinit] enter" );

  logger_->trace( "[deinit] exit" );
}

void CircleVideoGenerator::render() {
  logger_->trace( "[render] enter" );

  if( !is_ready_ ) {
    logger_->error( "[render] generator is not ready!" );
    return;
  }

  prepare_audio();

  calculate_frames();

  prepare_surfaces();

  prepare_fft();

  prepare_threads();

  start_threads();

  join_threads();

  clean_up();

  logger_->trace( "[render] exit" );
}

void CircleVideoGenerator::prepare_audio() {
  logger_->trace( "[prepare_audio] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_audio] generator is not ready!" );
    return;
  }

  audio_data_ = std::make_shared< CircleVideoGenerator::AudioData >();

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

void CircleVideoGenerator::calculate_frames() {
  logger_->trace( "[calculate_frames] enter" );

  if( !is_ready_ ) {
    logger_->error( "[calculate_frames] generator is not ready!" );
    return;
  }

  frame_information_ = std::make_shared< CircleVideoGenerator::FrameInformation >();

  frame_information_->amount_output_frames = size_t( std::ceil( audio_data_->duration * FPS ) );
  logger_->debug( "[calculate_frames] frame_information_->amount_output_frames: {}", frame_information_->amount_output_frames );
  frame_information_->pcm_frames_per_output_frame = double( audio_data_->total_pcm_frame_count ) / double( frame_information_->amount_output_frames );
  logger_->debug( "[calculate_frames] frame_information_->pcm_frames_per_output_frame: {}", frame_information_->pcm_frames_per_output_frame );

  // frame_information_->fft_size = 1;
  // while( frame_information_->fft_size < frame_information_->pcm_frames_per_output_frame ) {
  //   frame_information_->fft_size = frame_information_->fft_size << 1;
  // }
  // for( int e = 0; e < EXTRA_FFT_SIZE; e++ ) {
  //   // extra passes of fft size
  //   frame_information_->fft_size = frame_information_->fft_size << 1;
  // }
  // logger_->debug( "[calculate_frames] frame_information_->fft_size: {}", frame_information_->fft_size );
  // frame_information_->fancy_fft_size = frame_information_->fft_size;
  // for( int e = 0; e < FANCY_EXTRA_FFT_SIZE; e++ ) {
  //   // extra passes of fft size
  //   frame_information_->fancy_fft_size = frame_information_->fancy_fft_size << 1;
  // }
  // logger_->debug( "[calculate_frames] frame_information_->fancy_fft_size: {}", frame_information_->fancy_fft_size );

  logger_->trace( "[calculate_frames] exit" );
}

void CircleVideoGenerator::prepare_surfaces() {
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

Point catmullRom( Point const& p0, Point const& p1, Point const& p2, Point const& p3, double t ) {
  double t2 = t * t;
  double t3 = t2 * t;

  return { 0.5
               * ( ( 2.0 * p1.first ) + ( -p0.first + p2.first ) * t + ( 2.0 * p0.first - 5.0 * p1.first + 4.0 * p2.first - p3.first ) * t2
                   + ( -p0.first + 3.0 * p1.first - 3.0 * p2.first + p3.first ) * t3 ),
           0.5
               * ( ( 2.0 * p1.second ) + ( -p0.second + p2.second ) * t + ( 2.0 * p0.second - 5.0 * p1.second + 4.0 * p2.second - p3.second ) * t2
                   + ( -p0.second + 3.0 * p1.second - 3.0 * p2.second + p3.second ) * t3 ) };
}

void CircleVideoGenerator::prepare_fft() {
  logger_->trace( "[prepare_fft] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_fft] generator is not ready!" );
    return;
  }

  frame_information_->fft_output_values_per_frame.reserve( frame_information_->amount_output_frames );

  uint64_t pcm_frame_count = uint64_t( double( frame_information_->pcm_frames_per_output_frame ) * PCM_FRAME_COUNT_MULT );
  logger_->trace( "[prepare_fft] pcm_frame_count: {}", pcm_frame_count );

  size_t fft_size = 1;
  while( fft_size < pcm_frame_count ) {
    fft_size = fft_size << 1;
  }
  logger_->trace( "[prepare_fft] fft_size: {}", fft_size );

  size_t fft_output_size = fft_size / 2 + 1;
  logger_->trace( "[prepare_fft] fft_output_size: {}", fft_output_size );

  // have vector as input data for fft, so have it fft_size large, but only populate pcm_frame_count data
  std::shared_ptr< double[] > fft_windows = std::make_shared< double[] >( fft_size );
  nuttallwin_octave( fft_windows.get(), fft_size, false );

  // have vector as input data for fft, so have it fft_size large, but only populate pcm_frame_count data
  std::shared_ptr< float[] > signal_data_for_frame = std::make_shared< float[] >( fft_size );

  // fftwf_complex is a float[2], so just have a float vector with double the size
  std::shared_ptr< fftwf_complex[] > fft_output = std::make_shared< fftwf_complex[] >( fft_output_size );

  fftwf_plan fft_plan = fftwf_plan_dft_r2c_1d( fft_size, signal_data_for_frame.get(), fft_output.get(), FFTW_PLAN_FLAGS );

  double pcm_frame_offset_dbl = 0.0;
  for( size_t i = 0; i < frame_information_->amount_output_frames; i++ ) {
    // played sample will be in the middle of the shown samples
    int64_t pcm_frame_offset = std::min< int64_t >( audio_data_->total_pcm_frame_count, int64_t( pcm_frame_offset_dbl ) - int64_t( pcm_frame_count / 2 ) );
    // logger_->debug( "[prepare_threads] output frame {} from sample {} to {}", i, pcm_frame_offset, pcm_frame_offset + ( pcm_frame_count - 1 ) );

    for( int64_t si = 0; si < fft_size; si++ ) {
      signal_data_for_frame[si] = 0.0f;
    }
    for( int64_t si = 0; si < fft_output_size; si++ ) {
      fft_output[si][0] = 0.0f;
      fft_output[si][1] = 0.0f;
    }

    for( uint64_t si = 0; si < pcm_frame_count; si++ ) {
      int64_t sample_frame_index = ( pcm_frame_offset + int64_t( si ) );
      if( sample_frame_index < 0 ) {
        continue;
      }
      if( sample_frame_index >= int64_t( audio_data_->total_pcm_frame_count ) ) {
        continue;
      }

      float average_sample = 0.0f;
      for( uint32_t channel = 0; channel < audio_data_->channels; channel++ ) {
        average_sample += audio_data_->sample_data[( sample_frame_index * audio_data_->channels ) + channel];
      }

      signal_data_for_frame[si] = ( average_sample / float( audio_data_->channels ) ) * fft_windows[si];
    }

    fftwf_execute( fft_plan );

    double min_mag_freq = 0.0;
    double min_mag = std::numeric_limits< float >::max();
    double max_mag_freq = 0.0;
    double max_mag = -min_mag;

    std::vector< std::pair< double, double > > fft_output_vals;
    fft_output_vals.reserve( fft_output_size - 1 );

    for( uint32_t fi = 0; fi < fft_output_size - 1; fi++ ) {
      double freq = double( fi + 1 ) * double( audio_data_->sample_rate ) / double( fft_size );
      double mag_compensation = std::sqrt( freq / ( 1.0 * double( audio_data_->sample_rate ) / double( fft_size ) ) );

      std::pair< double, double > val;
      val.first = freq;
      double mag_real = fft_output[fi + 1][0];
      double mag_imag = fft_output[fi + 1][1];
      double mag = std::sqrt( ( mag_real * mag_real ) + ( mag_imag * mag_imag ) ) * mag_compensation / double( fft_size );
      val.second = 20.0 * std::log10( mag + 1e-12 );
      val.second = std::clamp( val.second, FFT_DISPLAY_MIN_MAG_DB, FFT_DISPLAY_MAX_MAG_DB );
      fft_output_vals.push_back( val );

      if( ( val.second ) < min_mag ) {
        min_mag_freq = val.first;
        min_mag = val.second;
      }
      if( ( val.second ) > max_mag ) {
        max_mag_freq = val.first;
        max_mag = val.second;
      }
    }

    std::shared_ptr< std::vector< std::pair< double, double > > > formatted_fft_output_values
        = std::make_shared< std::vector< std::pair< double, double > > >();
    formatted_fft_output_values->reserve( fft_output_size - 1 );

    for( uint32_t bin = 0; bin < FFT_DISPLAY_BIN_AMOUNT; bin++ ) {
      double relative_freq = double( bin ) / double( FFT_DISPLAY_BIN_AMOUNT - 1 );
      double freq = FFT_DISPLAY_MIN_FREQ + ( ( FFT_DISPLAY_MAX_FREQ - FFT_DISPLAY_MIN_FREQ ) * relative_freq );
      double fft_freq_bin = ( double( fft_size ) * freq / audio_data_->sample_rate ) - 1.0;  // -1 because we skipped the first index earlier

      int64_t a_index = int64_t( std::floor( fft_freq_bin ) );
      int64_t b_index = int64_t( std::ceil( fft_freq_bin ) );
      double t = fft_freq_bin - double( a_index );
      std::pair< double, double > val;
      val.first = freq;
      // todo: fixme: change this lerp to something like what chatgpt told me
      // val.second = std::lerp( fft_output_vals[a_index].second, fft_output_vals[b_index].second, t );
      val.second = catmullRom( fft_output_vals[a_index - 1], fft_output_vals[a_index], fft_output_vals[b_index], fft_output_vals[b_index + 1], t ).second;
      if( frame_information_->fft_output_values_per_frame.size() > 0 ) {
        // apply smoothing
        val.second
            = ( FFT_COMPUTE_ALPHA * val.second ) + ( ( 1.0 - FFT_COMPUTE_ALPHA ) * frame_information_->fft_output_values_per_frame.back()->at( bin ).second );
      }
      formatted_fft_output_values->push_back( val );
    }

    logger_->debug( "[prepare_threads] output frame {} has compensated fft vals (min_mag: {} dB at {} Hz, max_mag: {} dB at {} Hz)",
                    i,
                    min_mag,
                    min_mag_freq,
                    max_mag,
                    max_mag_freq );
    frame_information_->fft_output_values_per_frame.push_back( formatted_fft_output_values );

    pcm_frame_offset_dbl += frame_information_->pcm_frames_per_output_frame;
  }

  logger_->trace( "[prepare_fft] exit" );
}

void CircleVideoGenerator::prepare_threads() {
  logger_->trace( "[prepare_threads] enter" );

  if( !is_ready_ ) {
    logger_->error( "[prepare_threads] generator is not ready!" );
    return;
  }

  uint32_t thread_count = std::thread::hardware_concurrency();
  logger_->debug( "[prepare_threads] thread_count: {}", thread_count );
  frame_information_->thread_input_lists.reserve( std::thread::hardware_concurrency() );
  for( int i = 0; i < frame_information_->thread_input_lists.capacity(); i++ ) {
    std::vector< CircleVideoGenerator::ThreadInputData > vec;
    vec.reserve( ( frame_information_->amount_output_frames / frame_information_->thread_input_lists.capacity() ) + 1 );
    frame_information_->thread_input_lists.push_back( vec );
  }

  double pcm_frame_offset = 0.0;
  for( size_t i = 0; i < frame_information_->amount_output_frames; i++ ) {
    CircleVideoGenerator::ThreadInputData input_data;
    input_data.i = i;
    input_data.amount_output_frames = frame_information_->amount_output_frames;
    input_data.project_temp_pictureset_picture_path = project_temp_pictureset_path_ / fmt::format( "{}.png", i );
    input_data.pcm_frame_count = uint64_t( double( frame_information_->pcm_frames_per_output_frame ) * PCM_FRAME_COUNT_MULT );
    // played sample will be in the middle of the shown samples
    input_data.pcm_frame_offset
        = std::min< int64_t >( audio_data_->total_pcm_frame_count, int64_t( pcm_frame_offset ) - int64_t( input_data.pcm_frame_count / 2 ) );
    input_data.audio_data_ptr = audio_data_;
    input_data.common_epilepsy_warning_surface = frame_information_->common_epilepsy_warning_surface;
    input_data.common_bg_surface = frame_information_->common_bg_surface;
    input_data.common_circle_surface = frame_information_->common_circle_surface;
    input_data.project_art_surface = frame_information_->project_art_surface;
    input_data.static_text_surface = frame_information_->static_text_surface;
    input_data.fft_output = frame_information_->fft_output_values_per_frame[i];
    // logger_->debug( "[prepare_threads] output frame {} from sample {} to {}",
    //                 input_data.i,
    //                 input_data.pcm_frame_offset,
    //                 input_data.pcm_frame_offset + ( input_data.pcm_frame_count - 1 ) );

    size_t thread_index = i % frame_information_->thread_input_lists.capacity();
    frame_information_->thread_input_lists[thread_index].push_back( input_data );

    pcm_frame_offset += frame_information_->pcm_frames_per_output_frame;
  }

  logger_->trace( "[prepare_threads] exit" );
}

void CircleVideoGenerator::start_threads() {
  logger_->trace( "[start_threads] enter" );

  if( !is_ready_ ) {
    logger_->error( "[start_threads] generator is not ready!" );
    return;
  }

  frame_information_->thread_input_lists.reserve( frame_information_->thread_input_lists.size() );
  for( auto const& input_list : frame_information_->thread_input_lists ) {
    frame_information_->thread_list.emplace_back( CircleVideoGenerator::thread_run, input_list );
  }

  logger_->trace( "[start_threads] exit" );
}

void CircleVideoGenerator::join_threads() {
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

void CircleVideoGenerator::clean_up() {
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

void CircleVideoGenerator::save_surface( std::shared_ptr< cairo_surface_t > surface, std::filesystem::path const& file_path ) {
  // logger_->trace( "[save_surface] enter: surface: {}, file_path: {:?}", static_cast< void* >( surface.get() ), file_path.string() );

  cairo_status_t status = cairo_surface_write_to_png( surface.get(), file_path.string().c_str() );
  if( status != cairo_status_t::CAIRO_STATUS_SUCCESS ) {
    logger_->error( "[save_surface] error in save_surface: {}", cairo_status_to_string( status ) );
  }

  // logger_->trace( "[save_surface] exit" );
}

void CircleVideoGenerator::create_lowpass_for_audio_data() {
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

void CircleVideoGenerator::create_epilepsy_warning() {
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

double CircleVideoGenerator::goertzel_magnitude( std::vector< float > const& signal, double freq ) {
  // logger_->trace( "[goertzel_magnitude] enter: signal: {}, freq: {}", signal, freq );

  if( !is_ready_ ) {
    logger_->error( "[goertzel_magnitude] generator is not ready!" );
    return 0.0;
  }

  return general_goeretzel( signal, signal.size(), double( audio_data_->sample_rate ), freq );

  // number points instead of in-size maybe?
  int32_t N = int32_t( signal.size() );
  // double k = ( double( N ) * freq ) / double( audio_data_->sample_rate );
  // int32_t bin = int32_t( std::round( k ) );

  // double omega = ( 2.0 * std::numbers::pi_v< double > * double( bin ) ) / double( N );
  double omega = 2.0 * std::numbers::pi_v< double > * freq / double( audio_data_->sample_rate );
  double cosine = std::cos( omega );
  double sine = std::sin( omega );
  double coeff = 2.0 * cosine;

  double window = 0.0;
  {
    // rectwin: { 1.0 }
    // hann: { 0.5, -0.5 }
    // hamming: { 0.54, -0.46 }
    // blackman: { 0.42, -0.5, 0.08 }
    // blackman-harris: { 0.35875, -0.48829, 0.14128, -0.01168 }
    // nuttall: { 0.3635819, -0.4891775, 0.1365995, -0.0106411 }
    // nuttall octave: { 0.355768, -0.487396, 0.144232, -0.012604 }
    // flattop: { 0.21557895, -0.41663158, 0.277263158, -0.083578947, 0.006947368 }
    // flattop octave: { 1.0 / 4.6402, -1.93 / 4.6402, 1.29 / 4.6402, -0.388 / 4.6402, 0.0322 / 4.6402 }
    std::vector< double > window_coeffs{ 0.355768, -0.487396, 0.144232, -0.012604 };
    for( int32_t j = 0; j < window_coeffs.size(); j++ ) {
      window += window_coeffs[j] * std::cos( double( j ) * omega );
    }
  }
  // double window = 0.0;
  // { window = 0.5 * ( 1.0 - std::cos( 2.0 * std::numbers::pi_v< double > * freq / double( audio_data_->sample_rate ) ) ); }

  double q0 = 0.0, q1 = 0.0, q2 = 0.0;

  for( int32_t i = 0; i < N; ++i ) {
    q0 = coeff * q1 - q2 + ( signal[i] );
    q2 = q1;
    q1 = q0;
  }

  double real = q1 - q2 * cosine;
  double imag = q2 * sine;
  double ret = std::sqrt( real * real + imag * imag );

  // logger_->trace( "[goertzel_magnitude] exit: ret: {}", ret );
  return ret;
}

void CircleVideoGenerator::draw_samples_on_surface( std::shared_ptr< cairo_surface_t > surface, CircleVideoGenerator::ThreadInputData const& input_data ) {
  // logger_->trace( "[draw_samples_on_surface] enter: surface: {}", static_cast< void* >( surface.get() ) );

  if( !is_ready_ ) {
    logger_->error( "[draw_samples_on_surface] generator is not ready!" );
    return;
  }

  // logger_->trace( "[draw_samples_on_surface] exit" );
  return;

  /*
  // surface_fill( surface, 0.0, 0.0, 0.0, 0.0 );

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
  */
}

void CircleVideoGenerator::draw_freqs_on_surface( std::shared_ptr< cairo_surface_t > surface, CircleVideoGenerator::ThreadInputData const& input_data ) {
  // logger_->trace( "[draw_freqs_on_surface] enter: surface: {}", static_cast< void* >( surface.get() ) );

  if( !is_ready_ ) {
    logger_->error( "[draw_freqs_on_surface] generator is not ready!" );
    return;
  }

  // surface_fill( surface, 0.0, 0.0, 0.0, 0.0 );

  cairo_t* cr = cairo_create( surface.get() );
  cairo_save( cr );

  double const width = cairo_image_surface_get_width( surface.get() );
  double const height = cairo_image_surface_get_height( surface.get() );
  double const middle_width = width / 2.0;
  double const middle_height = height / 2.0;

  std::function< double( double ) > get_theta = []( double norm ) { return norm * 2.0 * std::numbers::pi_v< double >; };
  std::function< double( double, double ) > get_x = [middle_width]( double theta, double r ) { return middle_width + sin( theta ) * r; };
  std::function< double( double, double ) > get_x_mirrored = [middle_width]( double theta, double r ) { return middle_width - sin( theta ) * r; };
  std::function< double( double, double ) > get_y = [middle_height]( double theta, double r ) { return middle_height - cos( theta ) * r; };
  std::function< double( double, double ) > get_radius_mult = []( double norm, double mag ) {
    double mult = 0.0;
    // if( norm <= double( 1.0 / 4.0 ) ) {
    //   mult = std::sqrt( std::lerp( 0.125, 1.0, ( norm - double( 0.0 / 4.0 ) ) * double( 4.0 / 1.0 ) ) );
    // } else if( norm < double( 3.0 / 4.0 ) ) {
    //   mult = std::lerp( 1.0, 0.75, ( norm - double( 1.0 / 4.0 ) ) * double( 4.0 / 2.0 ) );
    // } else {
    //   mult = std::lerp( 0.75, 0.25, ( norm - double( 3.0 / 4.0 ) ) * double( 4.0 / 1.0 ) );
    // }
    double a = -16. / 3.;
    double b = 40. / 3.;
    double c = -41. / 3.;
    double d = 17. / 3.;
    double e = 0.25;
    mult = ( a * std::pow( norm, 4.0 ) ) + ( b * std::pow( norm, 3.0 ) ) + ( c * std::pow( norm, 2.0 ) ) + ( d * std::pow( norm, 1.0 ) )
           + ( e * std::pow( norm, 0.0 ) );

    // mult = std::pow( mult, 2.0 );
    return mag * mult;
  };

  {
    std::vector< std::pair< double, double > > freq_mags;

    for( int64_t i = 0; i < input_data.fft_output->size(); i++ ) {
      auto const& pair = input_data.fft_output->at( i );

      double freq = pair.first;
      double mag_db = pair.second;

      // Normalize
      double norm_freq = ( freq - FFT_DISPLAY_MIN_FREQ ) / ( FFT_DISPLAY_MAX_FREQ - FFT_DISPLAY_MIN_FREQ );
      double norm_freq_log = ( std::log( freq ) - std::log( FFT_DISPLAY_MIN_FREQ ) ) / ( std::log( FFT_DISPLAY_MAX_FREQ ) - std::log( FFT_DISPLAY_MIN_FREQ ) );
      double norm_mag = ( mag_db - FFT_DISPLAY_MIN_MAG_DB ) / ( FFT_DISPLAY_MAX_MAG_DB - FFT_DISPLAY_MIN_MAG_DB );
      norm_mag = std::clamp( norm_mag, 0.0, 1.0 );

      freq_mags.emplace_back( norm_freq, norm_mag );
    }

    std::vector< std::vector< std::pair< double, double > > > paths;
    {
      std::vector< double > dist_mults{ 0.6, 0.7, 0.8, 0.9, 1.0 };
      double base_radius = height * 0.25;
      double max_radius = height * 0.5;
      double radius_range = max_radius - base_radius;
      double x = 0.0;
      double y = 0.0;

      for( auto const& dist_mult : dist_mults ) {
        std::vector< std::pair< double, double > > path;
        for( auto it = freq_mags.begin(); it < freq_mags.end(); it++ ) {
          double r = get_radius_mult( it->first, it->second );
          x = get_x( get_theta( it->first / 2.0 ), base_radius + ( radius_range * r * dist_mult ) );
          y = get_y( get_theta( it->first / 2.0 ), base_radius + ( radius_range * r * dist_mult ) );

          path.emplace_back( x, y );
        }
        // now we should be at the bottom middle
        for( auto it = freq_mags.end() - 1; it >= freq_mags.begin(); it-- ) {
          double r = get_radius_mult( it->first, it->second );
          x = get_x_mirrored( get_theta( it->first / 2.0 ), base_radius + ( radius_range * r * dist_mult ) );
          y = get_y( get_theta( it->first / 2.0 ), base_radius + ( radius_range * r * dist_mult ) );

          path.emplace_back( x, y );
        }
        // now we should be back at the top middle
        paths.push_back( path );
      }
    }
    {
      std::vector< std::tuple< double, double, double > > colours{
          { 0.0, 0.0, 0.5 },
          { 0.0, 0.0, 0.625 },
          { 0.0, 1.0 / 3.0, 0.75 },
          { 0.0, 2.0 / 3.0, 0.875 },
          { 0.0, 1.0, 1.0 },
      };
      cairo_set_line_cap( cr, cairo_line_cap_t::CAIRO_LINE_CAP_ROUND );
      cairo_set_line_width( cr, 3.0 );
      for( int i = paths.size() - 1; i >= 0; i-- ) {
        auto const& path = paths[i];
        auto const& colour = colours[i];
        // add outlines
        cairo_new_path( cr );

        cairo_move_to( cr, path.front().first, path.front().second );
        for( auto const& point : path ) {
          cairo_line_to( cr, point.first, point.second );
        }
        cairo_close_path( cr );

        cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 0.6 - ( double( paths.size() - i ) * 0.1 ) );
        cairo_fill( cr );
      }
      for( int i = 0; i < paths.size(); i++ ) {
        auto const& path = paths[i];
        auto const& colour = colours[i];
        // add outlines
        cairo_new_path( cr );

        cairo_move_to( cr, path.front().first, path.front().second );
        for( auto const& point : path ) {
          cairo_line_to( cr, point.first, point.second );
        }
        cairo_close_path( cr );

        cairo_set_source_rgba( cr, std::get< 0 >( colour ), std::get< 1 >( colour ), std::get< 2 >( colour ), 1.0 );
        cairo_stroke( cr );
      }
    }
  }

  cairo_restore( cr );
  cairo_destroy( cr );

  // logger_->trace( "[draw_freqs_on_surface] exit" );
}

void CircleVideoGenerator::thread_run( std::vector< CircleVideoGenerator::ThreadInputData > inputs ) {
  logger_->trace( "[thread_run] enter: inputs: [{} items]", inputs.size() );

  if( !is_ready_ ) {
    logger_->error( "[thread_run] generator is not ready!" );
    return;
  }

  cairo_rectangle_t dynamic_waves_dest_rect;
  cairo_rectangle_t dynamic_freqs_dest_rect;

  dynamic_waves_dest_rect.x = 0;
  dynamic_waves_dest_rect.y = 0;
  dynamic_waves_dest_rect.width = VIDEO_WIDTH;
  dynamic_waves_dest_rect.height = VIDEO_HEIGHT;
  dynamic_freqs_dest_rect.x = 0;
  dynamic_freqs_dest_rect.y = 0;
  dynamic_freqs_dest_rect.width = VIDEO_WIDTH;
  dynamic_freqs_dest_rect.height = VIDEO_HEIGHT;

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

    // range: 0.0 - 1.0
    float min_bass_sample_value = 0.0;
    float max_bass_sample_value = 0.0;
    float min_sound_sample_value = 0.0;
    float max_sound_sample_value = 0.0;
    for( int64_t i = 0; i <= input_data.pcm_frame_count; i++ ) {
      for( int64_t c = 0; c <= input_data.audio_data_ptr->channels; c++ ) {
        int64_t sample_index = ( ( input_data.pcm_frame_offset + i ) * input_data.audio_data_ptr->channels ) + c;
        float bass_sample = 0.0;
        float sound_sample = 0.0;
        if( ( sample_index >= 0 ) && ( sample_index < ( input_data.audio_data_ptr->total_pcm_frame_count * input_data.audio_data_ptr->channels ) ) ) {
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

    // double const bass_intensity = 0.0;
    // double const sound_intensity = 0.0;
    // double const circle_intensity_scale = 0.5;
    // double const colour_displace_intensity_scale = 0.15;

    std::shared_ptr< cairo_surface_t > copied_bg_surface = surface_create_size( VIDEO_WIDTH, VIDEO_HEIGHT );
    surface_fill( copied_bg_surface, 0.0, 0.0, 0.0, 1.0 );
    std::shared_ptr< cairo_surface_t > dynamic_waves_surface = surface_create_size( dynamic_waves_dest_rect.width, dynamic_waves_dest_rect.height );
    std::shared_ptr< cairo_surface_t > dynamic_freqs_surface = surface_create_size( dynamic_freqs_dest_rect.width, dynamic_freqs_dest_rect.height );

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

    surface_blit( dynamic_waves_surface,
                  copied_bg_surface,
                  dynamic_waves_dest_rect.x,
                  dynamic_waves_dest_rect.y,
                  dynamic_waves_dest_rect.width,
                  dynamic_waves_dest_rect.height,
                  1.0 );
    surface_blit( dynamic_freqs_surface,
                  copied_bg_surface,
                  dynamic_freqs_dest_rect.x,
                  dynamic_freqs_dest_rect.y,
                  dynamic_freqs_dest_rect.width,
                  dynamic_freqs_dest_rect.height,
                  1.0 );
    dynamic_freqs_surface.reset();
    dynamic_waves_surface.reset();

    surface_shake_and_blit( copied_bg_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ), true );
    copied_bg_surface.reset();

    // // put art on canvas, shakily
    // surface_shake_and_blit( input_data.project_art_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // // put title on canvas, shakily
    // surface_shake_and_blit( input_data.static_text_surface, frame_surface_to_save, ( colour_displace_intensity_scale * bass_intensity ) );

    // put warning on top, with alpha
    surface_blit( input_data.common_epilepsy_warning_surface, frame_surface_to_save, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, epilepsy_warning_alpha );

    // save canvas
    save_surface( frame_surface_to_save, input_data.project_temp_pictureset_picture_path );
    frame_surface_to_save.reset();
  }


  logger_->trace( "[thread_run] exit" );
}
