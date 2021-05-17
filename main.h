/*
 * main.h
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
*/

/*****************************************************************************************/
/*****************************************************************************************/
/*                                                                                       */
/*                               C L A S E S  M A I N                                    */
/*                                                                                       */
/*****************************************************************************************/
/*****************************************************************************************/

#ifndef _OPAL_SERVER_MAIN_H
#define _OPAL_SERVER_MAIN_H

#include "precompile.h" 
#include "video.h"


class OPALMCUURL : public PURL
{
  public:
    OPALMCUURL(PString str);

    void SetDisplayName(PString name) { m_displayName = name; }

    const PString & GetDisplayName() const { return m_displayName; }
    const PString & GetUrl() const { return m_partyUrl; }
    const PString & GetUrlId() const { return m_urlId; }
    const PString & GetMemberFormatName() const { return m_memberName; }
    const PString GetPort() const { return PString(m_port); }
    const PString & GetSipProto() const { return m_sipProto; }

  protected:
    PString m_partyUrl;
    PString m_displayName;
    PString m_URLScheme;
    PString m_urlId;
    PString m_memberName;
    PString m_sipProto;
};

/*****************************************************************************************/
/*                                                                                       */
/* <clase OpalMCUEIE>                                                                    */
/*                                                                                       */
/* <Descripción>                                                                         */
/*   Clase derivada de PHTTPServiceProcess que describe un servidor HTTP.                */
/*                                                                                       */
/*****************************************************************************************/
class OpalMCUEIE : public PHTTPServiceProcess
{
  PCLASSINFO(OpalMCUEIE, PHTTPServiceProcess)
  public:
    OpalMCUEIE();
    ~OpalMCUEIE();
                     
    virtual void Main(); 
    virtual void CreateHTTPResource(const PString & name);                                                                   
    virtual void OnConfigChanged();                                      
    virtual void OnControl();                                                                               
    virtual void OnStop();
    virtual PBoolean OnStart();
    virtual PBoolean Initialise(const char * initMsg);
    
    virtual void LogMessage(const PString & str);
    virtual void LogMessageHTML(PString str);
    virtual void HttpWrite_(PString evt);
    virtual void HttpWriteEvent(PString evt);
    virtual void HttpWriteEventRoom(PString evt, PString room);
    virtual void HttpWriteCmdRoom(PString evt, PString room);
    virtual void HttpWriteCmd(PString evt);
    virtual PString HttpGetEvents(int & idx, PString room);
    virtual PString HttpStartEventReading(int & idx, PString room);
    
    static OpalMCUEIE & Current() { return (OpalMCUEIE&)PProcess::Current(); }
    Manager & GetManager()        { return * m_manager; }
    int GetRoomTimeLimit() const  { return m_roomTimeLimit; }
    int GetHttpBuffer()    const  { return m_httpBuffer; }
    virtual bool GetPreMediaFrame(void * buffer, int width, int height, PINDEX & amount)
    { return true; }
    PString GetHtmlCopyright();
   
    static VideoMixConfigurator vmcfg;
    //PStringArray      addressBook;
    
    PString           m_defaultRoomName;
    PFilePath         m_logFilename;
    int               m_currentLogLevel, 
                      m_currentTraceLevel;
    bool              m_autoDeleteRoom,
                      m_allowLoopbackCalls,
                      m_traceFileRotated,
                      m_copyWebLogToLog;
  
  protected:
    Manager         * m_manager;
    PStringArray      m_httpBufferedEvents;
    bool              m_httpBufferComplete;
    int               m_roomTimeLimit,
                      m_httpBuffer, 
                      m_httpBufferIndex;
    
    
    PDECLARE_MUTEX(m_httpBufferMutex);
 
};
#endif
