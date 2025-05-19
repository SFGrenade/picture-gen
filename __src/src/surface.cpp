#include "surface.h"

#include <cairo-ft.h>
#include <cairo.h>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <ft2build.h>
#include <iostream>
#include <random>

#include FT_FREETYPE_H

#include "utils.h"

void surface_blit( cairo_surface_t* src, cairo_surface_t* dest, double dest_x, double dest_y, double dest_width, double dest_height ) {
  double src_width = cairo_image_surface_get_width( src );
  double src_height = cairo_image_surface_get_height( src );

  cairo_t* cr = cairo_create( dest );
  cairo_save( cr );

  cairo_translate( cr, dest_x, dest_y );
  cairo_scale( cr, dest_width / src_width, dest_height / src_height );
  cairo_set_source_surface( cr, src, 0, 0 );
  cairo_paint( cr );

  cairo_restore( cr );
  cairo_destroy( cr );
}

cairo_surface_t* surface_load_file( std::filesystem::path const& filepath ) {
  cairo_surface_t* ret = nullptr;
  while( ( !ret ) || ( cairo_surface_status( ret ) != cairo_status_t::CAIRO_STATUS_SUCCESS ) ) {
    ret = cairo_image_surface_create_from_png( filepath.string().c_str() );
  }
  return ret;
}

cairo_surface_t* surface_create_size( int32_t width, int32_t height ) {
  cairo_surface_t* ret = nullptr;
  while( ( !ret ) || ( cairo_surface_status( ret ) != cairo_status_t::CAIRO_STATUS_SUCCESS ) ) {
    ret = cairo_image_surface_create( cairo_format_t::CAIRO_FORMAT_ARGB32, width, height );
  }
  return ret;
}

cairo_surface_t* surface_embed_in_overlay( cairo_surface_t* surface,
                                           int32_t overlay_width,
                                           int32_t overlay_height,
                                           double x,
                                           double y,
                                           double width,
                                           double height ) {
  cairo_surface_t* overlay = surface_create_size( overlay_width, overlay_height );

  surface_blit( surface, overlay, x, y, width, height );

  return overlay;
}

cairo_surface_t* surface_load_file_into_overlay( std::filesystem::path const& filepath,
                                                 int32_t overlay_width,
                                                 int32_t overlay_height,
                                                 int32_t x,
                                                 int32_t y,
                                                 int32_t width,
                                                 int32_t height ) {
  cairo_surface_t* surface = surface_load_file( filepath );
  cairo_surface_t* overlay = surface_embed_in_overlay( surface, overlay_width, overlay_height, x, y, width, height );
  cairo_surface_destroy( surface );
  return overlay;
}

cairo_surface_t* surface_render_text_into_overlay( std::filesystem::path const& filepath,
                                                   int32_t overlay_width,
                                                   int32_t overlay_height,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height ) {
  std::filesystem::path const font_path = std::filesystem::path( "C:" ) / "Windows" / "Fonts" / "NotoSans-Regular.ttf";
  int const font_size = 60;
  double const line_spacing = 1.1;

  std::ifstream file_stream( filepath );
  std::string file_content( ( std::istreambuf_iterator< char >( file_stream ) ), ( std::istreambuf_iterator< char >() ) );
  file_content = replace( file_content, "\r\n", "\n" );
  std::vector< std::string > content_lines = split_multiline( file_content );

  cairo_surface_t* surface = surface_create_size( width, height );
  {
    cairo_t* cr = cairo_create( surface );
    cairo_save( cr );

    cairo_font_options_t* cairo_ft_options = cairo_font_options_create();
    cairo_font_options_set_antialias( cairo_ft_options, cairo_antialias_t::CAIRO_ANTIALIAS_GRAY );

    cairo_font_face_t* cairo_ft_face = nullptr;
    FT_Library ft_library;
    if( 0 == FT_Init_FreeType( &ft_library ) ) {
      FT_Face ft_face;
      if( 0 == FT_New_Face( ft_library, font_path.string().c_str(), 0, &ft_face ) ) {
        cairo_ft_face = cairo_ft_font_face_create_for_ft_face( ft_face, 0 );
        FT_Done_Face( ft_face );
      }
      FT_Done_FreeType( ft_library );
    }
    cairo_set_font_face( cr, cairo_ft_face );
    cairo_set_font_size( cr, font_size );
    cairo_set_source_rgb( cr, 1, 1, 1 );

    double line_height = font_size * line_spacing;
    double total_text_height = content_lines.size() * line_height;

    // starting height
    double y = ( ( height - total_text_height ) / 2.0 ) + ( font_size / line_spacing );
    for( std::string const& content_line : content_lines ) {
      cairo_text_extents_t extents;
      cairo_text_extents( cr, content_line.c_str(), &extents );
      double x = ( width - extents.width ) / 2.0 - extents.x_bearing;

      cairo_move_to( cr, x, y );
      cairo_show_text( cr, content_line.c_str() );
      y += line_height;
    }

    cairo_restore( cr );
    cairo_destroy( cr );
    cairo_font_face_destroy( cairo_ft_face );
    cairo_font_options_destroy( cairo_ft_options );
  }

  cairo_surface_t* overlay = surface_embed_in_overlay( surface, overlay_width, overlay_height, x, y, width, height );
  cairo_surface_destroy( surface );
  return overlay;
}

void surface_fill( cairo_surface_t* s, double r, double g, double b, double a ) {
  cairo_t* cr = cairo_create( s );
  cairo_save( cr );

  cairo_set_source_rgb( cr, r, g, b );
  cairo_paint_with_alpha( cr, a );

  cairo_restore( cr );
  cairo_destroy( cr );
}

cairo_surface_t* surface_copy( cairo_surface_t* s ) {
  double s_width = cairo_image_surface_get_width( s );
  double s_height = cairo_image_surface_get_height( s );
  cairo_surface_t* ret = surface_create_size( s_width, s_height );
  surface_blit( s, ret, 0, 0, s_width, s_height );
  return ret;
}

static uint8_t int_surface_unpremultiply( uint8_t channel, uint8_t alpha ) {
  if( alpha == 0 )
    return 0;
  return std::clamp( std::round( ( double( channel ) * 255.0 + double( alpha ) / 2.0 ) / double( alpha ) ), 0.0, 255.0 );
}

static uint8_t int_surface_premultiply( uint8_t channel, uint8_t alpha ) {
  return std::clamp( std::round( double( channel ) * ( double( alpha ) / 255.0 ) ), 0.0, 255.0 );
}

/**
 * @brief blit single channel from src to dst
 *
 * @param src source
 * @param dst destination
 * @param channel_offset 0 = blue, 1 = green, 2 = red
 * @param x_offset x offset
 * @param y_offset y offset
 */
void int_surface_blit_channel( cairo_surface_t* src, cairo_surface_t* dst, int32_t channel_offset, int32_t x_offset, int32_t y_offset ) {
  cairo_surface_flush( src );
  cairo_surface_flush( dst );

  uint8_t* src_data = static_cast< uint8_t* >( cairo_image_surface_get_data( src ) );
  uint8_t* dst_data = static_cast< uint8_t* >( cairo_image_surface_get_data( dst ) );

  int32_t src_stride = cairo_image_surface_get_stride( src );
  int32_t dst_stride = cairo_image_surface_get_stride( dst );

  int32_t width = cairo_image_surface_get_width( src );
  int32_t height = cairo_image_surface_get_height( src );
  int32_t dest_width = cairo_image_surface_get_width( dst );
  int32_t dest_height = cairo_image_surface_get_height( dst );

  for( int32_t y = 0; y < height; ++y ) {
    for( int32_t x = 0; x < width; ++x ) {
      int32_t src_idx = y * src_stride + x * 4;
      uint8_t src_alpha = src_data[src_idx + 3];
      uint8_t src_val_premult = src_data[src_idx + channel_offset];

      uint8_t src_val = int_surface_unpremultiply( src_val_premult, src_alpha );

      int32_t dst_x = my_mod( x + x_offset, dest_width );
      int32_t dst_y = my_mod( y + y_offset, dest_height );

      int32_t dst_idx = dst_y * dst_stride + dst_x * 4;
      uint8_t dst_alpha = src_alpha;  // dst_data[dst_idx + 3];

      dst_data[dst_idx + channel_offset] = int_surface_premultiply( src_val, dst_alpha );
    }
  }

  cairo_surface_mark_dirty( dst );
}

void surface_set_alpha( cairo_surface_t* src ) {
  int32_t src_width = cairo_image_surface_get_width( src );
  int32_t src_height = cairo_image_surface_get_height( src );
  int32_t src_stride = cairo_image_surface_get_stride( src );

  cairo_surface_flush( src );

  uint8_t* src_data = static_cast< uint8_t* >( cairo_image_surface_get_data( src ) );

  for( int32_t y = 0; y < src_height; ++y ) {
    for( int32_t x = 0; x < src_width; ++x ) {
      int32_t src_idx = y * src_stride + x * 4;

      uint8_t src_red = src_data[src_idx + 2];
      uint8_t src_green = src_data[src_idx + 1];
      uint8_t src_blue = src_data[src_idx + 0];
      uint8_t src_alpha = src_data[src_idx + 3];

      src_data[src_idx + 3] = std::max( std::max( src_red, src_green ), src_blue );
    }
  }

  cairo_surface_mark_dirty( src );
}

void surface_shake_and_blit( cairo_surface_t* source, cairo_surface_t* dest, double shake_intensity, bool red_only ) {
  int32_t source_width = cairo_image_surface_get_width( source );
  int32_t source_height = cairo_image_surface_get_height( source );
  int32_t dest_width = cairo_image_surface_get_width( dest );
  int32_t dest_height = cairo_image_surface_get_height( dest );

  cairo_surface_t* shaken = surface_create_size( source_width, source_height );
  surface_fill( shaken, 0.0, 0.0, 0.0, 1.0 );

  std::random_device random_device;
  std::mt19937_64 gen( random_device() );
  std::uniform_int_distribution<> dist( static_cast< int >( -128.0 * shake_intensity ), static_cast< int >( 128.0 * shake_intensity ) );

  int32_t x_offset_red = dist( gen );
  int32_t y_offset_red = dist( gen );
  int32_t x_offset_green = dist( gen );
  int32_t y_offset_green = dist( gen );
  int32_t x_offset_blue = dist( gen );
  int32_t y_offset_blue = dist( gen );
  // int32_t x_offset_alpha = 0;
  // int32_t y_offset_alpha = 0;
  if( red_only ) {
    x_offset_blue = x_offset_green = x_offset_red;
    y_offset_blue = y_offset_green = y_offset_red;
  }

  int_surface_blit_channel( source, shaken, 2, x_offset_red, y_offset_red );
  int_surface_blit_channel( source, shaken, 1, x_offset_green, y_offset_green );
  int_surface_blit_channel( source, shaken, 0, x_offset_blue, y_offset_blue );
  surface_set_alpha( shaken );

  surface_blit( shaken, dest, 0, 0, dest_width, dest_height );
  cairo_surface_destroy( shaken );
}
