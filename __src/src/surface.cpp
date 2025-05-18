#include "surface.h"

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <iostream>
#include <random>

#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "utils.h"

SDL_Surface* load_image_and_check_surface( std::filesystem::path const& filepath ) {
  SDL_Surface* file_surface = nullptr;
  while( file_surface == nullptr ) {
    file_surface = IMG_Load( filepath.string().c_str() );
  }
  SDL_Surface* ret = nullptr;
  SDL_Surface* tmp = create_rgb_surface(1, 1);
  while( ret == nullptr ) {
    ret = SDL_ConvertSurface(file_surface, tmp->format, tmp->flags);
  }
  SDL_FreeSurface(tmp);
  SDL_FreeSurface(file_surface);
  return ret;
}

SDL_Surface* create_rgb_surface( int width, int height ) {
  SDL_Surface* ret = nullptr;
  while( ret == nullptr ) {
    ret = SDL_CreateRGBSurface( 0, width, height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF );
  }
  return ret;
}

SDL_Surface* put_surface_into_overlay_at( SDL_Surface* surface,
                                         int32_t overlay_w,
                                         int32_t overlay_h,
                                         int32_t x,
                                         int32_t y,
                                         int32_t w,
                                         int32_t h ) {
  SDL_Surface* overlay = create_rgb_surface( overlay_w, overlay_h );

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  if( 0 != SDL_BlitScaled( surface, nullptr, overlay, &rect ) ) {
    std::cerr << fmt::format( "error: SDL_BlitScaled: {}", SDL_GetError() ) << std::endl;
  }
  return overlay;
}

SDL_Surface* load_image_into_overlay_at( std::filesystem::path const& filepath,
                                         int32_t overlay_w,
                                         int32_t overlay_h,
                                         int32_t x,
                                         int32_t y,
                                         int32_t w,
                                         int32_t h ) {
  SDL_Surface* surface = load_image_and_check_surface( filepath );
  SDL_Surface* overlay = put_surface_into_overlay_at( surface, overlay_w, overlay_h, x, y, w, h );
  SDL_FreeSurface( surface );
  return overlay;
}

void set_colour_on_surface( SDL_Surface* s, uint32_t pixel ) {
  for( int y = 0; y < s->h; y++ ) {
    for( int x = 0; x < s->w; x++ ) {
      static_cast< uint32_t* >( s->pixels )[( y * ( s->pitch / s->format->BytesPerPixel ) ) + ( x )] = pixel;
    }
  }
}

SDL_Surface* copy_surface( SDL_Surface* s ) {
  SDL_Surface* ret = create_rgb_surface( s->w, s->h );
  for( int y = 0; y < s->h; y++ ) {
    for( int x = 0; x < s->w; x++ ) {
      set_pixel( ret, x, y, get_pixel( s, x, y ) );
    }
  }
  return ret;
}

uint32_t get_pixel( SDL_Surface* s, size_t x, size_t y ) {
  return static_cast< uint32_t* >( s->pixels )[( y * ( s->pitch / s->format->BytesPerPixel ) ) + ( x )];
}

uint8_t get_pixel_r( SDL_Surface* s, size_t x, size_t y ) {
  return static_cast< uint8_t >( ( get_pixel( s, x, y ) >> ( 3 * 8 ) ) & 0xFF );
}

uint8_t get_pixel_g( SDL_Surface* s, size_t x, size_t y ) {
  return static_cast< uint8_t >( ( get_pixel( s, x, y ) >> ( 2 * 8 ) ) & 0xFF );
}

uint8_t get_pixel_b( SDL_Surface* s, size_t x, size_t y ) {
  return static_cast< uint8_t >( ( get_pixel( s, x, y ) >> ( 1 * 8 ) ) & 0xFF );
}

uint8_t get_pixel_a( SDL_Surface* s, size_t x, size_t y ) {
  return static_cast< uint8_t >( ( get_pixel( s, x, y ) >> ( 0 * 8 ) ) & 0xFF );
}

void set_pixel( SDL_Surface* s, size_t x, size_t y, uint32_t pixel ) {
  static_cast< uint32_t* >( s->pixels )[( y * ( s->pitch / s->format->BytesPerPixel ) ) + ( x )] = pixel;
}

void set_pixel_rgba( SDL_Surface* s, size_t x, size_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
  uint32_t pixel = 0x00000000;
  pixel = pixel | ( static_cast< uint32_t >( r & 0xFF ) << ( 3 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( g & 0xFF ) << ( 2 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( b & 0xFF ) << ( 1 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( a & 0xFF ) << ( 0 * 8 ) );
  set_pixel( s, x, y, pixel );
}

void set_big_pixel( SDL_Surface* s, size_t x, size_t y, uint32_t pixel ) {
  size_t from_x;
  size_t from_y;
  size_t to_x;
  size_t to_y;

  if (x == 0) {
    from_x = x;
  } else {
    from_x = x - 1;
  }
  if (y == 0) {
    from_y = y;
  } else {
    from_y = y - 1;
  }
  if (x == s->w - 1) {
    to_x = x;
  } else {
    to_x = x + 1;
  }
  if (y == s->h - 1) {
    to_y = y;
  } else {
    to_y = y + 1;
  }

  for (size_t n_y = from_y; n_y <= to_y; n_y++) {
    for (size_t n_x = from_x; n_x <= to_x; n_x++) {
      set_pixel(s, n_x, n_y, pixel);
    }
  }
}

void set_big_pixel_rgba( SDL_Surface* s, size_t x, size_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
  uint32_t pixel = 0x00000000;
  pixel = pixel | ( static_cast< uint32_t >( r & 0xFF ) << ( 3 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( g & 0xFF ) << ( 2 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( b & 0xFF ) << ( 1 * 8 ) );
  pixel = pixel | ( static_cast< uint32_t >( a & 0xFF ) << ( 0 * 8 ) );
  set_big_pixel( s, x, y, pixel );
}

void shake_and_blit_surface( SDL_Surface* source, SDL_Surface* dest, double shake_intensity, bool red_only ) {
  SDL_Surface* shaken = create_rgb_surface( source->w, source->h );
  set_colour_on_surface( shaken, 0x000000FF );

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

  for( int32_t y = 0; y < source->h; y++ ) {
    int32_t y_red = my_mod( y + y_offset_red, source->h );
    int32_t y_green = my_mod( y + y_offset_green, source->h );
    int32_t y_blue = my_mod( y + y_offset_blue, source->h );
    // int32_t y_alpha = my_mod( y + y_offset_alpha, source->h );
    if (red_only) {
      y_green = y_red;
      y_blue = y_red;
    }
    for( int32_t x = 0; x < source->w; x++ ) {
      int32_t x_red = my_mod( x + x_offset_red, source->w );
      int32_t x_green = my_mod( x + x_offset_green, source->w );
      int32_t x_blue = my_mod( x + x_offset_blue, source->w );
      // int32_t x_alpha = my_mod( x + x_offset_alpha, source->w );
      if (red_only) {
        x_green = x_red;
        x_blue = x_red;
      }

      uint8_t alpha = static_cast< uint8_t >( ( static_cast< uint32_t >( get_pixel_a( source, x_red, y_red ) )
                                                + static_cast< uint32_t >( get_pixel_a( source, x_green, y_green ) )
                                                + static_cast< uint32_t >( get_pixel_a( source, x_blue, y_blue ) ) )
                                              / 3 );

      set_pixel_rgba( shaken,
                      x,
                      y,
                      get_pixel_r( source, x_red, y_red ),
                      get_pixel_g( source, x_green, y_green ),
                      get_pixel_b( source, x_blue, y_blue ),
                      alpha );
    }
  }

  if( 0 != SDL_BlitScaled( shaken, nullptr, dest, nullptr ) ) {
    std::cerr << fmt::format( "error: SDL_BlitScaled: {}", SDL_GetError() ) << std::endl;
  }
  SDL_FreeSurface( shaken );
}
