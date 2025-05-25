#pragma once

#include <cairo-ft.h>
#include <cairo.h>
#include <cstdint>
#include <filesystem>

#include "_spdlog.h"

class FontManager {
  public:
  static void init( std::filesystem::path const& base_font_path );
  static void deinit();

  static cairo_font_face_t* get_font_face( std::string const& name );

  private:
  static spdlogger logger_;
  static FT_Library ft_library_;
  static std::map<std::string, std::pair<FT_Face, cairo_font_face_t*>> font_map_;
};
