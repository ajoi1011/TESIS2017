
#ifndef _MAIN_H
#define _MAIN_H

#include "precompile.h"
#include "config.h"
#include "conference.h"

const WORD DefaultHTTPPort = 1719;

static const char LogLevelKey[]           = "Log Level";
static const char TraceLevelKey[]         = "Trace level";
static const char TraceRotateKey[]        = "Rotate trace files at startup";
static const char UserNameKey[]           = "Username";
static const char PasswordKey[]           = "Password";
static const char HttpPortKey[]           = "HTTP Port";
static const char HttpLinkEventBufferKey[]= "Room control event buffer size";

static const char CallLogFilenameKey[]    = "Call log filename";

#if P_SSL
static const char HTTPSecureKey[]           = "Enable HTTP secure";
static const char HTTPCertificateFileKey[]  = "HTTP Certificate";
static PString DefaultHTTPCertificateFile   = PString(SYS_CONFIG_DIR)+PString(PATH_SEPARATOR)+"server.pem";
#endif
static const char DefaultRoomKey[]          = "Default room";
static const char DefaultRoomTimeLimitKey[] = "Room time limit";
static const char AutoStartRecorderKey[]    = "Auto start recorder";
static const char AutoStopRecorderKey[]     = "Auto stop recorder";
static const char AutoDeleteRoomKey[]       = "Auto delete empty rooms";
static const char LockTplByDefaultKey[]     = "Template locks conference by default";
static const char DefaultCallLogFilename[] = "mcu_log.txt"; 
static const char DefaultRoom[]            = "room101";
static const char CreateEmptyRoomKey[]     = "Auto create empty room";
static const char RecallLastTemplateKey[]  = "Auto recall last template";
static const char AllowLoopbackCallsKey[]  = "Allow loopback calls";

static const char SipListenerKey[]         = "SIP Listener";

#if OPENMCU_VIDEO
const unsigned int DefaultVideoFrameRate = 10;
const unsigned int DefaultVideoQuality   = 10;

static const char RecorderFfmpegPathKey[]  = "Path to ffmpeg";
static const char RecorderFfmpegOptsKey[]  = "Ffmpeg options";
static const char RecorderFfmpegDirKey[]   = "Video Recorder directory";
static const char RecorderFrameWidthKey[]  = "Video Recorder frame width";
static const char RecorderFrameHeightKey[] = "Video Recorder frame height";
static const char RecorderFrameRateKey[]   = "Video Recorder frame rate";
static const char RecorderSampleRateKey[]  = "Video Recorder sound rate";
static const char RecorderAudioChansKey[]  = "Video Recorder sound channels";
static const char RecorderMinSpaceKey[]    = "Video Recorder minimum disk space";
#ifdef _WIN32
static const char DefaultFfmpegPath[]         = "ffmpeg.exe";
#else
static const char DefaultFfmpegPath[]         = FFMPEG_PATH;
#endif
static const char DefaultFfmpegOptions[]      = "-y -f s16le -ac %C -ar %S -i %A -f rawvideo -r %R -s %F -i %V -f asf -acodec pcm_s16le -ac %C -vcodec msmpeg4v2 %O.asf";
#ifdef RECORDS_DIR
static const char DefaultRecordingDirectory[] = RECORDS_DIR;
#else
static const char DefaultRecordingDirectory[] = "records";
#endif
static const int  DefaultRecorderFrameWidth   = 704;
static const int  DefaultRecorderFrameHeight  = 576;
static const int  DefaultRecorderFrameRate    = 10;
static const int  DefaultRecorderSampleRate   = 16000;
static const int  DefaultRecorderAudioChans   = 1;

static const char ForceSplitVideoKey[]   = "Force split screen video (enables Room Control page)";

static const char H264LevelForSIPKey[]        = "H.264 Default Level for SIP";
#endif

static PString DefaultConnectingWAVFile = PString(SYS_RESOURCE_DIR)+"/connecting.wav";
static PString DefaultEnteringWAVFile   = PString(SYS_RESOURCE_DIR)+"/entering.wav";
static PString DefaultLeavingWAVFile    = PString(SYS_RESOURCE_DIR)+"/leaving.wav";

static const char ConnectingWAVFileKey[]  = "Connecting WAV File";
static const char EnteringWAVFileKey[]    = "Entering WAV File";
static const char LeavingWAVFileKey[]     = "Leaving WAV File";

static const char InterfaceKey[]          = "H.323 Listener";
static const char LocalUserNameKey[]      = "Local User Name";
static const char GatekeeperUserNameKey[] = "Gatekeeper Username";
static const char GatekeeperAliasKey[]    = "Gatekeeper Room Names";
static const char GatekeeperPasswordKey[] = "Gatekeeper Password";
static const char GatekeeperPrefixesKey[] = "Gatekeeper Prefixes";
static const char GatekeeperModeKey[]     = "Gatekeeper Mode";
static const char GatekeeperKey[]         = "Gatekeeper";
static const char DisableCodecsKey[]      = "Disable codecs - deprecated, use capability.conf instead!";
static const char NATRouterIPKey[]        = "NAT Router IP";
static const char NATTreatAsGlobalKey[]   = "Treat as global for NAT";
static const char DisableFastStartKey[]   = "Disable Fast-Start";
static const char DisableH245TunnelingKey[]="Disable H.245 Tunneling";
static const char RTPPortBaseKey[]        = "RTP Base Port";
static const char RTPPortMaxKey[]         = "RTP Max Port";
static const char DefaultProtocolKey[]    = "Default protocol for outgoing calls";

static const char RejectDuplicateNameKey[] = "Reject duplicate name";


#define DEFAULT_H323 0
#define DEFAULT_SIP  1


class MCUConfig: public PConfig
{
 public:
   MCUConfig(const PString & section)
    : PConfig(CONFIG_PATH, section){};
};

class MCUURL : public PURL
{
  public:
    //MCUURL();
    MCUURL(PString str);

    void SetDisplayName(PString name) { displayName = name; }

    const PString & GetDisplayName() const { return displayName; }
    const PString & GetUrl() const { return partyUrl; }
    const PString & GetUrlId() const { return urlId; }
    const PString & GetMemberFormatName() const { return memberName; }
    const PString GetPort() const { return PString(m_port); }
    const PString & GetSipProto() const { return sipProto; }

  protected:
    PString partyUrl;
    PString displayName;
    PString URLScheme;
    PString urlId;
    PString memberName;
    PString sipProto;
    
};

class MyProcess : public PHTTPServiceProcess                             
{                                                                        
  PCLASSINFO(MyProcess, PHTTPServiceProcess)                             
  public:                                                                
    MyProcess();                                                         
    ~MyProcess();                                                        
                                                                         
    virtual PBoolean Initialise(const char * initMsg);                   
    virtual void OnConfigChanged();                                      
    virtual void OnControl();                                            
    virtual PBoolean OnStart();                                          
    virtual void OnStop();                                               
    void Main();                                                         
                                                                         
    static MyProcess & Current()                                         
    {                                                                    
      return (MyProcess&)PProcess::Current();                            
    }                                                                    
    void CreateHTTPResource(const PString & name);                     
                                                                         
    //virtual ConferenceManager * CreateConferenceManager();
    //virtual OpenMCUH323EndPoint * CreateEndPoint(ConferenceManager & manager);

    //virtual void OnCreateConfigPage(PConfig & /*cfg*/, PConfigPage & /*page*/)
    //{ }

    PString GetDefaultRoomName() const { return defaultRoomName; }
    BOOL AreLoopbackCallsAllowed() const { return allowLoopbackCalls; }
    /*PString GetNewRoomNumber();*/
    void LogMessage(const PString & str);
    void LogMessageHTML(PString str);

    ConferenceManager & GetConferenceManager() { return * manager; }
 
    int GetRoomTimeLimit() const
    { return roomTimeLimit; }

    int GetHttpBuffer() const { return httpBuffer; }

    virtual void HttpWrite_(PString evt) {
      httpBufferMutex.Wait();
      httpBufferedEvents[httpBufferIndex]=evt; httpBufferIndex++;
      if(httpBufferIndex>=httpBuffer){ httpBufferIndex=0; httpBufferComplete=1; }
      httpBufferMutex.Signal();
    }
    virtual void HttpWriteEvent(PString evt) {
      PString evt0; PTime now;
      evt0 += now.AsString("h:mm:ss. ", PTime::Local) + evt;
      HttpWrite_(evt0+"<br>\n");
      if(copyWebLogToLog) LogMessageHTML(evt0);
    }
    virtual void HttpWriteEventRoom(PString evt, PString room){
      PString evt0; PTime now;
      evt0 += room + "\t" + now.AsString("h:mm:ss. ", PTime::Local) + evt;
      HttpWrite_(evt0+"<br>\n");
      if(copyWebLogToLog) LogMessageHTML(evt0);
    }
    virtual void HttpWriteCmdRoom(PString evt, PString room){
      PStringStream evt0;
      evt0 << room << "\t<script>p." << evt << "</script>\n";
      HttpWrite_(evt0);
    }
    virtual void HttpWriteCmd(PString evt){
      PStringStream evt0;
      evt0 << "<script>p." << evt << "</script>\n";
      HttpWrite_(evt0);
    }
    virtual PString HttpGetEvents(int &idx, PString room){
      PStringStream result;
      while (idx!=httpBufferIndex){
        PINDEX pos=httpBufferedEvents[idx].Find("\t",0);
        if(pos==P_MAX_INDEX)result << httpBufferedEvents[idx];
        else if(room=="")result << httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
        else if(httpBufferedEvents[idx].Left(pos)==room) result << httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
        idx++;
        if(idx>=httpBuffer)idx=0;
      }
      return result;
    }
    virtual PString HttpStartEventReading(int &idx, PString room){
      PStringStream result;
      if(httpBufferComplete){idx=httpBufferIndex+1; if(idx>httpBuffer)idx=0;} else idx=0;
      while (idx!=httpBufferIndex){
        if(httpBufferedEvents[idx].Find("<script>",0)==P_MAX_INDEX){
          PINDEX pos=httpBufferedEvents[idx].Find("\t",0);
          if(pos==P_MAX_INDEX)result << httpBufferedEvents[idx];
          else if(room=="")result << httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
          else if(httpBufferedEvents[idx].Left(pos)==room) result << httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
        }
        idx++;
        if(idx>=httpBuffer)idx=0;
      }
      return result;
    }
    PString GetHtmlCopyright()
    {
      PHTML html(PHTML::InBody);
      html << "Copyright &copy;"
       << m_compilationDate.AsString("yyyy") << " by "
       << PHTML::HotLink(m_copyrightHomePage + "\" target=\"_blank\"")
       << m_copyrightHolder;
      return html;
    }

#if OPENMCU_VIDEO
    static VideoMixConfigurator vmcfg;
    BOOL GetForceScreenSplit() const
    { return forceScreenSplit; }

    virtual MCUVideoMixer * CreateVideoMixer()
    { return new MCUSimpleVideoMixer(true); }

    virtual void RemovePreMediaFrame()
    { }

    virtual BOOL GetPreMediaFrame(void * buffer, int width, int height, PINDEX & amount)
    { return FALSE; }

    virtual BOOL GetEmptyMediaFrame(void * buffer, int width, int height, PINDEX & amount)
    { return GetPreMediaFrame(buffer, width, height, amount); }

#if USE_LIBYUV
    virtual libyuv::FilterMode GetScaleFilter(){ return scaleFilter; }
    virtual void SetScaleFilter(libyuv::FilterMode newScaleFilter){ scaleFilter=newScaleFilter; }
#endif

#endif



    static int defaultRoomCount;

    // video recorder
    PString    vr_ffmpegPath, vr_ffmpegOpts, vr_ffmpegDir;
    PString    ffmpegCall;
    int        vr_framewidth, vr_frameheight, vr_framerate;
    unsigned   vr_sampleRate, vr_audioChans, vr_minimumSpaceMiB;

    BOOL       recallRoomTemplate;

    int        h264DefaultLevelForSip;

    PFilePath GetLeavingWAVFile() const
    { return leavingWAVFile; }

    PFilePath GetEnteringWAVFile() const
    { return enteringWAVFile; }

    BOOL GetConnectingWAVFile(PFilePath & fn) const
    { fn = connectingWAVFile; return TRUE; }

    PFilePath connectingWAVFile;
    PFilePath enteringWAVFile;
    PFilePath leavingWAVFile;

    PFilePath  logFilename;
    BOOL       copyWebLogToLog;

    //PString GetEndpointParamFromUrl(PString param, PString url);

    PStringArray addressBook;
    PINDEX autoStartRecord, autoStopRecord;
    BOOL autoDeleteRoom;
    BOOL lockTplByDefault;
    BYTE defaultProtocol; // H.323 or SIP

  protected:
    int        currentLogLevel, currentTraceLevel;
    PFilePath executableFile;
    ConferenceManager * manager;
    OpalManager * m_manager;
    long GetCodec(const PString & codecname);


    PString    defaultRoomName;
    BOOL       allowLoopbackCalls;
    BOOL       traceFileRotated;

    int        httpBuffer, httpBufferIndex;
    PStringArray httpBufferedEvents;
    PMutex     httpBufferMutex;
    BOOL       httpBufferComplete;

    int        roomTimeLimit;

#if OPENMCU_VIDEO
    BOOL forceScreenSplit;
#if USE_LIBYUV
    libyuv::FilterMode scaleFilter;
#endif
#endif                                                               
                                                                                                                                 
                                                                   
                                                                         
    
};                                                                       

class ExternalVideoRecorderThread
{
  public:
    ExternalVideoRecorderThread(const PString & _roomName);
    ~ExternalVideoRecorderThread();

    unsigned state;

    PString roomName; // the only thing we need to identify the room
    PString fileName; // to replace %o
    //PPipeChannel ffmpegPipe;
};

#endif  //_MAIN_H

