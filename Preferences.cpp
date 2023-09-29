
/** $VER: Preferences.cpp (2023.07.21) P. Stuer **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

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

const WCHAR * PlaybackModes[] =
{
    L"Loop never",
    L"Loop",
    L"Loop with fade out",
    L"Loop forever"
};
/*
const uint32_t SynthesisRates[] =
{
    SOUND_55K,
    SOUND_55K_2,
    SOUND_48K,
    SOUND_44K,
    SOUND_22K,
    SOUND_11K,
};
*/
/// <summary>
/// Implements the preferences page for the component.
/// </summary>
#pragma warning(disable: 4820) // x bytes padding added after last data member
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
        CfgPlaybackMode = _PlaybackMode;
        CfgLoopCount = _LoopCount;
        CfgFadeOutDuration = _FadeOutDuration;
        CfgSynthesisRate = _SynthesisRate;

        OnChanged();
    }

    /// <summary>
    /// Resets this page's content to the default values. Does not apply any changes - lets user preview the changes before hitting "apply".
    /// </summary>
    virtual void reset() final
    {
        _SamplesPath = DefaultSamplesPath;

        UpdateDialog();

        OnChanged();
    }
    #pragma endregion

    //WTL message map
    BEGIN_MSG_MAP_EX(Preferences)
        MSG_WM_INITDIALOG(OnInitDialog)

        COMMAND_HANDLER_EX(IDC_SAMPLES_PATH, EN_KILLFOCUS, OnEditUpdate)
        COMMAND_HANDLER_EX(IDC_SAMPLES_PATH_SELECT, BN_CLICKED, OnButtonClicked)

        COMMAND_HANDLER_EX(IDC_PLAYBACK_MODE, CBN_SELCHANGE, OnSelectionChanged)
        COMMAND_HANDLER_EX(IDC_LOOP_COUNT, EN_UPDATE, OnEditUpdate)
        COMMAND_HANDLER_EX(IDC_FADE_OUT_DURATION, EN_UPDATE, OnEditUpdate)

//      COMMAND_HANDLER_EX(IDC_SYNTHESIS_RATE, CBN_SELCHANGE, OnSelectionChanged)
    END_MSG_MAP()

private:
    /// <summary>
    /// Initializes the dialog.
    /// </summary>
    BOOL OnInitDialog(CWindow, LPARAM) noexcept
    {
        _DarkModeHooks.AddDialogWithControls(*this);

        _SamplesPath = CfgSamplesPath;
        _PlaybackMode = CfgPlaybackMode;
        _LoopCount = CfgLoopCount;
        _FadeOutDuration = CfgFadeOutDuration;
        _SynthesisRate = CfgSynthesisRate;

        ::uSetDlgItemText(m_hWnd, IDC_SAMPLES_PATH, _SamplesPath);

        {
            auto cb = (CComboBox) GetDlgItem(IDC_PLAYBACK_MODE);

            for (int i = 0; i < _countof(PlaybackModes); ++i)
                cb.AddString(PlaybackModes[i]);

            cb.SetCurSel((int) _PlaybackMode);
        }

        ::uSetDlgItemText(m_hWnd, IDC_LOOP_COUNT, pfc::format_int(_LoopCount));
        ::uSetDlgItemText(m_hWnd, IDC_FADE_OUT_DURATION, pfc::format_int(_FadeOutDuration));
/*
        {
            auto cb = (CComboBox) GetDlgItem(IDC_SYNTHESIS_RATE);

            int Index = 0;

            for (int i = 0; i < _countof(SynthesisRates); ++i)
            {
                cb.AddString(pfc::wideFromUTF8(pfc::format_int(SynthesisRates[i])));

                if (_SynthesisRate == SynthesisRates[i])
                    Index = i;
            }

            cb.SetCurSel(Index);
        }
*/
        UpdateDialog();

        return FALSE;
    }
/*
    /// <summary>
    /// Handles the notification when a control loses focus.
    /// </summary>
    void OnLostFocus(UINT code, int id, CWindow) noexcept
    {
        if (!((code == EN_KILLFOCUS) && (id == IDC_SAMPLES_PATH)))
            return;

        WCHAR Text[MAX_PATH];

        GetDlgItemText(id, Text, _countof(Text));

        _SamplesPath = pfc::utf8FromWide(Text);

        OnChanged();
    }
*/
    /// <summary>
    /// Handles a click on a button.
    /// </summary>
    void OnButtonClicked(UINT, int id, CWindow) noexcept
    {
        if (id != IDC_SAMPLES_PATH_SELECT)
            return;

        pfc::string8 DirectoryPath = _SamplesPath;

        if (::uBrowseForFolder(m_hWnd, "Locate PDX samples...", DirectoryPath))
        {
            _SamplesPath = DirectoryPath;

            pfc::wstringLite w = pfc::wideFromUTF8(DirectoryPath);

            SetDlgItemText(IDC_SAMPLES_PATH, w);

            OnChanged();
        }
    }

    /// <summary>
    /// Handles a combobox change.
    /// </summary>
    void OnSelectionChanged(UINT, int id, CWindow) noexcept
    {
        switch (id)
        {
            case IDC_PLAYBACK_MODE:
            {
                auto cb = (CComboBox) GetDlgItem(IDC_PLAYBACK_MODE);

                _PlaybackMode = (uint32_t) cb.GetCurSel();
                break;
            }
/*
            case IDC_SYNTHESIS_RATE:
            {
                auto cb = (CComboBox) GetDlgItem(IDC_SYNTHESIS_RATE);

                _SynthesisRate = SynthesisRates[cb.GetCurSel()];
                break;
            }
*/
        }

        UpdateDialog();

        OnChanged();
    }

    /// <summary>
    /// Handles a textbox update.
    /// </summary>
    void OnEditUpdate(UINT, int id, CWindow) noexcept
    {
        switch (id)
        {
            case IDC_SAMPLES_PATH:
            {
                _SamplesPath = ::uGetDlgItemText(m_hWnd, IDC_SAMPLES_PATH);
                OnChanged();
                break;
            }

            case IDC_LOOP_COUNT:
            {
                pfc::string8 Text = ::uGetDlgItemText(m_hWnd, IDC_LOOP_COUNT);

                char * p; long Value = ::strtol(Text, &p, 10);

                if ((*p == '\0') && (Value > 0) && (Value <= ~0U))
                {
                    _LoopCount = (uint32_t) Value;
                    OnChanged();
                }
                break;
            }

            case IDC_FADE_OUT_DURATION:
            {
                pfc::string8 Text = ::uGetDlgItemText(m_hWnd, IDC_FADE_OUT_DURATION);

                char * p; long Value = ::strtol(Text, &p, 10);

                if ((*p == '\0') && (Value > 0) && (Value <= ~0U))
                {
                    _FadeOutDuration = (uint32_t) Value;
                    OnChanged();
                }
                break;
            }
        }
    }

    /// <summary>
    /// Returns whether our dialog content is different from the current configuration (whether the Apply button should be enabled or not)
    /// </summary>
    bool HasChanged() const noexcept
    {
        if (_SamplesPath != CfgSamplesPath)
            return true;

        if (_PlaybackMode != CfgPlaybackMode)
            return true;

        if (_LoopCount != CfgLoopCount)
            return true;

        if (_FadeOutDuration != CfgFadeOutDuration)
            return true;

//      if (_SynthesisRate != CfgSynthesisRate)
//          return true;

        return false;
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
        bool Flag = (_PlaybackMode == PlaybackModes::Loop) || (_PlaybackMode == PlaybackModes::LoopWithFadeOut);

        GetDlgItem(IDC_LOOP_COUNT).EnableWindow(Flag);

        Flag = (_PlaybackMode == PlaybackModes::LoopWithFadeOut);

        GetDlgItem(IDC_FADE_OUT_DURATION).EnableWindow(Flag);
    }

private:
    const preferences_page_callback::ptr _Callback;

    fb2k::CDarkModeHooks _DarkModeHooks;

    pfc::string8 _SamplesPath;
    uint32_t _PlaybackMode;
    uint32_t _LoopCount;
    uint32_t _FadeOutDuration;
    uint32_t _SynthesisRate;
};
#pragma warning(default: 4820) // x bytes padding added after last data member

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
