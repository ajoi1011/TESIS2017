/*
 * html.h
 *
 * Project html header
 *
 */

#ifndef _HTML_H
#define _HTML_H

#include "call.h"
#include "main.h"
#include "h323.h" 
#include <stdio.h>
#include <string.h>
///////////////////////////////////////////////////////////////////////////////////////////////////

void BeginPage(
          PStringStream & html, 
          PStringStream & pmeta, 
          const char        * ptitle, 
          const char        * title, 
          const char  * quotekey
        );
  
void EndPage (PStringStream & html, PString copyr);

///////////////////////////////////////////////////////////////////////////////////////////////////

class BaseStatusPage : public PServiceHTTPString
{
    PCLASSINFO(BaseStatusPage, PServiceHTTPString);
  public:
    BaseStatusPage(MyManager & mgr, const PHTTPAuthority & auth, const char * name);

    virtual PString LoadText(
      PHTTPRequest & request    // Information on this request.
      );

    virtual PBoolean Post(
      PHTTPRequest & request,
      const PStringToString &,
      PHTML & msg
    );

    MyManager & m_manager;
    

  protected:
    virtual const char * GetTitle() const = 0;
    virtual void CreateHTML(PHTML & html_page, const PStringToString & query);
    virtual void CreateContent(PHTML & html, const PStringToString & query) const = 0;
    virtual bool OnPostControl(const PStringToString & /*data*/, PHTML & /*msg*/) { return false; }

    unsigned m_refreshRate;
    MyCallDetailRecord * m_cdr;
};

class RegistrationStatusPage : public BaseStatusPage
{
    PCLASSINFO(RegistrationStatusPage, BaseStatusPage);
  public:
    RegistrationStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

    typedef map<PString, PString> StatusMap;
#if OPAL_H323
    void GetH323(StatusMap & copy) const;
    size_t GetH323Count() const { return m_h323.size(); }
#endif
#if OPAL_SIP
    void GetSIP(StatusMap & copy) const;
    size_t GetSIPCount() const { return m_sip.size(); }
#endif

  protected:
    virtual PString LoadText(PHTTPRequest & request);
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;

#if OPAL_H323
    StatusMap m_h323;
#endif
#if OPAL_SIP
    StatusMap m_sip;
#endif
    PDECLARE_MUTEX(m_mutex);
};

#if OPAL_H323

class GkStatusPage : public BaseStatusPage
{
    PCLASSINFO(GkStatusPage, BaseStatusPage);
  public:
    GkStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

  protected:
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
    virtual bool OnPostControl(const PStringToString & data, PHTML & msg);

    MyGatekeeperServer & m_gkServer;

    friend class PServiceMacro_H323EndPointStatus;
};

#endif // OPAL_H323

#if OPAL_SIP

class RegistrarStatusPage : public BaseStatusPage
{
    PCLASSINFO(RegistrarStatusPage, BaseStatusPage);
  public:
    RegistrarStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

  protected:
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
    virtual bool OnPostControl(const PStringToString & data, PHTML & msg);

    MySIPEndPoint & m_registrar;

    friend class PServiceMacro_SIPEndPointStatus;
};

#endif // OPAL_SIP


class CallStatusPage : public BaseStatusPage
{
    PCLASSINFO(CallStatusPage, BaseStatusPage);
  public:
    CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

    void GetCalls(PArray<PString> & copy) const;
    PINDEX GetCallCount() const { return m_calls.GetSize(); }

  protected:
    virtual PString LoadText(PHTTPRequest & request);
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
    virtual bool OnPostControl(const PStringToString & data, PHTML & msg);

    PArray<PString> m_calls;
    PDECLARE_MUTEX(m_mutex);
};


class CDRListPage : public BaseStatusPage
{
  PCLASSINFO(CDRListPage, BaseStatusPage);
public:
  CDRListPage(MyManager & mgr, const PHTTPAuthority & auth);
protected:
  virtual const char * GetTitle() const;
  virtual void CreateContent(PHTML & html, const PStringToString & query) const;
};

class CDRPage : public BaseStatusPage
{
    PCLASSINFO(CDRPage, BaseStatusPage);
  public:
    CDRPage(MyManager & mgr, const PHTTPAuthority & auth);
  protected:
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
};

class InvitePage : public PServiceHTTPString
{
  public:
    InvitePage(MyProcess & app, PHTTPAuthority & auth);

    virtual PBoolean Post(
      PHTTPRequest & request,       // Information on this request.
      const PStringToString & data, // Variables in the POST data.
      PHTML & replyMessage          // Reply message for post.
    );
    void CreateHTML(PHTML & html);
  private:
    MyProcess & app;
    PHTTPAuthority & m_authorityInvite;
    
    
};

class SelectRoomPage : public PServiceHTTPString
{
  public:
    SelectRoomPage(MyProcess & app, PHTTPAuthority & auth);
    PBoolean OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo);
    
  private:
    MyProcess & app;
};

class WelcomePage : public PServiceHTTPString
{
  public:
    WelcomePage(MyProcess & app, PHTTPAuthority & auth);

  private:
    MyProcess & app; 
};


#endif // _HTML_H
