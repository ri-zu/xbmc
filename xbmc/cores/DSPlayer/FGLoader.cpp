/*
 *      Copyright (C) 2005-2009 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef HAS_DS_PLAYER

#include "FGLoader.h"
#include "DSPlayer.h"
#include "Filters/RendererSettings.h"
#include "PixelShaderList.h"
#include "streamsmanager.h"
#include "DSUtil/DSUtil.h"
#include "DSUtil/SmartPtr.h"

#include "utils/charsetconverter.h"
#include "utils/Log.h"
#include "dialogs/GUIDialogOK.h"
#include "guilib/GUIWindowManager.h"
#include "filtercorefactory/filtercorefactory.h"
#include "profiles/ProfilesManager.h"
#include "settings/Settings.h"
#include "utils/SystemInfo.h"

#include "filters/XBMCFileSource.h"
#include "filters/VMR9AllocatorPresenter.h"
#include "filters/EVRAllocatorPresenter.h"
#include "Filters/madVRAllocatorPresenter.h"

#include "Utils/AudioEnumerator.h"
#include "Utils/DSFilterEnumerator.h"
#include "DVDFileInfo.h"
#include "video/VideoInfoTag.h"
#include "utils/URIUtils.h"
#include "utils/DSFileUtils.h"
#include "Filters/Sanear/Factory.h"
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

namespace
{
  std::vector<std::pair<std::string, std::string>> GetDevices()
  {
    std::vector<std::pair<std::string, std::string>> ret;

    Com::SmartPtr<IMMDeviceEnumerator> enumerator;
    Com::SmartPtr<IMMDeviceCollection> collection;
    UINT count = 0;

    if (SUCCEEDED(enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER)) &&
      SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection)) &&
      SUCCEEDED(collection->GetCount(&count))) {

      for (UINT i = 0; i < count; i++) {
        LPWSTR id = nullptr;
        Com::SmartPtr<IMMDevice> device;
        Com::SmartPtr<IPropertyStore> devicePropertyStore;
        PROPVARIANT friendlyName;
        PropVariantInit(&friendlyName);

        if (SUCCEEDED(collection->Item(i, &device)) &&
          SUCCEEDED(device->GetId(&id)) &&
          SUCCEEDED(device->OpenPropertyStore(STGM_READ, &devicePropertyStore)) &&
          SUCCEEDED(devicePropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName))) {

          std::string sFriendlyName;
          std::string sId;
          g_charsetConverter.wToUTF8(friendlyName.pwszVal, sFriendlyName);
          g_charsetConverter.wToUTF8(static_cast<LPWSTR>(id), sId);

          ret.emplace_back(sFriendlyName, sId);
          PropVariantClear(&friendlyName);
          CoTaskMemFree(id);
        }
      }
    }

    return ret;
  }
}

using namespace std;

CFGLoader::CFGLoader()
  :m_pFGF(NULL)
  , m_bIsAutoRender(false)
{
}

CFGLoader::~CFGLoader()
{
  CSingleLock lock(*this);

  CFilterCoreFactory::Destroy();
  SAFE_DELETE(m_pFGF);

  CLog::Log(LOGDEBUG, "%s Ressources released", __FUNCTION__);
}

HRESULT CFGLoader::InsertSourceFilter(CFileItem& pFileItem, const std::string& filterName)
{

  HRESULT hr = E_FAIL;

  /* XBMC SOURCE FILTER  */
  /*if (CUtil::IsInArchive(pFileItem.GetPath()))
  {
  CLog::Log(LOGNOTICE,"%s File \"%s\" need a custom source filter", __FUNCTION__, pFileItem.GetPath().c_str());
  CXBMCAsyncStream* pXBMCStream = new CXBMCAsyncStream(pFileItem.GetPath(), &CGraphFilters::Get()->Source.pBF, &hr);
  if (SUCCEEDED(hr))
  {
  hr = g_dsGraph->pFilterGraph->AddFilter(CGraphFilters::Get()->Source.pBF, L"XBMC Source Filter");
  if (FAILED(hr))
  {
  CLog::Log(LOGERROR, "%s Failed to add xbmc source filter to the graph", __FUNCTION__);
  return hr;
  }
  CGraphFilters::Get()->Source.osdname = "XBMC File Source";
  CGraphFilters::Get()->Source.pData = (void *) pXBMCStream;
  CGraphFilters::Get()->Source.isinternal = true;
  CLog::Log(LOGNOTICE, "%s Successfully added xbmc source filter to the graph", __FUNCTION__);
  }
  return hr;
  }*/
  /* DVD NAVIGATOR */
  if (pFileItem.IsDVDFile())
  {
    std::string path = pFileItem.GetPath();
    if (StringUtils::EqualsNoCase(StringUtils::Left(path, 6), "smb://"))
    {
      StringUtils::Replace(path, "smb://", "//");
      pFileItem.SetPath(path);
    }

    hr = InsertFilter(filterName, CGraphFilters::Get()->Splitter);
    if (SUCCEEDED(hr))
    {
      if (!((CGraphFilters::Get()->DVD.dvdControl = CGraphFilters::Get()->Splitter.pBF) && (CGraphFilters::Get()->DVD.dvdInfo = CGraphFilters::Get()->Splitter.pBF)))
      {
        CGraphFilters::Get()->DVD.Clear();
        return E_NOINTERFACE;
      }
    }

    CGraphFilters::Get()->SetIsDVD(true);
    std::string dirA;
    std::wstring dirW;
    dirA = URIUtils::GetDirectory(pFileItem.GetPath());
    g_charsetConverter.utf8ToW(dirA, dirW);

    hr = CGraphFilters::Get()->DVD.dvdControl->SetDVDDirectory(dirW.c_str());
    if (FAILED(hr))
      CLog::Log(LOGERROR, "%s Failed loading dvd directory.", __FUNCTION__);

    CGraphFilters::Get()->DVD.dvdControl->SetOption(DVD_ResetOnStop, FALSE);
    CGraphFilters::Get()->DVD.dvdControl->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);

    return hr;
  }

  if (StringUtils::EqualsNoCase(filterName, "internal_urlsource"))
  {
    //TODO
    //add IAMOpenProgress for requesting the status of stream without this interface the player failed if connection is too slow
    Com::SmartQIPtr<IFileSourceFilter> pSourceUrl;
    Com::SmartPtr<IUnknown> pUnk = NULL;

    pUnk.CoCreateInstance(CLSID_URLReader, NULL);
    hr = pUnk->QueryInterface(IID_IBaseFilter, (void**)&CGraphFilters::Get()->Source.pBF);
    if (SUCCEEDED(hr))
    {
      hr = g_dsGraph->pFilterGraph->AddFilter(CGraphFilters::Get()->Source.pBF, L"URLReader");
      CGraphFilters::Get()->Source.osdname = "URLReader";
      std::wstring strUrlW; g_charsetConverter.utf8ToW(pFileItem.GetPath(), strUrlW);

      if (pSourceUrl = pUnk)
        hr = pSourceUrl->Load(strUrlW.c_str(), NULL);

      if (FAILED(hr))
      {
        g_dsGraph->pFilterGraph->RemoveFilter(CGraphFilters::Get()->Source.pBF);
        CLog::Log(LOGERROR, "%s Failed to add url source filter to the graph.", __FUNCTION__);
        CGraphFilters::Get()->Source.pBF = NULL;
        return E_FAIL;
      }

      ParseStreamingType(pFileItem, CGraphFilters::Get()->Source.pBF);

      return S_OK;
    }
  }

  /* Two cases:
  1/ The source filter is also a splitter. We insert it to the graph as a splitter and load the file
  2/ The source filter is only a source filter. Add it to the graph as a source filter
  */

  // TODO: Loading an url must be done!

  // If the source filter has more than one ouput pin, it's a splitter too.
  // Only problem, the file must be loaded in the source filter to see the
  // number of output pin
  CURL url(pFileItem.GetPath());

  std::string pWinFilePath = url.Get();
  /*
  * Convert SMB to windows UNC
  * SMB: smb://HOSTNAME/share/file.ts
  * SMB: smb://user:pass@HOSTNAME/share/file.ts
  * windows UNC: \\\\HOSTNAME\share\file.ts
  */
  pWinFilePath = CDSFile::SmbToUncPath(pWinFilePath);

  if (!pFileItem.IsInternetStream(true))
    StringUtils::Replace(pWinFilePath, "/", "\\");

  std::wstring strFileW;
  g_charsetConverter.utf8ToW(pWinFilePath, strFileW, false);
  SFilterInfos infos;
  try // Load() may crash on bad designed codec. Prevent XBMC to hang
  {
    if (FAILED(hr = InsertFilter(filterName, infos)))
    {
      if (infos.isinternal)
        delete infos.pData;
      return E_FAIL;
    }

    Com::SmartQIPtr<IFileSourceFilter> pFS = infos.pBF;

    if (SUCCEEDED(pFS->Load(strFileW.c_str(), NULL)))
      CLog::Log(LOGNOTICE, "%s Successfully loaded file in the splitter/source", __FUNCTION__);
    else
    {
      CLog::Log(LOGERROR, "%s Failed to load file in the splitter/source", __FUNCTION__);

      if (infos.isinternal)
        delete infos.pData;

      return E_FAIL;
    }
  }
  catch (...) {
    CLog::Log(LOGERROR, "%s An exception has been thrown by the codec...", __FUNCTION__);

    if (infos.isinternal)
      delete infos.pData;

    return E_FAIL;
  }

  bool isSplitterToo = IsSplitter(infos.pBF);
  if ((pFileItem.GetVideoInfoTag()->m_streamDetails.GetVideoStreamCount() == 1 && pFileItem.GetVideoInfoTag()->m_streamDetails.GetAudioStreamCount() == 0) 
    || (GetCLSID(infos.pBF) == CLSID_LAVSplitterSource))
    isSplitterToo = true;

  if (isSplitterToo)
  {
    CLog::Log(LOGDEBUG, "%s The source filter is also a splitter.", __FUNCTION__);
    CGraphFilters::Get()->Splitter = infos;
  }
  else {
    CGraphFilters::Get()->Source = infos;
  }

  ParseStreamingType(pFileItem, infos.pBF);

  return hr;
}

void CFGLoader::ParseStreamingType(CFileItem& pFileItem, IBaseFilter* pBF)
{
  // Detect the type of the streaming, in order to choose the right splitter
  // Often, streaming url does not have an extension ...
  GUID guid;
  BeginEnumPins(pBF, pEP, pPin)
  {
    PIN_DIRECTION pPinDir;
    pPin->QueryDirection(&pPinDir);
    if (pPinDir != PINDIR_OUTPUT)
      continue;

    BeginEnumMediaTypes(pPin, pEMT, pMT)
    {
      if (pMT->majortype != MEDIATYPE_Stream)
        continue;

      std::string str;
      g_charsetConverter.wToUTF8(GetMediaTypeName(pMT->subtype), str);
      CLog::Log(LOGDEBUG, __FUNCTION__" Streaming output pin media tpye: %s", str.c_str());
      guid = pMT->subtype;
    }
    EndEnumMediaTypes(pMT);
  }
  EndEnumPins;

}

HRESULT CFGLoader::InsertSplitter(const CFileItem& pFileItem, const std::string& filterName)
{
  HRESULT hr = InsertFilter(filterName, CGraphFilters::Get()->Splitter);

  if (SUCCEEDED(hr))
  {
    if (SUCCEEDED(hr = ConnectFilters(g_dsGraph->pFilterGraph, CGraphFilters::Get()->Source.pBF, CGraphFilters::Get()->Splitter.pBF)))
      CLog::Log(LOGNOTICE, "%s Successfully connected the source to the splitter", __FUNCTION__);
    else
      CLog::Log(LOGERROR, "%s Failed to connect the source to the splitter", __FUNCTION__);
  }

  return hr;
}

HRESULT CFGLoader::InsertAudioRenderer(const std::string& filterName)
{
  HRESULT hr = S_FALSE;
  CFGFilterRegistry* pFGF;
  std::string sAudioRenderName, sAudioRenderDisplayName;
  if (!filterName.empty())
  {
    if (SUCCEEDED(InsertFilter(filterName, CGraphFilters::Get()->AudioRenderer)))
      return S_OK;
    else
      CLog::Log(LOGERROR, "%s Failed to insert custom audio renderer, fallback to default one", __FUNCTION__);
  }

  std::vector<DSFilterInfo> deviceList;
  START_PERFORMANCE_COUNTER
    CAudioEnumerator p_dsound;
  p_dsound.GetAudioRenderers(deviceList);
  END_PERFORMANCE_COUNTER("Loaded audio renderer list");

  //see if there a config first 
  const std::string renderer = CSettings::GetInstance().GetString(CSettings::SETTING_DSPLAYER_AUDIORENDERER);

  if (renderer == CGraphFilters::INTERNAL_SANEAR)
  {
    struct SaneAudioRendererFilter : CFGFilter {
      SaneAudioRendererFilter(std::string name) :
        CFGFilter(SaneAudioRenderer::Factory::GetFilterGuid(), CFGFilter::FILE, name) {}

      HRESULT Create(IBaseFilter** ppBF) override {
        return SaneAudioRenderer::Factory::CreateFilter(CGraphFilters::Get()->sanear, ppBF);
      }
    };

    CGraphFilters::Get()->SetSanearSettings();

    CFGFilter *pFilter;
    START_PERFORMANCE_COUNTER
      pFilter = DNew SaneAudioRendererFilter("(i) Sanear Audio Renderer");
    hr = pFilter->Create(&CGraphFilters::Get()->AudioRenderer.pBF);
    delete pFilter;
    END_PERFORMANCE_COUNTER("Loaded internal sanear audio renderer");

    if (SUCCEEDED(hr))
    {
      START_PERFORMANCE_COUNTER
        hr = g_dsGraph->pFilterGraph->AddFilter(CGraphFilters::Get()->AudioRenderer.pBF, L"(i) Sanear Audio Renderer");
      END_PERFORMANCE_COUNTER("Added internal sanear audio renderer to the graph");

      CGraphFilters::Get()->AudioRenderer.internalFilter = true;
      CLog::Log(LOGNOTICE, "%s Successfully added internal sanear audio renderer to the graph", __FUNCTION__);
    } 
    else
    {
      CLog::Log(LOGERROR, "%s Failed to create the intenral audio renderer (%X)", __FUNCTION__, hr);
    }
  }
  
  if (renderer != CGraphFilters::INTERNAL_SANEAR || FAILED(hr))
  {
    for (std::vector<DSFilterInfo>::const_iterator iter = deviceList.begin(); !renderer.empty() && (iter != deviceList.end()); ++iter)
    {
      DSFilterInfo dev = *iter;
      if (StringUtils::EqualsNoCase(renderer, dev.lpstrName))
      {
        sAudioRenderName = dev.lpstrName;
        sAudioRenderDisplayName = dev.lpstrDisplayName;
        START_PERFORMANCE_COUNTER
          //pFGF = new CFGFilterRegistry(GUIDFromString(dev.lpstrGuid));
          pFGF = new CFGFilterRegistry(dev.lpstrDisplayName);
        hr = pFGF->Create(&CGraphFilters::Get()->AudioRenderer.pBF);
        delete pFGF;
        END_PERFORMANCE_COUNTER("Loaded audio renderer from registry");

        if (FAILED(hr))
        {
          CLog::Log(LOGERROR, "%s Failed to create the audio renderer (%X)", __FUNCTION__, hr);
          return hr;
        }

        START_PERFORMANCE_COUNTER
          std::wstring sNameW;
        g_charsetConverter.utf8ToW(dev.lpstrName, sNameW);
        hr = g_dsGraph->pFilterGraph->AddFilter(CGraphFilters::Get()->AudioRenderer.pBF, sNameW.c_str());
        END_PERFORMANCE_COUNTER("Added audio renderer to the graph");

        break;
      }
    }  
    
  if (SUCCEEDED(hr))
    CLog::Log(LOGNOTICE, "%s Successfully added \"%s\" - DiplayName: %s to the graph", __FUNCTION__, sAudioRenderName.c_str(), sAudioRenderDisplayName.c_str());
  else
    CLog::Log(LOGNOTICE, "%s Failed to add \"%s\" to the graph (result: %X)", __FUNCTION__, sAudioRenderName.c_str(), hr);
  }

  return hr;
}

HRESULT CFGLoader::InsertVideoRenderer()
{
  HRESULT hr = S_OK;

  std::string videoRender;
  videoRender = CSettings::GetInstance().GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER);

  if (videoRender == "EVR")
  { 
    m_pFGF = new CFGFilterVideoRenderer(CLSID_EVRAllocatorPresenter, "Kodi EVR");
  }
  if (videoRender == "VMR9")
  { 
    m_pFGF = new CFGFilterVideoRenderer(CLSID_VMR9AllocatorPresenter, "Kodi VMR9");
  }
  if (videoRender == "madVR")
  {
    m_pFGF = new CFGFilterVideoRenderer(CLSID_madVRAllocatorPresenter, "Kodi madVR");
  }

  hr = m_pFGF->Create(&CGraphFilters::Get()->VideoRenderer.pBF);
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "%s Failed to create allocator presenter (hr = %X)", __FUNCTION__, hr);
    return hr;
  }
  hr = g_dsGraph->pFilterGraph->AddFilter(CGraphFilters::Get()->VideoRenderer.pBF, m_pFGF->GetNameW().c_str());

  /* Query IQualProp from the renderer */
  CGraphFilters::Get()->VideoRenderer.pBF->QueryInterface(IID_IQualProp, (void **)&CGraphFilters::Get()->VideoRenderer.pQualProp);

  if (SUCCEEDED(hr))
  {
    CLog::Log(LOGDEBUG, "%s Allocator presenter successfully added to the graph (Renderer: %s)", __FUNCTION__, CGraphFilters::Get()->VideoRenderer.osdname.c_str());
  }
  else {
    CLog::Log(LOGERROR, "%s Failed to add allocator presenter to the graph (hr = %X)", __FUNCTION__, hr);
  }

  return hr;
}

HRESULT CFGLoader::LoadFilterRules(const CFileItem& _pFileItem)
{
  CFileItem pFileItem = _pFileItem;

  if (!pFileItem.IsInternetStream() && !CFilterCoreFactory::SomethingMatch(pFileItem))
  {

    CLog::Log(LOGERROR, "%s Extension \"%s\" not found. Please check mediasconfig.xml",
      __FUNCTION__, CURL(pFileItem.GetPath()).GetFileType().c_str());
    CGUIDialogOK *dialog = (CGUIDialogOK *)g_windowManager.GetWindow(WINDOW_DIALOG_OK);
    if (dialog)
    {
      dialog->SetHeading("Extension not found");
      dialog->SetLine(0, "Impossible to play the media file : the media");
      dialog->SetLine(1, "extension \"" + CURL(pFileItem.GetPath()).GetFileType() + "\" isn't declared in mediasconfig.xml.");
      dialog->SetLine(2, "");
      CDSPlayer::errorWindow = dialog;
    }
    return E_FAIL;
  }

  std::string filter = "";

  START_PERFORMANCE_COUNTER
    CFilterCoreFactory::GetAudioRendererFilter(pFileItem, filter);
  InsertAudioRenderer(filter); // First added, last connected
  END_PERFORMANCE_COUNTER("Loading audio renderer");

  START_PERFORMANCE_COUNTER
    InsertVideoRenderer();
  END_PERFORMANCE_COUNTER("Loading video renderer");

  // We *need* those informations for filter loading. If the user wants it, be sure it's loaded
  // before using it.
  bool hasStreamDetails = false;
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_EXTRACTFLAGS) &&
    pFileItem.HasVideoInfoTag() && !pFileItem.GetVideoInfoTag()->HasStreamDetails())
  {
    CLog::Log(LOGDEBUG, "%s - trying to extract filestream details from video file %s", __FUNCTION__, CURL::GetRedacted(pFileItem.GetPath()).c_str());
    hasStreamDetails = CDVDFileInfo::GetFileStreamDetails(&pFileItem);
  }
  else
    hasStreamDetails = pFileItem.HasVideoInfoTag() && pFileItem.GetVideoInfoTag()->HasStreamDetails();

  START_PERFORMANCE_COUNTER
    if (FAILED(CFilterCoreFactory::GetSourceFilter(pFileItem, filter)))
    {
    CLog::Log(LOGERROR, __FUNCTION__" Failed to get the source filter");
    return E_FAIL;
    }

  if (FAILED(InsertSourceFilter(pFileItem, filter)))
  {
    CLog::Log(LOGERROR, __FUNCTION__" Failed to insert the source filter");
    return E_FAIL;
  }
  END_PERFORMANCE_COUNTER("Loading source filter");
  START_PERFORMANCE_COUNTER
    if (!CGraphFilters::Get()->Splitter.pBF)
    {
    if (FAILED(CFilterCoreFactory::GetSplitterFilter(pFileItem, filter)))
      return E_FAIL;

    if (FAILED(InsertSplitter(pFileItem, filter)))
    {
      return E_FAIL;
    }
    }
  END_PERFORMANCE_COUNTER("Loading splitter filter");


  START_PERFORMANCE_COUNTER
    if (SUCCEEDED(CFilterCoreFactory::GetSubsFilter(pFileItem, filter, CGraphFilters::Get()->IsUsingDXVADecoder())))
    {
      if (CSettings::GetInstance().GetInt(CSettings::SETTING_DSPLAYER_FILTERSMANAGEMENT) == INTERNALFILTERS 
        && CSettings::GetInstance().GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER) == "EVR"
        && filter == CGraphFilters::INTERNAL_XYSUBFILTER)
        filter = CGraphFilters::INTERNAL_XYVSFILTER;

      if (FAILED(InsertFilter(filter, CGraphFilters::Get()->Subs)))
        return E_FAIL;
      END_PERFORMANCE_COUNTER("Loading subs filter");
    }

    // Init Streams manager, and load streams
    START_PERFORMANCE_COUNTER
      CStreamsManager::Create();
    CStreamsManager::Get()->InitManager();
    CStreamsManager::Get()->LoadStreams();
    END_PERFORMANCE_COUNTER("Loading streams informations");

    if (!hasStreamDetails) {
      // We will use our own stream detail
      // We need to make a copy of our streams details because
      // Reset() delete the streams
      if (CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_EXTRACTFLAGS)) // Only warn user if the option is enabled
        CLog::Log(LOGWARNING, __FUNCTION__" VideoPlayer failed to fetch streams details. Using DirectShow ones");

      pFileItem.GetVideoInfoTag()->m_streamDetails.AddStream(
        new CDSStreamDetailVideo((const CDSStreamDetailVideo &)(*CStreamsManager::Get()->GetVideoStreamDetail()))
        );
      std::vector<CDSStreamDetailAudio *>& streams = CStreamsManager::Get()->GetAudios();
      for (std::vector<CDSStreamDetailAudio *>::const_iterator it = streams.begin();
        it != streams.end(); ++it)
        pFileItem.GetVideoInfoTag()->m_streamDetails.AddStream(
        new CDSStreamDetailAudio((const CDSStreamDetailAudio &)(**it))
        );
    }

    std::vector<std::string> extras;
    START_PERFORMANCE_COUNTER
      if (FAILED(CFilterCoreFactory::GetExtraFilters(pFileItem, extras, CGraphFilters::Get()->IsUsingDXVADecoder())))
      {
      //Dont want the loading to fail for an error there
      CLog::Log(LOGERROR, "Failed loading extras filters in filtersconfig.xml");
      }

    // Insert extra first because first added, last connected!
    for (unsigned int i = 0; i < extras.size(); i++)
    {
      SFilterInfos f;
      if (SUCCEEDED(InsertFilter(extras[i], f)))
        CGraphFilters::Get()->Extras.push_back(f);
    }
    if (CSettings::GetInstance().GetInt(CSettings::SETTING_DSPLAYER_FILTERSMANAGEMENT) == INTERNALFILTERS)
    {
      for (unsigned int i = 0; i < 3; i++)
      {
        std::string filter;
        std::string setting;
        setting = StringUtils::Format("dsplayer.extrafilter%i", i);
        filter = CSettings::GetInstance().GetString(setting);
        if (filter != "[null]")
        {
          SFilterInfos f;
          if (SUCCEEDED(InsertFilter(filter, f)))
            CGraphFilters::Get()->Extras.push_back(f);
        }
        else
          break;
      }
    }
    extras.clear();
    END_PERFORMANCE_COUNTER("Loading extra filters");

    START_PERFORMANCE_COUNTER
      if (FAILED(CFilterCoreFactory::GetVideoFilter(pFileItem, filter, CGraphFilters::Get()->IsUsingDXVADecoder())))
        goto clean;

    if (FAILED(InsertFilter(filter, CGraphFilters::Get()->Video)))
      goto clean;

    END_PERFORMANCE_COUNTER("Loading video filter");

    START_PERFORMANCE_COUNTER
      if (FAILED(CFilterCoreFactory::GetAudioFilter(pFileItem, filter, CGraphFilters::Get()->IsUsingDXVADecoder())))
        goto clean;

    if (FAILED(InsertFilter(filter, CGraphFilters::Get()->Audio)))
      goto clean;

    CStreamsManager::Get()->SetAudioInterface();
    END_PERFORMANCE_COUNTER("Loading audio filter");


    // Shaders
    {
      std::vector<uint32_t> shaders;
      std::vector<uint32_t> shadersStages;
      START_PERFORMANCE_COUNTER
        if (SUCCEEDED(CFilterCoreFactory::GetShaders(pFileItem, shaders, shadersStages, CGraphFilters::Get()->IsUsingDXVADecoder())))
        {
        for (int unsigned i = 0; i < shaders.size(); i++)
        {
          g_dsSettings.pixelShaderList->EnableShader(shaders[i], shadersStages[i]);
        }
        }
      END_PERFORMANCE_COUNTER("Loading shaders");
    }

    CLog::Log(LOGDEBUG, "%s All filters added to the graph", __FUNCTION__);

    // If we have add our own informations, clear it to prevent xbmc to save it to the database
    if (!hasStreamDetails) {
      pFileItem.GetVideoInfoTag()->m_streamDetails.Reset();
    }
    return S_OK;

  clean:
    if (!hasStreamDetails) {
      pFileItem.GetVideoInfoTag()->m_streamDetails.Reset();
    }
    return E_FAIL;
}

HRESULT CFGLoader::LoadConfig(FILTERSMAN_TYPE filterManager)
{
  CXBMCTinyXML configXML;
  if (configXML.LoadFile("special://xbmc/system/players/dsplayer/dsplayerconfig.xml"))
  {
    TiXmlElement *pElement = configXML.RootElement();
    if (pElement && strcmpi(pElement->Value(), "settings") == 0)
    {
      XMLUtils::GetBoolean(pElement, "autorender", m_bIsAutoRender);
    }
  }
  // Two steps
  if (filterManager == NOFILTERMAN)
    filterManager = (FILTERSMAN_TYPE)CSettings::GetInstance().GetInt(CSettings::SETTING_DSPLAYER_FILTERSMANAGEMENT);

  if (filterManager == MEDIARULES)
  {
    // First, filters
    LoadFilterCoreFactorySettings(CProfilesManager::GetInstance().GetUserDataItem("dsplayer/filtersconfig.xml"), FILTERS, true);
    LoadFilterCoreFactorySettings("special://xbmc/system/players/dsplayer/filtersconfig.xml", FILTERS, false);

    // Second, medias rules
    LoadFilterCoreFactorySettings(CProfilesManager::GetInstance().GetUserDataItem("dsplayer/mediasconfig.xml"), MEDIAS, false);
    LoadFilterCoreFactorySettings("special://xbmc/system/players/dsplayer/mediasconfig.xml", MEDIAS, false, 100);
  }
  else if (filterManager == INTERNALFILTERS)
  {
    LoadFilterCoreFactorySettings("special://xbmc/system/players/dsplayer/filtersconfig_internal.xml", FILTERS, true);
    LoadFilterCoreFactorySettings(CProfilesManager::GetInstance().GetUserDataItem("dsplayer/filtersconfig.xml"), FILTERS, false);
    LoadFilterCoreFactorySettings("special://xbmc/system/players/dsplayer/filtersconfig.xml", FILTERS, false);
    LoadFilterCoreFactorySettings("special://xbmc/system/players/dsplayer/mediasconfig_internal.xml", MEDIAS, false);
  }
  return S_OK;
}

HRESULT CFGLoader::InsertFilter(const std::string& filterName, SFilterInfos& f)
{
  HRESULT hr = S_OK;
  f.pBF = NULL;

  CFGFilter *filter = NULL;
  if (!(filter = CFilterCoreFactory::GetFilterFromName(filterName)))
    return E_FAIL;

  if (SUCCEEDED(hr = filter->Create(&f.pBF)))
  {
    // Setup InternalFilters settings
    if (filterName == "lavsplitter_internal" || filterName == "lavsource_internal")
    {
      f.internalFilter = true;
      CGraphFilters::Get()->SetupLavSettings(CGraphFilters::INTERNAL_LAVSPLITTER, f.pBF);
    }
    if (filterName == CGraphFilters::INTERNAL_LAVVIDEO)
    {
      f.internalFilter = true;
      CGraphFilters::Get()->SetupLavSettings(CGraphFilters::INTERNAL_LAVVIDEO, f.pBF);
    }
    if (filterName == CGraphFilters::INTERNAL_LAVAUDIO)
    {
      f.internalFilter = true;
      CGraphFilters::Get()->SetupLavSettings(CGraphFilters::INTERNAL_LAVAUDIO, f.pBF);
    }
    if (filterName == CGraphFilters::INTERNAL_XYSUBFILTER || filterName == CGraphFilters::INTERNAL_XYVSFILTER)
      f.internalFilter = true;

    f.osdname = filter->GetName();
    if (filter->GetType() == CFGFilter::INTERNAL)
    {
      CLog::Log(LOGDEBUG, "%s Using an internal filter", __FUNCTION__);
      f.isinternal = true;
      f.pData = filter;
    }
    if (SUCCEEDED(hr = g_dsGraph->pFilterGraph->AddFilter(f.pBF, filter->GetNameW().c_str())))
      CLog::Log(LOGNOTICE, "%s Successfully added \"%s\" to the graph", __FUNCTION__, f.osdname.c_str());
    else
      CLog::Log(LOGERROR, "%s Failed to add \"%s\" to the graph", __FUNCTION__, f.osdname.c_str());

    f.guid = filter->GetCLSID();
  }
  else
  {
    CLog::Log(LOGERROR, "%s Failed to create filter \"%s\"", __FUNCTION__, filterName.c_str());
  }

  return hr;
}

void CFGLoader::SettingOptionsDSVideoRendererFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  list.push_back(std::make_pair("Enhanced Video Renderer (EVR)", "EVR"));
  //todo dx11 
  //list.push_back(std::make_pair("Video Mixing Renderer 9 (VMR9)", "VMR9"));
  
  CDSFilterEnumerator p_dsfilter;
  std::vector<DSFiltersInfo> dsfilterList;
  p_dsfilter.GetDSFilters(dsfilterList);
  std::vector<DSFiltersInfo>::const_iterator iter = dsfilterList.begin();

  for (int i = 1; iter != dsfilterList.end(); i++)
  {
    DSFiltersInfo dev = *iter;
    if (dev.lpstrName == "madVR")
    {
      list.push_back(std::make_pair("madshi Video Renderer (madVR)", "madVR"));
      break;
    }
    ++iter;
  }
}

void CFGLoader::SettingOptionsDSAudioRendererFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  list.push_back(std::make_pair("Internal Audio Renderer (Sanear)", CGraphFilters::INTERNAL_SANEAR));
  list.push_back(std::make_pair("System Default", "System Default"));

  CAudioEnumerator p_dsound;
  std::vector<DSFilterInfo> deviceList;
  p_dsound.GetAudioRenderers(deviceList);
  std::vector<DSFilterInfo>::const_iterator iter = deviceList.begin();

  for (int i = 1; iter != deviceList.end(); i++)
  {
    DSFilterInfo dev = *iter;
    list.push_back(std::make_pair(dev.lpstrName, dev.lpstrName));
    ++iter;
  }
}

void CFGLoader::SettingOptionsSanearDevicesFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  list.push_back(std::make_pair("System Default", "System Default"));
  for (const auto& device : GetDevices())
  {
    list.push_back(std::make_pair(device.first, device.second));
  }
}

bool CFGLoader::LoadFilterCoreFactorySettings(const std::string& fileStr, ESettingsType type, bool clear, int iPriority)
{
  if (clear)
  {
    CFilterCoreFactory::Destroy();
  }

  CLog::Log(LOGNOTICE, "Loading filter core factory settings from %s (%s configuration).", fileStr.c_str(), (type == MEDIAS) ? "medias" : "filters");
  if (!XFILE::CFile::Exists(fileStr))
  { // tell the user it doesn't exist
    CLog::Log(LOGNOTICE, "%s does not exist. Skipping.", fileStr.c_str());
    return false;
  }

  CXBMCTinyXML filterCoreFactoryXML;
  if (!filterCoreFactoryXML.LoadFile(fileStr))
  {
    CLog::Log(LOGERROR, "Error loading %s, Line %d (%s)", fileStr.c_str(), filterCoreFactoryXML.ErrorRow(), filterCoreFactoryXML.ErrorDesc());
    return false;
  }

  return ((type == MEDIAS) ? SUCCEEDED(CFilterCoreFactory::LoadMediasConfiguration(filterCoreFactoryXML.RootElement(), iPriority))
    : SUCCEEDED(CFilterCoreFactory::LoadFiltersConfiguration(filterCoreFactoryXML.RootElement())));
}

#endif
