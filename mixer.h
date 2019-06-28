/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  M I X E R                          */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _MIXER_H
#define _MIXER_H

#include "manager.h"
#include "precompile.h"

#if OPAL_HAS_MIXER
class MyMixerEndPoint;
/*************************************************************************/
/*                                                                       */
/* <clase MyMixerConnection>                                             */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de OpalMixerConnection que describe una conexión     */
/*  creada por una Multipoint Control Unit (MCU) para una conferencia.   */
/*                                                                       */
/*************************************************************************/
class MyMixerConnection : public OpalMixerConnection                     //
{                                                                        //
  PCLASSINFO(MyMixerConnection, OpalMixerConnection);                    //
  public:                                                                //
    MyMixerConnection(                                                   //
      PSafePtr<OpalMixerNode> node,                                      //
      OpalCall & call,                                                   //
      MyMixerEndPoint & endpoint,                                        //
      void * userData,                                                   //
      unsigned options,                                                  //
      OpalConnection::StringOptions * stringOptions                      //
    );                                                                   //
                                                                         //
    virtual OpalMediaStream * CreateMediaStream(                         //
      const OpalMediaFormat & mediaFormat,                               //
      unsigned sessionID,                                                //
      PBoolean isSource                                                  //
    );                                                                   //
                                                                         //
  protected:                                                             //
    MyMixerEndPoint & m_endpoint;                                        //
                                                                         //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyMixerEndPoint>                                               */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de OpalConsoleMixerEndPoint que describe una         */
/*  Multipoint Control Unit (MCU).                                       */
/*                                                                       */
/*************************************************************************/
class MyMixerEndPoint : public OpalConsoleMixerEndPoint                  //
{                                                                        //
  PCLASSINFO(MyMixerEndPoint, OpalConsoleMixerEndPoint);                 //
  public:                                                                //
    MyMixerEndPoint(MyManager & manager);                                //
                                                                         //
    virtual bool Initialise(                                             //
      PArgList & args,                                                   //
      bool verbose,                                                      //
      const PString &                                                    //
    );                                                                   //
                                                                         //
    virtual OpalMixerConnection * CreateConnection(                      //
      PSafePtr<OpalMixerNode> node,                                      //
      OpalCall & call,                                                   //
      void * userData,                                                   //
      unsigned options,                                                  //
      OpalConnection::StringOptions * stringOptions                      //
    );                                                                   //
    virtual OpalMixerNode * CreateNode(OpalMixerNodeInfo * info);        //  
    virtual OpalVideoStreamMixer * CreateVideoMixer(                     //
      const OpalMixerNodeInfo & info                                     //
    );                                                                   //
    bool Configure(PConfig & cfg, PConfigPage * rsrc);                   //
                                                                         //
  protected:                                                             //
                                                                         //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyMixerMediaStream>                                            */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de OpalMixerMediaStream que describe un media stream */
/*  enviado/recibido por una Multipoint Control Unit (MCU).              */
/*                                                                       */
/*************************************************************************/
class MyMixerMediaStream : public OpalMixerMediaStream                   //
{                                                                        //
  PCLASSINFO(MyMixerMediaStream, OpalMixerMediaStream);                  //
  public:                                                                //
    MyMixerMediaStream(                                                  //
      OpalConnection & conn,                                             //
      const OpalMediaFormat & mediaFormat,                               //
      unsigned sessionID,                                                //
      bool isSource,                                                     //
      PSafePtr<OpalMixerNode> node,                                      //
      bool listenOnly,                                                   //
      PVideoOutputDevice * outputDevice,                                 //
      bool autoDeleteOutput = true                                       //
    );                                                                   //
    ~MyMixerMediaStream();                                               //
                                                                         //
    virtual PVideoOutputDevice * GetVideoOutputDevice() const            //
    {                                                                    //
      return m_outputDevice;                                             //
    }                                                                    //
                                                                         //
    virtual PBoolean Open();                                             //
    virtual PBoolean WriteData(                                          //
      const BYTE * data,                                                 //
      PINDEX length,                                                     //
      PINDEX & written                                                   //
    );                                                                   //
    virtual PBoolean WritePacket(                                        //
      RTP_DataFrame & packet                                             //
    );                                                                   //
                                                                         //
  protected:                                                             //
    virtual void InternalClose();                                        //
    PVideoOutputDevice * m_outputDevice;                                 //
    bool                 m_autoDeleteOutput;                             //
    PDECLARE_MUTEX(m_devicesMutex);                                      //
                                                                         //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyVideoStreamMixer>                                            */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de OpalVideoStreamMixer que describe un frame store  */
/*  de video multiplexado.                                               */
/*                                                                       */
/*************************************************************************/
class MyVideoStreamMixer : public OpalVideoStreamMixer                   //
{                                                                        //
  PCLASSINFO(MyVideoStreamMixer, OpalVideoStreamMixer);                  //
  public:                                                                //
    MyVideoStreamMixer(const OpalMixerNodeInfo & info);                  //
    ~MyVideoStreamMixer();                                               //
                                                                         //
    virtual bool StartMix(                                               //
      unsigned & x,                                                      //
      unsigned & y,                                                      //
      unsigned & w,                                                      //
      unsigned & h,                                                      //
      unsigned & left                                                    //
    );                                                                   //
                                                                         //
};                                                                       //
/*************************************************************************/
#endif // OPAL_HAS_MIXER
#endif // _MIXER_H
/************************Final del Header*********************************/