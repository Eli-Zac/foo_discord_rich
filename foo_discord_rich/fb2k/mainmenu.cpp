#include <stdafx.h>

#include <discord/discord_impl.h>
#include <fb2k/config.h>

namespace
{

class DisplayMainMenuCommandsImpl : public mainmenu_commands
{
public:
    t_uint32 get_command_count() override;
    GUID get_command( t_uint32 p_index ) override;
    void get_name( t_uint32 p_index, pfc::string_base& p_out ) override;
    bool get_description( t_uint32 /* p_index */, pfc::string_base& p_out ) override;
    GUID get_parent() override;
    void execute( t_uint32 p_index, service_ptr_t<service_base> p_callback ) override;
    bool get_display( t_uint32 p_index, pfc::string_base& p_out, t_uint32& p_flags ) override;
};

class ArtworkUploadMainMenuCommandsImpl : public mainmenu_commands
{
public:
    t_uint32 get_command_count() override;
    GUID get_command( t_uint32 p_index ) override;
    void get_name( t_uint32 p_index, pfc::string_base& p_out ) override;
    bool get_description( t_uint32 p_index, pfc::string_base& p_out ) override;
    GUID get_parent() override;
    void execute( t_uint32 p_index, service_ptr_t<service_base> p_callback ) override;
    bool get_display( t_uint32 p_index, pfc::string_base& p_out, t_uint32& p_flags ) override;
};

} // namespace

namespace
{

t_uint32 DisplayMainMenuCommandsImpl::get_command_count()
{
    return 1;
}

GUID DisplayMainMenuCommandsImpl::get_command( t_uint32 p_index )
{
    switch ( p_index )
    {
    case 0:
        return drp::guid::mainmenu_cmd_enable;
    default:
        uBugCheck();
    }
}

void DisplayMainMenuCommandsImpl::get_name( t_uint32 p_index, pfc::string_base& p_out )
{
    switch ( p_index )
    {
    case 0:
        p_out = "Display Discord Rich Presence";
        return;
    default:
        uBugCheck();
    }
}

bool DisplayMainMenuCommandsImpl::get_description( t_uint32 /* p_index */, pfc::string_base& p_out )
{
    p_out = "Toggles Discord Rich Presence";
    return true;
}

GUID DisplayMainMenuCommandsImpl::get_parent()
{
    return mainmenu_groups::view;
}

void DisplayMainMenuCommandsImpl::execute( t_uint32 p_index, service_ptr_t<service_base> p_callback )
{
    PFC_ASSERT( p_index == 0 );

    drp::config::isEnabled = !drp::config::isEnabled;
    drp::DiscordHandler::GetInstance().OnSettingsChanged();
}

bool DisplayMainMenuCommandsImpl::get_display( t_uint32 p_index, pfc::string_base& p_out, t_uint32& p_flags )
{
    get_name( p_index, p_out );
    p_flags = drp::config::isEnabled ? mainmenu_commands::flag_checked : 0;
    return true;
}

t_uint32 ArtworkUploadMainMenuCommandsImpl::get_command_count()
{
    return 1;
}

GUID ArtworkUploadMainMenuCommandsImpl::get_command( t_uint32 p_index )
{
    switch ( p_index )
    {
    case 0:
        return drp::guid::mainmenu_cmd_auto_upload_artwork;
    default:
        uBugCheck();
    }
}

void ArtworkUploadMainMenuCommandsImpl::get_name( t_uint32 p_index, pfc::string_base& p_out )
{
    switch ( p_index )
    {
    case 0:
        p_out = "Automatic Artwork Upload";
        return;
    default:
        uBugCheck();
    }
}

bool ArtworkUploadMainMenuCommandsImpl::get_description( t_uint32 /* p_index */, pfc::string_base& p_out )
{
    p_out = "Toggles automatic artwork uploads during playback";
    return true;
}

GUID ArtworkUploadMainMenuCommandsImpl::get_parent()
{
    return drp::guid::mainmenu_group_discord_rich_presence;
}

void ArtworkUploadMainMenuCommandsImpl::execute( t_uint32 p_index, service_ptr_t<service_base> p_callback )
{
    PFC_ASSERT( p_index == 0 );

    drp::config::autoUploadArtwork = !drp::config::autoUploadArtwork;
    drp::DiscordHandler::GetInstance().OnSettingsChanged();
}

bool ArtworkUploadMainMenuCommandsImpl::get_display( t_uint32 p_index, pfc::string_base& p_out, t_uint32& p_flags )
{
    get_name( p_index, p_out );
    p_flags = drp::config::autoUploadArtwork ? mainmenu_commands::flag_checked : 0;
    return true;
}

} // namespace

namespace
{

mainmenu_group_popup_factory g_mainmenuGroupDiscordRichPresence( drp::guid::mainmenu_group_discord_rich_presence,
                                                                 mainmenu_groups::library,
                                                                 mainmenu_commands::sort_priority_last - 1,
                                                                 "Discord Rich Presence" );
mainmenu_commands_factory_t<DisplayMainMenuCommandsImpl> g_displayMainMenuCommands;
mainmenu_commands_factory_t<ArtworkUploadMainMenuCommandsImpl> g_artworkUploadMainMenuCommands;

} // namespace
