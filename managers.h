/*
 * managers.h
 * 
 * Copyright 2020 ajoi1011 <ajoi1011@debian>
 * 
*/
 
/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  M A N A G E R                      */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#include "call.h"
#include "precompile.h"

#if OPAL_PTLIB_NAT
struct NATInfo {
  PString m_method;
  PString m_friendly;
  bool    m_active;
  PString m_server;
  PString m_interface;
  NATInfo(const PNatMethod & method)
    : m_method(method.GetMethodName())
    , m_friendly(method.GetFriendlyName())
    , m_active(method.IsActive())
    , m_server(method.GetServer())
    , m_interface(method.GetInterface())
  { 
  }

  __inline bool operator<(const NATInfo & other) const { 
	  return m_method < other.m_method; 
  }
};
#endif // OPAL_PTLIB_NAT

#if OPAL_SIP && OPAL_H323 && OPAL_HAS_MIXER
class ConferenceManager;
class MCUH323EndPoint;
//class MySIPEndPoint;
class PVideoOutputDevice_MCU;
class MCUVideoMixer; typedef void * Id;
#endif // OPAL_SIP && OPAL_H323 && OPAL_HAS_MIXER

/****************************************************************************************/
/*                                                                                      */
/* <clase Manager>                                                                      */
/*                                                                                      */
/* <Descripción>                                                                        */
/*   Clase derivada de OpalManagerConsole que describe un gestor de terminales H.323,   */
/*  SIP, MCU, PCSS y establece los parámetros para una llamada en el sistema OPAL.      */
/*                                                                                      */
/****************************************************************************************/
class Manager : public OpalManagerConsole
{
  PCLASSINFO(Manager, OpalManagerConsole)
  public:
    Manager();
    
    virtual void EndRun(bool interrupt);
    virtual bool PreInitialise(PArgList & args, bool verbose = false); 
    virtual bool Initialise(
      PArgList & args, 
      bool verbose = false,
      const PString & defaultRoute = PString::Empty()
    );
    
    virtual void OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch); 
    virtual void OnStopMediaPatch (OpalConnection & connection, OpalMediaPatch & patch);   
    
    virtual OpalCall * CreateCall(void *);  
    
    virtual void PrintVersion() const ;
    virtual bool Configure(PConfig & cfg);
#if OPAL_H323 || OPAL_HAS_MIXER
    virtual H323ConsoleEndPoint * CreateMCUH323EndPoint();
    MCUH323EndPoint & GetMCUH323EndPoint() const; 
    virtual OpalConsoleMixerEndPoint * CreateConferenceManager();
    ConferenceManager & GetConferenceManager() const;
#endif 
    
    unsigned GetMaxCalls() { return m_maxCalls;}
    MediaTransferMode GetMediaTransferMode() { return m_mediaTransferMode; }
    bool ConfigureCDR(PConfig & cfg);
    typedef std::list<MCUCallDetailRecord> CDRList; 
    CDRList::const_iterator BeginCDR();
    void DropCDR(const MCUCall & call, bool final);
    bool FindCDR(const PString & guid, MCUCallDetailRecord & cdr);
    bool NotEndCDR(const CDRList::const_iterator & it);
    PString GetMonitorText(); 
  
    OpalProductInfo       m_savedProductInfo;
    PTextFile             m_cdrTextFile;
    protected:
    OpalConsoleEndPoint * GetConsoleEndPoint(const PString & prefix);
    bool                  m_verbose;
    unsigned              m_maxCalls;
    MediaTransferMode     m_mediaTransferMode;
    PString               m_prefVideo; 
    PString               m_maxVideo; 
    double                m_rate;  
    OpalBandwidth         m_bitrate;
    CDRList               m_cdrList; 
    size_t                m_cdrListMax;
    
    PString               m_cdrFormat;
    PString               m_cdrTable;
    PString               m_cdrFieldNames[MCUCall::NumFieldCodes];
    PDECLARE_MUTEX(m_cdrMutex);  
};

/****************************************************************************************/
/*                                                                                      */
/* <clase ConferenceManager>                                                            */
/*                                                                                      */
/* <Descripción>                                                                        */
/*   Clase derivada de OpalConsoleMixerEndPoint que describe una Unidad de Control      */
/* Multipunto (MCU).                                                                    */
/*                                                                                      */
/****************************************************************************************/
class ConferenceManager : public OpalConsoleMixerEndPoint
{
  PCLASSINFO(ConferenceManager, OpalConsoleMixerEndPoint);
  public:
    ConferenceManager(Manager & manager);
    ~ConferenceManager();
    
    PStringArray GetAddressBook() { return m_addressBook; } 
    typedef PSafeDictionary<PString, OpalMixerNode> NodesByName;
    bool Configure(PConfig & cfg);
    virtual bool Initialise(PArgList & args, bool verbose,const PString &/**/); 
    virtual bool OnIncomingVideo(PString & room, Id id, const void * buffer, int width, int height, PINDEX amount);
    virtual bool OnOutgoingVideo(PString & room, void * buffer, int width, int height, PINDEX &amount);
    
    virtual OpalMixerConnection * CreateConnection(
      PSafePtr<OpalMixerNode> node,
      OpalCall & call,
      void * userData,
      unsigned options,
      OpalConnection::StringOptions * stringOptions
    );
        
    
    NodesByName & GetNodesByName() { return m_nodesByName;}
    PString SetRoomParams(const PStringToString & data);
    PString OTFControl(const PString room, const PStringToString & data);
    PString RoomCtrlPage(const PString room, bool ctrl, int n, OpalMixerNode & node, Id *idp);
    PString GetConferenceOptsJavascript(OpalMixerNode * node);
    
    virtual void Unmoderate(OpalMixerNode * node);
    virtual void CreateConferenceRoom(const PString & room);
    virtual void RemoveConferenceRoom(const PString & room);
    PString GetMemberListOptsJavascript(OpalMixerNode * node);
    
    virtual OpalVideoStreamMixer * CreateVideoMixer(const OpalMixerNodeInfo & info);
    virtual OpalMixerNode * CreateNode(OpalMixerNodeInfo * info);
        
    OpalMixerNodeInfo                      m_adHoc;
    PStringArray                           m_addressBook;
    /*PDECLARE_MUTEX(m_mutex);*/
    
    bool                                   m_verbose;
    
    
};

#if OPAL_HAS_PCSS

/****************************************************************************************/
/*                                                                                      */
/* <clase PCSSEndPoint>                                                                 */
/*                                                                                      */
/* <Descripcion>                                                                        */
/*   Clase derivada de OpalConsolePCSSEndPoint que describe un terminal PCSS (PC Sound  */
/* System).                                                                             */
/*                                                                                      */
/****************************************************************************************/
class PCSSEndPoint : public OpalConsolePCSSEndPoint 
{  
  PCLASSINFO(PCSSEndPoint, OpalConsolePCSSEndPoint); 
  public: 
    PCSSEndPoint(Manager & manager) 
      : OpalConsolePCSSEndPoint(manager) 
    { 
    } 

    virtual PBoolean OnShowIncoming(const OpalPCSSConnection &connection)
    { 
		return AcceptIncomingCall(connection.GetToken()); 
    }
 }; 

#endif // OPAL_HAS_PCSS
