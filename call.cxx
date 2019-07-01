
#include "call.h"
#include "config.h"
#include "manager.h"

static struct
{
  PVarType::BasicType m_type;
  unsigned            m_size;
  const char *        m_name;
} const CDRFields[MyCall::NumFieldCodes] = {
  { PVarType::VarDynamicString,  36, "CallId"                       },
  { PVarType::VarTime,            0, "StartTime"                    },
  { PVarType::VarTime,            0, "ConnectTime"                  },
  { PVarType::VarTime,            0, "EndTime"                      },
  { PVarType::VarInt16,           0, "CallState"                    },
  { PVarType::VarDynamicString,   0, "CallResult"                   },
  { PVarType::VarDynamicString,  50, "OriginatorID"                 },
  { PVarType::VarDynamicString, 100, "OriginatorURI"                },
  { PVarType::VarDynamicString,  46, "OriginatorSignalAddress"      },
  { PVarType::VarDynamicString,  50, "DestinationID"                },
  { PVarType::VarDynamicString, 100, "DestinationURI"               },
  { PVarType::VarDynamicString,  46, "DestinationSignalAddress"     },
  { PVarType::VarDynamicString,  15, "AudioCodec"                   },
  { PVarType::VarDynamicString,  46, "AudioOriginatorMediaAddress"  },
  { PVarType::VarDynamicString,  46, "AudioDestinationMediaAddress" },
  { PVarType::VarDynamicString,  15, "VideoCodec"                   },
  { PVarType::VarDynamicString,  46, "VideoOriginatorMediaAddress"  },
  { PVarType::VarDynamicString,  46, "VideoDestinationMediaAddress" },
  { PVarType::VarInt32,           0, "Bandwidth"                    }
};

static PString GetDefaultTextHeadings()
{
  PStringStream strm;

  for (MyCall::FieldCodes f = MyCall::BeginFieldCodes; f < MyCall::EndFieldCodes; ++f) {
    if (f > MyCall::BeginFieldCodes)
      strm << ',';
    strm << CDRFields[f].m_name;
  }

  return strm;
}

static PString GetDefaultTextFormats()
{
  PStringStream strm;

  for (MyCall::FieldCodes f = MyCall::BeginFieldCodes; f < MyCall::EndFieldCodes; ++f) {
    if (f > MyCall::BeginFieldCodes)
      strm << ',';
    if (CDRFields[f].m_type != PVarType::VarDynamicString)
      strm << '%' << CDRFields[f].m_name << '%';
    else
      strm << "\"%" << CDRFields[f].m_name << "%\"";
  }

  return strm;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MyManager::ConfigureCDR(PConfig & cfg, PConfigPage * rsrc)
{
  m_cdrMutex.Wait();

  m_cdrTextFile.Close();

  PString filename = rsrc->AddStringField(CDRTextFileKey, 0, m_cdrTextFile.GetFilePath(), "Nombre archivo de texto del CDR.", 1, 30);
  PString cdrHeadings = rsrc->AddStringField(CDRTextHeadingsKey, 0, GetDefaultTextHeadings(), "Headers del CDR en el texto de salida.", 1, 30);
  m_cdrFormat = rsrc->AddStringField(CDRTextFormatKey, 0, GetDefaultTextFormats(), "Formato de salida de texto CDR.", 1, 30);

  if (!filename.IsEmpty()) {
    if (m_cdrTextFile.Open(filename, PFile::WriteOnly, PFile::Create)) {
      m_cdrTextFile.SetPosition(0, PFile::End);
      if (m_cdrTextFile.GetPosition() == 0)
        m_cdrTextFile << cdrHeadings << endl;
    }
    else
      PSYSTEMLOG(Error, "Could not open CDR text file \"" << filename << '"');
  }

  m_cdrListMax = rsrc->AddIntegerField(CDRWebPageLimitKey, 1, 1000000, m_cdrListMax,
                                       "", "Máximo número de registros CDR guardados para mostrar en página web.");

  m_cdrMutex.Signal();

  return true;
}

void MyManager::DropCDR(const MyCall & call, bool final)
{
  m_cdrMutex.Wait();

  if (final) {
    m_cdrList.push_back(call);
    while (m_cdrList.size() > m_cdrListMax)
      m_cdrList.pop_front();

    if (m_cdrTextFile.IsOpen()) {
      call.OutputText(m_cdrTextFile, m_cdrFormat.IsEmpty() ? GetDefaultTextFormats() : m_cdrFormat);
      PSYSTEMLOG(Info, "Dropped text CDR for " << call.GetGUID());
    }
  }

  m_cdrMutex.Signal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyCallDetailRecord::MyCallDetailRecord()
  : m_ConnectTime(0)
  , m_EndTime(0)
  , m_CallState(CallSetUp)
  , m_Bandwidth(0)
{
}

PString MyCallDetailRecord::ListMedia() const
{
  PStringStream strm;

  for (MediaMap::const_iterator it = m_media.begin(); it != m_media.end(); ++it) {
    if (!strm.IsEmpty())
      strm << ", ";
    if (it->second.m_closed)
      strm << '(' << it->second.m_Codec << ')';
    else
      strm << it->second.m_Codec;
  }

  if (strm.IsEmpty())
    strm << "None";

  return strm;
}

PString MyCallDetailRecord::GetCallState() const
{
  static const char * const CallStates[] = {
    "Completed", "Setting Up", "Proceeding", "Alerting", "Connected", "Established"
  };

  if (m_CallState < PARRAYSIZE(CallStates))
    return CallStates[m_CallState];

  return psprintf("CallState=%u", m_CallState);
}

MyCall::Media MyCallDetailRecord::GetMedia(const OpalMediaType & mediaType) const
{
  for (MediaMap::const_iterator it = m_media.begin(); it != m_media.end(); ++it) {
    if (it->second.m_Codec.GetMediaType() == mediaType)
      return it->second;
  }

  return Media();
}

void MyCallDetailRecord::OutputDetailedHTML(PHTML & html) const
{
  html << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4);

  for (MyCall::FieldCodes f = MyCall::BeginFieldCodes; f < MyCall::EndFieldCodes; ++f) {
    html << PHTML::TableRow()
         << PHTML::TableHeader() << CDRFields[f].m_name
         << PHTML::TableData();
    switch (f) {
      case CallId:
        html << m_GUID;
        break;
      case StartTime:
        html << m_StartTime.AsString(PTime::LongDateTime);
        break;
      case ConnectTime:
        if (m_ConnectTime.IsValid())
          html << m_ConnectTime.AsString(PTime::LongDateTime);
        break;
      case EndTime:
        html << m_EndTime.AsString(PTime::LongDateTime);
        break;
      case CallState:
        html << (m_CallState != CallCompleted ? "Active" : "Completed");
        break;
      case CallResult:
        if (m_CallState == CallCompleted)
          html << OpalConnection::GetCallEndReasonText(m_CallResult);
        break;
      case OriginatorID:
        html << m_OriginatorID;
        break;
      case OriginatorURI:
        html << m_OriginatorURI;
        break;
      case OriginatorSignalAddress:
        html << m_OriginatorSignalAddress;
        break;
      case DestinationID:
        html << m_DestinationID;
        break;
      case DestinationURI:
        html << m_DestinationURI;
        break;
      case DestinationSignalAddress:
        if (m_DestinationSignalAddress.IsValid())
          html << m_DestinationSignalAddress;
        break;
      case AudioCodec:
        html << GetAudio().m_Codec;
        break;
      case AudioOriginatorMediaAddress:
        html << GetAudio().m_OriginatorAddress;
        break;
      case AudioDestinationMediaAddress:
        html << GetAudio().m_DestinationAddress;
        break;
#if OPAL_VIDEO
      case VideoCodec:
        html << GetVideo().m_Codec;
        break;
      case VideoOriginatorMediaAddress:
        html << GetVideo().m_OriginatorAddress;
        break;
      case VideoDestinationMediaAddress:
        html << GetVideo().m_DestinationAddress;
        break;
#endif //OPAL_VIDEO
      case Bandwidth:
        if (m_Bandwidth > 0)
          html << (m_Bandwidth + 999) / 1000;
        break;
      default:
        break;
    }
  }

  html << PHTML::TableEnd();
}

void MyCallDetailRecord::OutputSummaryHTML(PHTML & html) const
{
  html << PHTML::TableRow()
       << PHTML::TableData() << PHTML::HotLink("CDR?guid=" + m_GUID.AsString()) << "<span style='color:blue'>" << m_GUID << "</span>" <<PHTML::HotLink()
       << PHTML::TableData() << PHTML::Escape(m_OriginatorURI)
       << PHTML::TableData() << PHTML::Escape(m_DestinationURI)
       << PHTML::TableData() << m_StartTime.AsString(PTime::LoggingFormat)
       << PHTML::TableData() << m_EndTime.AsString(PTime::LoggingFormat)
       << PHTML::TableData();
  if (m_CallState != CallCompleted)
    html << "Active";
  else
    html << OpalConnection::GetCallEndReasonText(m_CallResult);
}

void MyCallDetailRecord::OutputText(ostream & strm, const PString & format) const
{
  PINDEX percent = format.Find('%');
  PINDEX last = 0;
  while (percent != P_MAX_INDEX) {
    if (percent > last)
      strm << format(last, percent - 1);

    bool quoted = percent > 0 && format[percent - 1] == '"';

    last = percent + 1;
    percent = format.Find('%', last);
    if (percent == P_MAX_INDEX || percent == last)
      return;

    PCaselessString fieldName = format(last, percent - 1);
    for (FieldCodes f = BeginFieldCodes; f < EndFieldCodes; ++f) {
      if (fieldName == CDRFields[f].m_name) {
        switch (f) {
          case CallId:
            strm << m_GUID;
            break;
          case StartTime:
            strm << m_StartTime.AsString(PTime::LoggingFormat);
            break;
          case ConnectTime:
            if (m_ConnectTime.IsValid())
              strm << m_ConnectTime.AsString(PTime::LoggingFormat);
            break;
          case EndTime:
            strm << m_EndTime.AsString(PTime::LoggingFormat);
            break;
          case CallState:
            if (m_CallState != CallCompleted)
              strm << -m_CallState;
            else
              strm << m_CallResult.AsInteger();
            break;
          case CallResult:
            if (m_CallState == CallCompleted)
              strm << OpalConnection::GetCallEndReasonText(m_CallResult);
            break;
          case OriginatorID:
            strm << m_OriginatorID;
            break;
          case OriginatorURI:
            if (quoted) {
              PString str = m_OriginatorURI;
              str.Replace("\"", "\"\"", true);
              strm << str;
            }
            else
              strm << m_OriginatorURI;
            break;
          case OriginatorSignalAddress:
            strm << m_OriginatorSignalAddress;
            break;
          case DestinationID:
            strm << m_DestinationID;
            break;
          case DestinationURI:
            if (quoted) {
              PString str = m_DestinationURI;
              str.Replace("\"", "\"\"", true);
              strm << str;
            }
            else
              strm << m_DestinationURI;
            break;
          case DestinationSignalAddress:
            if (m_DestinationSignalAddress.IsValid())
              strm << m_DestinationSignalAddress;
            break;
          case AudioCodec:
            strm << GetAudio().m_Codec;
            break;
          case AudioOriginatorMediaAddress:
            strm << GetAudio().m_OriginatorAddress;
            break;
          case AudioDestinationMediaAddress:
            strm << GetAudio().m_DestinationAddress;
            break;
#if OPAL_VIDEO
          case VideoCodec:
            strm << GetVideo().m_Codec;
            break;
          case VideoOriginatorMediaAddress:
            strm << GetVideo().m_OriginatorAddress;
            break;
          case VideoDestinationMediaAddress:
            strm << GetVideo().m_DestinationAddress;
            break;
#endif // OPAL_VIDEO
          case Bandwidth:
            if (m_Bandwidth > 0)
              strm << (m_Bandwidth + 999) / 1000;
            break;
          default:
            break;
        }
      }
    }

    last = percent + 1;
    percent = format.Find('%', last);
  }
  strm << format.Mid(last) << endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyCall::MyCall(MyManager & manager)
  : OpalCall(manager)
  , m_manager(manager)
{
}

PBoolean MyCall::OnAlerting(OpalConnection & connection)
{
  m_CallState = MyCall::CallAlerting;
  connection.GetRemoteAddress().GetIpAndPort(m_DestinationSignalAddress);

  m_manager.DropCDR(*this, false);

  return OpalCall::OnAlerting(connection);
}

void MyCall::OnCleared()
{
  m_CallState = MyCall::CallCompleted;
  m_CallResult = GetCallEndReason();
  m_EndTime.SetCurrentTime();

  m_manager.DropCDR(*this, true);

  OpalCall::OnCleared();
}

PBoolean MyCall::OnConnected(OpalConnection & connection)
{
  m_ConnectTime.SetCurrentTime();
  m_CallState = MyCall::CallConnected;
  connection.GetRemoteAddress().GetIpAndPort(m_DestinationSignalAddress);

  m_manager.DropCDR(*this, false);

  return OpalCall::OnConnected(connection);
}

void MyCall::OnEstablishedCall()
{
  m_CallState = MyCall::CallEstablished;

  m_manager.DropCDR(*this, false);

  OpalCall::OnEstablishedCall();
}

void MyCall::OnProceeding(OpalConnection & connection)
{
  m_CallState = MyCall::CallProceeding;
  m_DestinationURI = GetPartyB();
  connection.GetRemoteAddress().GetIpAndPort(m_DestinationSignalAddress);

  m_manager.DropCDR(*this, false);

  OpalCall::OnProceeding(connection);
}

PBoolean MyCall::OnSetUp(OpalConnection & connection)
{
  if (!OpalCall::OnSetUp(connection))
    return false;
  
  PSafePtr<OpalConnection> a = GetConnection(0);
  if (a != NULL) {
    m_OriginatorID = a->GetIdentifier();
    m_OriginatorURI = GetPartyA();
    m_DestinationURI = GetPartyB();
    a->GetRemoteAddress().GetIpAndPort(m_OriginatorSignalAddress);
  }

  PSafePtr<OpalConnection> b = GetConnection(1);
  if (b != NULL) {
    m_DestinationID = b->GetIdentifier();
    m_DestinationURI = GetPartyB();
  }

  m_manager.DropCDR(*this, false);

  return true;
}

void MyCall::OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  OpalMediaStream & stream = patch.GetSource();

  Media & media = m_media[stream.GetID()];
  media.m_Codec = stream.GetMediaFormat();

  OpalRTPConnection * rtpConn = dynamic_cast<OpalRTPConnection *>(&connection);
  if (rtpConn == NULL)
    return;

  OpalMediaSession * session = rtpConn->GetMediaSession(stream.GetSessionID());
  if (session == NULL)
    return;

  session->GetRemoteAddress().GetIpAndPort(&connection == GetConnection(0) ? media.m_OriginatorAddress : media.m_DestinationAddress);
}

void MyCall::OnStopMediaPatch(OpalMediaPatch & patch)
{
  OpalMediaStream & stream = patch.GetSource();

  m_media[stream.GetID()].m_closed = true;

#if OPAL_STATISTICS
  OpalMediaStatistics stats;
  stream.GetStatistics(stats);
  int duration = (PTime() - stats.m_startTime).GetSeconds();
  if (duration > 0)
    m_Bandwidth += stats.m_totalBytes * 8 / duration;
#endif
} // Final del Archivo