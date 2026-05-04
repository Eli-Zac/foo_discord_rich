#include <stdafx.h>

#include "ui_pref_tab_artwork.h"

#include <discord/discord_impl.h>
#include <fb2k/config.h>
#include <qwr/fb2k_config_ui_option.h>
#include <ui/ui_pref_tab_manager.h>

namespace drp::ui
{

using namespace config;

PreferenceTabArtwork::PreferenceTabArtwork( PreferenceTabManager* pParent )
    : pParent_( pParent )
    , uploadArtwork_( config::uploadArtwork )
    , artworkMetadbKey_( config::artworkMetadbKey )
    , blurPercent_( config::blurPercent )
    , hiddenPlaceholderUrl_( config::hiddenPlaceholderUrl )
    , overrideArtworkUrl_( config::overrideArtworkUrl )
    , artworkOverrideUrl_( config::artworkOverrideUrl )
    , ddxOptions_( {
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_CheckBox>( uploadArtwork_, IDC_CHECK_UPLOAD_ARTWORK ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( artworkMetadbKey_, IDC_TEXTBOX_METADB_KEY ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEditNum>( blurPercent_, IDC_TEXTBOX_BLUR_PERCENT ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( hiddenPlaceholderUrl_, IDC_TEXTBOX_HIDDEN_PLACEHOLDER_URL ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_CheckBox>( overrideArtworkUrl_, IDC_CHECK_OVERRIDE_ARTWORK_URL ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( artworkOverrideUrl_, IDC_TEXTBOX_ARTWORK_OVERRIDE_URL ),
      } )
{
}

PreferenceTabArtwork::~PreferenceTabArtwork()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Revert();
    }
}

HWND PreferenceTabArtwork::CreateTab( HWND hParent )
{
    return Create( hParent );
}

CDialogImplBase& PreferenceTabArtwork::Dialog()
{
    return *this;
}

const wchar_t* PreferenceTabArtwork::Name() const
{
    return L"Artwork";
}

t_uint32 PreferenceTabArtwork::get_state()
{
    const bool hasChanged =
        ddxOptions_.cend() != std::find_if( ddxOptions_.cbegin(), ddxOptions_.cend(), []( const auto& ddxOpt ) {
            return ddxOpt->Option().HasChanged();
        } );

    const bool needsRestart =
        ddxOptions_.cend() != std::find_if( ddxOptions_.cbegin(), ddxOptions_.cend(), []( const auto& ddxOpt ) {
            return ddxOpt->Option().HasChanged() && ddxOpt->Ddx().IsMatchingId( IDC_TEXTBOX_METADB_KEY );
        } );

    return preferences_state::resettable
           | ( hasChanged ? preferences_state::changed : 0 )
           | ( needsRestart ? preferences_state::needs_restart : 0 );
}

void PreferenceTabArtwork::apply()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Apply();
    }

    config::blurPercent = config::GetValidatedBlurPercent();
    config::hiddenPlaceholderUrl = config::TrimWhitespace( static_cast<std::string>( config::hiddenPlaceholderUrl ) );
    config::artworkOverrideUrl = config::TrimWhitespace( static_cast<std::string>( config::artworkOverrideUrl ) );

    blurPercent_ = static_cast<int>( config::blurPercent );
    hiddenPlaceholderUrl_ = static_cast<std::string>( config::hiddenPlaceholderUrl );
    artworkOverrideUrl_ = static_cast<std::string>( config::artworkOverrideUrl );

    UpdateUiFromCfg();
}

void PreferenceTabArtwork::reset()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().ResetToDefault();
    }

    UpdateUiFromCfg();
}

BOOL PreferenceTabArtwork::OnInitDialog( HWND hwndFocus, LPARAM lParam )
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Ddx().SetHwnd( m_hWnd );
    }
    UpdateUiFromCfg();

    return TRUE; // set focus to default control
}

void PreferenceTabArtwork::OnEditChange( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    auto it = std::find_if( ddxOptions_.begin(), ddxOptions_.end(), [nID]( auto& val ) {
        return val->Ddx().IsMatchingId( nID );
    } );

    if ( ddxOptions_.end() != it )
    {
        ( *it )->Ddx().ReadFromUi();
    }

    OnChanged();
}

void PreferenceTabArtwork::OnChanged()
{
    pParent_->OnDataChanged();
}

void PreferenceTabArtwork::UpdateUiFromCfg()
{
    if ( !this->m_hWnd )
    {
        return;
    }

    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Ddx().WriteToUi();
    }
}

} // namespace drp::ui
