/*
 * mixer.h
 *
 * Project mixer header
 *
 */

#ifndef _MIXER_H
#define _MIXER_H

#include "precompile.h"
#include "manager.h"

class MyMixerEndPoint;

class MyMixerConnection : public OpalMixerConnection
{
  PCLASSINFO(MyMixerConnection, OpalMixerConnection);
  public:
    MyMixerConnection(
     PSafePtr<OpalMixerNode> node,
     OpalCall & call,
     MyMixerEndPoint & endpoint,
     void * userData,
     unsigned options,
     OpalConnection::StringOptions * stringOptions
    );
    
    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      PBoolean isSource                    ///<  Is a source stream
    );
    
  protected:
    MyMixerEndPoint & m_endpoint;
    
};


class MyMixerEndPoint : public OpalConsoleMixerEndPoint
{
  PCLASSINFO(MyMixerEndPoint, OpalConsoleMixerEndPoint);
  public:
    MyMixerEndPoint(MyManager & manager);

    virtual bool Initialise(PArgList & args, bool verbose, const PString &);

    bool Configure(PConfig & cfg, PConfigPage * rsrc);
 
    virtual OpalMixerConnection * CreateConnection(
     PSafePtr<OpalMixerNode> node,
     OpalCall & call,
     void * userData,
     unsigned options,
     OpalConnection::StringOptions * stringOptions
    );

    virtual OpalMixerNode * CreateNode(OpalMixerNodeInfo * info);  

    virtual OpalVideoStreamMixer * CreateVideoMixer(const OpalMixerNodeInfo & info);

    PStringArray GetRoomBook() { return m_roomBook;}
  protected:
    PStringArray m_roomBook;

};

class MyMixerMediaStream : public OpalMixerMediaStream
{
  PCLASSINFO(MyMixerMediaStream, OpalMixerMediaStream);
  public:
  MyMixerMediaStream(
   OpalConnection & conn,               ///<  Connection for media stream
   const OpalMediaFormat & mediaFormat, ///<  Media format for stream
   unsigned sessionID,                  ///<  Session number for stream
   bool isSource,                       ///<  Is a source stream
   PSafePtr<OpalMixerNode> node,        ///<  Mixer node to send data
   bool listenOnly,                     ///<  Effectively initial pause state
   PVideoOutputDevice * outputDevice,   ///<  Device to use for video display
   bool autoDeleteOutput = true         ///<  Automatically delete PVideoOutputDevice
  );
  
  ~MyMixerMediaStream(); 
  virtual PBoolean Open();

  virtual PBoolean WriteData(
      const BYTE * data,   ///<  Data to write
      PINDEX length,       ///<  Length of data to read.
      PINDEX & written     ///<  Length of data actually written
    );

  virtual PBoolean WritePacket(
      RTP_DataFrame & packet
    );

    /** Get the output device (e.g. for statistics)
      */
    virtual PVideoOutputDevice * GetVideoOutputDevice() const
    {
      return m_outputDevice;
    }
    
    virtual void InternalClose();
    bool InternalAdjustDevices();
    
    PVideoOutputDevice * m_outputDevice;
    bool                 m_autoDeleteOutput;
    PDECLARE_MUTEX(m_devicesMutex); 
  
};

class MyVideoStreamMixer : public OpalVideoStreamMixer
{
  PCLASSINFO(MyVideoStreamMixer, OpalVideoStreamMixer);
  public:
    MyVideoStreamMixer(const OpalMixerNodeInfo & info);
    ~MyVideoStreamMixer();
  virtual bool StartMix(unsigned & x, unsigned & y, unsigned & w, unsigned & h, unsigned & left);
    
};



#endif // _MIXER_H
