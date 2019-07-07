/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  M A I N                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _MAIN_H
#define _MAIN_H

#include "precompile.h" 
#include "manager.h"

/*************************************************************************/
/*                                                                       */
/* <clase MyPConfigPage>                                                 */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PConfigPage que describe una instancia de         */
/*  configuración para la aplicación.                                    */
/*                                                                       */
/*************************************************************************/
class MyPConfigPage : public PConfigPage                                 //
{                                                                        //
  public:                                                                //
    MyPConfigPage(PHTTPServiceProcess & app,                             //
                  const PString & title,                                 //
                  const PString & section,                               //
                  const PHTTPAuthority & auth)                           //
    : PConfigPage(app,title,section,auth)                                //
    {                                                                    //
    }                                                                    //
                                                                         //
    void BuildHTML(PHTML & html, BuildOptions option = CompleteHTML);    //
                                                                         //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyProcess>                                                     */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PHTTPServiceProcess que describe un servidor      */
/*  HTTP.                                                                */
/*                                                                       */
/*************************************************************************/
class MyProcess : public PHTTPServiceProcess                             //
{                                                                        //
  PCLASSINFO(MyProcess, PHTTPServiceProcess)                             //
  public:                                                                //
    MyProcess();                                                         //
    ~MyProcess();                                                        //
                                                                         //
    virtual PBoolean Initialise(const char * initMsg);                   //
    virtual bool InitialiseBase(Params & params);                        //
    virtual void OnConfigChanged();                                      //
    virtual void OnControl();                                            //
    virtual PBoolean OnStart();                                          //
    virtual void OnStop();                                               //
    void Main();                                                         //
                                                                         //
    /** Funciones implementadas en OpenMCU-ru */                         //
    //@{                                                                 //
    void CreateHTTPResource(const PString & name);                       //
                                                                         //
    static MyProcess & Current()                                         //
    {                                                                    //
      return (MyProcess&)PProcess::Current();                            //
    }                                                                    //
                                                                         //
    PString GetHtmlCopyright()                                           //
    {                                                                    //
      PHTML html(PHTML::InBody);                                         //
      html << "OpalMCU-EIE &copy;" << m_compilationDate.AsString("yyyy") //
           << " por " << m_copyrightHolder;                              //
                                                                         //
      return html;                                                       //
    }                                                                    //
    //@}                                                                 //
                                                                         //
    MyManager & GetManager() { return *m_manager; }                      //
                                                                         //
  protected:                                                             //
    MyManager        * m_manager;                                        //
    MyPConfigPage    * m_pageConfigure;                                  //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyClearLogPage>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de ClearLogPage que describe una instancia que borra */
/*  trazo log generado por la aplicación.                                */
/*                                                                       */
/*************************************************************************/
class MyClearLogPage : public ClearLogPage                               //
{                                                                        //
  PCLASSINFO(MyClearLogPage, ClearLogPage);                              //
  public:                                                                //
    MyClearLogPage(                                                      //
      PHTTPServiceProcess & process,                                     //
      const PURL & url,                                                  //
      const PHTTPAuthority & auth                                        //
    );                                                                   //
                                                                         //
    virtual PString LoadText(                                            //
      PHTTPRequest & request                                             //
    );                                                                   //
    virtual PBoolean Post(                                               //
      PHTTPRequest & request,                                            //
      const PStringToString &,                                           //
      PHTML & msg                                                        //
    );                                                                   //
                                                                         //
};                                                                       //
/*************************************************************************/
#endif // _MAIN_H
/************************Final del Header*********************************/