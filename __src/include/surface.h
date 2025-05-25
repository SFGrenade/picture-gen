#pragma once

#include <cairo-ft.h>
#include <cairo.h>
#include <cstdint>
#include <filesystem>

void surface_blit( cairo_surface_t* src,
                   cairo_surface_t* dest,
                   double const dest_x,
                   double const dest_y,
                   double const dest_width,
                   double const dest_height,
                   double const alpha = 1.0 );

cairo_surface_t* surface_load_file( std::filesystem::path const& filepath );

cairo_surface_t* surface_create_size( int32_t const width, int32_t const height );

cairo_surface_t* surface_embed_in_overlay( cairo_surface_t* surface,
                                           int32_t const overlay_width,
                                           int32_t const overlay_height,
                                           double const x,
                                           double const y,
                                           double const width,
                                           double const height );

cairo_surface_t* surface_load_file_into_overlay( std::filesystem::path const& filepath,
                                                 int32_t const overlay_width,
                                                 int32_t const overlay_height,
                                                 int32_t const x,
                                                 int32_t const y,
                                                 int32_t const width,
                                                 int32_t const height );

cairo_surface_t* surface_render_text_into_overlay( cairo_font_face_t* font_face,
                                                   std::filesystem::path const& filepath,
                                                   int32_t const overlay_width,
                                                   int32_t const overlay_height,
                                                   int32_t const x,
                                                   int32_t const y,
                                                   int32_t const width,
                                                   int32_t const height );

cairo_surface_t* surface_render_text_advanced_into_overlay( cairo_font_face_t* header_font_face,
                                                            cairo_font_face_t* content_font_face,
                                                            std::filesystem::path const& filepath,
                                                            int32_t const overlay_width,
                                                            int32_t const overlay_height,
                                                            int32_t const x,
                                                            int32_t const y,
                                                            int32_t const width,
                                                            int32_t const height );

void surface_fill( cairo_surface_t* s, double const r, double const g, double const b, double const a );

cairo_surface_t* surface_copy( cairo_surface_t* s );

void surface_shake_and_blit( cairo_surface_t* source, cairo_surface_t* dest, double const shake_intensity = 1.0, bool const red_only = false );
