/*
 * h323.h
 *
 * Project h323 header
 *
 */

#ifndef _H323_H
#define _H323_H

#include "precompile.h"
#include "manager.h"
///////////////////////////////////////////////////////////////////////////////
#if OPAL_H323
class MyH323EndPoint : public H323ConsoleEndPoint
{
  PCLASSINFO(MyH323EndPoint, H323ConsoleEndPoint);
  public:
    MyH323EndPoint(MyManager & manager);
    virtual bool Initialise(PArgList & args, bool verbose, const PString & defaultRoute);
    bool Configure(PConfig & cfg, PConfigPage * rsrc);

  protected:
    MyManager        & m_manager;
    bool               m_firstConfig;
    PStringArray       m_configuredAliases;
    PStringArray       m_configuredAliasPatterns;

};
#endif 
#endif // _H323_H
// End of File ////////////////////////////////////////////////////////////////
