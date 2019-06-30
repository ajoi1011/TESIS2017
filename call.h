/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  C A L L                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _CALL_H
#define _CALL_H

#include "precompile.h"

class MyManager;
/*************************************************************************/
/*                                                                       */
/* <clase MyCallDetailRecord>                                            */
/*                                                                       */
/* <Descripcion>                                                         */
/*   Clase que describe un registro detallado de los parámetros de las   */
/*  llamadas entrantes y salientes (CDR).                                */
/*                                                                       */
/*************************************************************************/
class MyCallDetailRecord                                                 // 
{                                                                        //
  public:                                                                //
    MyCallDetailRecord();                                                //
                                                                         //
    P_DECLARE_ENUM(FieldCodes,                                           //
      CallId,                                                            //
      StartTime,                                                         //
      ConnectTime,                                                       //
      EndTime,                                                           //
      CallState,                                                         //
      CallResult,                                                        //
      OriginatorID,                                                      //
      OriginatorURI,                                                     //
      OriginatorSignalAddress,                                           //
      DestinationID,                                                     //
      DestinationURI,                                                    //
      DestinationSignalAddress,                                          //
      AudioCodec,                                                        //
      AudioOriginatorMediaAddress,                                       //
      AudioDestinationMediaAddress,                                      //
      VideoCodec,                                                        //
      VideoOriginatorMediaAddress,                                       //
      VideoDestinationMediaAddress,                                      //
      Bandwidth                                                          //
    );                                                                   //
                                                                         //
    P_DECLARE_ENUM(ActiveCallStates,                                     //
      CallCompleted,                                                     //
      CallSetUp,                                                         //
      CallProceeding,                                                    //
      CallAlerting,                                                      //
      CallConnected,                                                     //
      CallEstablished                                                    //
    );                                                                   //
                                                                         //
    struct Media                                                         //
    {                                                                    //
      Media() : m_closed(false) { }                                      //
                                                                         //
      OpalMediaFormat         m_Codec;                                   //
      PIPSocketAddressAndPort m_OriginatorAddress;                       //
      PIPSocketAddressAndPort m_DestinationAddress;                      //
      bool                    m_closed;                                  //
    };                                                                   //
    typedef std::map<PString, Media> MediaMap;                           //
                                                                         //
    PString ListMedia() const;                                           //
    Media GetAudio() const { return GetMedia(OpalMediaType::Audio()); }  //
    PString GetCallState() const;                                        //
    PString GetGUID() const { return m_GUID.AsString(); }                //
    Media GetMedia(const OpalMediaType & mediaType) const;               //
#if OPAL_VIDEO                                                           //
    Media GetVideo() const { return GetMedia(OpalMediaType::Video()); }  //
#endif                                                                   //
    void OutputDetailedHTML(PHTML & html) const;                         //
    void OutputSummaryHTML(PHTML & html) const;                          //
    void OutputText(ostream & strm, const PString & format) const;       //
                                                                         //
  protected:                                                             //
    PGloballyUniqueID             m_GUID;                                //
    PTime                         m_StartTime;                           //
    PTime                         m_ConnectTime;                         //
    PTime                         m_EndTime;                             //
    ActiveCallStates              m_CallState;                           //
    OpalConnection::CallEndReason m_CallResult;                          //
    PString                       m_OriginatorID;                        //
    PString                       m_OriginatorURI;                       //
    PIPSocketAddressAndPort       m_OriginatorSignalAddress;             //
    PString                       m_DestinationID;                       //
    PString                       m_DestinationURI;                      //
    PIPSocketAddressAndPort       m_DestinationSignalAddress;            //
    uint64_t                      m_Bandwidth;                           //
    MediaMap                      m_media;                               //
};                                                                       //
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase MyCall>                                                        */
/*                                                                       */
/* <Descripcion>                                                         */
/*   Clase derivada de OpalCall y MyCallDetailRecord que describe una    */
/*  llamada OPAL con registro.                                           */
/*                                                                       */
/*************************************************************************/
class MyCall : public OpalCall, public MyCallDetailRecord                //
{                                                                        //
  PCLASSINFO(MyCall, OpalCall);                                          //
  public:                                                                //
    MyCall(MyManager & manager);                                         //
                                                                         //
    virtual PBoolean OnAlerting(OpalConnection & connection);            //
    virtual void OnCleared();                                            //
    virtual PBoolean OnConnected(OpalConnection & connection);           //
    virtual void OnEstablishedCall();                                    //
    virtual void OnProceeding(OpalConnection & connection);              //
    virtual PBoolean OnSetUp(OpalConnection & connection);               //
                                                                         //
    /** Implementadas en clase MyManager */                              //
    //@{                                                                 //
    void OnStartMediaPatch(                                              //
      OpalConnection & connection,                                       // 
      OpalMediaPatch & patch                                             //
    );                                                                   //
    void OnStopMediaPatch(OpalMediaPatch & patch);                       //
    //@}                                                                 //
                                                                         //
  protected:                                                             //
    MyManager & m_manager;                                               //
};                                                                       //
/*************************************************************************/
#endif // _CALL_H
/************************Final del Header*********************************/
