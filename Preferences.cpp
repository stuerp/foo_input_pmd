
/** $VER: Preferences.cpp (2023.07.09) P. Stuer **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <sdk/foobar2000-lite.h>

#include <helpers/foobar2000+atl.h>
#include <helpers/atl-misc.h>
#include <helpers/advconfig_impl.h>
#include <helpers/DarkMode.h>

#include <pfc/string-conv-lite.h>

#include "Preferences.h"
#include "Configuration.h"

#include "Resources.h"

#pragma hdrstop

const GUID PreferencesPageGUID = {0xea2369b2,0xf82e,0x425a,{0xbd,0x39,0x2f,0x4d,0xcf,0xe1,0x9e,0x38}}; // {ea2369b2-f82e-425a-bd39-2f4dcfe19e38}

/// <summary>
/// Implements the preferences page for the component.
/// </summary>
class Preferences : public CDialogImpl<Preferences>, public preferences_page_instance
{
public:
    Preferences(preferences_page_callback::ptr callback) : m_bMsgHandled(FALSE), _Callback(callback) { }

    Preferences(const Preferences&) = delete;
    Preferences(const Preferences&&) = delete;
    Preferences& operator=(const Preferences&) = delete;
    Preferences& operator=(Preferences&&) = delete;

    virtual ~Preferences() { };

    enum
    {
        IDD = IDD_PREFERENCES
    };

    #pragma region("preferences_page_instance")
    /// <summary>
    /// Returns a combination of preferences_state constants.
    /// </summary>
    virtual t_uint32 get_state() final
    {
        t_uint32 State = preferences_state::resettable | preferences_state::dark_mode_supported;

        if (HasChanged())
            State |= preferences_state::changed;

        return State;
    }

    /// <summary>
    /// Applies the changes to the preferences.
    /// </summary>
    virtual void apply() final
    {
        CfgSamplesPath = _SamplesPath;

        OnChanged();
    }

    /// <summary>
    /// Resets this page's content to the default values. Does not apply any changes - lets user preview the changes before hitting "apply".
    /// </summary>
    virtual void reset() final
    {
        _SamplesPath = ".";

        UpdateDialog();

        OnChanged();
    }
    #pragma endregion

    //WTL message map
    BEGIN_MSG_MAP_EX(Preferences)
        MSG_WM_INITDIALOG(OnInitDialog)

        COMMAND_HANDLER_EX(IDC_SAMPLES_PATH, EN_KILLFOCUS, OnLostFocus)
        COMMAND_HANDLER_EX(IDC_SAMPLES_PATH_SELECT, BN_CLICKED, OnButtonClicked)
    END_MSG_MAP()

private:
    /// <summary>
    /// Initializes the dialog.
    /// </summary>
    BOOL OnInitDialog(CWindow, LPARAM) noexcept
    {
        _DarkModeHooks.AddDialogWithControls(*this);

        _SamplesPath = CfgSamplesPath;

        UpdateDialog();

        return FALSE;
    }

    /// <summary>
    /// Handles the notification when a control loses focus.
    /// </summary>
    void OnLostFocus(UINT code, int id, CWindow) noexcept
    {
        if (code != EN_KILLFOCUS)
            return;

        WCHAR Text[MAX_PATH];

        GetDlgItemText(id, Text, _countof(Text));

        switch (id)
        {
            case IDC_SAMPLES_PATH:
                _SamplesPath = pfc::utf8FromWide(Text);
                break;

            default:
                return;
        }

        OnChanged();
    }

    /// <summary>
    /// Handles a click on a button.
    /// </summary>
    void OnButtonClicked(UINT, int id, CWindow) noexcept
    {
        if (id == IDC_SAMPLES_PATH_SELECT)
        {
            pfc::string8 DirectoryPath = _SamplesPath;

            if (::uBrowseForFolder(m_hWnd, "Locate PDX samples...", DirectoryPath))
            {
                _SamplesPath = DirectoryPath;

                pfc::wstringLite w = pfc::wideFromUTF8(DirectoryPath);

                SetDlgItemText(IDC_SAMPLES_PATH, w);

                OnChanged();
            }
        }
    }

    /// <summary>
    /// Returns whether our dialog content is different from the current configuration (whether the Apply button should be enabled or not)
    /// </summary>
    bool HasChanged() const noexcept
    {
        bool HasChanged = false;

        if (_SamplesPath != CfgSamplesPath)
            HasChanged = true;

        return HasChanged;
    }

    /// <summary>
    /// Tells the host that our state has changed to enable/disable the apply button appropriately.
    /// </summary>
    void OnChanged() const noexcept
    {
        _Callback->on_state_changed();
    }

    /// <summary>
    /// Updates the appearance of the dialog according to the values of the settings.
    /// </summary>
    void UpdateDialog() const noexcept
    {
        ::uSetDlgItemText(m_hWnd, IDC_SAMPLES_PATH, _SamplesPath);
    }

private:
    const preferences_page_callback::ptr _Callback;

    fb2k::CDarkModeHooks _DarkModeHooks;

    pfc::string8 _SamplesPath;
};

#pragma region("PreferencesPage")
/// <summary>
/// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
/// </summary>
class PreferencesPage : public preferences_page_impl<Preferences>
{
public:
    PreferencesPage() noexcept { };

    PreferencesPage(const PreferencesPage &) = delete;
    PreferencesPage(const PreferencesPage &&) = delete;
    PreferencesPage & operator=(const PreferencesPage &) = delete;
    PreferencesPage & operator=(PreferencesPage &&) = delete;

    virtual ~PreferencesPage() noexcept { };

    const char * get_name() noexcept
    {
        return STR_COMPONENT_NAME;
    }

    GUID get_guid() noexcept
    {
        return PreferencesPageGUID;
    }

    GUID get_parent_guid() noexcept
    {
        return guid_input;
    }
};

static preferences_page_factory_t<PreferencesPage> _PreferencesPageFactory;
#pragma endregion
