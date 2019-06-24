/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  H T M L                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _HTML_H
#define _HTML_H

#include "call.h"
#include "h323.h" 
#include "main.h"
#include <stdio.h>
#include <string.h>

void BeginPage(
  PStringStream & html, 
  PStringStream & pmeta, 
  const char        * ptitle, 
  const char        * title, 
  const char  * quotekey
);
  
void EndPage (PStringStream & html, PString copyr);

/*************************************************************************/
/*                                                                       */
/* <clase BaseStatusPage>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PServiceHTTPString que describe un post HTTP      */
/*  con información detallada de estados, bases de datos, registros en   */
/*  el servidor HTTP.                                                    */
/*                                                                       */
/*************************************************************************/
class BaseStatusPage : public PServiceHTTPString                         //
{                                                                        //
  PCLASSINFO(BaseStatusPage, PServiceHTTPString);                        //
  public:                                                                //
    BaseStatusPage(                                                      //
      MyManager & mgr,                                                   //
      const PHTTPAuthority & auth,                                       //
      const char * name                                                  //
    );                                                                   //
                                                                         //
    virtual PString LoadText(                                            //
      PHTTPRequest & request                                             //
      );                                                                 //
    virtual PBoolean Post(                                               //
      PHTTPRequest & request,                                            //
      const PStringToString &,                                           //
      PHTML & msg                                                        //
    );                                                                   //
    MyManager & GetManager() { return m_manager; }                       //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const = 0;                                                         //
    virtual void CreateHTML(                                             //
      PHTML & html_page,                                                 //
      const PStringToString & query                                      //
    );                                                                   //
    virtual const char * GetTitle() const = 0;                           //
    virtual bool OnPostControl(const PStringToString & /*data*/,         // 
                               PHTML & /*msg*/)                          //
    {                                                                    //
      return false;                                                      //
    }                                                                    //
                                                                         //
    MyManager            & m_manager;                                    //
    unsigned               m_refreshRate;                                //
    MyCallDetailRecord   * m_cdr;                                        //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase RegistrationStatusPage>                                        */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  de registros en el servidor HTTP.                                    */
/*                                                                       */
/*************************************************************************/
class RegistrationStatusPage : public BaseStatusPage                     //
{                                                                        //
  PCLASSINFO(RegistrationStatusPage, BaseStatusPage);                    //
  public:                                                                //
    RegistrationStatusPage(                                              //
      MyManager & mgr,                                                   //
      const PHTTPAuthority & auth                                        //
    );                                                                   //
                                                                         //
    typedef map<PString, PString> StatusMap;                             //
#if OPAL_H323                                                            //
    void GetH323(StatusMap & copy) const;                                //
    size_t GetH323Count() const { return m_h323.size(); }                //
#endif                                                                   //
#if OPAL_SIP                                                             //
    void GetSIP(StatusMap & copy) const;                                 //
    size_t GetSIPCount() const { return m_sip.size(); }                  //
#endif                                                                   //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual const char * GetTitle() const;                               //
    virtual PString LoadText(PHTTPRequest & request);                    //
#if OPAL_H323                                                            //
    StatusMap m_h323;                                                    //
#endif                                                                   //
#if OPAL_SIP                                                             //
    StatusMap m_sip;                                                     //
#endif                                                                   //
    PDECLARE_MUTEX(m_mutex);                                             //
};                                                                       //
/*************************************************************************/

#if OPAL_H323
/*************************************************************************/
/*                                                                       */
/* <clase GkStatusPage>                                                  */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  del servidor gatekeeper H.323.                                       */
/*                                                                       */
/*************************************************************************/
class GkStatusPage : public BaseStatusPage                               //
{                                                                        //
  PCLASSINFO(GkStatusPage, BaseStatusPage);                              //
  public:                                                                //
    GkStatusPage(MyManager & mgr, const PHTTPAuthority & auth);          //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual const char * GetTitle() const;                               //
    virtual bool OnPostControl(                                          //
      const PStringToString & data,                                      //
      PHTML & msg                                                        //
    );                                                                   //
    MyGatekeeperServer & m_gkServer;                                     //
    friend class PServiceMacro_H323EndPointStatus;                       //
};                                                                       //
/*************************************************************************/
#endif // OPAL_H323

#if OPAL_SIP
/*************************************************************************/
/*                                                                       */
/* <clase RegistrarStatusPage>                                           */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  del servidor SIP Registrar.                                          */
/*                                                                       */
/*************************************************************************/
class RegistrarStatusPage : public BaseStatusPage                        //
{                                                                        //
    PCLASSINFO(RegistrarStatusPage, BaseStatusPage);                     //
  public:                                                                //
    RegistrarStatusPage(MyManager & mgr, const PHTTPAuthority & auth);   //
                                                                         //
  protected:                                                             //
    virtual const char * GetTitle() const;                               //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual bool OnPostControl(                                          //
      const PStringToString & data,                                      //
      PHTML & msg                                                        //
    );                                                                   //
                                                                         //
    MySIPEndPoint & m_registrar;                                         //
    friend class PServiceMacro_SIPEndPointStatus;                        //
};                                                                       //
/*************************************************************************/
#endif // OPAL_SIP

/*************************************************************************/
/*                                                                       */
/* <clase CallStatusPage>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  de las llamadas OPAL entrantes/salientes.                            */
/*                                                                       */
/*************************************************************************/
class CallStatusPage : public BaseStatusPage                             //
{                                                                        //
    PCLASSINFO(CallStatusPage, BaseStatusPage);                          //
  public:                                                                //
    CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth);        //
                                                                         //
    void GetCalls(PArray<PString> & copy) const;                         //
    PINDEX GetCallCount() const { return m_calls.GetSize(); }            //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual const char * GetTitle() const;                               //
    virtual PString LoadText(PHTTPRequest & request);                    //
    virtual bool OnPostControl(                                          //
      const PStringToString & data,                                      //
      PHTML & msg                                                        //
    );                                                                   //
    PArray<PString> m_calls;                                             //
    PDECLARE_MUTEX(m_mutex);                                             //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase CDRListPage>                                                   */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  resumida del CDR (Call Detail Record).                               */
/*                                                                       */
/*************************************************************************/
class CDRListPage : public BaseStatusPage                                //
{                                                                        //
  PCLASSINFO(CDRListPage, BaseStatusPage);                               //
  public:                                                                //
    CDRListPage(MyManager & mgr, const PHTTPAuthority & auth);           //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual const char * GetTitle() const;                               //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase CDRPage>                                                       */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  detallada del CDR (Call Detail Record).                              */
/*                                                                       */
/*************************************************************************/
class CDRPage : public BaseStatusPage                                    //
{                                                                        //
  PCLASSINFO(CDRPage, BaseStatusPage);                                   //
  public:                                                                //
    CDRPage(MyManager & mgr, const PHTTPAuthority & auth);               //
                                                                         //
  protected:                                                             //
    virtual void CreateContent(                                          //
      PHTML & html,                                                      //
      const PStringToString & query                                      //
    ) const;                                                             //
    virtual const char * GetTitle() const;                               //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase InvitePage>                                                    */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PServiceHTTPString que describe un post HTTP para */
/*  invitar a miembros a la sala de conferencia.                         */
/*                                                                       */
/*************************************************************************/
class InvitePage : public PServiceHTTPString                             //
{                                                                        //
  public:                                                                //
    InvitePage(MyProcess & app, PHTTPAuthority & auth);                  //
                                                                         //
    PBoolean OnGET (                                                     //
      PHTTPServer & server,                                              //
      const PHTTPConnectionInfo & connectInfo                            //
    );                                                                   //
    virtual PBoolean Post(                                               //
      PHTTPRequest & request,                                            //
      const PStringToString & data,                                      //
      PHTML & replyMessage                                               //
    );                                                                   //
                                                                         //
  protected:                                                             //
    void CreateHTML(PHTML & html);                                       //
    MyProcess & app;                                                     //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase SelectRoomPage>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PServiceHTTPString que describe un post HTTP para */
/*  crear salas de conferencias en la Multipoint Control Unit (MCU).     */
/*                                                                       */
/*************************************************************************/
class SelectRoomPage : public PServiceHTTPString                         //
{                                                                        //
  public:                                                                //
    PBoolean OnGET (                                                     //
      PHTTPServer & server,                                              //
      const PHTTPConnectionInfo & connectInfo                            //
    );                                                                   //
    SelectRoomPage(MyProcess & app, PHTTPAuthority & auth);              //
                                                                         //
  protected:                                                             //
    MyProcess & app;                                                     //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase HomePage>                                                      */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PServiceHTTPString que describe un post HTTP para */
/*  mostrar la página principal del servidor web HTTP.                   */
/*                                                                       */
/*************************************************************************/
class HomePage : public PServiceHTTPString                               //
{                                                                        //
  public:                                                                //
    HomePage(MyProcess & app, PHTTPAuthority & auth);                    //
                                                                         //
  protected:                                                             //
    MyProcess & app;                                                     //
};                                                                       //
/*************************************************************************/

#endif // _HTML_H
/************************Final del Header*********************************/