/*
 * sip.h
 *
 * Project sip header
 *
 */

#ifndef _SIP_H
#define _SIP_H

#include "precompile.h"
#include "manager.h"

class MySIPEndPoint : public SIPConsoleEndPoint
{
  PCLASSINFO(MySIPEndPoint, SIPConsoleEndPoint);
public:
  MySIPEndPoint(MyManager & manager);
  virtual bool Initialise(PArgList & args, bool verbose, const PString & defaultRoute);

  /*void AutoRegisterCisco(const PString & server, const PString & wildcard, const PString & deviceType, bool registering);*/

  bool Configure(PConfig & cfg, PConfigPage * rsrc);
  /*PString OnLoadEndPointStatus(const PString & htmlBlock);
  bool ForceUnregister(const PString id);

#if OPAL_H323
  virtual void OnChangedRegistrarAoR(RegistrarAoR & ua);
#endif

#if OPAL_H323
  bool m_autoRegisterH323;
#endif*/


protected:
  MyManager & m_manager;

  /*unsigned m_ciscoDeviceType;
  PString  m_ciscoDevicePattern;*/
};





#endif // _SIP_H
