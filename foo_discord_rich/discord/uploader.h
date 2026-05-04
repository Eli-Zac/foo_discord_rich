#pragma once

#include <cstdint>
#include <string>

#include <fb2k/artwork_metadb.h>

namespace drp::uploader
{

struct UploadOptions
{
    ArtworkMode mode = ArtworkMode::Normal;
    bool regenerate = false;
    bool allowCachedReuse = true;
    bool allowUrlPlaceholderReuse = true;
    uint32_t blurPercent = 0;
};

class threaded_process_artwork_uploader : public threaded_process_callback
{
public:
    threaded_process_artwork_uploader( const pfc::map_t<metadb_index_hash, metadb_handle_ptr>& hashes, const bool regenerate );
    void on_init(ctx_t p_wnd) override;
    void run(threaded_process_status & p_status,abort_callback & p_abort) override;
    void on_done(ctx_t p_wnd,bool p_was_aborted) override;
private:
    pfc::map_t<metadb_index_hash, metadb_handle_ptr> hashes_{};
    const bool regenerate_;
};


struct artwork_info
{
    album_art_data_ptr data;
    // This being a char* caused problems where the contents would randomly change. pfc::string8 did not seem to have this problem
    pfc::string8 path{};
    bool success = false;
    pfc::string8 artwork_hash{};
};

bool extractAndUploadArtwork( const metadb_handle_ptr track,
                              abort_callback &abort,
                              pfc::string8 &artwork_url,
                              metadb_index_hash hash,
                              const UploadOptions& options = {},
                              bool* recordChanged = nullptr );
artwork_info extractArtwork( const metadb_handle_ptr track, abort_callback &abort );
pfc::string8 uploadArtwork( artwork_info& art,
                            abort_callback &abort,
                            const UploadOptions& options = {},
                            metadb_index_hash hash = metadb_index_hash() );
bool usesUrlPlaceholder( const std::string &commandString );
bool usesBlurPercentPlaceholder( const std::string &commandString );
bool isValidUrl( const pfc::string8 &url );

} // namespace drp::uploader
