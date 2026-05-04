#pragma once

#include <fb2k/config.h>
#include <qwr/fb2k_config_ui_option.h>
#include <qwr/macros.h>
#include <qwr/ui_ddx_option.h>
#include <ui/ui_itab.h>

#include <resource.h>

#include <array>

namespace drp::ui
{

class PreferenceTabManager;

class PreferenceTabArtwork
    : public CDialogImpl<PreferenceTabArtwork>
    , public CWinDataExchange<PreferenceTabArtwork>
    , public ITab
{
public:
    enum
    {
        IDD = IDD_PREFS_ARTWORK_TAB
    };

    BEGIN_MSG_MAP( PreferenceTabArtwork )
        MSG_WM_INITDIALOG( OnInitDialog )
        COMMAND_HANDLER_EX( IDC_CHECK_UPLOAD_ARTWORK, BN_CLICKED, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_METADB_KEY, EN_CHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_BLUR_PERCENT, EN_CHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_HIDDEN_PLACEHOLDER_URL, EN_CHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_CHECK_OVERRIDE_ARTWORK_URL, BN_CLICKED, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_ARTWORK_OVERRIDE_URL, EN_CHANGE, OnEditChange )
    END_MSG_MAP()

public:
    PreferenceTabArtwork( PreferenceTabManager* pParent );
    ~PreferenceTabArtwork() override;

    // IUiTab
    HWND CreateTab( HWND hParent ) override;
    CDialogImplBase& Dialog() override;
    const wchar_t* Name() const override;
    t_uint32 get_state() override;
    void apply() override;
    void reset() override;

private:
    BOOL OnInitDialog( HWND hwndFocus, LPARAM lParam );
    void OnEditChange( UINT uNotifyCode, int nID, CWindow wndCtl );
    void OnChanged();
    void UpdateUiFromCfg();

private:
    PreferenceTabManager* pParent_ = nullptr;

#define SPTF_DEFINE_UI_OPTION( name ) \
    qwr::ui::UiOption<decltype( config::name )> name##_;

#define SPTF_DEFINE_UI_OPTIONS( ... ) \
    QWR_EXPAND( QWR_PASTE( SPTF_DEFINE_UI_OPTION, __VA_ARGS__ ) )

    SPTF_DEFINE_UI_OPTIONS( uploadArtwork,
                            artworkMetadbKey,
                            blurPercent,
                            hiddenPlaceholderUrl,
                            overrideArtworkUrl,
                            artworkOverrideUrl )

#undef SPTF_DEFINE_UI_OPTIONS
#undef SPTF_DEFINE_UI_OPTION

    std::array<std::unique_ptr<qwr::ui::IUiDdxOption>, 6> ddxOptions_;
};

} // namespace drp::ui
