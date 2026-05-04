#include <stdafx.h>

#include "ui_pref_tab_uploader.h"

#include <fb2k/config.h>
#include <qwr/fb2k_config_ui_option.h>
#include <ui/ui_pref_tab_manager.h>

namespace drp::ui
{

using namespace config;

PreferenceTabUploader::PreferenceTabUploader( PreferenceTabManager* pParent )
    : pParent_( pParent )
    , uploaderMode_( config::uploaderMode,
                     { { UploaderMode::Bundled, IDC_RADIO_UPLOADER_MODE_BUNDLED },
                       { UploaderMode::CustomCommand, IDC_RADIO_UPLOADER_MODE_CUSTOM_COMMAND } } )
    , bundledUploaderService_( config::bundledUploaderService,
                               { { BundledUploaderService::Imgur, 0 },
                                 { BundledUploaderService::Catbox, 1 } } )
    , bundledUploaderApiKey_( config::bundledUploaderApiKey )
    , uploadArtworkCommand_( config::uploadArtworkCommand )
    , processTimeout_( config::processTimeout )
    , ddxOptions_( {
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_RadioRange>( uploaderMode_, std::initializer_list<int>{ IDC_RADIO_UPLOADER_MODE_BUNDLED, IDC_RADIO_UPLOADER_MODE_CUSTOM_COMMAND } ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_ComboBox>( bundledUploaderService_, IDC_COMBO_BUNDLED_UPLOADER_SERVICE ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( bundledUploaderApiKey_, IDC_TEXTBOX_BUNDLED_UPLOADER_API_KEY ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( uploadArtworkCommand_, IDC_TEXTBOX_ARTWORK_COMMAND ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEditNum>( processTimeout_, IDC_TEXTBOX_PROCESS_TIMEOUT ),
      } )
{
    config::EnsureUploaderPreferencesMigrated();
}

PreferenceTabUploader::~PreferenceTabUploader()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Revert();
    }
}

HWND PreferenceTabUploader::CreateTab( HWND hParent )
{
    return Create( hParent );
}

CDialogImplBase& PreferenceTabUploader::Dialog()
{
    return *this;
}

const wchar_t* PreferenceTabUploader::Name() const
{
    return L"Uploader";
}

t_uint32 PreferenceTabUploader::get_state()
{
    const bool hasChanged =
        ddxOptions_.cend() != std::find_if( ddxOptions_.cbegin(), ddxOptions_.cend(), []( const auto& ddxOpt ) {
            return ddxOpt->Option().HasChanged();
        } );

    return preferences_state::resettable | ( hasChanged ? preferences_state::changed : 0 );
}

void PreferenceTabUploader::apply()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Apply();
    }

    config::uploadArtworkCommand = config::TrimWhitespace( static_cast<std::string>( config::uploadArtworkCommand ) );
    config::bundledUploaderApiKey = config::TrimWhitespace( static_cast<std::string>( config::bundledUploaderApiKey ) );

    uploadArtworkCommand_ = static_cast<std::string>( config::uploadArtworkCommand );
    bundledUploaderApiKey_ = static_cast<std::string>( config::bundledUploaderApiKey );

    UpdateUiFromCfg();
}

void PreferenceTabUploader::reset()
{
    uploaderMode_.SetValue( config::uploaderMode.GetDefaultValue() );
    bundledUploaderService_.SetValue( config::bundledUploaderService.GetDefaultValue() );
    bundledUploaderApiKey_.ResetToDefault();
    uploadArtworkCommand_.ResetToDefault();
    processTimeout_.ResetToDefault();

    UpdateUiFromCfg();
}

BOOL PreferenceTabUploader::OnInitDialog( HWND hwndFocus, LPARAM lParam )
{
    PopulateServiceCombo();

    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Ddx().SetHwnd( m_hWnd );
    }
    UpdateUiFromCfg();

    return TRUE;
}

void PreferenceTabUploader::OnEditChange( UINT uNotifyCode, int nID, CWindow wndCtl )
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

void PreferenceTabUploader::OnChanged()
{
    pParent_->OnDataChanged();
}

void PreferenceTabUploader::UpdateUiFromCfg()
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

void PreferenceTabUploader::PopulateServiceCombo()
{
    CComboBox serviceCombo( GetDlgItem( IDC_COMBO_BUNDLED_UPLOADER_SERVICE ).m_hWnd );
    serviceCombo.ResetContent();
    serviceCombo.AddString( L"Imgur" );
    serviceCombo.AddString( L"Catbox" );
}

} // namespace drp::ui
