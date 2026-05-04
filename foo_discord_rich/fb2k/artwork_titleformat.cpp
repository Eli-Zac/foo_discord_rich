#include "stdafx.h"

#include "artwork_metadb.h"

namespace drp
{

namespace
{

enum ArtworkTitleFormatField : t_uint32
{
    field_artwork_mode = 0,
    field_artwork_url,
    field_blurred_artwork_url,
    field_count
};

bool HashHandle( metadb_handle* handle, metadb_index_hash& out )
{
    if ( !handle )
    {
        return false;
    }

    metadb_info_container::ptr info;
    if ( !handle->get_info_ref( info ) )
    {
        return false;
    }

    out = clientByGUID( guid::artwork_url_index )->transform( info->info(), handle->get_location() );
    return true;
}

class artwork_titleformat_provider : public metadb_display_field_provider
{
public:
    t_uint32 get_field_count() override
    {
        return field_count;
    }

    void get_field_name( t_uint32 index, pfc::string_base& out ) override
    {
        switch ( index )
        {
        case field_artwork_mode:
            out = "drp_artwork_mode";
            break;
        case field_artwork_url:
            out = "drp_artwork_url";
            break;
        case field_blurred_artwork_url:
            out = "drp_blurred_artwork_url";
            break;
        default:
            out = "";
            break;
        }
    }

    bool process_field( t_uint32 index, metadb_handle* handle, titleformat_text_out* out ) override
    {
        metadb_index_hash hash;
        if ( !HashHandle( handle, hash ) )
        {
            return false;
        }

        const auto rec = record_get( hash );
        pfc::string8 value;

        switch ( index )
        {
        case field_artwork_mode:
            value = ArtworkModeToTitleFormatString( rec.artwork_mode );
            break;
        case field_artwork_url:
            value = rec.artwork_url;
            break;
        case field_blurred_artwork_url:
            value = GetEligibleBlurredArtworkUrl( rec );
            break;
        default:
            return false;
        }

        if ( value.is_empty() )
        {
            return false;
        }

        out->write( titleformat_inputtypes::meta, value.c_str(), value.get_length() );
        return true;
    }
};

static service_factory_single_t<artwork_titleformat_provider> g_artwork_titleformat_provider;

} // namespace

} // namespace drp
