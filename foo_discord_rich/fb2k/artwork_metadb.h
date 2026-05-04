#pragma once

#include <cstdint>

namespace drp
{

enum class ArtworkMode : uint8_t
{
    Normal = 0,
    Hidden = 1,
    Blurred = 2,
};

// A class that turns metadata + location info into hashes to which our data gets pinned by the backend.
class metadb_index_client_impl : public metadb_index_client
{
public:
    metadb_index_client_impl( const char * pinTo );
    metadb_index_hash transform(const file_info & info, const playable_location & location);
private:
    titleformat_object::ptr m_keyObj;
};

/* metadb record */
struct record_t {
    pfc::string8 artwork_url;
    ArtworkMode artwork_mode = ArtworkMode::Normal;
    pfc::string8 blurred_artwork_url;
    uint32_t blurred_artwork_percent = 0;
    pfc::string8 blurred_artwork_hash;
};

metadb_index_client_impl * clientByGUID( const GUID & guid );
metadb_index_manager::ptr cached_index_api();
void record_set( metadb_index_hash hash, const record_t & record);
record_t record_get( metadb_index_hash hash);
bool artwork_url_set( metadb_index_hash hash, const pfc::string8 &artwork_url );
bool artwork_mode_set( metadb_index_hash hash, ArtworkMode artwork_mode );
bool blurred_artwork_url_set( metadb_index_hash hash,
                              const pfc::string8& artwork_url,
                              uint32_t blurPercent,
                              const pfc::string8& artworkHash );
pfc::string8 ArtworkModeToDisplayString( ArtworkMode mode );
pfc::string8 ArtworkModeToTitleFormatString( ArtworkMode mode );
pfc::string8 GetEligibleBlurredArtworkUrl( const record_t& rec );

}
