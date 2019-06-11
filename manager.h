/*
 * manager.h
 *
 * Project manager header
 *
 */

#ifndef _MANAGER_H
#define _MANAGER_H

#include "precompile.h"
#include "call.h"

void ExpandWildcards(const PStringArray & input, const PString & defaultServer, PStringArray & names, PStringArray & servers);

///////////////////////////////////////////////////////////////////////////////////////////////////
class MyH323EndPoint;
class MySIPEndPoint;
class MyMixerEndPoint; 

class MyManager : public MyManagerParent
{
  PCLASSINFO(MyManager, OpalManager);
  public:
    MyManager();

    ~MyManager();

    virtual void PrintVersion() const ;

    virtual bool PreInitialise(PArgList & args, bool verbose = false);

    virtual bool Initialise(
     PArgList & args, 
     bool verbose = false, 
     const PString & defaultRoute = PString::Empty()
    );

    bool Configure(PConfig & cfg, PConfigPage * rsrc);

    bool ConfigureCommon(OpalEndPoint * ep, const PString & cfgPrefix, PConfig & cfg, PConfigPage * rsrc);

    virtual void OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch);

    virtual void OnStopMediaPatch(OpalConnection & connection, OpalMediaPatch & patch);

    virtual OpalCall * CreateCall(void *);

    bool ConfigureCDR(PConfig & cfg, PConfigPage * rsrc);

    void DropCDR(const MyCall & call, bool final);

    typedef std::list<MyCallDetailRecord> CDRList;
    CDRList::const_iterator BeginCDR();
    
    bool NotEndCDR(const CDRList::const_iterator & it);
    
    bool FindCDR(const PString & guid, MyCallDetailRecord & cdr);

#if OPAL_SIP && OPAL_H323
    void OnChangedRegistrarAoR(const PURL & aor, bool registering);
#endif

#if OPAL_H323
    virtual H323ConsoleEndPoint * CreateH323EndPoint();

    MyH323EndPoint & GetH323EndPoint() const;
#endif

#if OPAL_SIP
    virtual SIPConsoleEndPoint * CreateSIPEndPoint();

    MySIPEndPoint & GetSIPEndPoint() const;
#endif

#if OPAL_HAS_MIXER
    virtual OpalConsoleMixerEndPoint * CreateMixerEndPoint();
    
    MyMixerEndPoint & GetMixerEndPoint() const ;
#endif

 
  protected:
    OpalConsoleEndPoint * GetConsoleEndPoint(const PString & prefix);
    OpalProductInfo   m_savedProductInfo;
    unsigned          m_maxCalls;
    MediaTransferMode m_mediaTransferMode;
    PBoolean          m_verbose;

    CDRList   m_cdrList;
    size_t    m_cdrListMax;
    PTextFile m_cdrTextFile;
    PString   m_cdrFormat;
    PString   m_cdrTable;
    PString   m_cdrFieldNames[MyCall::NumFieldCodes];
    PDECLARE_MUTEX(m_cdrMutex);

};
///////////////////////////////////////////////////////////////////////////////////////////////////

#if OPAL_HAS_PCSS
class MyPCSSEndPoint : public OpalConsolePCSSEndPoint
{
  PCLASSINFO(MyPCSSEndPoint, OpalConsolePCSSEndPoint);
  public:
    MyPCSSEndPoint(MyManager & mgr)
      : OpalConsolePCSSEndPoint(mgr)
    {
    }

    virtual PBoolean OnShowIncoming(const OpalPCSSConnection & connection)
    {
      return AcceptIncomingCall(connection.GetToken());
    }

};
#endif
#endif // _MANAGER_H

// End of File ////////////////////////////////////////////////////////////////////////////////////
