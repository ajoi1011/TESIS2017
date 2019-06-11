/*
 * main.h
 *
 * Project main header
 *
*/

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                 B A S E   O B J E C T   C L A S S E S                 */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* <Type>                                                                */
  /*    FT_Face_Internal                                                   */
  /*                                                                       */
  /* <Description>                                                         */
  /*    An opaque handle to an `FT_Face_InternalRec' structure that models */
  /*    the private data of a given @FT_Face object.                       */
  /*                                                                       */
  /*    This structure might change between releases of FreeType~2 and is  */
  /*    not generally available to client applications.                    */
  /*                                                                       */
  /*************************************************************************/


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

    void CreateHTTPResource(const PString & name);

    PString GetHtmlCopyright()
    {
      PHTML html(PHTML::InBody);
      html << "MCU &copy;" << m_compilationDate.AsString("yyyy") << " por " << m_copyrightHolder;
      return html;
    }

    MyManager & GetManager() { return *m_manager;}

  protected:
    MyManager        * m_manager;
    MyPConfigPage    * m_pageConfigure;
  
};
#endif

// End of File ////////////////////////////////////////////////////////////////////////////////////
