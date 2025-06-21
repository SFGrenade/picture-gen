#include "fontManager.h"

#include "freetype/freetype.h"
#include "loggerFactory.h"


spdlogger FontManager::logger_ = nullptr;
FT_Library FontManager::ft_library_ = nullptr;
std::map< std::string, std::pair< FT_Face, cairo_font_face_t* > > FontManager::font_map_;

void FontManager::init( std::filesystem::path const& base_font_path ) {
  FontManager::logger_ = LoggerFactory::get_logger( "FontManager" );
  FontManager::logger_->trace( "[init] enter: base_font_path: {:?}", base_font_path.string() );

  FT_Error ft_ret;
  if( ( ft_ret = FT_Init_FreeType( &FontManager::ft_library_ ) ) != 0 ) {
    FontManager::logger_->error( "[init] FT_Init_FreeType returned {}", ft_ret );
    return;
  }
  for( auto const& dir_entry : std::filesystem::recursive_directory_iterator( base_font_path ) ) {
    if( !dir_entry.is_regular_file() ) {
      continue;
    }
    std::string file_extension = dir_entry.path().extension().string();
    if( ( file_extension != ".ttf" ) && ( file_extension != ".otf" ) ) {
      continue;
    }
    std::string filepath = dir_entry.path().string();
    std::string filename = dir_entry.path().filename().string();
    cairo_font_face_t* cairo_ft_face = nullptr;
    FT_Face ft_face = nullptr;
    if( ( ft_ret = FT_New_Face( FontManager::ft_library_, filepath.c_str(), 0, &ft_face ) ) != 0 ) {
      FontManager::logger_->error( "[init] FT_New_Face returned {} for {:?}", ft_ret, filepath );
      continue;
    }
    cairo_ft_face = cairo_ft_font_face_create_for_ft_face( ft_face, 0 );
    FontManager::logger_->trace( "[init] loaded font {:?}", filepath );
    FontManager::font_map_.insert( { filename, { ft_face, cairo_ft_face } } );
  }

  FontManager::logger_->trace( "[init] exit" );
}

void FontManager::deinit() {
  FontManager::logger_->trace( "[deinit] enter" );

  FT_Error ft_ret;
  for( auto const& pair : FontManager::font_map_ ) {
    cairo_font_face_destroy( pair.second.second );
    if( ( ft_ret = FT_Done_Face( pair.second.first ) ) != 0 ) {
      FontManager::logger_->error( "[deinit] FT_New_Face returned {}", ft_ret );
    }
  }
  FontManager::font_map_.clear();
  if( ( ft_ret = FT_Done_FreeType( FontManager::ft_library_ ) ) != 0 ) {
    FontManager::logger_->error( "[deinit] FT_Done_FreeType returned {}", ft_ret );
  }

  FontManager::logger_->trace( "[deinit] exit" );
}

cairo_font_face_t* FontManager::get_font_face( std::string const& name ) {
  FontManager::logger_->trace( "[get_font_face] enter: name: {:?}", name );

  cairo_font_face_t* ret = nullptr;
  auto const& font_map_iter = font_map_.find( name );
  if( font_map_iter != font_map_.end() ) {
    ret = font_map_iter->second.second;
  } else {
    FontManager::logger_->error( "[get_font_face] name {:?} wasn't found in font map!", name );
  }

  FontManager::logger_->trace( "[get_font_face] exit: font_face: {}", static_cast< void* >( ret ) );
  return ret;
}
