/*
 * call.h
 *
 * Project call header
 *
 */

#ifndef _CALL_H
#define _CALL_H
#include "precompile.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

class MyManager;

class MyCallDetailRecord
{
  public:
    MyCallDetailRecord();

    P_DECLARE_ENUM(FieldCodes,
      CallId,
      StartTime,
      ConnectTime,
      EndTime,
      CallState,
      CallResult,
      OriginatorID,
      OriginatorURI,
      OriginatorSignalAddress,
      DestinationID,
      DestinationURI,
      DestinationSignalAddress,
      AudioCodec,
      AudioOriginatorMediaAddress,
      AudioDestinationMediaAddress,
      VideoCodec,
      VideoOriginatorMediaAddress,
      VideoDestinationMediaAddress,
      Bandwidth
    );

    P_DECLARE_ENUM(ActiveCallStates,
      CallCompleted,
      CallSetUp,
      CallProceeding,
      CallAlerting,
      CallConnected,
      CallEstablished
    );

    struct Media
    {
      Media() : m_closed(false) { }

      OpalMediaFormat         m_Codec;
      PIPSocketAddressAndPort m_OriginatorAddress;
      PIPSocketAddressAndPort m_DestinationAddress;
      bool                    m_closed;
    };
    typedef std::map<PString, Media> MediaMap;

    Media GetAudio() const { return GetMedia(OpalMediaType::Audio()); }
#if OPAL_VIDEO
    Media GetVideo() const { return GetMedia(OpalMediaType::Video()); }
#endif
    Media GetMedia(const OpalMediaType & mediaType) const;
    PString ListMedia() const;

    PString GetGUID() const { return m_GUID.AsString(); }
    PString GetCallState() const;

    void OutputText(ostream & strm, const PString & format) const;
    void OutputSummaryHTML(PHTML & html) const;
    void OutputDetailedHTML(PHTML & html) const;

  protected:
    PGloballyUniqueID             m_GUID;
    PTime                         m_StartTime;
    PTime                         m_ConnectTime;
    PTime                         m_EndTime;
    ActiveCallStates              m_CallState;
    OpalConnection::CallEndReason m_CallResult;
    PString                       m_OriginatorID;
    PString                       m_OriginatorURI;
    PIPSocketAddressAndPort       m_OriginatorSignalAddress;
    PString                       m_DestinationID;
    PString                       m_DestinationURI;
    PIPSocketAddressAndPort       m_DestinationSignalAddress;
    uint64_t                      m_Bandwidth;
    MediaMap                      m_media;
};

class MyCall : public OpalCall, public MyCallDetailRecord
{
  PCLASSINFO(MyCall, OpalCall);
  public:
    MyCall(MyManager & manager);
    
    // Callbacks from OPAL
    virtual PBoolean OnSetUp(OpalConnection & connection);
    virtual void OnProceeding(OpalConnection & connection);
    virtual PBoolean OnAlerting(OpalConnection & connection);
    virtual PBoolean OnConnected(OpalConnection & connection);
    virtual void OnEstablishedCall();
    virtual void OnCleared();

    // Called by MyManager
    void OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch);
    void OnStopMediaPatch(OpalMediaPatch & patch);
  
  protected:
    MyManager & m_manager;
};

#endif
