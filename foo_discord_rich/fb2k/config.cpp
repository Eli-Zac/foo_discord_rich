#include <stdafx.h>

#include "config.h"

#include <algorithm>
#include <string_view>

#include <discord/discord_impl.h>
#include <discord/uploader.h>

namespace drp::config
{

std::string TrimWhitespace( std::string_view raw )
{
    const auto first = raw.find_first_not_of( " \t\n\r" );
    if ( first == std::string_view::npos )
    {
        return {};
    }

    const auto last = raw.find_last_not_of( " \t\n\r" );
    return std::string{ raw.substr( first, last - first + 1 ) };
}

qwr::fb2k::ConfigBool isEnabled( guid::conf_is_enabled, true );
qwr::fb2k::ConfigUint8Enum<ImageSetting> largeImageSettings( guid::conf_large_image_settings, ImageSetting::Light );
qwr::fb2k::ConfigUint8Enum<ImageSetting> smallImageSettings( guid::conf_small_image_settings, ImageSetting::Light );
qwr::fb2k::ConfigUint8Enum<TimeSetting> timeSettings( guid::conf_time_settings, TimeSetting::ProgressBar );
qwr::fb2k::ConfigUint8Enum<StatusSetting> statusSettings( guid::conf_status_settings, StatusSetting::Name );

qwr::fb2k::ConfigString stateQuery( guid::conf_state_query, "[%title%]" );
qwr::fb2k::ConfigString detailsQuery( guid::conf_details_query, "[%album artist%[: %album%]]" );

qwr::fb2k::ConfigString discordAppToken( guid::conf_app_token, "507982587416018945" );
qwr::fb2k::ConfigString largeImageId_Light( guid::conf_large_image_id_light, "foobar2000" );
qwr::fb2k::ConfigString largeImageId_Dark( guid::conf_large_image_id_dark, "foobar2000-dark" );
qwr::fb2k::ConfigString playingImageId_Light( guid::conf_playing_image_id_light, "playing" );
qwr::fb2k::ConfigString playingImageId_Dark( guid::conf_playing_image_id_dark, "playing-dark" );
qwr::fb2k::ConfigString pausedImageId_Light( guid::conf_paused_image_id_light, "paused" );
qwr::fb2k::ConfigString pausedImageId_Dark( guid::conf_paused_image_id_dark, "paused-dark" );
qwr::fb2k::ConfigString uploadArtworkCommand( guid::conf_upload_artwork_command, "" );
qwr::fb2k::ConfigUint8Enum<UploaderMode> uploaderMode( guid::conf_uploader_mode, UploaderMode::Bundled );
qwr::fb2k::ConfigUint8Enum<BundledUploaderService> bundledUploaderService( guid::conf_bundled_uploader_service, BundledUploaderService::Catbox );
qwr::fb2k::ConfigString bundledUploaderApiKey( guid::conf_bundled_uploader_api_key, "" );
qwr::fb2k::ConfigBool uploaderPreferencesMigrated( guid::conf_uploader_preferences_migrated, false );
qwr::fb2k::ConfigString artworkMetadbKey( guid::conf_artwork_metadb_key, "%album artist% - $if2([%album%],%title%) [ - %discnumber%]" );
qwr::fb2k::ConfigUint32 blurPercent( guid::conf_blur_percent, 70 );
qwr::fb2k::ConfigString hiddenPlaceholderUrl( guid::conf_hidden_placeholder_url, "" );
qwr::fb2k::ConfigUint32 processTimeout( guid::conf_process_timeout, 10 );
qwr::fb2k::ConfigBool overrideArtworkUrl( guid::conf_override_artwork_url, false );
qwr::fb2k::ConfigString artworkOverrideUrl( guid::conf_artwork_override_url, "" );

qwr::fb2k::ConfigBool disableWhenPaused( guid::conf_disable_when_paused, false );
qwr::fb2k::ConfigBool swapSmallImages( guid::conf_swap_small_images, false );
qwr::fb2k::ConfigBool uploadArtwork( guid::conf_upload_artwork, false );

namespace
{

bool IsValidUploaderMode( UploaderMode mode )
{
    return mode == UploaderMode::Bundled
           || mode == UploaderMode::CustomCommand;
}

bool IsValidBundledUploaderService( BundledUploaderService service )
{
    return service == BundledUploaderService::Imgur
           || service == BundledUploaderService::Catbox;
}

void NormalizeUploaderPreferences()
{
    if ( !IsValidUploaderMode( uploaderMode.GetValue() ) )
    {
        uploaderMode = UploaderMode::Bundled;
    }

    if ( !IsValidBundledUploaderService( bundledUploaderService.GetValue() ) )
    {
        bundledUploaderService = BundledUploaderService::Catbox;
    }
}

class UploaderConfigMigrationCallback
    : public init_stage_callback
{
public:
    void on_init_stage( t_uint32 stage ) override
    {
        if ( stage == init_stages::after_config_read )
        {
            EnsureUploaderPreferencesMigrated();
        }
    }
};

service_factory_single_t<UploaderConfigMigrationCallback> g_uploader_config_migration_callback;

} // namespace

void EnsureUploaderPreferencesMigrated()
{
    NormalizeUploaderPreferences();

    if ( uploaderPreferencesMigrated )
    {
        return;
    }

    const auto existingCommand = TrimWhitespace( uploadArtworkCommand.GetValue() );
    uploaderMode = existingCommand.empty() ? UploaderMode::Bundled : UploaderMode::CustomCommand;
    uploaderPreferencesMigrated = true;
}

UploaderMode GetValidatedUploaderMode()
{
    EnsureUploaderPreferencesMigrated();
    return uploaderMode.GetValue();
}

BundledUploaderService GetValidatedBundledUploaderService()
{
    EnsureUploaderPreferencesMigrated();
    return bundledUploaderService.GetValue();
}

std::string GetBundledUploaderServiceCliName()
{
    switch ( GetValidatedBundledUploaderService() )
    {
    case BundledUploaderService::Imgur:
        return "imgur";
    case BundledUploaderService::Catbox:
        return "catbox";
    default:
        return "catbox";
    }
}

uint32_t GetValidatedBlurPercent()
{
    return std::min<uint32_t>( static_cast<uint32_t>( blurPercent ), 100 );
}

std::string GetValidatedHiddenPlaceholderUrl()
{
    static std::string lastLoggedInvalidValue;

    const auto trimmed = TrimWhitespace( static_cast<std::string>( hiddenPlaceholderUrl ) );
    if ( trimmed.empty() )
    {
        lastLoggedInvalidValue.clear();
        return {};
    }

    if ( uploader::isValidUrl( pfc::string8{ trimmed.c_str() } ) )
    {
        lastLoggedInvalidValue.clear();
        return trimmed;
    }

    if ( lastLoggedInvalidValue != trimmed )
    {
        lastLoggedInvalidValue = trimmed;
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION
                                 << ": Hidden/Blurred Fallback URL is invalid and will be treated as unset: "
                                 << trimmed.c_str();
    }

    return {};
}

std::string GetValidatedArtworkOverrideUrl()
{
    static std::string lastLoggedInvalidValue;

    if ( !overrideArtworkUrl )
    {
        lastLoggedInvalidValue.clear();
        return {};
    }

    const auto trimmed = TrimWhitespace( static_cast<std::string>( artworkOverrideUrl ) );
    if ( trimmed.empty() )
    {
        lastLoggedInvalidValue.clear();
        return {};
    }

    if ( uploader::isValidUrl( pfc::string8{ trimmed.c_str() } ) )
    {
        lastLoggedInvalidValue.clear();
        return trimmed;
    }

    if ( lastLoggedInvalidValue != trimmed )
    {
        lastLoggedInvalidValue = trimmed;
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION
                                 << ": Artwork Override URL is invalid and will be treated as unset: "
                                 << trimmed.c_str();
    }

    return {};
}

} // namespace drp::config
