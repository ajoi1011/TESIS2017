/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  H 3 2 3                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _H323_H
#define _H323_H

#include "manager.h"
#include "precompile.h"

#if OPAL_H323
class MyGatekeeperServer;
/*************************************************************************/
/*                                                                       */
/* <clase MyGatekeeperCall>                                              */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de H323GatekeeperCall que describe una llamada       */
/*  activa en un gatekeeper.                                             */
/*                                                                       */
/*************************************************************************/
class MyGatekeeperCall : public H323GatekeeperCall                       //
{									                                                       //
  PCLASSINFO(MyGatekeeperCall, H323GatekeeperCall);                      //
  public:								                                                 //
    MyGatekeeperCall(                                                    //
      MyGatekeeperServer & server,                                       //
      const OpalGloballyUniqueID & callIdentifier,                       //
      Direction direction                                                //
    );                                                                   //
    ~MyGatekeeperCall();                                                 //
                                                                         //
    virtual H323GatekeeperRequest::Response OnAdmission(                 //
      H323GatekeeperARQ & request                                        //
    );                                                                   //
                                                                         //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyGatekeeperServer>                                            */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de H323GatekeeperServer que describe un servidor     */
/*  gatekeeper básico.                                                   */
/*                                                                       */
/*************************************************************************/
class MyGatekeeperServer : public H323GatekeeperServer                   //
{                                                                        //
  PCLASSINFO(MyGatekeeperServer, H323GatekeeperServer);                  //
  public:                                                                //
    MyGatekeeperServer(H323EndPoint & ep);                               //
    //** Funciones implementadas en OpalServer */                        //
    virtual H323GatekeeperCall * CreateCall(                             //
      const OpalGloballyUniqueID & callIdentifier,                       //
      H323GatekeeperCall::Direction direction                            //
    );                                                                   //
    virtual PBoolean TranslateAliasAddress(                              //
      const H225_AliasAddress & alias,                                   //
      H225_ArrayOf_AliasAddress & aliases,                               //
      H323TransportAddress & address,                                    //
      PBoolean & isGkRouted,                                             //
      H323GatekeeperCall * call                                          //
    );                                                                   //
                                                                         //
    bool Configure(PConfig & cfg, PConfigPage * rsrc);                   //
    bool ForceUnregister(const PString id);                              //
    PString OnLoadEndPointStatus(const PString & htmlBlock);             //
                                                                         //
  private:                                                               //
    H323EndPoint & endpoint;                                             //
                                                                         //
    class RouteMap : public PObject                                      //
    {                                                                    //
      PCLASSINFO(RouteMap, PObject);                                     //
      public:                                                            //
        RouteMap(const PString & alias, const PString & host);           //
        const H323TransportAddress & GetHost() const { return m_host; }  //
        bool IsMatch(const PString & alias) const;                       //
        bool IsValid() const;                                            //
        void PrintOn(ostream & strm) const;                              //
                                                                         //
      private:                                                           //
        PString              m_alias;                                    //
        PRegularExpression   m_regex;                                    //
        H323TransportAddress m_host;                                     //
    };                                                                   //
    PList<RouteMap> routes;                                              //
                                                                         //
    PDECLARE_MUTEX(reconfigurationMutex);                                //
    //@}                                                                 //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyH323EndPoint>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de H323ConsoleEndPoint que describe un terminal      */
/*  H.323.                                                               */
/*                                                                       */
/*************************************************************************/
class MyH323EndPoint : public H323ConsoleEndPoint                        //
{                                                                        //
  PCLASSINFO(MyH323EndPoint, H323ConsoleEndPoint);                       //
  public:                                                                //
    MyH323EndPoint(MyManager & manager);                                 //
                                                                         //
    virtual bool Initialise(                                             //
      PArgList & args,                                                   //
      bool verbose,                                                      //
      const PString & defaultRoute                                       //
    );                                                                   //
                                                                         //
    void AutoRegister(                                                   //
      const PString & alias,                                             //
      const PString & gk,                                                //
      bool registering                                                   //
    );                                                                   //
    bool Configure(PConfig & cfg, PConfigPage * rsrc);                   //
                                                                         //
    const MyGatekeeperServer & GetGatekeeperServer() const               // 
    {                                                                    //
      return m_gkServer;                                                 //
    }                                                                    //
                                                                         //
    MyGatekeeperServer & GetGatekeeperServer() { return m_gkServer; }    //
                                                                         //
  protected:                                                             //
    MyManager        & m_manager;                                        //
    MyGatekeeperServer m_gkServer;                                       //
    bool               m_firstConfig;                                    //
    PStringArray       m_configuredAliases;                              //
    PStringArray       m_configuredAliasPatterns;                        //
                                                                         //
};                                                                       //
/*************************************************************************/
#endif // OPAL_H323
#endif // _H323_H
/************************Final del Header*********************************/