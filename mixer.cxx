/*
 * mixer.cxx
 *
 * Project mixer class
 *
 */

#include "mixer.h"

MyMixerEndPoint::MyMixerEndPoint(MyManager & manager)
  : OpalConsoleMixerEndPoint(manager)
{
}

bool MyMixerEndPoint::Initialise(PArgList & args, bool verbose, const PString &)
{
  if (verbose)
   cout << "Terminal MCU inicializado" << endl;
  
  SetDeferredAnswer(false);
  return true;
}

bool MyMixerEndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  OpalMixerNodeInfo adHoc;
    if (GetAdHocNodeInfo() != NULL)
      adHoc = *GetAdHocNodeInfo();
    adHoc.m_name = "OpalServer";
    adHoc.m_mediaPassThru = rsrc->AddBooleanField("ConfMediaPassThruKey", adHoc.m_mediaPassThru, "Conference media pass though optimisation");

#if OPAL_VIDEO
    adHoc.m_audioOnly = rsrc->AddBooleanField("ConfAudioOnlyKey", adHoc.m_audioOnly, "Conference is audio only");

    PVideoFrameInfo::ParseSize(rsrc->AddStringField("ConfVideoResolutionKey", 10,
      PVideoFrameInfo::AsString(adHoc.m_width, adHoc.m_height),
      "Conference video frame resolution"),
      adHoc.m_width, adHoc.m_height);


  static const char * const MixingModes[] = { "SideBySideLetterbox", "SideBySideScaled", "StackedPillarbox", "StackedScaled", "Grid" };
  PString mode = rsrc->AddSelectField("VideoMixingModeKey", PStringArray(PARRAYSIZE(MixingModes), MixingModes),
    MixingModes[adHoc.m_style], "Video mixing mode.");
  for (PINDEX i = 0; i < PARRAYSIZE(MixingModes); ++i) {
    if (mode == MixingModes[i]) {
      adHoc.m_style = (OpalVideoMixer::Styles)i;
      break;
    }
  }
#endif
  adHoc.m_closeOnEmpty = true;
  SetAdHocNodeInfo(adHoc);
  
  return true;
}

OpalMixerConnection * MyMixerEndPoint::CreateConnection(PSafePtr<OpalMixerNode> node,
                                                        OpalCall & call,
                                                        void * userData,
                                                        unsigned options,
                                                        OpalConnection::StringOptions * stringOptions)
{
  return new MyMixerConnection(node, call, *this, userData, options, stringOptions);
}

OpalMixerNode * MyMixerEndPoint::CreateNode(OpalMixerNodeInfo * info)
{
  cout << "Created new conference \"" << info->m_name << '"';

  return OpalMixerEndPoint::CreateNode(info);
}

OpalVideoStreamMixer * MyMixerEndPoint::CreateVideoMixer(const OpalMixerNodeInfo & info)
{
  return new MyVideoStreamMixer(info);
}


MyMixerConnection::MyMixerConnection(PSafePtr<OpalMixerNode> node,
                                     OpalCall & call,
                                     MyMixerEndPoint & ep,
                                     void * userData,
                                     unsigned options,
                                     OpalConnection::StringOptions * stringOptions)
  : OpalMixerConnection(node, call, ep, userData, options, stringOptions)
  , m_endpoint(ep)
{
}

OpalMediaStream * MyMixerConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                         unsigned sessionID,
                                                         PBoolean isSource)
{
  cout << mediaFormat.GetDescription() << endl;
  
  PVideoOutputDevice * previewDevice;
  PBoolean autoDeletePreview;
  if (CreateVideoOutputDevice(mediaFormat, false, previewDevice, autoDeletePreview))
     PTRACE(4, "OpalCon\tCreated preview device \"" << previewDevice->GetDeviceName() << '"');
  else
     previewDevice = NULL;
  
     
  MyMixerMediaStream * stream = new MyMixerMediaStream(*this, mediaFormat, sessionID, isSource, m_node, m_listenOnly, previewDevice, autoDeletePreview);

  
   return stream; 

}

MyMixerMediaStream::MyMixerMediaStream(OpalConnection & conn,
                                       const OpalMediaFormat & format,
                                       unsigned sessionID,
                                       bool isSource,
                                       PSafePtr<OpalMixerNode> node,
                                       bool listenOnly,
                                       PVideoOutputDevice * out,
                                       PBoolean delOut)
  : OpalMixerMediaStream(conn, format, sessionID, isSource, node, listenOnly)
  , m_outputDevice(out)
  , m_autoDeleteOutput(delOut)
{
}


MyMixerMediaStream::~MyMixerMediaStream()
{
  Close();
}


PBoolean MyMixerMediaStream::Open()
{
  if (m_isOpen)
    return true;

  P_INSTRUMENTED_LOCK_READ_WRITE(return false);

  if (m_mediaFormat.GetMediaType() != OpalMediaType::Audio()
#if OPAL_VIDEO
   && m_mediaFormat.GetMediaType() != OpalMediaType::Video()
#endif
  ) {
    PTRACE(3, "Cannot open media stream of type " << m_mediaFormat.GetMediaType());
    return false;
  }

  InternalAdjustDevices();
  
  SetPaused(IsSink() && m_listenOnly);
  
  if (!IsPaused() && !m_node->AttachStream(this))
    return false; //(4)

  return OpalMediaStream::Open();
}

PBoolean MyMixerMediaStream::WritePacket(RTP_DataFrame & packet)
{   
  OpalMediaStream::WritePacket(packet);
  return IsOpen() && m_node->WritePacket(*this, packet);
}

PBoolean MyMixerMediaStream::WriteData(const BYTE * data, PINDEX length, PINDEX & written)
{
  /*if (!IsOpen())
    return false;
  
  if (IsSource()) {
    cout << "Tried to write to source media stream" << endl;
    return false;
  }*/

  PWaitAndSignal mutex(m_devicesMutex);

  // Assume we are writing the exact amount (check?)
  written = length;

  // Check for missing packets, we do nothing at this level, just ignore it
  if (data == NULL)
    return true;

  const OpalVideoTranscoder::FrameHeader * frame = (const OpalVideoTranscoder::FrameHeader *)data;

  if (m_outputDevice != NULL) 
    cout << "Creating device"<< m_outputDevice->GetDeviceName()<< "with format"  <<  frame->width << " x " << frame->height << endl;

  if (!m_outputDevice->SetFrameSize(frame->width, frame->height)) {
    PTRACE(1, "Could not resize video display device to " << frame->width << 'x' << frame->height);
    return false;
  }
  
  if (!m_outputDevice->Start()) {
    PTRACE(1, "Could not start video display device");
    return false;
  }
  bool keyFrameNeeded = false;
  if (!m_outputDevice->SetFrameData(frame->x, frame->y,
                                    frame->width, frame->height,
                                    OpalVideoFrameDataPtr(frame),
                                    m_marker, keyFrameNeeded))
    return false;
  if (keyFrameNeeded)
    ExecuteCommand(OpalVideoUpdatePicture());
  cout << "Writing packets" << endl;
  return true;
}

bool MyMixerMediaStream::InternalAdjustDevices()
{
  PVideoFrameInfo video(m_mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption(), PVideoFrameInfo::QCIFWidth),
                        m_mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption(), PVideoFrameInfo::QCIFHeight),
                        m_mediaFormat.GetName(),
                       (m_mediaFormat.GetClockRate()+m_mediaFormat.GetFrameTime()/2)/m_mediaFormat.GetFrameTime());

  if (m_outputDevice != NULL) {
    if (!m_outputDevice->SetFrameInfoConverter(video)){
      return false;
    }
  }
  cout << "Adjusted video devices to " << video << " on " << *this << endl;
  return true;
}

void MyMixerMediaStream::InternalClose()
{
  if (m_outputDevice != NULL) {
    if (m_autoDeleteOutput)
      m_outputDevice->Close();
    else
      m_outputDevice->Stop();
  }
  m_node->DetachStream(this);
}

MyVideoStreamMixer::MyVideoStreamMixer(const OpalMixerNodeInfo & info)
 : OpalVideoStreamMixer(info)
{
}

MyVideoStreamMixer::~MyVideoStreamMixer()
{
  StopPushThread();
}

bool MyVideoStreamMixer::StartMix(unsigned & x, unsigned & y, unsigned & w, unsigned & h, unsigned & left)
{
  switch (m_style) {
    case eSideBySideLetterbox:
      x = left = 0;
      y = m_height / 4;
      w = m_width / 2;
      h = m_height / 2;
      break;

    case eSideBySideScaled:
      x = left = 0;
      y = 0;
      w = m_width / 2;
      h = m_height;
      break;

    case eStackedPillarbox:
      x = left = m_width / 4;
      y = 0;
      w = m_width / 2;
      h = m_height / 2;
      break;

    case eStackedScaled:
      x = left = 0;
      y = 0;
      w = m_width;
      h = m_height / 2;
      break;

    case eGrid:
      x = left = 0;
      y = 0;
      if (m_lastStreamCount != m_inputStreams.size()) {
        PColourConverter::FillYUV420P(0, 0, m_width, m_height, m_width, m_height,
                                      m_frameStore.GetPointer(),
                                      m_bgFillRed, m_bgFillGreen, m_bgFillBlue);
        m_lastStreamCount = m_inputStreams.size();
      }
      switch (m_lastStreamCount) {
        case 0:
        case 1:
          x = left = m_width / 5;
      y = 0;
      w = m_width / 2;
      h = m_height / 2;
          break;

        case 2:
          y = m_height / 4;
          // Fall into next case

        case 3:
        case 4:
          w = m_width / 2;
          h = m_height / 2;
          break;

        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          w = m_width / 3;
          h = m_height / 3;
          break;

        default:
          w = m_width / 4;
          h = m_height / 4;
          break;
      }
      break;

    default:
      return false;
  }

  return true;
}