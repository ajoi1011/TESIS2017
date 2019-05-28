/*
 * main.h
 *
 * Project main header
 *
*/

#ifndef _MAIN_H
#define _MAIN_H

#include "precompile.h" 
#include "manager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

class MyPConfigPage : public PConfigPage
{
  public:
    MyPConfigPage(PHTTPServiceProcess & app,const PString & title, const PString & section, const PHTTPAuthority & auth)
      : PConfigPage(app,title,section,auth)
    {
    }

    void BuildHTML(PHTML & html, BuildOptions option = CompleteHTML);

  protected:
    PConfig cfg;   
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class MyProcess : public MyProcessHTTP
{
  PCLASSINFO(MyProcess, MyProcessHTTP)
  public:
    MyProcess();

    ~MyProcess();

    virtual PBoolean OnStart();

    virtual void OnStop();

    virtual void OnControl();

    virtual void OnConfigChanged();

    virtual PBoolean Initialise(const char * initMsg);

    void Main();

    static MyProcess & Current() { return (MyProcess&)PProcess::Current(); }

    virtual bool InitialiseBase(Params & params);

    //void CreateHTTPResource(const PString & name);

    PString GetHtmlCopyright()
    {
      PHTML html(PHTML::InBody);
      html << "MCU &copy;" << m_compilationDate.AsString("yyyy") << " por " << m_copyrightHolder;
      return html;
    }

  protected:
    MyManager        * m_manager;
    MyPConfigPage    * m_pageConfigure;
  
};
#endif

// End of File ////////////////////////////////////////////////////////////////////////////////////
