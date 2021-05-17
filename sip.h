/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  S I P                              */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _SIP_H
#define _SIP_H

#include "manager.h"
#include "precompile.h"

#if OPAL_SIP
/*************************************************************************/
/*                                                                       */
/* <clase MySIPEndPoint>                                                 */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de SIPConsoleEndPoint que describe un terminal SIP.  */
/*                                                                       */
/*************************************************************************/
class MySIPEndPoint : public SIPConsoleEndPoint                          //
{                                                                        //
  PCLASSINFO(MySIPEndPoint, SIPConsoleEndPoint);                         //
  public:                                                                //
    MySIPEndPoint(MyManager & manager);                                  //
                                                                         //
    virtual bool Initialise(                                             //
      PArgList & args,                                                   //
      bool verbose,                                                      //
      const PString & defaultRoute                                       //
    );                                                                   //
    void AutoRegisterCisco(                                              //
      const PString & server,                                            //
      const PString & wildcard,                                          //
      const PString & deviceType,                                        //
      bool registering                                                   //
    );                                                                   //
    bool Configure(PConfig & cfg, PConfigPage * rsrc);                   //
    bool ForceUnregister(const PString id);                              //
    PString OnLoadEndPointStatus(const PString & htmlBlock);             //
#if OPAL_H323                                                            //
    virtual void OnChangedRegistrarAoR(RegistrarAoR & ua);               //
#endif                                                                   //
    bool GetAutoRegisterH323() { return m_autoRegisterH323; }            //
                                                                         //
  protected:                                                             //
    MyManager & m_manager;                                               //
#if OPAL_H323                                                            //
    bool m_autoRegisterH323;                                             //
#endif                                                                   //
    unsigned m_ciscoDeviceType;                                          //
    PString  m_ciscoDevicePattern;                                       //
};                                                                       //
/*************************************************************************/
#endif // OPAL_SIP
#endif // _SIP_H
/************************Final del Header********************************/
