#include "stdafx.h"

#include "artwork_metadb.h"
#include "metadb_helpers.h"
#include "config.h"
#include "discord/discord_impl.h"
#include "discord/uploader.h"
#include "discord/image_hasher.h"
#include "foobar2000/SDK/component.h"
#include "ui/url_input.h"

namespace drp
{

namespace
{

struct ArtworkGroupSelection
{
    pfc::map_t<metadb_index_hash, metadb_handle_ptr> groups;
    size_t skippedUnhashable = 0;
};

ArtworkGroupSelection collectArtworkGroups( metadb_handle_list_cref tracks )
{
    ArtworkGroupSelection selection;
    auto client = clientByGUID( guid::artwork_url_index );

    const auto count = tracks.get_count();
    for ( size_t i = 0; i < count; ++i )
    {
        metadb_index_hash hash;
        if ( client->hashHandle( tracks[i], hash ) )
        {
            if ( !selection.groups.exists( hash ) )
            {
                selection.groups.set( hash, tracks[i] );
            }
        }
        else
        {
            ++selection.skippedUnhashable;
        }
    }

    if ( selection.skippedUnhashable > 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Skipped " << selection.skippedUnhashable
                                 << " unhashable track(s)";
    }

    return selection;
}

void updateCurrentArtwork()
{
    DiscordHandler::GetInstance().GetPresenceModifier().UpdateImage();
}

template <typename GetValue>
pfc::string8 aggregateRecordValue( const pfc::avltree_t<metadb_index_hash>& hashes, GetValue getValue )
{
    pfc::string8 value;
    bool first = true;
    bool various = false;

    for ( auto i = hashes.first(); i.is_valid(); ++i )
    {
        const auto rec = record_get( *i );
        const auto currentValue = getValue( rec );

        if ( first )
        {
            value = currentValue;
        }
        else if ( !various && value != currentValue )
        {
            various = true;
            value = "<various>";
        }

        first = false;
    }

    return value;
}

void setArtworkMode( metadb_handle_list_cref tracks, ArtworkMode mode )
{
    const auto selection = collectArtworkGroups( tracks );
    if ( selection.groups.get_count() == 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION
                                 << ": Could not hash any of the tracks due to unavailable metadata, bailing";
        return;
    }

    pfc::list_t<metadb_index_hash> lstChanged;
    for ( auto iter = selection.groups.first(); iter.is_valid(); ++iter )
    {
        const auto kv = *iter;
        if ( artwork_mode_set( kv.m_key, mode ) )
        {
            lstChanged += kv.m_key;
        }
    }

    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Updated Artwork Mode for " << lstChanged.get_count()
                             << " artwork group(s)";
    if ( lstChanged.get_count() > 0 )
    {
        cached_index_api()->dispatch_refresh( guid::artwork_url_index, lstChanged );
        updateCurrentArtwork();
    }
}

} // namespace

// Our group in Properties dialog / Details tab, see track_property_provider_impl
static const char strPropertiesGroup[] = "Discord rich presence";


void generateUrls( metadb_handle_list_cref tracks, bool regenerate ) {
    const auto selection = collectArtworkGroups( tracks );
    if ( selection.groups.get_count() == 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Could not hash any of the tracks due to unavailable metadata, bailing";
        return;
    }

    auto thread_impl = new service_impl_t<uploader::threaded_process_artwork_uploader>( selection.groups, regenerate );
    const std::string p_title = regenerate ? "Regenerating Artwork URL" : "Generating Artwork URL";

    threaded_process::g_run_modeless(
        thread_impl,
        threaded_process::flag_show_progress | threaded_process::flag_show_abort | threaded_process::flag_show_delayed,
        g_foobar2000_api->get_main_window(),
        p_title.c_str(),
        p_title.length()
    );
}

void clearUrls( metadb_handle_list_cref tracks ) {
    const auto selection = collectArtworkGroups( tracks );
    if ( selection.groups.get_count() == 0 )
    {
        return;
    }

    pfc::list_t<metadb_index_hash> lstChanged; // Linear list of hashes that actually changed

    for ( auto iter = selection.groups.first(); iter.is_valid(); ++iter )
    {
        const auto kv = *iter;
        const auto hash = kv.m_key;
        if (artwork_url_set( hash, "" ))
        {
            lstChanged += hash;
        }
    }

    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": " << lstChanged.get_count() << " entries cleared";
    if (lstChanged.get_count() > 0) {
        // This gracefully tells everyone about what just changed, in one pass regardless of how many items got altered
        cached_index_api()->dispatch_refresh(guid::artwork_url_index, lstChanged);
        updateCurrentArtwork();
    }
}

void clearArtworkHashes( metadb_handle_list_cref tracks )
{
	const auto selection = collectArtworkGroups( tracks );
	if ( selection.groups.get_count() == 0 ) {
		return;
	}

	pfc::avltree_t<pfc::string8> urls;
	pfc::avltree_t<pfc::string8> artworkHashes;

	for ( auto iter = selection.groups.first(); iter.is_valid(); ++iter )
	{
        const auto kv = *iter;
		const auto hash = kv.m_key;
		const auto rec = record_get( hash );
		if (rec.artwork_url.get_length() != 0)
		{
			urls += rec.artwork_url;
		}
        if ( rec.blurred_artwork_url.get_length() != 0 )
        {
            urls += rec.blurred_artwork_url;
        }

        auto artwork = uploader::extractArtwork( kv.m_value, fb2k::noAbort );
        if ( artwork.data.is_valid() )
        {
            artworkHashes += static_api_ptr_t<hasher_md5>()
                                 ->process_single( artwork.data->get_ptr(), artwork.data->get_size() )
                                 .asString();
        }
	}

	int deleted = 0;
	uploader::delete_artwork_cache_entries( urls, artworkHashes, fb2k::noAbort, deleted );
	FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Deleted " << deleted << " cached artwork upload entries.";
}

class contextmenu_artwork_url : public contextmenu_item_simple {
public:
	GUID get_parent() {
		return guid::context_menu_group;
	}

	unsigned get_num_items() {
		return 6;
	}

	void get_item_name(unsigned p_index, pfc::string_base & p_out) {
		PFC_ASSERT( p_index < get_num_items() );
		switch(p_index) {
		    case 0:
		        p_out = "Generate Artwork URL"; break;
		    case 1:
		        p_out = "Clear Artwork URL"; break;
			case 2:
				p_out = "Delete Cached Artwork Upload Hashes"; break;
			case 3:
				p_out = "Clear All Cached Artwork Upload Hashes"; break;
			case 4:
				p_out = "Set Artwork URL..."; break;
			case 5:
				p_out = "Regenerate Artwork URL"; break;
		}
	}

	void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) {
		PFC_ASSERT( p_index < get_num_items() );

        switch (p_index)
        {
        case 0:
            generateUrls( p_data, false );
            break;
        case 1:
            clearUrls( p_data );
            break;
        case 2:
        	clearArtworkHashes( p_data );
        	break;
        case 3:
        	uploader::clear_all_hashes(fb2k::noAbort);
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Cleared all cached artwork upload hashes";
        	break;
        case 4:
        	ui::InputDialog::OpenDialog(p_data);
        	break;
        case 5:
        	generateUrls( p_data, true );
        	break;
        default:
            uBugCheck();
        }
	}

	GUID get_item_guid(unsigned p_index) {
		switch(p_index) {
		    case 0:	return guid::context_menu_item_generate_url;
		    case 1:	return guid::context_menu_item_clear_url;
		    case 2:	return guid::context_menu_item_clear_hash_urls;
		    case 3:	return guid::context_menu_item_clear_all_hash_urls;
		    case 4:	return guid::context_menu_item_enter_url;
		    case 5:	return guid::context_menu_item_regenerate_url;
		    default: uBugCheck();
		}
	}

	bool get_item_description(unsigned p_index, pfc::string_base & p_out) {
		PFC_ASSERT( p_index < get_num_items() );
		switch( p_index ) {
		case 0:
			p_out = "Generate artwork for the selected artwork groups according to Artwork Mode.";
			return true;
		case 1:
		    p_out = "Clear stored artwork URLs for the selected artwork groups.";
		    return true;
		case 2:
			p_out = "Delete uploader cache entries for the selected artwork URLs.";
			return true;
		case 3:
			p_out = "Delete all uploader cache entries.";
			return true;
		case 4:
			p_out = "Set a stored artwork URL for the selected artwork groups.";
			return true;
		case 5:
			p_out = "Force regeneration of artwork for the selected artwork groups according to Artwork Mode.";
			return true;
		default:
			PFC_ASSERT(!"Should not get here");
			return false;
		}
	}
};

static contextmenu_group_popup_factory g_mygroup( guid::context_menu_group, contextmenu_groups::root, "Discord rich presence", 0 );
static contextmenu_item_factory_t< contextmenu_artwork_url > g_contextmenu_rating;
static contextmenu_group_popup_factory g_artwork_mode_group( guid::context_menu_group_artwork_mode,
                                                             guid::context_menu_group,
                                                             "Artwork Mode",
                                                             1 );

class contextmenu_artwork_mode : public contextmenu_item_simple
{
public:
    GUID get_parent() override
    {
        return guid::context_menu_group_artwork_mode;
    }

    unsigned get_num_items() override
    {
        return 3;
    }

    void get_item_name( unsigned p_index, pfc::string_base& p_out ) override
    {
        switch ( p_index )
        {
        case 0:
            p_out = "Normal";
            break;
        case 1:
            p_out = "Hidden";
            break;
        case 2:
            p_out = "Blurred";
            break;
        default:
            uBugCheck();
        }
    }

    void context_command( unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller ) override
    {
        switch ( p_index )
        {
        case 0:
            setArtworkMode( p_data, ArtworkMode::Normal );
            break;
        case 1:
            setArtworkMode( p_data, ArtworkMode::Hidden );
            break;
        case 2:
            setArtworkMode( p_data, ArtworkMode::Blurred );
            break;
        default:
            uBugCheck();
        }
    }

    GUID get_item_guid( unsigned p_index ) override
    {
        switch ( p_index )
        {
        case 0:
            return guid::context_menu_item_artwork_mode_normal;
        case 1:
            return guid::context_menu_item_artwork_mode_hidden;
        case 2:
            return guid::context_menu_item_artwork_mode_blurred;
        default:
            uBugCheck();
        }
    }

    bool get_item_description( unsigned p_index, pfc::string_base& p_out ) override
    {
        p_out = "Set Artwork Mode for the selected artwork groups.";
        return true;
    }
};

static contextmenu_item_factory_t<contextmenu_artwork_mode> g_contextmenu_artwork_mode;


class track_property_provider_impl : public track_property_provider_v2 {
public:
	void workThisIndex(GUID const & whichID, double priorityBase, metadb_handle_list_cref p_tracks, track_property_callback & p_out) {
		auto client = clientByGUID( whichID );
		pfc::avltree_t<metadb_index_hash> hashes;
		const size_t trackCount = p_tracks.get_count();
		for (size_t trackWalk = 0; trackWalk < trackCount; ++trackWalk) {
			metadb_index_hash hash;
			if (client->hashHandle(p_tracks[trackWalk], hash)) {
				hashes += hash;
			}
		}

        const auto mode = aggregateRecordValue( hashes, []( const record_t& rec ) {
            return ArtworkModeToDisplayString( rec.artwork_mode );
        } );
        const auto normalUrl = aggregateRecordValue( hashes, []( const record_t& rec ) {
            return rec.artwork_url;
        } );
        const auto blurredUrl = aggregateRecordValue( hashes, []( const record_t& rec ) {
            return GetEligibleBlurredArtworkUrl( rec );
        } );

		p_out.set_property(strPropertiesGroup, priorityBase, PFC_string_formatter() << "Artwork Mode", mode);
        p_out.set_property(strPropertiesGroup, priorityBase + 1, PFC_string_formatter() << "Normal Artwork URL", normalUrl);
        p_out.set_property(strPropertiesGroup, priorityBase + 2, PFC_string_formatter() << "Blurred Artwork URL", blurredUrl);
	}
	void enumerate_properties(metadb_handle_list_cref p_tracks, track_property_callback & p_out) {
		workThisIndex( guid::artwork_url_index, 0, p_tracks, p_out );
	}
	void enumerate_properties_v2(metadb_handle_list_cref p_tracks, track_property_callback_v2 & p_out) {
		if ( p_out.is_group_wanted( strPropertiesGroup ) ) {
			enumerate_properties( p_tracks, p_out );
		}
	}

	bool is_our_tech_info(const char * p_name) {
		// If we do stuff with tech infos read from the file itself (see file_info::info_* methods), signal whether this field belongs to us
		// We don't do any of this, hence false
		return false;
	}
};

static service_factory_single_t<track_property_provider_impl> g_track_property_provider_impl;
}
