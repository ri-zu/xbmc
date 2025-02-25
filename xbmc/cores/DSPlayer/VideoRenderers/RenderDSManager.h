#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include <list>

#include "guilib/Resolution.h"
#include "threads/CriticalSection.h"
#include "settings/VideoSettings.h"
#include "Videorenderers/WinDsRenderer.h"
#include "../VideoPlayer/Videorenderers/DebugRenderer.h"
#include "threads/Event.h"

class IPaintCallback;
class CWinDSRenderer;
class CRenderDSManager;

class IRenderDSMsg
{
  friend CRenderDSManager;
protected:
  virtual void SetRenderOnDS(bool bRender) = 0;
  virtual void SetDSWndVisible(bool bVisible) = 0;
  virtual void VideoParamsChange() = 0;
  virtual void GetDebugInfo(std::string &audio, std::string &video, std::string &general) = 0;
};

class CRenderDSManager
{
public:
  CRenderDSManager(IRenderDSMsg *player);
  ~CRenderDSManager();

  // Functions called from render thread
  void GetVideoRect(CRect &source, CRect &dest, CRect &view);
  float GetAspectRatio();
  void Update();
  void FrameMove();
  bool HasFrame();
  void Render(bool clear, DWORD flags = 0, DWORD alpha = 255, bool gui = true);
  bool IsGuiLayer();
  bool IsVideoLayer();
  RESOLUTION GetResolution();
  void UpdateResolution();
  void TriggerUpdateResolution(float fps, int width, int flags);
  void SetViewMode(int iViewMode);
  void PreInit();
  void UnInit();
  bool Flush();
  bool IsConfigured() const;
  void ToggleDebug();
  void Reset();

  // Functions called from GUI
  bool Supports(ERENDERFEATURE feature);
  bool Supports(ESCALINGMETHOD method);

  double GetDisplayLatency() { return m_displayLatency; }

  bool Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags);
  void DisplayChange(bool bExternalChange);
  void EndRender();

protected:

  void PresentSingle(bool clear, DWORD flags, DWORD alpha);

  bool Configure();
  void CreateRenderer();
  void DeleteRenderer();

  CDebugRenderer m_debugRenderer;
  CBaseRenderer *m_pRenderer;
  CCriticalSection m_statelock;
  CCriticalSection m_datalock;
  bool m_bTriggerUpdateResolution;
  bool m_bTriggerDisplayChange;
  
  bool m_renderDebug;
  XbmcThreads::EndTime m_debugTimer;
  enum EPRESENTSTEP
  {
    PRESENT_IDLE     = 0
  , PRESENT_READY
  };

  enum ERENDERSTATE
  {
    STATE_UNCONFIGURED = 0,
    STATE_CONFIGURING,
    STATE_CONFIGURED,
  };
  ERENDERSTATE m_renderState;
  CEvent m_stateEvent;
  bool m_bWaitingForRenderOnDS;
  RESOLUTION m_Resolution;

  double m_displayLatency;
  void UpdateDisplayLatency();
  unsigned int m_width, m_height, m_dwidth, m_dheight;
  unsigned int m_flags;
  float m_fps;
  bool m_bPreInit;

  CEvent m_flushEvent;
  IRenderDSMsg *m_playerPort;
};
