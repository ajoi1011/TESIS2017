/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  M A N A G E R                      */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _MANAGER_H
#define _MANAGER_H

#include "call.h"
#include "precompile.h"

#if OPAL_SIP
void ExpandWildcards(
  const PStringArray & input, 
  const PString & defaultServer, 
  PStringArray & names, 
  PStringArray & servers
);
#endif

#if OPAL_SIP && OPAL_H323 && OPAL_HAS_MIXER
class MyH323EndPoint;
class MySIPEndPoint;
class MyMixerEndPoint;
#endif

/*************************************************************************/
/*                                                                       */
/* <clase MyManager>                                                     */
/*                                                                       */
/* <Descripcion>                                                         */
/*   Clase derivada de OpalManagerConsole para crear un objeto que       */
/*  representa un gestor de terminales H.323, SIP, MCU, PCSS y establece */
/*  los parámetros para la llamada en un sistema OPAL.                   */
/*                                                                       */
/*************************************************************************/ 
class MyManager : public OpalManagerConsole                              //
{                                                                        //
  PCLASSINFO(MyManager, OpalManagerConsole);                             //
  public:                                                                //
    MyManager();                                                         //
                                                                         //
    ~MyManager();                                                        //
    /** Sobrecarga de OpalManagerConsole */                              //
    //@{                                                                 //
    virtual OpalCall * CreateCall(void *);                               //
    void CmdPresentationToken();                                         //
    virtual void EndRun(bool interrupt);                                 //
    virtual bool Initialise(                                             //
      PArgList & args,                                                   //
      bool verbose = false,                                              //
      const PString & defaultRoute = PString::Empty()                    //
    );                                                                   //
    virtual void OnStartMediaPatch(                                      //
      OpalConnection & connection,                                       //
      OpalMediaPatch & patch                                             //
    );                                                                   //
    virtual void OnStopMediaPatch(                                       //
      OpalConnection & connection,                                       //
      OpalMediaPatch & patch                                             //
    );                                                                   //
    virtual bool OnChangedPresentationRole(                              //
      OpalConnection & connection,                                       //
      const PString & newChairURI,                                       //
      bool request                                                       //
    );                                                                   //
    virtual bool PreInitialise(PArgList & args, bool verbose = false);   //
    virtual void PrintVersion() const ;                                  //
    //@}                                                                 //
                                                                         //
    /** Funciones desarrolladas a partir de OpalServer */                //
    //@{                                                                 //
    bool Configure(PConfig & cfg, PConfigPage * rsrc);                   //
    bool ConfigureCDR(PConfig & cfg, PConfigPage * rsrc);                //
    typedef std::list<MyCallDetailRecord> CDRList;                       //
    CDRList::const_iterator BeginCDR();                                  //
    bool ConfigureCommon(                                                //
      OpalEndPoint * ep,                                                 //
      const PString & cfgPrefix,                                         //
      PConfig & cfg,                                                     //
      PConfigPage * rsrc                                                 //
    );                                                                   //
    void DropCDR(const MyCall & call, bool final);                       //
    bool FindCDR(const PString & guid, MyCallDetailRecord & cdr);        //
    bool NotEndCDR(const CDRList::const_iterator & it);                  //
    void StartRecordingCall(MyCall & call) const;                        //
#if OPAL_SIP && OPAL_H323                                                //
    void OnChangedRegistrarAoR(const PURL & aor, bool registering);      //
#endif                                                                   //
#if OPAL_H323                                                            //
    virtual H323ConsoleEndPoint * CreateH323EndPoint();                  //
    MyH323EndPoint & GetH323EndPoint() const;                            //
#endif                                                                   //
#if OPAL_SIP                                                             //
    virtual SIPConsoleEndPoint * CreateSIPEndPoint();                    //
    MySIPEndPoint & GetSIPEndPoint() const;                              //
#endif                                                                   //
#if OPAL_HAS_MIXER                                                       //
    virtual OpalConsoleMixerEndPoint * CreateMixerEndPoint();            //
    MyMixerEndPoint & GetMixerEndPoint() const ;                         //
#endif                                                                   //
    PStringArray GetAddressBook() { return m_addressBook; }              //
    //@}                                                                 //
                                                                         //
  protected:                                                             //
    OpalConsoleEndPoint * GetConsoleEndPoint(const PString & prefix);    //
    OpalProductInfo       m_savedProductInfo;                            //
    unsigned              m_maxCalls;                                    //
    MediaTransferMode     m_mediaTransferMode;                           //
    PBoolean              m_verbose;                                     //
#if OPAL_HAS_MIXER                                                       //
    bool                       m_recordingEnabled;                       //
    PString                    m_recordingTemplate;                      //
    OpalRecordManager::Options m_recordingOptions;                       //
#endif                                                                   //
    PStringArray   m_addressBook;                                        //
    CDRList        m_cdrList;                                            //
    size_t         m_cdrListMax;                                         //
    PTextFile      m_cdrTextFile;                                        //
    PString        m_cdrFormat;                                          //
    PString        m_cdrTable;                                           //
    PString        m_cdrFieldNames[MyCall::NumFieldCodes];               //
    PDECLARE_MUTEX(m_cdrMutex);                                          //
                                                                         //
};                                                                       //
/*************************************************************************/

#if OPAL_HAS_PCSS
/*************************************************************************/
/*                                                                       */
/* <clase MyPCSSEndPoint>                                                */
/*                                                                       */
/* <Descripcion>                                                         */
/*   Clase derivada de OpalConsolePCSSEndPoint para crear un objeto que  */
/*  representa un terminal PCSS (PC Sound System).                       */
/*                                                                       */
/*************************************************************************/ 
class MyPCSSEndPoint : public OpalConsolePCSSEndPoint                    //
{                                                                        //
  PCLASSINFO(MyPCSSEndPoint, OpalConsolePCSSEndPoint);                   //
  public:                                                                //
    MyPCSSEndPoint(MyManager & mgr)                                      //
      : OpalConsolePCSSEndPoint(mgr)                                     //
    {                                                                    //
    }                                                                    //
                                                                         //
    virtual PBoolean OnShowIncoming(const OpalPCSSConnection &connection)//
    {                                                                    //
      return AcceptIncomingCall(connection.GetToken());                  //
    }                                                                    //
                                                                         //
};                                                                       //
#endif                                                                   //
#endif // _MANAGER_H                                                     //
                                                                         //
/************************Final del Header*********************************/