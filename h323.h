/*
 * h323.h
 *
 * Project h323 header
 *
 */

#ifndef _H323_H
#define _H323_H

#include "precompile.h"
#include "main.h"
#include "manager.h"
///////////////////////////////////////////////////////////////////////////////
#if OPAL_H323

class MyGatekeeperServer;

class MyGatekeeperCall : public H323GatekeeperCall
{
    PCLASSINFO(MyGatekeeperCall, H323GatekeeperCall);
  public:
    MyGatekeeperCall(
      MyGatekeeperServer & server,
      const OpalGloballyUniqueID & callIdentifier, /// Unique call identifier
      Direction direction
    );
    ~MyGatekeeperCall();

    virtual H323GatekeeperRequest::Response OnAdmission(
      H323GatekeeperARQ & request
    );

};

///////////////////////////////////////

class MyGatekeeperServer : public H323GatekeeperServer
{
    PCLASSINFO(MyGatekeeperServer, H323GatekeeperServer);
  public:
    MyGatekeeperServer(H323EndPoint & ep);

    // Overrides
    virtual H323GatekeeperCall * CreateCall(
      const OpalGloballyUniqueID & callIdentifier,
      H323GatekeeperCall::Direction direction
    );
    virtual PBoolean TranslateAliasAddress(
      const H225_AliasAddress & alias,
      H225_ArrayOf_AliasAddress & aliases,
      H323TransportAddress & address,
      PBoolean & isGkRouted,
      H323GatekeeperCall * call
    );

    // new functions
    bool Configure(PConfig & cfg, PConfigPage * rsrc);
    PString OnLoadEndPointStatus(const PString & htmlBlock);
    bool ForceUnregister(const PString id);


  private:
    H323EndPoint & endpoint;

    class RouteMap : public PObject {
        PCLASSINFO(RouteMap, PObject);
      public:
        RouteMap(
          const PString & alias,
          const PString & host
        );

        void PrintOn(
          ostream & strm
        ) const;

        bool IsValid() const;

        bool IsMatch(
          const PString & alias
        ) const;

        const H323TransportAddress & GetHost() const { return m_host; }

      private:
        PString              m_alias;
        PRegularExpression   m_regex;
        H323TransportAddress m_host;
    };
    PList<RouteMap> routes;

    PDECLARE_MUTEX(reconfigurationMutex);

};

class MyH323EndPoint : public H323ConsoleEndPoint
{
  PCLASSINFO(MyH323EndPoint, H323ConsoleEndPoint);
  public:
    MyH323EndPoint(MyManager & manager);
    virtual bool Initialise(PArgList & args, bool verbose, const PString & defaultRoute);
    void AutoRegister(const PString & alias, const PString & gk, bool registering);
    bool Configure(PConfig & cfg, PConfigPage * rsrc);
    const MyGatekeeperServer & GetGatekeeperServer() const { return m_gkServer; }
          MyGatekeeperServer & GetGatekeeperServer()       { return m_gkServer; }

  protected:
    MyManager        & m_manager;
    MyGatekeeperServer m_gkServer;
    bool               m_firstConfig;
    PStringArray       m_configuredAliases;
    PStringArray       m_configuredAliasPatterns;

};
#endif 
#endif // _H323_H
// End of File ////////////////////////////////////////////////////////////////
