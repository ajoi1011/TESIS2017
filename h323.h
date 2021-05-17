/*
 * h323.h
 * 
 * Copyright 2020 ajoi1011 <ajoi1011@debian>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  H 3 2 3                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _H323_H
#define _H323_H

#include "precompile.h"
class Manager;

/*************************************************************************/
/*                                                                       */
/* <clase MCUH323EndPoint>                                               */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de H323ConsoleEndPoint que describe un terminal      */
/*  H.323.                                                               */
/*                                                                       */
/*************************************************************************/
class MCUH323EndPoint : public H323ConsoleEndPoint  
{
  PCLASSINFO(MCUH323EndPoint, H323ConsoleEndPoint); 
  public:
    MCUH323EndPoint(Manager & manager);
    
    virtual bool Initialise(
      PArgList & args,
      bool verbose,
      const PString & defaultRoute
    );
    
    bool Configure(PConfig & cfg);
    
  //protected:
    Manager          & m_manager; 
    //MyGatekeeperServer m_gkServer;
    bool               m_firstConfig;
    PStringArray       m_configuredAliases;
    PStringArray       m_configuredAliasPatterns;
    
};  
#endif
