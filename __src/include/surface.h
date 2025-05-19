#pragma once

#include <cairo.h>
#include <cstdint>
#include <filesystem>

void surface_blit( cairo_surface_t* src, cairo_surface_t* dest, double dest_x, double dest_y, double dest_width, double dest_height );

cairo_surface_t* surface_load_file( std::filesystem::path const& filepath );

cairo_surface_t* surface_create_size( int32_t width, int32_t height );

cairo_surface_t* surface_embed_in_overlay( cairo_surface_t* surface,
                                           int32_t overlay_width,
                                           int32_t overlay_height,
                                           double x,
                                           double y,
                                           double width,
                                           double height );

cairo_surface_t* surface_load_file_into_overlay( std::filesystem::path const& filepath,
                                                 int32_t overlay_width,
                                                 int32_t overlay_height,
                                                 int32_t x,
                                                 int32_t y,
                                                 int32_t width,
                                                 int32_t height );

cairo_surface_t* surface_render_text_into_overlay( std::filesystem::path const& filepath,
                                                   int32_t overlay_width,
                                                   int32_t overlay_height,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height );

void surface_fill( cairo_surface_t* s, double r, double g, double b, double a );

cairo_surface_t* surface_copy( cairo_surface_t* s );

void surface_shake_and_blit( cairo_surface_t* source, cairo_surface_t* dest, double shake_intensity = 1.0, bool red_only = false );
