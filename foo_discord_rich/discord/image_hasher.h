#pragma once
#include "uploader.h"


namespace drp::uploader
{

bool set_artwork_url_hash( const pfc::string8& artwork_url,
                           const pfc::string8& artwork_hash,
                           abort_callback &abort,
                           ArtworkMode mode = ArtworkMode::Normal,
                           uint32_t blurPercent = 0 );
bool check_artwork_hash( artwork_info& artwork,
                         abort_callback &abort,
                         pfc::string8& artwork_url,
                         ArtworkMode mode = ArtworkMode::Normal,
                         uint32_t blurPercent = 0 );
bool delete_artwork_cache_entries( const pfc::avltree_t<pfc::string8>& urls,
                                   const pfc::avltree_t<pfc::string8>& artworkHashes,
                                   abort_callback &abort,
                                   int& deleted );
bool clear_all_hashes(abort_callback &abort);

} // namespace drp::uploader
