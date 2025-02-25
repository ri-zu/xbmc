/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *      Copyright (C) 2014-2015 Aracnoz
 *      http://github.com/aracnoz/xbmc
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "GUIDialogLAVSplitter.h"
#include "Application.h"
#include "URL.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/File.h"
#include "guilib/LocalizeStrings.h"
#include "profiles/ProfilesManager.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "input/Key.h"
#include "utils/XMLUtils.h"
#include "Filters/RendererSettings.h"
#include "PixelShaderList.h"
#include "cores/playercorefactory/PlayerCoreFactory.h"
#include "Filters/LAVAudioSettings.h"
#include "Filters/LAVVideoSettings.h"
#include "Filters/LAVSplitterSettings.h"
#include "utils/CharsetConverter.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "addons/Skin.h"
#include "GraphFilters.h"
#include "utils/CharsetConverter.h"

#define LAVSPLITTER_PROPERTYPAGE      "lavsplitter.propertypage"
#define LAVSPLITTER_TRAYICON          "lavsplitter.trayicon"
#define LAVSPLITTER_PREFAUDIOLANG     "lavsplitter.prefaudiolang"
#define LAVSPLITTER_PREFSUBLANG       "lavsplitter.prefsublang"
#define LAVSPLITTER_PREFSUBADVANCED   "lavsplitter.prefsubadcanced"
#define LAVSPLITTER_SUBMODE           "lavsplitter.submode"
#define LAVSPLITTER_PGSFORCEDSTREAM   "lavsplitter.pgsforcedstream"
#define LAVSPLITTER_PGSONLYFORCED     "lavsplitter.pgsonlyforced"
#define LAVSPLITTER_IVC1MODE          "lavsplitter.ivc1mode"
#define LAVSPLITTER_MATROSKAEXTERNAL  "lavsplitter.matroskaexternal"
#define LAVSPLITTER_SUBSTREAM         "lavsplitter.substream"
#define LAVSPLITTER_REMAUDIOSTREAM    "lavsplitter.remaudiostream"
#define LAVSPLITTER_PREFHQAUDIO       "lavsplitter.prefhqaudio"
#define LAVSPLITTER_IMPAIREDAUDIO     "lavsplitter.impairedaudio"
#define LAVSPLITTER_RESET             "lavsplitter.reset"

using namespace std;

CGUIDialogLAVSplitter::CGUIDialogLAVSplitter()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_LAVSPLITTER, "DialogSettings.xml")
{
}

CGUIDialogLAVSplitter::~CGUIDialogLAVSplitter()
{ 
}

void CGUIDialogLAVSplitter::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(55079);

  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogLAVSplitter::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  CSettingCategory *category = AddCategory("dsplayerlavsplitter", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupProperty = AddGroup(category);
  if (groupProperty == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *group = AddGroup(category);
  if (group == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  CSettingGroup *groupPreflang = AddGroup(category);
  if (groupPreflang == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupSubmode = AddGroup(category);
  if (groupSubmode == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupBluraysub = AddGroup(category);
  if (groupBluraysub == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupFormat = AddGroup(category);
  if (groupFormat == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  CSettingGroup *groupDemuxer = AddGroup(category);
  if (groupDemuxer == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  CSettingGroup *groupQueueNet = AddGroup(category);
  if (groupQueueNet == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVSplitter: unable to setup settings");
    return;
  }
  CSettingGroup *groupReset = AddGroup(category);
  if (groupReset == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVAudio: unable to setup settings");
    return;
  }

  // Get settings from the current running filter
  IBaseFilter *pBF;
  CGraphFilters::Get()->GetInternalFilter(CGraphFilters::INTERNAL_LAVSPLITTER, &pBF);
  CGraphFilters::Get()->GetLavSettings(CGraphFilters::INTERNAL_LAVSPLITTER, pBF);

  StaticIntegerSettingOptions entries;
  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  // BUTTON
  AddButton(groupProperty, LAVSPLITTER_PROPERTYPAGE, 80013, 0);

  // TRAYICON
  AddToggle(group, LAVSPLITTER_TRAYICON, 80001, 0, lavSettings.splitter_bTrayIcon);

  // PREFLANG

  // dependencies
  CSettingDependency dependencyPrefSubLangVisible(SettingDependencyTypeVisible, m_settingsManager);
  dependencyPrefSubLangVisible.Or()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(LAVSPLITTER_SUBMODE, "3", SettingDependencyOperatorEquals, true, m_settingsManager)));
  SettingDependencies depsPrefSubLangVisible;
  depsPrefSubLangVisible.push_back(dependencyPrefSubLangVisible);

  CSettingDependency dependencyPrefSubAdvVisible(SettingDependencyTypeVisible, m_settingsManager);
  dependencyPrefSubAdvVisible.Or()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(LAVSPLITTER_SUBMODE, "3", SettingDependencyOperatorEquals, false, m_settingsManager)));
  SettingDependencies depsPrefSubAdvVisible;
  depsPrefSubAdvVisible.push_back(dependencyPrefSubAdvVisible);

  std::string str;
  g_charsetConverter.wToUTF8(lavSettings.splitter_prefAudioLangs, str, false);
  AddEdit(groupPreflang, LAVSPLITTER_PREFAUDIOLANG, 82001, 0, str, true);
  
  g_charsetConverter.wToUTF8(lavSettings.splitter_prefSubLangs , str, false);
  CSetting *settingPrefSubLang;
  settingPrefSubLang = AddEdit(groupPreflang, LAVSPLITTER_PREFSUBLANG, 82002, 0, str, true);
  settingPrefSubLang->SetDependencies(depsPrefSubLangVisible);

  g_charsetConverter.wToUTF8(lavSettings.splitter_subtitleAdvanced, str, false);
  CSetting *settingPrefSubAdv;
  settingPrefSubAdv = AddEdit(groupPreflang, LAVSPLITTER_PREFSUBADVANCED, 82016, 0, str, true);
  settingPrefSubAdv->SetDependencies(depsPrefSubAdvVisible);

  //SUBMODE
  entries.clear();
  entries.emplace_back(82004, LAVSubtitleMode_NoSubs);
  entries.emplace_back(82005, LAVSubtitleMode_ForcedOnly);
  entries.emplace_back(82006, LAVSubtitleMode_Default);
  entries.emplace_back(82007, LAVSubtitleMode_Advanced);
  AddList(groupSubmode, LAVSPLITTER_SUBMODE, 82003, 0, lavSettings.splitter_subtitleMode, entries, 82003);

  //BLURAYSUB
  AddToggle(groupBluraysub, LAVSPLITTER_PGSFORCEDSTREAM, 82008, 0, lavSettings.splitter_bPGSForcedStream);
  AddToggle(groupBluraysub, LAVSPLITTER_PGSONLYFORCED, 82009, 0, lavSettings.splitter_bPGSOnlyForced);

  //FORMAT
  AddToggle(groupFormat, LAVSPLITTER_IVC1MODE, 82010, 0, lavSettings.splitter_iVC1Mode);
  AddToggle(groupFormat, LAVSPLITTER_MATROSKAEXTERNAL, 82011, 0, lavSettings.splitter_bMatroskaExternalSegments);

  //DEMUXER
  AddToggle(groupDemuxer, LAVSPLITTER_SUBSTREAM, 82012, 0, lavSettings.splitter_bSubstreams);
  AddToggle(groupDemuxer, LAVSPLITTER_REMAUDIOSTREAM, 82013, 0, lavSettings.splitter_bStreamSwitchRemoveAudio);
  AddToggle(groupDemuxer, LAVSPLITTER_PREFHQAUDIO, 82014, 0, lavSettings.splitter_bPreferHighQualityAudio);
  AddToggle(groupDemuxer, LAVSPLITTER_IMPAIREDAUDIO, 82015, 0, lavSettings.splitter_bImpairedAudio);

  // BUTTON RESET
  if (!g_application.m_pPlayer->IsPlayingVideo())
    AddButton(groupReset, LAVSPLITTER_RESET, 10041, 0);
}

void CGUIDialogLAVSplitter::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);
  const std::string &settingId = setting->GetId();

  std::wstring strW;

  if (settingId == LAVSPLITTER_TRAYICON)
    lavSettings.splitter_bTrayIcon = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_PREFAUDIOLANG)
  { 
    g_charsetConverter.utf8ToW(static_cast<std::string>(static_cast<const CSettingString*>(setting)->GetValue()), strW, false);
    lavSettings.splitter_prefAudioLangs = strW;
  }
  if (settingId == LAVSPLITTER_PREFSUBLANG)
  {
    g_charsetConverter.utf8ToW(static_cast<std::string>(static_cast<const CSettingString*>(setting)->GetValue()), strW, false);
    lavSettings.splitter_prefSubLangs = strW;
  }
  if (settingId == LAVSPLITTER_PREFSUBADVANCED)
  {
    g_charsetConverter.utf8ToW(static_cast<std::string>(static_cast<const CSettingString*>(setting)->GetValue()), strW, false);
    lavSettings.splitter_subtitleAdvanced = strW;
  }
  if (settingId == LAVSPLITTER_SUBMODE)
    lavSettings.splitter_subtitleMode = (LAVSubtitleMode)static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_PGSFORCEDSTREAM)
    lavSettings.splitter_bPGSForcedStream = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_PGSONLYFORCED)
    lavSettings.splitter_bPGSOnlyForced = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_IVC1MODE)
    lavSettings.splitter_iVC1Mode = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_MATROSKAEXTERNAL)
    lavSettings.splitter_bMatroskaExternalSegments = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_SUBSTREAM)
    lavSettings.splitter_bSubstreams = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_REMAUDIOSTREAM)
    lavSettings.splitter_bStreamSwitchRemoveAudio = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_PREFHQAUDIO)
    lavSettings.splitter_bPreferHighQualityAudio = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVSPLITTER_IMPAIREDAUDIO)
    lavSettings.splitter_bImpairedAudio = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());

  // Get current running filter
  IBaseFilter *pBF;
  CGraphFilters::Get()->GetInternalFilter(CGraphFilters::INTERNAL_LAVSPLITTER, &pBF);

  // Set settings changes into the running filter
  CGraphFilters::Get()->SetLavSettings(CGraphFilters::INTERNAL_LAVSPLITTER, pBF);

  // Save new settings into DSPlayer DB
  CGraphFilters::Get()->SaveLavSettings(CGraphFilters::INTERNAL_LAVSPLITTER);
}

void CGUIDialogLAVSplitter::OnSettingAction(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);
  const std::string &settingId = setting->GetId();

  if (settingId == LAVSPLITTER_PROPERTYPAGE)
  {
    CGraphFilters::Get()->ShowInternalPPage(CGraphFilters::INTERNAL_LAVSPLITTER, true);
    this->Close();
  }

  if (settingId == LAVSPLITTER_RESET)
  {
    if (!CGUIDialogYesNo::ShowAndGetInput(10041, 10042, 0, 0))
      return;

    CGraphFilters::Get()->EraseLavSetting(CGraphFilters::INTERNAL_LAVSPLITTER);
    this->Close();
  }
}

