#include "surface.h"

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <random>

#include "cairo.h"
#include "loggerFactory.h"
#include "utils.h"

std::shared_ptr< cairo_surface_t > make_surface_shared_ptr( cairo_surface_t* s ) {
  return std::shared_ptr< cairo_surface_t >( s, []( cairo_surface_t* p ) { cairo_surface_destroy( p ); } );
}

void surface_blit( std::shared_ptr< cairo_surface_t > src,
                   std::shared_ptr< cairo_surface_t > dest,
                   double const dest_x,
                   double const dest_y,
                   double const dest_width,
                   double const dest_height,
                   double const alpha ) {
  double const src_width = cairo_image_surface_get_width( src.get() );
  double const src_height = cairo_image_surface_get_height( src.get() );

  cairo_t* cr = cairo_create( dest.get() );
  cairo_save( cr );

  cairo_translate( cr, dest_x, dest_y );
  cairo_scale( cr, dest_width / src_width, dest_height / src_height );
  cairo_set_source_surface( cr, src.get(), 0, 0 );
  cairo_paint_with_alpha( cr, std::clamp( alpha, 0.0, 1.0 ) );

  cairo_restore( cr );
  cairo_destroy( cr );
}

std::shared_ptr< cairo_surface_t > surface_load_file( std::filesystem::path const& filepath ) {
  std::shared_ptr< cairo_surface_t > ret = nullptr;
  while( ( !ret ) || ( cairo_surface_status( ret.get() ) != cairo_status_t::CAIRO_STATUS_SUCCESS ) ) {
    ret = make_surface_shared_ptr( cairo_image_surface_create_from_png( filepath.string().c_str() ) );
  }
  return ret;
}

std::shared_ptr< cairo_surface_t > surface_create_size( int32_t const width, int32_t const height ) {
  std::shared_ptr< cairo_surface_t > ret = nullptr;
  while( ( !ret ) || ( cairo_surface_status( ret.get() ) != cairo_status_t::CAIRO_STATUS_SUCCESS ) ) {
    ret = make_surface_shared_ptr( cairo_image_surface_create( cairo_format_t::CAIRO_FORMAT_ARGB32, width, height ) );
  }
  return ret;
}

std::shared_ptr< cairo_surface_t > surface_embed_in_overlay( std::shared_ptr< cairo_surface_t > surface,
                                                             int32_t const overlay_width,
                                                             int32_t const overlay_height,
                                                             double const x,
                                                             double const y,
                                                             double const width,
                                                             double const height ) {
  std::shared_ptr< cairo_surface_t > overlay = surface_create_size( overlay_width, overlay_height );

  surface_blit( surface, overlay, x, y, width, height );

  return overlay;
}

std::shared_ptr< cairo_surface_t > surface_load_file_into_overlay( std::filesystem::path const& filepath,
                                                                   int32_t const overlay_width,
                                                                   int32_t const overlay_height,
                                                                   int32_t const x,
                                                                   int32_t const y,
                                                                   int32_t const width,
                                                                   int32_t const height ) {
  std::shared_ptr< cairo_surface_t > surface = surface_load_file( filepath );
  std::shared_ptr< cairo_surface_t > overlay = surface_embed_in_overlay( surface, overlay_width, overlay_height, x, y, width, height );
  surface.reset();
  return overlay;
}

void int_surface_load_font( FT_Library ft_library, std::filesystem::path const& font_path, std::function< void( cairo_font_face_t* ) > callback ) {
  spdlogger logger = LoggerFactory::get_logger( "int_surface_load_font" );
  logger->trace( "enter: font_path: {:?}", font_path.string() );

  cairo_font_face_t* cairo_ft_face = nullptr;
  FT_Face ft_face = nullptr;
  FT_Error ft_ret;
  if( ( ft_ret = FT_New_Face( ft_library, font_path.string().c_str(), 0, &ft_face ) ) == 0 ) {
    cairo_ft_face = cairo_ft_font_face_create_for_ft_face( ft_face, 0 );

    callback( cairo_ft_face );
    // callback( nullptr );

    cairo_font_face_destroy( cairo_ft_face );
    if( ( ft_ret = FT_Done_Face( ft_face ) ) != 0 ) {
      logger->error( "FT_New_Face returned {}", ft_ret );
    }
  } else {
    logger->error( "FT_New_Face returned {}", ft_ret );
  }

  logger->trace( "exit" );
}

std::shared_ptr< cairo_surface_t > surface_render_text_into_overlay( cairo_font_face_t* font_face,
                                                                     std::filesystem::path const& filepath,
                                                                     int32_t const overlay_width,
                                                                     int32_t const overlay_height,
                                                                     int32_t const x,
                                                                     int32_t const y,
                                                                     int32_t const width,
                                                                     int32_t const height ) {
  spdlogger logger = LoggerFactory::get_logger( "surface_render_text_into_overlay" );
  logger->trace( "enter: ft_library: {}, filepath: {:?}, overlay_width: {}, overlay_height: {}, x: {}, y: {}, width: {}, height: {}",
                 static_cast< void* >( font_face ),
                 filepath.string(),
                 overlay_width,
                 overlay_height,
                 x,
                 y,
                 width,
                 height );

  int font_size = 60;
  double line_spacing = 1.1;

  logger->trace( "font_size: {}", font_size );
  logger->trace( "line_spacing: {}", line_spacing );

  std::ifstream file_stream( filepath );
  std::string file_content( ( std::istreambuf_iterator< char >( file_stream ) ), ( std::istreambuf_iterator< char >() ) );
  file_content = replace( file_content, "\r\n", "\n" );
  std::vector< std::string > content_lines = split_multiline( file_content );

  if( content_lines.size() == 0 ) {
    logger->trace( "exit: no content" );
    return surface_create_size( overlay_width, overlay_height );
  }

  double total_text_height = 0.0;  // content_lines.size() * line_height;

  logger->trace( "total_text_height: {}", total_text_height );

  std::shared_ptr< cairo_surface_t > surface = surface_create_size( width, height );
  cairo_t* cr = cairo_create( surface.get() );
  cairo_save( cr );

  logger->trace( "surface: {}", static_cast< void* >( surface.get() ) );
  logger->trace( "cr: {}", static_cast< void* >( cr ) );

  cairo_font_options_t* cairo_ft_options = cairo_font_options_create();
  cairo_get_font_options( cr, cairo_ft_options );
  cairo_font_options_set_antialias( cairo_ft_options, cairo_antialias_t::CAIRO_ANTIALIAS_GRAY );
  cairo_set_font_options( cr, cairo_ft_options );

  logger->trace( "cairo_ft_options: {}", static_cast< void* >( cairo_ft_options ) );

  cairo_set_font_face( cr, font_face );
  cairo_set_font_size( cr, font_size );
  cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 1.0 );

  for( int i = 0; i < content_lines.size(); i++ ) {
    std::string content_line = content_lines[i];
    cairo_text_extents_t extents;
    cairo_text_extents( cr, content_line.c_str(), &extents );

    double line_height = extents.height * line_spacing;
    total_text_height += line_height;
  }

  logger->trace( "total_text_height: {}", total_text_height );

  for( size_t i = 0; i < content_lines.size(); i++ ) {
    std::string content_line = content_lines[i];
    cairo_text_extents_t extents;
    cairo_text_extents( cr, content_line.c_str(), &extents );

    double line_height = extents.height * line_spacing;
    double text_x = ( width - extents.width ) / 2.0 - extents.x_bearing;
    double text_y = ( ( height - total_text_height ) / 2.0 ) - extents.y_bearing + ( line_height * i );

    cairo_move_to( cr, text_x, text_y );
    cairo_show_text( cr, content_line.c_str() );
  }

  cairo_font_options_destroy( cairo_ft_options );

  cairo_restore( cr );
  cairo_destroy( cr );

  std::shared_ptr< cairo_surface_t > overlay = surface_embed_in_overlay( surface, overlay_width, overlay_height, x, y, width, height );
  surface.reset();
  return overlay;
}

std::shared_ptr< cairo_surface_t > surface_render_text_advanced_into_overlay( cairo_font_face_t* header_font_face,
                                                                              cairo_font_face_t* content_font_face,
                                                                              std::filesystem::path const& filepath,
                                                                              int32_t const overlay_width,
                                                                              int32_t const overlay_height,
                                                                              int32_t const x,
                                                                              int32_t const y,
                                                                              int32_t const width,
                                                                              int32_t const height ) {
  spdlogger logger = LoggerFactory::get_logger( "surface_render_text_advanced_into_overlay" );
  logger
      ->trace( "enter: header_font_face: {}, content_font_face: {}, filepath: {:?}, overlay_width: {}, overlay_height: {}, x: {}, y: {}, width: {}, height: {}",
               static_cast< void* >( header_font_face ),
               static_cast< void* >( content_font_face ),
               filepath.string(),
               overlay_width,
               overlay_height,
               x,
               y,
               width,
               height );

  int content_font_size = 60;
  int header_font_size = content_font_size * 1.8;
  double line_spacing = 1.1;

  logger->trace( "content_font_size: {}", content_font_size );
  logger->trace( "header_font_size: {}", header_font_size );
  logger->trace( "line_spacing: {}", line_spacing );

  std::ifstream file_stream( filepath );
  std::string file_content( ( std::istreambuf_iterator< char >( file_stream ) ), ( std::istreambuf_iterator< char >() ) );
  file_content = replace( file_content, "\r\n", "\n" );
  std::vector< std::string > content_lines = split_multiline( file_content );

  if( content_lines.size() == 0 ) {
    logger->trace( "exit: no content" );
    return surface_create_size( overlay_width, overlay_height );
  }

  double header_line_height = 0.0;         // header_font_size * line_spacing;
  double header_total_text_height = 0.0;   // header_line_height;
  double content_line_height = 0.0;        // content_font_size * line_spacing;
  double content_total_text_height = 0.0;  // ( content_lines.size() - 1 ) * content_line_height;
  double total_text_height = 0.0;          // header_total_text_height + content_total_text_height;

  logger->trace( "header_line_height: {}", header_line_height );
  logger->trace( "header_total_text_height: {}", header_total_text_height );
  logger->trace( "content_line_height: {}", content_line_height );
  logger->trace( "content_total_text_height: {}", content_total_text_height );
  logger->trace( "total_text_height: {}", total_text_height );

  std::shared_ptr< cairo_surface_t > surface = surface_create_size( width, height );
  cairo_t* cr = cairo_create( surface.get() );
  cairo_save( cr );

  logger->trace( "surface: {}", static_cast< void* >( surface.get() ) );
  logger->trace( "cr: {}", static_cast< void* >( cr ) );

  cairo_font_options_t* cairo_ft_options = cairo_font_options_create();
  cairo_get_font_options( cr, cairo_ft_options );
  cairo_font_options_set_antialias( cairo_ft_options, cairo_antialias_t::CAIRO_ANTIALIAS_GRAY );
  cairo_set_font_options( cr, cairo_ft_options );

  logger->trace( "cairo_ft_options: {}", static_cast< void* >( cairo_ft_options ) );

  {
    // header
    cairo_set_font_face( cr, header_font_face );
    cairo_set_font_size( cr, header_font_size );
    cairo_set_source_rgba( cr, 1.0, 0.0, 0.0, 1.0 );

    std::string content_line = content_lines[0];
    cairo_text_extents_t extents;
    cairo_text_extents( cr, content_line.c_str(), &extents );

    header_line_height = extents.height;

    header_line_height = header_line_height * line_spacing;
    header_total_text_height = header_line_height;
    total_text_height += header_total_text_height;
  }
  {
    // content
    cairo_set_font_face( cr, content_font_face );
    cairo_set_font_size( cr, content_font_size );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 1.0 );

    for( int i = 1; i < content_lines.size(); i++ ) {
      std::string content_line = content_lines[i];
      cairo_text_extents_t extents;
      cairo_text_extents( cr, content_line.c_str(), &extents );

      content_line_height = extents.height;

      content_line_height = content_line_height * line_spacing;
      content_total_text_height += content_line_height;
    }

    total_text_height += content_total_text_height;
  }

  logger->trace( "header_line_height: {}", header_line_height );
  logger->trace( "header_total_text_height: {}", header_total_text_height );
  logger->trace( "content_line_height: {}", content_line_height );
  logger->trace( "content_total_text_height: {}", content_total_text_height );
  logger->trace( "total_text_height: {}", total_text_height );

  // starting height
  double text_x = 0.0;
  double text_y = 0.0;
  {
    // draw header
    cairo_set_font_face( cr, header_font_face );
    cairo_set_font_size( cr, header_font_size );
    cairo_set_source_rgba( cr, 1.0, 0.0, 0.0, 1.0 );

    std::string content_line = content_lines[0];
    cairo_text_extents_t extents;
    cairo_text_extents( cr, content_line.c_str(), &extents );

    text_x = ( width - extents.width ) / 2.0 - extents.x_bearing;
    text_y = ( ( height - total_text_height ) / 2.0 ) - extents.y_bearing;

    cairo_move_to( cr, text_x, text_y );
    cairo_show_text( cr, content_line.c_str() );
    text_y += header_line_height;
  }
  {
    // draw content
    cairo_set_font_face( cr, content_font_face );
    cairo_set_font_size( cr, content_font_size );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 1.0 );

    for( size_t i = 1; i < content_lines.size(); i++ ) {
      std::string content_line = content_lines[i];
      cairo_text_extents_t extents;
      cairo_text_extents( cr, content_line.c_str(), &extents );
      text_x = ( width - extents.width ) / 2.0 - extents.x_bearing;

      cairo_move_to( cr, text_x, text_y );
      cairo_show_text( cr, content_line.c_str() );
      text_y += content_line_height;
    }
  }

  cairo_font_options_destroy( cairo_ft_options );

  cairo_restore( cr );
  cairo_destroy( cr );

  std::shared_ptr< cairo_surface_t > overlay = surface_embed_in_overlay( surface, overlay_width, overlay_height, x, y, width, height );
  surface.reset();

  logger->trace( "exit: overlay: {}", static_cast< void* >( overlay.get() ) );
  return overlay;
}

void surface_fill( std::shared_ptr< cairo_surface_t > s, double const r, double const g, double const b, double const a ) {
  cairo_t* cr = cairo_create( s.get() );
  cairo_save( cr );

  cairo_set_source_rgb( cr, r, g, b );
  cairo_paint_with_alpha( cr, a );

  cairo_restore( cr );
  cairo_destroy( cr );
}

std::shared_ptr< cairo_surface_t > surface_copy( std::shared_ptr< cairo_surface_t > s ) {
  double const s_width = cairo_image_surface_get_width( s.get() );
  double const s_height = cairo_image_surface_get_height( s.get() );
  std::shared_ptr< cairo_surface_t > ret = surface_create_size( s_width, s_height );
  surface_blit( s, ret, 0, 0, s_width, s_height );
  return ret;
}

static uint8_t int_surface_unpremultiply( uint8_t const channel, uint8_t const alpha ) {
  if( alpha == 0 )
    return 0;
  return std::clamp( std::round( ( double( channel ) * 255.0 + double( alpha ) / 2.0 ) / double( alpha ) ), 0.0, 255.0 );
}

static uint8_t int_surface_premultiply( uint8_t const channel, uint8_t const alpha ) {
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
void int_surface_blit_channel( std::shared_ptr< cairo_surface_t > src,
                               std::shared_ptr< cairo_surface_t > dst,
                               int32_t const channel_offset,
                               int32_t const x_offset,
                               int32_t const y_offset ) {
  cairo_surface_flush( src.get() );
  cairo_surface_flush( dst.get() );

  uint8_t* src_data = static_cast< uint8_t* >( cairo_image_surface_get_data( src.get() ) );
  uint8_t* dst_data = static_cast< uint8_t* >( cairo_image_surface_get_data( dst.get() ) );

  int32_t const src_stride = cairo_image_surface_get_stride( src.get() );
  int32_t const dst_stride = cairo_image_surface_get_stride( dst.get() );

  int32_t const width = cairo_image_surface_get_width( src.get() );
  int32_t const height = cairo_image_surface_get_height( src.get() );
  int32_t const dest_width = cairo_image_surface_get_width( dst.get() );
  int32_t const dest_height = cairo_image_surface_get_height( dst.get() );

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

  cairo_surface_mark_dirty( dst.get() );
}

void surface_set_alpha( std::shared_ptr< cairo_surface_t > src ) {
  int32_t const src_width = cairo_image_surface_get_width( src.get() );
  int32_t const src_height = cairo_image_surface_get_height( src.get() );
  int32_t const src_stride = cairo_image_surface_get_stride( src.get() );

  cairo_surface_flush( src.get() );

  uint8_t* src_data = static_cast< uint8_t* >( cairo_image_surface_get_data( src.get() ) );

  for( int32_t y = 0; y < src_height; ++y ) {
    for( int32_t x = 0; x < src_width; ++x ) {
      int32_t const src_idx = y * src_stride + x * 4;

      uint8_t const src_red = src_data[src_idx + 2];
      uint8_t const src_green = src_data[src_idx + 1];
      uint8_t const src_blue = src_data[src_idx + 0];
      uint8_t const src_alpha = src_data[src_idx + 3];

      src_data[src_idx + 3] = std::max( std::max( src_red, src_green ), src_blue );
    }
  }

  cairo_surface_mark_dirty( src.get() );
}

void surface_shake_and_blit( std::shared_ptr< cairo_surface_t > source, std::shared_ptr< cairo_surface_t > dest, double shake_intensity, bool red_only ) {
  int32_t const source_width = cairo_image_surface_get_width( source.get() );
  int32_t const source_height = cairo_image_surface_get_height( source.get() );
  int32_t const dest_width = cairo_image_surface_get_width( dest.get() );
  int32_t const dest_height = cairo_image_surface_get_height( dest.get() );

  std::shared_ptr< cairo_surface_t > shaken = surface_create_size( source_width, source_height );
  surface_fill( shaken, 0.0, 0.0, 0.0, 1.0 );

  std::random_device random_device;
  std::mt19937_64 gen( random_device() );
  std::uniform_int_distribution<> dist( static_cast< int >( -128.0 * shake_intensity ), static_cast< int >( 128.0 * shake_intensity ) );

  int32_t const x_offset_red = dist( gen );
  int32_t const y_offset_red = dist( gen );
  int32_t x_offset_green = dist( gen );
  int32_t y_offset_green = dist( gen );
  int32_t x_offset_blue = dist( gen );
  int32_t y_offset_blue = dist( gen );
  // int32_t const x_offset_alpha = 0;
  // int32_t const y_offset_alpha = 0;
  if( red_only ) {
    x_offset_blue = x_offset_green = x_offset_red;
    y_offset_blue = y_offset_green = y_offset_red;
  }

  int_surface_blit_channel( source, shaken, 2, x_offset_red, y_offset_red );
  int_surface_blit_channel( source, shaken, 1, x_offset_green, y_offset_green );
  int_surface_blit_channel( source, shaken, 0, x_offset_blue, y_offset_blue );
  surface_set_alpha( shaken );

  surface_blit( shaken, dest, 0, 0, dest_width, dest_height );
  shaken.reset();
}
