#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdint>
#include <filesystem>

SDL_Surface* load_image_and_check_surface( std::filesystem::path const& filepath );

SDL_Surface* create_rgb_surface( int width, int height );

SDL_Surface* put_surface_into_overlay_at( SDL_Surface* surface,
                                         int32_t overlay_w,
                                         int32_t overlay_h,
                                         int32_t x,
                                         int32_t y,
                                         int32_t w,
                                         int32_t h );

SDL_Surface* load_image_into_overlay_at( std::filesystem::path const& filepath,
                                         int32_t overlay_w,
                                         int32_t overlay_h,
                                         int32_t x,
                                         int32_t y,
                                         int32_t w,
                                         int32_t h );

void set_colour_on_surface( SDL_Surface* s, uint32_t pixel );

SDL_Surface* copy_surface( SDL_Surface* s );

uint32_t get_pixel( SDL_Surface* s, size_t x, size_t y );
uint8_t get_pixel_r( SDL_Surface* s, size_t x, size_t y );
uint8_t get_pixel_g( SDL_Surface* s, size_t x, size_t y );
uint8_t get_pixel_b( SDL_Surface* s, size_t x, size_t y );
uint8_t get_pixel_a( SDL_Surface* s, size_t x, size_t y );

void set_pixel( SDL_Surface* s, size_t x, size_t y, uint32_t pixel );
void set_pixel_rgba( SDL_Surface* s, size_t x, size_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a );

void set_big_pixel( SDL_Surface* s, size_t x, size_t y, uint32_t pixel );
void set_big_pixel_rgba( SDL_Surface* s, size_t x, size_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a );

void shake_and_blit_surface( SDL_Surface* source, SDL_Surface* dest, double shake_intensity = 1.0, bool red_only = false );
