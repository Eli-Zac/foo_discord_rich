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

class PreferenceTabUploader
    : public CDialogImpl<PreferenceTabUploader>
    , public CWinDataExchange<PreferenceTabUploader>
    , public ITab
{
public:
    enum
    {
        IDD = IDD_PREFS_UPLOADER_TAB
    };

    BEGIN_MSG_MAP( PreferenceTabUploader )
        MSG_WM_INITDIALOG( OnInitDialog )
        COMMAND_HANDLER_EX( IDC_RADIO_UPLOADER_MODE_BUNDLED, BN_CLICKED, OnEditChange )
        COMMAND_HANDLER_EX( IDC_RADIO_UPLOADER_MODE_CUSTOM_COMMAND, BN_CLICKED, OnEditChange )
        COMMAND_HANDLER_EX( IDC_COMBO_BUNDLED_UPLOADER_SERVICE, CBN_SELCHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_BUNDLED_UPLOADER_API_KEY, EN_CHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_ARTWORK_COMMAND, EN_CHANGE, OnEditChange )
        COMMAND_HANDLER_EX( IDC_TEXTBOX_PROCESS_TIMEOUT, EN_CHANGE, OnEditChange )
    END_MSG_MAP()

public:
    explicit PreferenceTabUploader( PreferenceTabManager* pParent );
    ~PreferenceTabUploader() override;

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
    void PopulateServiceCombo();

private:
    PreferenceTabManager* pParent_ = nullptr;

#define SPTF_DEFINE_UI_OPTION( name ) \
    qwr::ui::UiOption<decltype( config::name )> name##_;

#define SPTF_DEFINE_UI_OPTIONS( ... ) \
    QWR_EXPAND( QWR_PASTE( SPTF_DEFINE_UI_OPTION, __VA_ARGS__ ) )

    qwr::ui::UiOption<decltype( config::uploaderMode )> uploaderMode_;
    qwr::ui::UiOption<decltype( config::bundledUploaderService )> bundledUploaderService_;
    SPTF_DEFINE_UI_OPTIONS( bundledUploaderApiKey,
                            uploadArtworkCommand,
                            processTimeout )

#undef SPTF_DEFINE_UI_OPTIONS
#undef SPTF_DEFINE_UI_OPTION

    std::array<std::unique_ptr<qwr::ui::IUiDdxOption>, 5> ddxOptions_;
};

} // namespace drp::ui
