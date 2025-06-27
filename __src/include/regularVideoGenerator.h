#pragma once

#include <cairo-ft.h>
#include <cairo.h>
#include <fftw3.h>
#include <filesystem>

#include "_spdlog.h"

class RegularVideoGenerator {
  public:
  static double const FPS;
  static int32_t const VIDEO_WIDTH;
  static int32_t const VIDEO_HEIGHT;
  static int32_t const IIR_FILTER_ORDER;
  static double const BASS_LP_CUTOFF;
  static double const BASS_HP_CUTOFF;
  static uint16_t const EXTRA_FFT_SIZE;
  static double const FFT_INPUT_SIZE_MULT;
  static double const EPILEPSY_WARNING_VISIBLE_SECONDS;
  static double const EPILEPSY_WARNING_FADEOUT_SECONDS;
  static uint32_t const FFTW_PLAN_FLAGS;
  static std::string const EPILEPSY_WARNING_HEADER_FONT;
  static std::string const EPILEPSY_WARNING_CONTENT_FONT;
  static double const FFT_DISPLAY_MIN_FREQ;
  static double const FFT_DISPLAY_MAX_FREQ;
  static double const FFT_DISPLAY_MIN_MAG;
  static double const FFT_DISPLAY_MAX_MAG;

  private:
  struct AudioData {
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t total_pcm_frame_count;
    std::shared_ptr< float[] > sample_data = nullptr;  // dr_wav allocated
    float sample_min = 0.0;
    float sample_max = 0.0;
    std::shared_ptr< float[] > processed_sample_data = nullptr;  // new[] allocated
    float processed_sample_min = 0.0;
    float processed_sample_max = 0.0;
    double duration;
  };
  struct ThreadInputData {
    uint64_t i;
    uint64_t amount_output_frames;
    std::filesystem::path project_temp_pictureset_picture_path;
    int64_t pcm_frame_offset;
    uint64_t pcm_frame_count;
    std::shared_ptr< AudioData > audio_data_ptr = nullptr;
    std::shared_ptr< cairo_surface_t > common_epilepsy_warning_surface = nullptr;
    std::shared_ptr< cairo_surface_t > common_bg_surface = nullptr;
    std::shared_ptr< cairo_surface_t > common_circle_surface = nullptr;
    std::shared_ptr< cairo_surface_t > project_art_surface = nullptr;
    std::shared_ptr< cairo_surface_t > static_text_surface = nullptr;
    std::shared_ptr< fftwf_plan_s > fft_plan = nullptr;
    uint64_t fft_input_size;
    std::shared_ptr< float[] > fft_input = nullptr;
    std::shared_ptr< double[] > fft_window = nullptr;
    uint64_t fft_output_size;
    std::shared_ptr< fftwf_complex[] > fft_output = nullptr;
  };
  struct FrameInformation {
    size_t amount_output_frames;
    double pcm_frames_per_output_frame;
    size_t fft_size;
    std::shared_ptr< cairo_surface_t > common_epilepsy_warning_surface = nullptr;
    std::shared_ptr< cairo_surface_t > common_bg_surface = nullptr;
    std::shared_ptr< cairo_surface_t > common_circle_surface = nullptr;
    std::shared_ptr< cairo_surface_t > project_art_surface = nullptr;
    std::shared_ptr< cairo_surface_t > static_text_surface = nullptr;
    std::vector< std::vector< RegularVideoGenerator::ThreadInputData > > thread_input_lists;
    std::vector< std::thread > thread_list;
  };

  public:
  static void init( std::filesystem::path const& project_path, std::filesystem::path const& common_path );
  static void deinit();

  static void render();

  private:
  static void prepare_audio();
  static void calculate_frames();
  static void prepare_surfaces();
  static void prepare_threads();
  static void prepare_fft();
  static void start_threads();
  static void join_threads();
  static void clean_up();

  private:
  static void save_surface( std::shared_ptr< cairo_surface_t > surface, std::filesystem::path const& file_path );
  static void create_lowpass_for_audio_data();
  static void create_epilepsy_warning();
  static void draw_samples_on_surface( std::shared_ptr< cairo_surface_t > surface, RegularVideoGenerator::ThreadInputData const& input_data );
  static void draw_freqs_on_surface( std::shared_ptr< cairo_surface_t > surface, RegularVideoGenerator::ThreadInputData const& input_data );
  static void thread_run( std::vector< RegularVideoGenerator::ThreadInputData > inputs );

  private:
  // general class things
  static spdlogger logger_;
  static bool is_ready_;
  // need to be given
  static std::filesystem::path project_path_;
  static std::filesystem::path common_path_;

  // need to be present
  static std::filesystem::path common_epilepsy_warning_path_;
  static std::filesystem::path common_bg_path_;
  static std::filesystem::path common_circle_path_;
  static std::filesystem::path project_art_path_;
  static std::filesystem::path project_audio_path_;
  static std::filesystem::path project_title_path_;

  // will be created
  static std::filesystem::path project_temp_pictureset_path_;

  // will be computed
  static std::shared_ptr< RegularVideoGenerator::AudioData > audio_data_;
  static std::shared_ptr< RegularVideoGenerator::FrameInformation > frame_information_;
};
