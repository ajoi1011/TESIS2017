
#include "conference.h"
#include "html.h"
#include "config.h"

#if OPENMCU_VIDEO
#include <ptlib/vconvert.h>
#endif

PString JsQuoteScreen(PString s)
{
  PString r="\"";
  for(PINDEX i=0; i<s.GetLength(); i++)
  { BYTE c=(BYTE)s[i];
    if(c>31)
    { if     (c==0x22) r+="\\x22"; // "
      else if(c==0x5c) r+="\\x5c"; // backslash
      else if(c=='<') r+="&lt;";
      else if(c=='>') r+="&gt;";
      else r+=(char)c;
    }
    else
    {
      if(c==9) r+="&nbsp;|&nbsp;"; //tab
      if(c==10) if(r.Right(1)!=" ") r+=" ";
      if(c==13) if(r.Right(1)!=" ") r+=" ";
    }
  }
  r+="\"";
  return r;
}

// size of a PCM data packet, in samples
//#define PCM_PACKET_LEN          480
//#define PCM_PACKET_LEN          1920
#define PCM_BUFFER_LEN_MS /*ms */ 120

// size of a PCM data buffer, in bytes
//#define PCM_BUFFER_LEN          (PCM_PACKET_LEN * 2)

// number of PCM buffers to keep
#define PCM_BUFFER_COUNT        2

#define PCM_BUFFER_SIZE_CALC(freq,chans)\
  bufferSize = 2/* bytes*/ * chans * PCM_BUFFER_LEN_MS * PCM_BUFFER_COUNT * freq / 1000;\
  if(bufferSize < 4) bufferSize=200;\
  buffer.SetSize(bufferSize + 16);


//#define PCM_BUFFER_SIZE         (PCM_BUFFER_LEN * PCM_BUFFER_COUNT)

////////////////////////////////////////////////////////////////////////////////////

ConferenceManager::ConferenceManager(OpalManager & manager)
 : OpalMixerEndPoint(manager)
{
  maxConferenceCount = 0;
  monitor  = new ConferenceMonitor(*this);
}

ConferenceManager::~ConferenceManager()
{
  monitor->running = FALSE;
  monitor->WaitForTermination();
  delete monitor;
}


Conference * ConferenceManager::MakeAndLockConference(const PString & roomToCreate, const PString & name)
{
  PWaitAndSignal m(conferenceListMutex);
  OpalGloballyUniqueID conferenceID;
  ConferenceListType::const_iterator r;
  for (r = conferenceList.begin(); r != conferenceList.end(); ++r) {
    if (roomToCreate == r->second->GetNumber()) {
      conferenceID = r->second->GetID();
      break;
    }
  }

  return MakeAndLockConference(conferenceID, roomToCreate, name);
}

BOOL ConferenceManager::CheckAndLockConference(Conference * c)
{
  if(!c) return FALSE;
  conferenceListMutex.Wait();
  ConferenceListType::const_iterator r;
  for (r = conferenceList.begin(); r != conferenceList.end(); ++r)
  {
    if(r->second == c) return TRUE;
  }
  conferenceListMutex.Signal();
  return FALSE;
}

Conference * ConferenceManager::FindConferenceWithLock(const PString & n)
{
  if(n.IsEmpty()) return NULL;
  conferenceListMutex.Wait();
  ConferenceListType::const_iterator r;
  for (r = conferenceList.begin(); r != conferenceList.end(); ++r)
  {
    if(r->second->GetNumber() == n) return r->second;
  }
  conferenceListMutex.Signal();
  return NULL;
}

Conference * ConferenceManager::MakeAndLockConference(const OpalGloballyUniqueID & conferenceID, 
                                                                   const PString & roomToCreate, 
                                                                   const PString & name)
{
  conferenceListMutex.Wait();

  Conference * conference = NULL;
  BOOL newConference = FALSE;
  ConferenceListType::const_iterator r = conferenceList.find(conferenceID);
  if (r != conferenceList.end())
    conference = r->second;
  else {
    // create the conference
    conference = CreateConference(conferenceID, roomToCreate, name, mcuNumberMap.GetNumber(conferenceID));

    // insert conference into the map
    conferenceList.insert(ConferenceListType::value_type(conferenceID, conference));

    // set the conference count
    maxConferenceCount = PMAX(maxConferenceCount, (PINDEX)conferenceList.size());
    newConference = TRUE;
  }

  if (newConference)
    OnCreateConference(conference);

  return conference;
}

void ConferenceManager::OnCreateConference(Conference * conference)
{
  // set time limit, if there is one
  int timeLimit = MyProcess::Current().GetRoomTimeLimit();
  if (timeLimit > 0)
    monitor->AddMonitorEvent(new ConferenceTimeLimitInfo(conference->GetID(), PTime() + timeLimit*1000));

  if(MyProcess::Current().vr_minimumSpaceMiB) AddMonitorEvent( new CheckPartitionSpace(conference->GetID(), 30000));

  // add file recorder member    
#if ENABLE_TEST_ROOMS
  if((conference->GetNumber().Left(8) == "testroom") && (conference->GetNumber().GetLength() > 8)) return;
#endif
#if ENABLE_ECHO_MIXER
  if(conference->GetNumber().Left(4)*="echo") return;
#endif
  //conference->fileRecorder = new ConferenceFileMember(conference, (const PString) "recorder" , PFile::WriteOnly);

  if(conference->autoStartRecord==0) conference->StartRecorder(); // special case of start recording even if no participants currently connected

#if ENABLE_TEST_ROOMS
  if(conference->GetNumber() == "testroom") return;
#endif

  if(!MyProcess::Current().GetForceScreenSplit())
  { PTRACE(1,"Conference\tOnCreateConference: \"Force split screen video\" unchecked, " << conference->GetNumber() << " skipping members.conf"); return; }

  FILE *membLst;

  // read members.conf into conference->membersConf
  membLst = fopen(PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + "members_" + conference->GetNumber() + ".conf","rt");
  PStringStream membersConf;
  if(membLst!=NULL)
  { char buf [128];
    while(fgets(buf, 128, membLst)!=NULL) membersConf << buf;
    fclose(membLst);
  }

  conference->membersConf=membersConf;
  if(membersConf.Left(1)!="\n") membersConf="\n"+membersConf;

  // recall last template
  if(!MyProcess::Current().recallRoomTemplate) return;
  PINDEX dp=membersConf.Find("\nLAST_USED ");
  if(dp!=P_MAX_INDEX)
  { PINDEX dp2=membersConf.Find('\n',dp+10);
    if(dp2!=P_MAX_INDEX)
    { PString lastUsedTemplate=membersConf.Mid(dp+11,dp2-dp-11).Trim();
      PTRACE(4, "Extracting & loading last used template: " << lastUsedTemplate);
      conference->confTpl=conference->ExtractTemplate(lastUsedTemplate);
      conference->LoadTemplate(conference->confTpl);
    }
  }
}

void ConferenceManager::OnDestroyConference(Conference * conference)
{
  PString number = conference->GetNumber();

  PString jsName(number);
  jsName.Replace("\"","\\x27",TRUE,0); jsName.Replace("'","\\x22",TRUE,0);
  MyProcess::Current().HttpWriteCmdRoom("notice_deletion(1,'" + jsName + "')", number);

  conference->stopping=TRUE;

  PTRACE(2,"MCU\tOnDestroyConference() Cleaning out conference " << number);

// step 1: stop external video recorder:

  conference->StopRecorder();

// step 2: get the copy of memberList (because we will destroy the original):

  conference->GetMutex().Wait();
  Conference::MemberList theCopy(conference->GetMemberList());
  conference->GetMutex().Signal();

// step 3: disconnect remote endpoints:

  if(theCopy.size() > 0)
  for(Conference::MemberList::iterator r=theCopy.begin(); r!=theCopy.end(); ++r)
  {
    Conference::MemberList::iterator s; ConferenceMember * member;

    conference->GetMutex().Wait();
    if((s=conference->GetMemberList().find(r->first)) != conference->GetMemberList().end()) member=s->second; else member=NULL;
    if(member==NULL) { conference->GetMutex().Signal(); continue; }
    PString name=member->GetName();
    BOOL needsClose = !((name == "cache") || (name=="file recorder"));
    conference->GetMutex().Signal();

    member->SetConference(NULL); // prevent further attempts to read audio/video data from conference

    if(needsClose)
    {
      member->Close();
      r->second = NULL; // don't touch when will find caches
    }
  }

  MyProcess::Current().HttpWriteCmdRoom("notice_deletion(2,'" + jsName + "')", number);

// step 3.5: additinal check (linphone fails without it)
  PTRACE(3,"MCU\tOnDestroyConference() waiting for visibleMembersCount==0, up to 10 s");
  for(PINDEX i=0;i<100;i++) if(conference->GetVisibleMemberCount()==0) break; else PThread::Sleep(100);

// step 4: delete caches and file recorder:

  MyProcess::Current().HttpWriteCmdRoom("notice_deletion(3,'" + jsName + "')", number);

  if(theCopy.size() > 0)
  for(Conference::MemberList::iterator r=theCopy.begin(); r!=theCopy.end(); ++r)
  if(r->second != NULL)
  {
    if(r->second->GetName() == "cache")
    {
      //delete (ConferenceFileMember *)r->second; r->second=NULL;
      conference->GetMemberList().erase(r->first);
    }
    else 
    if(r->second->GetName() == "file recorder")
    {
      if(conference->fileRecorder != NULL)
      {
        delete conference->fileRecorder;
        conference->fileRecorder=NULL;
      }
      else
      {
        //delete (ConferenceFileMember *)r->second;
        r->second=NULL;
      }
      conference->GetMemberList().erase(r->first);
    }
  }

// step 5: all done. wait for empty member list (but not as long):

  MyProcess::Current().HttpWriteCmdRoom("notice_deletion(4,'" + jsName + "')", number);

  PTRACE(3,"MCU\tOnDestroyConference() Removal in progress. Waiting up to 10 s");

  for(PINDEX i=0;i<100;i++) if(conference->GetMemberList().size()==0) break; else PThread::Sleep(100);

  MyProcess::Current().HttpWriteCmdRoom("notice_deletion(5,'" + jsName + "')", number);
  PTRACE(3,"MCU\tOnDestroyConference() Removal done");
}


Conference * ConferenceManager::CreateConference(const OpalGloballyUniqueID & _guid,
                                                              const PString & _number,
                                                              const PString & _name,
                                                                          int _mcuNumber)
{ 
#if OPENMCU_VIDEO
#  if ENABLE_ECHO_MIXER
     if (_number.Left(4) *= "echo") return new Conference(*this, _guid, "echo"+_guid.AsString(), _name, _mcuNumber, new EchoVideoMixer());
#  endif
#  if ENABLE_TEST_ROOMS
     if (_number.Left(8) == "testroom")
     { PString number = _number; int count = 0;
       if (_number.GetLength() > 8)
       { count = _number.Mid(8).AsInteger(); if (count <= 0) { count = 0; number = "testroom"; } }
       if (count >= 0) return new Conference(*this, _guid, number, _name, _mcuNumber, new TestVideoMixer(count));
     }
#  endif
#endif

  //if(!MyProcess::Current().GetForceScreenSplit()) return new Conference(*this, _guid, _number, _name, _mcuNumber, NULL);

  PINDEX slashPos = _number.Find('/');
  PString number;
  if (slashPos != P_MAX_INDEX) number=_number.Left(slashPos);
  else number=_number;

  return new Conference(*this, _guid, number, _name, _mcuNumber
#if OPENMCU_VIDEO
                        ,MyProcess::Current().CreateVideoMixer()
#endif
                        ); 
}

BOOL ConferenceManager::HasConference(const OpalGloballyUniqueID & conferenceID, PString & number)
{
  PWaitAndSignal m(conferenceListMutex);
  ConferenceListType::const_iterator r = conferenceList.find(conferenceID);
  if (r == conferenceList.end())
    return FALSE;
  number = r->second->GetNumber();
  return TRUE;
}

BOOL ConferenceManager::HasConference(const PString & number, OpalGloballyUniqueID & conferenceID)
{
  PWaitAndSignal m(conferenceListMutex);
  ConferenceListType::const_iterator r;
  for (r = conferenceList.begin(); r != conferenceList.end(); ++r) {
    if (r->second->GetNumber() == number) {
      conferenceID = r->second->GetID();
      return TRUE;
    }
  }
  return FALSE;
}

void ConferenceManager::RemoveConference(const OpalGloballyUniqueID & confId)
{
  PTRACE(2, "RemoveConference\t3");
  conferenceListMutex.Wait();
  ConferenceListType::iterator r = conferenceList.find(confId);
  if(r == conferenceList.end()) { conferenceListMutex.Signal(); return; }
  Conference * conf = r->second;
  PAssert(conf != NULL, "Conference pointer is NULL");
  monitor->RemoveForConference(conf->GetID());
  OnDestroyConference(conf);
  conferenceList.erase(confId);
  mcuNumberMap.RemoveNumber(conf->GetMCUNumber());
  conferenceListMutex.Signal();
//  monitor->RemoveForConference(conf->GetID());
  PTRACE(2, "RemoveConference\t2");
  delete conf;
  PTRACE(2, "RemoveConference\t1");
}

void ConferenceManager::RemoveMember(const OpalGloballyUniqueID & confId, ConferenceMember * toRemove)
{
  PWaitAndSignal m(conferenceListMutex);
  Conference * conf = toRemove->GetConference();

  OpalGloballyUniqueID id = conf->GetID();  // make a copy of the ID because it may be about to disappear

  delete toRemove;
#if ENABLE_TEST_ROOMS
  if(conf->GetNumber().Left(8) == "testroom") if(!conf->GetVisibleMemberCount()) RemoveConference(id);
#endif
#if ENABLE_ECHO_MIXER
  if(conf->GetNumber().Left(4) *= "echo") RemoveConference(id);
#endif
  if(conf->autoDelete) if(!conf->GetVisibleMemberCount()) RemoveConference(id);
}

void ConferenceManager::AddMonitorEvent(ConferenceMonitorInfo * info)
{
  monitor->AddMonitorEvent(info);
}

///////////////////////////////////////////////////////////////

void ConferenceMonitor::Main()
{
  running = TRUE;
  for (;running;)
  {
    Sleep(1000);
    if (!running) break;

    PWaitAndSignal m(mutex);
    MonitorInfoList theCopy(monitorList);
    PTime now;
    for(MonitorInfoList::iterator r=theCopy.begin(), e=theCopy.end(); r!=e; ++r)
    {
      ConferenceMonitorInfo & info = **r;
      if (now < info.timeToPerform) continue;
      BOOL deleteAfterPerform = TRUE;
      {
        PWaitAndSignal m2(manager.GetConferenceListMutex());
        ConferenceListType & confList = manager.GetConferenceList();
        ConferenceListType::iterator s = confList.find(info.guid);
        if (s != confList.end()) deleteAfterPerform = info.Perform(*s->second);
      }
      if (deleteAfterPerform) monitorList.erase(r);
    }
  }
}

void ConferenceMonitor::AddMonitorEvent(ConferenceMonitorInfo * info)
{
  PWaitAndSignal m(mutex);
  monitorList.push_back(info);
}

void ConferenceMonitor::RemoveForConference(const OpalGloballyUniqueID & guid)
{
  PWaitAndSignal m(mutex);
  for(MonitorInfoList::iterator r = monitorList.begin(), e=monitorList.end(); r!=e;)
  {
    ConferenceMonitorInfo & info = **r;
    if (info.guid != guid) { ++r; continue; }
    delete *r;
    monitorList.erase(r);
  }
}

BOOL ConferenceTimeLimitInfo::Perform(Conference & conference)
{
  Conference::MemberList & list = conference.GetMemberList();
  Conference::MemberList::iterator r;
  for (r = list.begin(); r != list.end(); ++r)
    r->second->Close();
  return TRUE;
}

BOOL ConferenceRepeatingInfo::Perform(Conference & conference)
{
  this->timeToPerform = PTime() + repeatTime;
  return FALSE;
}


BOOL ConferenceMCUCheckInfo::Perform(Conference & conference)
{
  // see if any member of this conference is a not an MCU
  Conference::MemberList & list = conference.GetMemberList();
  Conference::MemberList::iterator r;
  for (r = list.begin(); r != list.end(); ++r)
    if (!r->second->IsMCU())
      break;

  // if there is any non-MCU member, check again later
  if (r != list.end())
    return ConferenceRepeatingInfo::Perform(conference);

  // else shut down the conference
  for (r = list.begin(); r != list.end(); ++r)
    r->second->Close();

  return TRUE;
}

BOOL CheckPartitionSpace::Perform(Conference & conference)
{
  if(!conference.RecorderCheckSpace()) conference.StopRecorder();
  return FALSE;
}

PString ConferenceManager::GetConferenceOptsJavascript(Conference & c)
{
  PStringStream r; //conf[0]=[videoMixerCount,bfw,bfh):
  PString jsRoom=c.GetNumber(); jsRoom.Replace("&","&amp;",TRUE,0); jsRoom.Replace("\"","&quot;",TRUE,0);
  r << "conf=Array(Array(" //l1&l2 open
    << c.videoMixerCount                                                // [0][0]  = mixerCount
    << "," << MyProcess::vmcfg.bfw                                        // [0][1]  = base frame width
    << "," << MyProcess::vmcfg.bfh                                        // [0][2]  = base frame height
    << ",\"" << jsRoom << "\""                                          // [0][3]  = room name
    << ",'" << c.IsModerated() << "'"                                   // [0][4]  = control
    << ",'" << c.IsMuteUnvisible() << "'"                               // [0][5]  = global mute
    << "," << c.VAlevel << "," << c.VAdelay << "," << c.VAtimeout       // [0][6-8]= vad

    << ",Array("; // l3 open
      GetConferenceListMutex().Wait();
      ConferenceListType & conferenceList = GetConferenceList();
      ConferenceListType::iterator l;
      for (l = conferenceList.begin(); l != conferenceList.end(); ++l) {
        jsRoom=(*(l->second)).GetNumber();
        jsRoom.Replace("&","&amp;",TRUE,0); jsRoom.Replace("\"","&quot;",TRUE,0);
        if(l!=conferenceList.begin()) r << ",";                         // [0][9][ci][0-2] roomName & memberCount & isModerated
        r << "Array(\"" << jsRoom << "\"," << (*(l->second)).GetVisibleMemberCount() << ",\"" << (*(l->second)).IsModerated() << "\")";
      }
      GetConferenceListMutex().Signal();
    r << ")"; // l3 close

#if USE_LIBYUV
    r << "," << MyProcess::Current().GetScaleFilter();                      // [0][10] = libyuv resizer filter mode
#else
    r << ",-1";
#endif

  if(c.externalRecorder != NULL) r << ",1"; else r << ",0";               // [0][11] = external video recording state (1=recording, 0=NO)
  if(c.lockedTemplate) r << ",1"; else r << ",0";                         // [0][12] = member list locked by template (1=yes, 0=no)

  r << ")"; //l2 close

  PWaitAndSignal m(c.videoMixerListMutex);
  Conference::VideoMixerRecord * vmr = c.videoMixerList;
  while (vmr!=NULL) {
    r << ",Array("; //l2 open
    MCUVideoMixer * mixer = vmr->mixer;
    unsigned n=mixer->GetPositionSet();
    VMPCfgSplitOptions & split=MyProcess::vmcfg.vmconf[n].splitcfg;
    VMPCfgOptions      * p    =MyProcess::vmcfg.vmconf[n].vmpcfg;


    r << "Array(" //l3 open                                             // conf[n][0]: base parameters:
      << split.mockup_width << "," << split.mockup_height                 // [n][0][0-1]= mw*mh
      << "," << n                                                         // [n][0][2]  = position set (layout)
    << "),Array("; //l3 reopen

    for(unsigned i=0;i<split.vidnum;i++)
      r << "Array(" << p[i].posx //l4 open                              // conf[n][1]: frame geometry for each position i:
        << "," << p[i].posy                                               // [n][1][i][0-1]= posx & posy
        << "," << p[i].width                                              // [n][1][i][2-3]= width & height
        << "," << p[i].height
        << "," << p[i].border                                             // [n][1][i][4]  = border
      << ")" << ((i==split.vidnum-1)?"":","); //l4 close

    r << ")," << mixer->VMPListScanJS() //l3 close                      // conf[n][2], conf[n][3]: members' ids & types
    << ")"; //l2 close

    vmr=vmr->next;
  }
  r << ");"; //l1 close
  return r;
}

PString ConferenceManager::GetMemberListOptsJavascript(Conference & conference)
{
 PStringStream members;
 PWaitAndSignal m(conference.GetMutex());
 Conference::MemberNameList & memberNameList = conference.GetMemberNameList();
 Conference::MemberNameList::const_iterator s;
 members << "members=Array(";
 int i=0;
 for (s = memberNameList.begin(); s != memberNameList.end(); ++s) 
 {
  PString username=s->first;
  username.Replace("&","&amp;",TRUE,0);
  username.Replace("\"","&quot;",TRUE,0);
  ConferenceMember * member = s->second;
  if(member==NULL){ // inactive member
    if(i>0) members << ",";
    members << "Array(0"
      << ",0"
      << ",\"" << username << "\""
      << ",0"
      << ",0"
      << ",0"
      << ",0"
      << ",0"
      //<< ",\"" << MCUURL(s->first).GetUrlId() << "\""
      << ")";
    i++;
  } else {          //   active member
    if(i>0) members << ",";
    members << "Array(1"                                // [i][ 0] = 1 : ONLINE
      << ",\"" << dec << (long)member->GetID() << "\""  // [i][ 1] = long id
      << ",\"" << username << "\""                      // [i][ 2] = name [ip]
      << "," << member->muteMask                        // [i][ 3] = mute
      << "," << member->disableVAD                      // [i][ 4] = disable vad
      << "," << member->chosenVan                       // [i][ 5] = chosen van
      << "," << member->GetAudioLevel()                 // [i][ 6] = audiolevel (peak)
      << "," << member->GetVideoMixerNumber()           // [i][ 7] = number of mixer member receiving
      << ",\"" << MCUURL(s->first).GetUrlId() << "\""   // [i][ 8] = URL
      << "," << (unsigned short)member->channelCheck    // [i][ 9] = RTP channels checking bit mask 0000vVaA
      << "," << member->kManualGainDB                   // [i][10] = Audio level gain for manual tune, integer: -20..60
      << "," << member->kOutputGainDB                   // [i][11] = Output audio gain, integer: -20..60
      << ")";
    i++;
  }
 }
 members << ");";

 members << "\np.addressbook=Array(";
 PStringArray abook = MyProcess::Current().addressBook;
 for(PINDEX i = 0; i < abook.GetSize(); i++)
 {
   if(i>0) members << ",";
   PString username = abook[i];
   username.Replace("&","&amp;",TRUE,0);
   username.Replace("\"","&quot;",TRUE,0);
   members << "Array("
      << "0"
      << ",\"" << MCUURL(abook[i]).GetUrlId() << "\""
      << ",\"" << username << "\""
      << ")";
 }
 members << ");";

 return members;
}

PString ConferenceManager::OTFControl(const PString room, const PStringToString & data)
{
  PTRACE(6,"WebCtrl\tRoom " << room << ", action " << data("action") << ", value " << data("v") << ", options " << data("o") << ", " << data("o2") << ", " << data("o3"));

  if(!data.Contains("action")) return "FAIL"; int action=data("action").AsInteger();

  if(!data.Contains("v")) return "FAIL"; PString value=data("v");
  long v = value.AsInteger();

#if USE_LIBYUV
  if(action == OTFC_YUV_FILTER_MODE)
  {
    if(v==1) MyProcess::Current().SetScaleFilter(libyuv::kFilterBilinear);
    else if(v==2) MyProcess::Current().SetScaleFilter(libyuv::kFilterBox);
    else MyProcess::Current().SetScaleFilter(libyuv::kFilterNone);
    PStringStream cmd;
    cmd << "conf[0][10]=" << v;
    MyProcess::Current().HttpWriteCmdRoom(cmd,room);
    MyProcess::Current().HttpWriteCmdRoom("top_panel()",room);
    MyProcess::Current().HttpWriteCmdRoom("alive()",room);
    return "OK";
  }
#endif

#define OTF_RET_OK { UnlockConference(); return "OK"; }
#define OTF_RET_FAIL { UnlockConference(); return "FAIL"; }

  Conference * conference = FindConferenceWithLock(room);
  if(conference == NULL) return "FAIL"; // todo: 404 (?)

  /*if(action == OTFC_REFRESH_VIDEO_MIXERS)
  {
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("mixrfr()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_VIDEO_RECORDER_START)
  { conference->StartRecorder(); OTF_RET_OK;
  }
  if(action == OTFC_VIDEO_RECORDER_STOP)
  { conference->StopRecorder(); OTF_RET_OK;
  }
  if(action == OTFC_TEMPLATE_RECALL)
  {
    MyProcess::Current().HttpWriteCmdRoom("alive()",room);

    if(conference->IsModerated()=="-")
    { conference->SetModerated(TRUE);
      conference->videoMixerListMutex.Wait();
      conference->videoMixerList->mixer->SetForceScreenSplit(TRUE);
      conference->videoMixerListMutex.Signal();
      conference->PutChosenVan();
      MyProcess::Current().HttpWriteEventRoom("<span style='background-color:#bfb'>Operator took the control</span>",room);
      MyProcess::Current().HttpWriteCmdRoom("r_moder()",room);
    }

    conference->confTpl = conference->ExtractTemplate(value);
    conference->LoadTemplate(conference->confTpl);
    conference->SetLastUsedTemplate(value);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      MyProcess::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_SAVE_TEMPLATE)
  {
    MyProcess::Current().HttpWriteCmdRoom("alive()",room);
    PString templateName=value.Trim();
    if(templateName=="") OTF_RET_FAIL;
    if(templateName.Right(1) == "*") templateName=templateName.Left(templateName.GetLength()-1).RightTrim();
    if(templateName=="") OTF_RET_FAIL;
    conference->confTpl = conference->SaveTemplate(templateName);
    conference->TemplateInsertAndRewrite(templateName, conference->confTpl);
    conference->LoadTemplate(conference->confTpl);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      MyProcess::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_DELETE_TEMPLATE)
  {
    MyProcess::Current().HttpWriteCmdRoom("alive()",room);
    PString templateName=value.Trim();
    if(templateName=="") OTF_RET_FAIL;
    if(templateName.Right(1) == "*") OTF_RET_FAIL;
    conference->DeleteTemplate(templateName);
    conference->LoadTemplate("");
    conference->SetLastUsedTemplate("");
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      MyProcess::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_TOGGLE_TPL_LOCK)
  {
    PString templateName=value.Trim();
    if((templateName.IsEmpty())||(templateName.Right(1) == "*")) if(!conference->lockedTemplate) OTF_RET_FAIL;
    conference->lockedTemplate = !conference->lockedTemplate;
    if(conference->lockedTemplate) MyProcess::Current().HttpWriteCmdRoom("tpllck(1)",room);
    else MyProcess::Current().HttpWriteCmdRoom("tpllck(0)",room);
    OTF_RET_OK;
  }
  if(action == OTFC_INVITE)
  { conference->InviteMember(value); OTF_RET_OK; }
  if(action == OTFC_ADD_AND_INVITE)
  {
    conference->AddOfflineMemberToNameList(value);
    conference->InviteMember(value);
    OTF_RET_OK;
  }
  if(action == OTFC_REMOVE_OFFLINE_MEMBER)
  {
    conference->RemoveOfflineMemberFromNameList(value);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p.members_refresh()";
    MyProcess::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_DROP_ALL_ACTIVE_MEMBERS)
  {
    conferenceManager.UnlockConference();
    PWaitAndSignal m(conference->GetMutex());
    Conference::MemberList & memberList = conference->GetMemberList();
    Conference::MemberList::iterator r;
    for (r = memberList.begin(); r != memberList.end(); ++r)
    {
      ConferenceMember * member = r->second;
      if(member->GetName()=="file recorder") continue;
      if(member->GetName()=="cache") continue;
      member->Close();
//      memberList.erase(r->first);
    }
    MyProcess::Current().HttpWriteEventRoom("Active members dropped by operator",room);
    MyProcess::Current().HttpWriteCmdRoom("drop_all()",room);
    return "OK";
  }
  if((action == OTFC_MUTE_ALL)||(action == OTFC_UNMUTE_ALL))
  {
    conferenceManager.UnlockConference();
    BOOL newValue = (action==OTFC_MUTE_ALL);
    PWaitAndSignal m(conference->GetMutex());
    Conference::MemberList & memberList = conference->GetMemberList();
    Conference::MemberList::iterator r;
    H323Connection_ConferenceMember * connMember;
    for (r = memberList.begin(); r != memberList.end(); ++r)
    {
      ConferenceMember * member = r->second;
      if(member->GetName()=="file recorder") continue;
      if(member->GetName()=="cache") continue;
      connMember = NULL;
      connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
      if(connMember==NULL) continue;
      if(newValue)connMember->SetChannelPauses  (1);
      else        connMember->UnsetChannelPauses(1);
    }
    return "OK";
  }
  if(action == OTFC_INVITE_ALL_INACT_MMBRS)
  { Conference::MemberNameList & memberNameList = conference->GetMemberNameList();
    Conference::MemberNameList::const_iterator r;
    for (r = memberNameList.begin(); r != memberNameList.end(); ++r) if(r->second==NULL) conference->InviteMember(r->first);
    OTF_RET_OK;
  }
  if(action == OTFC_REMOVE_ALL_INACT_MMBRS)
  { Conference::MemberNameList & memberNameList = conference->GetMemberNameList();
    Conference::MemberNameList::const_iterator r;
    for (r = memberNameList.begin(); r != memberNameList.end(); ++r) if(r->second==NULL) conference->RemoveOfflineMemberFromNameList((PString &)(r->first));
    MyProcess::Current().HttpWriteEventRoom("Offline members removed by operator",room);
    MyProcess::Current().HttpWriteCmdRoom("remove_all()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_SAVE_MEMBERS_CONF)
  { FILE *membLst;
    PString name="members_"+room+".conf";
    membLst = fopen(name,"w");
    if(membLst==NULL) { MyProcess::Current().HttpWriteEventRoom("<font color=red>Error: Can't save member list</font>",room); OTF_RET_FAIL; }
    fputs(conference->membersConf,membLst);
    fclose(membLst);
    MyProcess::Current().HttpWriteEventRoom("Member list saved",room);
    OTF_RET_OK;
  }*/
  if(action == OTFC_TAKE_CONTROL)
  { if(conference->IsModerated()=="-")
    { conference->SetModerated(true);
      conference->videoMixerListMutex.Wait();
      conference->videoMixerList->mixer->SetForceScreenSplit(true);
      conference->videoMixerListMutex.Signal();
      conference->PutChosenVan();
      MyProcess::Current().HttpWriteEventRoom("<span style='background-color:#bfb'>Operator took the control</span>",room);
      MyProcess::Current().HttpWriteCmdRoom("r_moder()",room);
    }
    OTF_RET_OK;
  }
  if(action == OTFC_DECONTROL)
  { if(conference->IsModerated()=="+")
    { conference->SetModerated(FALSE);
      {
        PWaitAndSignal m(conference->videoMixerListMutex);
        if(!conference->videoMixerList) OTF_RET_FAIL;
        if(!conference->videoMixerList->mixer) OTF_RET_FAIL;
        conference->videoMixerList->mixer->SetForceScreenSplit(true);
      }
      UnlockConference();  // we have to UnlockConference
      UnmoderateConference(*conference);  // before conference.GetMutex() usage
      MyProcess::Current().HttpWriteEventRoom("<span style='background-color:#acf'>Operator resigned</span>",room);
      MyProcess::Current().HttpWriteCmdRoom("r_unmoder()",room);
      MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
      MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
      return "OK";
    }
    OTF_RET_OK;
  }
  if(action == OTFC_ADD_VIDEO_MIXER)
  { if(conference->IsModerated()=="+" && conference->GetNumber() != "testroom")
    { unsigned n = conference->VMLAdd();
      PStringStream msg; msg << "Video mixer " << n << " added";
      MyProcess::Current().HttpWriteEventRoom(msg,room);
      MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
      MyProcess::Current().HttpWriteCmdRoom("mmw=-1;p.build_page()",room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }
  /*if(action == OTFC_DELETE_VIDEO_MIXER)
  { if(conference->IsModerated()=="+" && conference->GetNumber() != "testroom")
    {
      unsigned n_old=conference->videoMixerCount;
      unsigned n = conference->VMLDel(v);
      if(n_old!=n)
      { PStringStream msg; msg << "Video mixer " << v << " removed";
        MyProcess::Current().HttpWriteEventRoom(msg,room);
        MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
        MyProcess::Current().HttpWriteCmdRoom("mmw=-1;p.build_page()",room);
        OTF_RET_OK;
      }
    }
    OTF_RET_FAIL;
  }
  */if (action == OTFC_SET_VIDEO_MIXER_LAYOUT)
  { unsigned option = data("o").AsInteger();
    MCUVideoMixer * mixer = conference->VMLFind(option);
    if(mixer!=NULL)
    { mixer->MyChangeLayout(v);
      conference->PutChosenVan();
      conference->FreezeVideo(NULL);
      MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
      MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }
  /*if (action == OTFC_REMOVE_VMP)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    unsigned pos = data("o").AsInteger();
    mixer->MyRemoveVideoSource(pos,TRUE);
    conference->FreezeVideo(NULL);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_ARRANGE_VMP)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    Conference::MemberList & memberList = conference->GetMemberList();
    for (Conference::MemberList::const_iterator r = memberList.begin(); r != memberList.end(); ++r)
    if(r->second != NULL) if(r->second->IsVisible())
    { if (mixer->AddVideoSourceToLayout(r->second->GetID(), *(r->second))) r->second->SetFreezeVideo(FALSE); else break; }
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_CLEAR)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->MyRemoveAllVideoSource();
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SHUFFLE_VMP)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Shuffle();
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SCROLL_LEFT)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Scroll(TRUE);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SCROLL_RIGHT)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Scroll(FALSE);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_REVERT)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Revert();
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_GLOBAL_MUTE)
  { if(data("v")=="true")v=1; if(data("v")=="false") v=0; conference->SetMuteUnvisible((BOOL)v);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_SET_VAD_VALUES)
  { conference->VAlevel   = (unsigned short int) v;
    conference->VAdelay   = (unsigned short int) (data("o").AsInteger());
    conference->VAtimeout = (unsigned short int) (data("o2").AsInteger());
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if (action == OTFC_MOVE_VMP)
  { MCUVideoMixer * mixer1 = conference->VMLFind(v); if(mixer1==NULL) OTF_RET_FAIL;
    if(!data.Contains("o2")) OTF_RET_FAIL; MCUVideoMixer * mixer2=conference->VMLFind(data("o2").AsInteger()); if(mixer2==NULL) OTF_RET_FAIL;
    int pos1 = data("o").AsInteger(); int pos2 = data("o3").AsInteger();
    if(mixer1==mixer2) mixer1->Exchange(pos1,pos2);
    else
    {
      ConferenceMemberId id = mixer1->GetHonestId(pos1); if(((long)id<100)&&((long)id>=0)) id=NULL;
      ConferenceMemberId id2 = mixer2->GetHonestId(pos2); if(((long)id2<100)&&((long)id2>=0)) id2=NULL;
      mixer2->PositionSetup(pos2, 1, GetConferenceMemberById(conference, (long)id));
      mixer1->PositionSetup(pos1, 1, GetConferenceMemberById(conference, (long)id2));
    }
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if (action == OTFC_VAD_CLICK)
  { MCUVideoMixer * mixer = conference->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    unsigned pos = data("o").AsInteger();
    int type = data("o2").AsInteger();
    if((type<1)||(type>3)) type=2;
    long id = (long)mixer->GetHonestId(pos);
    if((type==1)&&(id>=0)&&(id<100)) //static but no member
    {
      Conference::MemberList & memberList = conference->GetMemberList();
      Conference::MemberList::const_iterator r;
      for (r = memberList.begin(); r != memberList.end(); ++r)
      {
        if(r->second != NULL)
        {
          if(r->second->IsVisible())
          {
            if (mixer->VMPListFindVMP(r->second->GetID())==NULL)
            {
              mixer->PositionSetup(pos, 1, r->second);
              r->second->SetFreezeVideo(FALSE);
              break;
            }
          }
        }
      }
      if(r==memberList.end()) mixer->PositionSetup(pos,1,NULL);
    }
    else if((id>=0)&&(id<100)) mixer->PositionSetup(pos,type,NULL);
    else mixer->SetPositionType(pos,type);
    conference->FreezeVideo(NULL);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }

  ConferenceMember * member = GetConferenceMemberById(conference, v); if(member==NULL) OTF_RET_FAIL;
  PStringStream cmd;

  if( action == OTFC_SET_VMP_STATIC )
  { unsigned n=data("o").AsInteger(); MCUVideoMixer * mixer = conference->VMLFind(n); if(mixer==NULL) OTF_RET_FAIL;
    int pos = data("o2").AsInteger(); mixer->PositionSetup(pos, 1, member);
    H323Connection_ConferenceMember *connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
    if(connMember==NULL) OTF_RET_FAIL;
    connMember->SetFreezeVideo(FALSE);
    MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if( action == OTFC_AUDIO_GAIN_LEVEL_SET )
  {
    int n=data("o").AsInteger();
    if(n<0) n=0;
    if(n>80) n=80;
    member->kManualGainDB=n-20;
    member->kManualGain=(float)pow(10.0,((float)member->kManualGainDB)/20.0);
    cmd << "setagl(" << v << "," << member->kManualGainDB << ")";
    MyProcess::Current().HttpWriteCmdRoom(cmd,room); OTF_RET_OK;
  }
  if( action == OTFC_OUTPUT_GAIN_SET )
  {
    int n=data("o").AsInteger();
    if(n<0) n=0;
    if(n>80) n=80;
    member->kOutputGainDB=n-20;
    member->kOutputGain=(float)pow(10.0,((float)member->kOutputGainDB)/20.0);
    cmd << "setogl(" << v << "," << member->kOutputGainDB << ")";
    MyProcess::Current().HttpWriteCmdRoom(cmd,room); OTF_RET_OK;
  }
  if(action == OTFC_MUTE)
  {
    H323Connection_ConferenceMember * connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
    if(connMember) connMember->SetChannelPauses(data("o").AsInteger());
    OTF_RET_OK;
  }
  if(action == OTFC_UNMUTE)
  {
    H323Connection_ConferenceMember * connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
    if(connMember) connMember->UnsetChannelPauses(data("o").AsInteger());
    OTF_RET_OK;
  }
  if( action == OTFC_REMOVE_FROM_VIDEOMIXERS)
  { if(conference->IsModerated()=="+" && conference->GetNumber() != "testroom")
    {
      PWaitAndSignal m(conference->videoMixerListMutex);
      Conference::VideoMixerRecord * vmr = conference->videoMixerList;
      while( vmr!=NULL ) 
      { MCUVideoMixer * mixer = vmr->mixer;
        int oldPos = mixer->GetPositionNum(member->GetID());
        if(oldPos != -1) mixer->MyRemoveVideoSource(oldPos, TRUE);
        vmr=vmr->next;
      }
      H323Connection_ConferenceMember *connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
      if(connMember!=NULL) connMember->SetFreezeVideo(TRUE);
      MyProcess::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
      MyProcess::Current().HttpWriteCmdRoom("build_page()",room);
      OTF_RET_OK;
    }
  }
  if( action == OTFC_DROP_MEMBER )
  {
    // MAY CAUSE DEADLOCK PWaitAndSignal m(conference->GetMutex();
//    conference->GetMemberList().erase(member->GetID());
    member->Close();
    OTF_RET_OK;
  }
  if (action == OTFC_VAD_NORMAL)
  {
    member->disableVAD=FALSE;
    member->chosenVan=FALSE;
    conferenceManager.UnlockConference();
    conference->PutChosenVan();
    cmd << "ivad(" << v << ",0)";
    MyProcess::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_VAD_CHOSEN_VAN)
  {
    member->disableVAD=FALSE;
    member->chosenVan=TRUE;
    conferenceManager.UnlockConference();
    conference->PutChosenVan();
    conference->FreezeVideo(member->GetID());
    cmd << "ivad(" << v << ",1)";
    MyProcess::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_VAD_DISABLE_VAD)
  {
    member->disableVAD=TRUE;
    member->chosenVan=FALSE;
    conferenceManager.UnlockConference();
    conference->PutChosenVan();
#if 1 // DISABLING VAD WILL CAUSE MEMBER REMOVAL FROM VAD POSITIONS
    {
      PWaitAndSignal m(conference->GetMutex());
      ConferenceMemberId id = member->GetID();
      conference->videoMixerListMutex.Wait();
      Conference::VideoMixerRecord * vmr = conference->videoMixerList;
      while(vmr!=NULL)
      {
        int type = vmr->mixer->GetPositionType(id);
        if(type<2 || type>3) { vmr=vmr->next; continue;} //-1:not found, 1:static, 2&3:VAD
        vmr->mixer->MyRemoveVideoSourceById(id, FALSE);
        vmr = vmr->next;
      }
      conference->videoMixerListMutex.Signal();
      conference->FreezeVideo(id);
    }
#endif
    cmd << "ivad(" << v << ",2)";
    MyProcess::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_SET_MEMBER_VIDEO_MIXER)
  { unsigned option = data("o").AsInteger();
    if(SetMemberVideoMixer(*conference,member,option))
    { cmd << "chmix(" << v << "," << option << ")";
      MyProcess::Current().HttpWriteCmdRoom(cmd,room);
      OTF_RET_OK;
    }
    if(SetMemberVideoMixer(*conference,member,0)) // rotate back to 0
    { cmd << "chmix(" << v << ",0)";
      MyProcess::Current().HttpWriteCmdRoom(cmd,room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }*/

  OTF_RET_FAIL;
}

PString ConferenceManager::RoomCtrlPage(const PString room, BOOL ctrl, int n, Conference & conference, ConferenceMemberId *idp)
{
// int i,j;
 PStringStream page, meta;

 BeginPage(page,meta,"Room Control","window.l_control","window.l_info_control");

 page << "<script src=\"control.js\"></script>";

 page << "<div id='cb1' name='cb1' class='input-append'>&nbsp;</div>"
   << "<div style='text-align:center'>"
     << "<div id='cb2' name='cb2' style='margin-left:auto;margin-right:auto;width:100px;height:100px;background-color:#ddd'>"
       << "<div id='logging0' style='position:relative;top:0px;left:-50px;width:0px;height:0px'>"
         << "<div id='logging1' style='position:absolute;width:50px;height:50px'>"
           << "<iframe style='background-color:#eef;border:1px solid #55c;padding:0px;margin:0px' id='loggingframe' name='loggingframe' src='Comm?room=" << room << "' width=50 height=50>"
             << "Your browser does not support IFRAME tag :("
           << "</iframe>"
         << "</div>"
       << "</div>"
       << "<div id='cb3' name='cb3' style='position:relative;top:0px;left:0px;width:0px;height:0px'>"
         << "&nbsp;"
       << "</div>"
     << "</div>"
   << "</div>"
 ;
 EndPage(page,MyProcess::Current().GetHtmlCopyright());
 return page;
}


void ConferenceManager::UnmoderateConference(Conference & conference)
{
  conference.videoMixerListMutex.Wait();
  MCUVideoMixer * mixer = NULL;
  if(conference.videoMixerList!=NULL) mixer = conference.videoMixerList->mixer;
  if(mixer==NULL)
  {
    conference.videoMixerListMutex.Signal();
    return;
  }
  mixer->MyRemoveAllVideoSource();
  conference.videoMixerListMutex.Signal();

  conference.GetMutex().Wait();
  Conference::MemberList & memberList = conference.GetMemberList();
  Conference::MemberList::const_iterator s;
  for (s = memberList.begin(); s != memberList.end(); ++s) 
  {
    ConferenceMember * member = s->second;
    //if(member->GetName()!="cache") SetMemberVideoMixer(conference, member, 0);
    if(!member->IsVisible()) continue;
    if(mixer->AddVideoSource(member->GetID(), *member)) member->SetFreezeVideo(FALSE);
    else member->SetFreezeVideo(TRUE);
  }
  conference.GetMutex().Signal();

  conference.videoMixerListMutex.Wait();
  while(conference.videoMixerCount>1) conference.VMLDel(conference.videoMixerCount-1);
  conference.videoMixerListMutex.Signal();
}


PString ConferenceManager::SetRoomParams(const PStringToString & data)
{
  PString room = data("room");

  // "On-the-Fly" Control via XMLHTTPRequest:
  if(data.Contains("otfc")) return OTFControl(room, data);

  if(data.Contains("refresh")) // JavaScript data refreshing
  {
    PTRACE(6,"WebCtrl\tJS refresh");
    ConferenceListType::iterator r;
    PWaitAndSignal m(GetConferenceListMutex());
    ConferenceListType & conferenceList = GetConferenceList();
    for (r = conferenceList.begin(); r != conferenceList.end(); ++r) if(r->second->GetNumber() == room) break;
    if(r == conferenceList.end() ) return "";
    Conference & conference = *(r->second);
    return GetMemberListOptsJavascript(conference);
  }

  MyProcess::Current().HttpWriteEventRoom("MCU Operator connected",room);
  PTRACE(6,"WebCtrl\tOperator connected");
  ConferenceListType::iterator r;
  PWaitAndSignal m(GetConferenceListMutex());
  ConferenceListType & conferenceList = GetConferenceList();
  for (r = conferenceList.begin(); r != conferenceList.end(); ++r) if(r->second->GetNumber() == room) break;
  if(r == conferenceList.end() ) return "MyProcess.ru: Bad room";
  Conference & conference = *(r->second);
  PWaitAndSignal m2(conference.videoMixerListMutex);
  MCUVideoMixer * mixer = conference.videoMixerList->mixer;
  ConferenceMemberId idr[100];
  for(int i=0;i<100;i++) idr[i]=mixer->GetPositionId(i);
  return RoomCtrlPage(room,conference.IsModerated()=="+",mixer->GetPositionSet(),conference,idr);
}


////////////////////////////////////////////////////////////////////////////////////

Conference::Conference(        ConferenceManager & _manager,
                       const OpalGloballyUniqueID & _guid,
                                    const PString & _number,
                                    const PString & _name,
                                                int _mcuNumber
#if OPENMCU_VIDEO
                                    ,MCUVideoMixer * _videoMixer
#endif
)
  : manager(_manager), guid(_guid), number(_number), name(_name), mcuNumber(_mcuNumber), mcuMonitorRunning(FALSE)
{ 
  stopping=FALSE;
#if OPENMCU_VIDEO
  VMLInit(_videoMixer);
#endif
  maxMemberCount = 0;
  moderated = false;
  muteUnvisible = FALSE;
  VAdelay = 1000;
  VAtimeout = 10000;
  VAlevel = 100;
  echoLevel = 0;
  vidmembernum = 0;
  fileRecorder = NULL;
  externalRecorder=NULL;
  autoDelete = 
    MyProcess::Current().autoDeleteRoom
#   if ENABLE_ECHO_MIXER
      || (number.Left(4) *= "echo")
#   endif
#   if ENABLE_TEST_ROOMS
      || (number.Left(8) == "testroom")
#   endif
  ;
  autoStartRecord=MyProcess::Current().autoStartRecord;
  autoStopRecord=MyProcess::Current().autoStopRecord;
  lockedTemplate=MyProcess::Current().lockTplByDefault;
  PTRACE(3, "Conference\tNew conference started: ID=" << guid << ", number = " << number);
}

Conference::~Conference()
{
#if OPENMCU_VIDEO
  VMLClear();
#endif
}

BOOL Conference::RecorderCheckSpace()
{
  PDirectory pd(MyProcess::Current().vr_ffmpegDir);
  PInt64 t, f;
  DWORD cs;
  if(!pd.GetVolumeSpace(t, f, cs))
  {
    PTRACE(1,"EVRT\tRecorder space check failed");
    return TRUE;
  }
  BOOL result = ((f>>20) >= MyProcess::Current().vr_minimumSpaceMiB);
  if(!result) MyProcess::Current().HttpWriteEvent("<b><font color='red'>Insufficient disk space</font>: Video Recorder DISABLED</b>");
  PTRACE_IF(1,!result,"EVRT\tInsufficient disk space: Video Recorder DISABLED");
  return result;
}

void Conference::StartRecorder()
{
  if(externalRecorder) return; // already started
  if(!RecorderCheckSpace()) return;
  if(!PDirectory::Exists(MyProcess::Current().vr_ffmpegDir))
  {
    PTRACE(1,"EVRT\tRecorder failed to start (check recorder directory)");
    MyProcess::Current().HttpWriteEventRoom("Recorder failed to start (check recorder directory)",number);
    return;
  }
  externalRecorder = new ExternalVideoRecorderThread(number);
  unsigned i;
  for(i=0;i<100;i++) if(externalRecorder->state) break; else PThread::Sleep(50);
  if(externalRecorder->state!=1)
  {
    delete externalRecorder;
    externalRecorder = NULL;
    PTRACE(1,"EVRT\tRecorder failed to start");
    return;
  }
  MyProcess::Current().HttpWriteEventRoom("Video recording started",number);
  MyProcess::Current().HttpWriteCmdRoom(MyProcess::Current().GetConferenceManager().GetConferenceOptsJavascript(*this),number);
  MyProcess::Current().HttpWriteCmdRoom("build_page()",number);
}

void Conference::StopRecorder()
{
  if(!externalRecorder) return; // already stopped
  ExternalVideoRecorderThread * t = externalRecorder;
  externalRecorder = NULL;
  delete t;
  PTRACE(4,"EVRT\tVideo Recorder is active - stopping now");
  MyProcess::Current().HttpWriteEventRoom("Video recording stopped",number);
  MyProcess::Current().HttpWriteCmdRoom(MyProcess::Current().GetConferenceManager().GetConferenceOptsJavascript(*this),number);
  MyProcess::Current().HttpWriteCmdRoom("build_page()",number);
}

int Conference::GetVisibleMemberCount() const
{
  PWaitAndSignal m(memberListMutex);
  int visibleMembers = 0;
  std::map<void *, ConferenceMember *>::const_iterator r;
  for (r = memberList.begin(); r != memberList.end(); r++) {
    if (r->second->IsVisible())
      ++visibleMembers;
  }
  return visibleMembers;
}

void Conference::AddMonitorEvent(ConferenceMonitorInfo * info)
{ 
  manager.AddMonitorEvent(info); 
}

void Conference::RefreshAddressBook()
{
  PStringArray abook = MyProcess::Current().addressBook;
  PStringStream msg;
  msg = "addressbook=Array(";
  for(PINDEX i = 0; i < abook.GetSize(); i++)
  {
    if(i>0) msg << ",";
    PString username = abook[i];
    username.Replace("&","&amp;",TRUE,0);
    username.Replace("\"","&quot;",TRUE,0);
    msg << "Array("
        << "0"
        << ",\"" << MCUURL(abook[i]).GetUrlId() << "\""
        << ",\"" << username << "\""
        << ")";
  }
  msg << ");";
  MyProcess::Current().HttpWriteCmdRoom(msg,number);
  msg = "abook_refresh()";
  MyProcess::Current().HttpWriteCmdRoom(msg,number);
}

BOOL Conference::InviteMember(const char *membName, void * userData)
{
  return TRUE;
}

BOOL Conference::AddMember(ConferenceMember * memberToAdd)
{
  PTRACE(3, "Conference\tAbout to add member " << memberToAdd->GetTitle() << " to conference " << guid);

  // see if the callback refuses the new member (always true)
  if (!BeforeMemberJoining(memberToAdd))
    return FALSE;

  memberToAdd->SetName();

  // lock the member list
  PWaitAndSignal m(memberListMutex);

  // check for duplicate name or very fast reconnect
  {
    Conference::MemberNameList::const_iterator s = memberNameList.find(memberToAdd->GetName());
    if(MCUConfig("Parameters").GetBoolean(RejectDuplicateNameKey, FALSE))
    {
      if(s != memberNameList.end())
      {
        if(s->second != NULL)
        {
          PString username=memberToAdd->GetName(); username.Replace("&","&amp;",TRUE,0); username.Replace("\"","&quot;",TRUE,0);
          PStringStream msg;
          msg << username << " REJECTED - DUPLICATE NAME";
          MyProcess::Current().HttpWriteEventRoom(msg, number);
          return FALSE;
        }
      }
    } else {
      while (s != memberNameList.end())
      {
        if(s->second == NULL) break;
        {
          PString username=memberToAdd->GetName();
          PString newName=username;
          PINDEX hashPos = newName.Find(" ##");
          if(hashPos == P_MAX_INDEX) newName += " ##2";
          else newName = newName.Left(hashPos+3) + PString(newName.Mid(hashPos+3).AsInteger() + 1);
          memberToAdd->SetName(newName);
          s = memberNameList.find(memberToAdd->GetName());

          username.Replace("&","&amp;",TRUE,0); username.Replace("\"","&quot;",TRUE,0);
          PStringStream msg;
          msg << username << " DUPLICATE NAME, renamed to " << newName;
          MyProcess::Current().HttpWriteEventRoom(msg, number);
        }
      }
    }
  }

  // add the member to the conference
  if (!memberToAdd->AddToConference(this))
    return FALSE;

  PINDEX visibleMembers = 0;
  {
    PTRACE(3, "Conference\tAdding member " << memberToAdd->GetName() << " " << memberToAdd->GetTitle() << " to conference " << guid);
    cout << "Adding member " << memberToAdd->GetName() << " " << memberToAdd->GetTitle() << " to conference " << guid << endl;

    std::map<void *, ConferenceMember *>::const_iterator r;
    ConferenceMemberId mid = memberToAdd->GetID();
    r = memberList.find(mid);
    if(r != memberList.end()) return FALSE;

#   if OPENMCU_VIDEO
      if(moderated==FALSE
#       if ENABLE_TEST_ROOMS
          || number=="testroom"
#       endif
      )
      {
        if (UseSameVideoForAllMembers() && memberToAdd->IsVisible())
        {
          videoMixerListMutex.Wait();
          if (!videoMixerList->mixer->AddVideoSource(mid, *memberToAdd)) memberToAdd->SetFreezeVideo(TRUE);
          videoMixerListMutex.Signal();
          PTRACE(3, "Conference\tUseSameVideoForAllMembers ");
        }
      }
      else memberToAdd->SetFreezeVideo(TRUE);
#   endif

    // add this member to the conference member list
    memberList.insert(MemberList::value_type(memberToAdd->GetID(), memberToAdd));

    int tid = terminalNumberMap.GetNumber(memberToAdd->GetID());
    memberToAdd->SetTerminalNumber(tid);

    // make sure each member has a connection created for the new member
    // make sure the new member has a connection created for each existing member
    /*for (r = memberList.begin(); r != memberList.end(); ++r)
    {
      ConferenceMember * conn = r->second;
      if (conn != memberToAdd)
      {
        conn->AddConnection(memberToAdd);
        memberToAdd->AddConnection(conn);
#       if OPENMCU_VIDEO
          if (moderated==FALSE
#           if ENABLE_TEST_ROOMS
              || number == "testroom"
#           endif
          )
          if (!UseSameVideoForAllMembers())
          {
            if (conn->IsVisible()) memberToAdd->AddVideoSource(conn->GetID(), *conn);
            if (memberToAdd->IsVisible()) conn->AddVideoSource(memberToAdd->GetID(), *memberToAdd);
          }
#       endif
      }
      if (conn->IsVisible())
        ++visibleMembers;
    }*/

    // update the statistics
    if (memberToAdd->IsVisible())
    {
//      ++visibleMembers;
      maxMemberCount = PMAX(maxMemberCount, visibleMembers);
      // trigger H245 thread for join message
//      new NotifyH245Thread(*this, TRUE, memberToAdd);
    }
  }

  // notify that member is joined
  memberToAdd->SetJoined(TRUE);

  // call the callback function
  OnMemberJoining(memberToAdd);

  if (memberToAdd->IsMCU() && !mcuMonitorRunning) {
    manager.AddMonitorEvent(new ConferenceMCUCheckInfo(GetID(), 1000));
    mcuMonitorRunning = TRUE;
  }

  PStringStream msg;

  // add this member to the conference member name list
  PString memberToAddUrlId = MCUURL(memberToAdd->GetName()).GetUrlId();
  if(memberToAdd!=memberToAdd->GetID())
  {
    if(memberToAdd->GetName().Find(" ##") == P_MAX_INDEX)
    {
      // поиск по UrlId
      BOOL found = FALSE;
      Conference::MemberNameList theCopy(memberNameList);
      for (Conference::MemberNameList::iterator s=theCopy.begin(), e=theCopy.end(); s!=e; ++s)
      {
        PString memberName = s->first;
        if(MCUURL(memberName).GetUrlId() == memberToAddUrlId)
        {
          if(s->second == NULL)
          {
            memberNameList.erase(memberName);
            if(!found) confTpl.Replace(memberName,memberToAdd->GetName(),TRUE,0);
            found = TRUE;
          }
        }
      }
    } else {
      memberNameList.erase(memberToAdd->GetName());
    }
    memberNameList.insert(MemberNameList::value_type(memberToAdd->GetName(),memberToAdd));

    PullMemberOptionsFromTemplate(memberToAdd, confTpl);

    PString username=memberToAdd->GetName(); username.Replace("&","&amp;",TRUE,0); username.Replace("\"","&quot;",TRUE,0);
    msg="addmmbr(1";
    msg << "," << (long)memberToAdd->GetID()
        << ",\"" << username << "\""
        << "," << memberToAdd->muteMask
        << "," << memberToAdd->disableVAD
        << "," << memberToAdd->chosenVan
        << "," << memberToAdd->GetAudioLevel()
        << "," << memberToAdd->GetVideoMixerNumber()
        << ",\"" << memberToAddUrlId << "\""
        << "," << dec << (unsigned)memberToAdd->channelCheck
        << "," << memberToAdd->kManualGainDB
        << "," << memberToAdd->kOutputGainDB
        << ")";
    MyProcess::Current().HttpWriteCmdRoom(msg,number);
  }

  msg = "<font color=green><b>+</b>";
  msg << memberToAdd->GetName() << "</font>"; MyProcess::Current().HttpWriteEventRoom(msg,number);

  PAssert(GetVisibleMemberCount() == visibleMembers, "Visible members counter failed");

  if((autoStopRecord>=0) && (visibleMembers <= autoStopRecord)) StopRecorder();
  else if((autoStartRecord>autoStopRecord) && (visibleMembers >= autoStartRecord)) StartRecorder();

  return TRUE;
}


BOOL Conference::RemoveMember(ConferenceMember * memberToRemove)
{
  PWaitAndSignal m(memberListMutex);
  if(memberToRemove == NULL) return TRUE;
  BOOL rmVisible = memberToRemove->IsVisible();

  PString username = memberToRemove->GetName();

  if(!memberToRemove->IsJoined())
  {
    PTRACE(4, "Conference\tNo need to remove call " << username << " from conference " << guid);
    return (memberList.size() == 0);
  }

  ConferenceMemberId userid = memberToRemove->GetID();

  // add this member to the conference member name list with zero id

  PTRACE(3, "Conference\tRemoving call " << username << " from conference " << guid << " with size " << (PINDEX)memberList.size());
  cout << username << " leaving conference " << number << "(" << guid << ")" << endl;


  BOOL closeConference;
  PINDEX visibleMembers = 0;
  {
    MemberNameList::iterator s;
    s = memberNameList.find(username);
    
    ConferenceMember *zerop=NULL;
    if(memberToRemove!=userid && s->second==memberToRemove && s!=memberNameList.end())
     {
       memberNameList.erase(username);
       memberNameList.insert(MemberNameList::value_type(username,zerop));
     }

    PStringStream msg; msg << "<font color=red><b>-</b>" << username << "</font>"; MyProcess::Current().HttpWriteEventRoom(msg,number);
    username.Replace("&","&amp;",TRUE,0); username.Replace("\"","&quot;",TRUE,0);
    msg="remmmbr(0";
    msg << ","  << (long)userid
        << ",\"" << username << "\""
        << ","  << memberToRemove->muteMask
        << "," << memberToRemove->disableVAD
        << ","  << memberToRemove->chosenVan
        << ","  << memberToRemove->GetAudioLevel()
        << ",\"" << MCUURL(memberToRemove->GetName()).GetUrlId() << "\"";
    if(s==memberNameList.end()) msg << ",1";
    msg << ")";
    MyProcess::Current().HttpWriteCmdRoom(msg,number);


    // remove this connection from the member list
    memberList.erase(userid);
    //memberToRemove->RemoveAllConnections();

    MemberList::iterator r;
    // remove this member from the connection lists for all other members
    /*for (r = memberList.begin(); r != memberList.end(); ++r)
    {
      ConferenceMember * conn = r->second;
      if((conn != NULL) && (conn != memberToRemove))
      {
        conn->RemoveConnection(userid);
#       if OPENMCU_VIDEO
          BOOL isVisible = conn->IsVisible();
          if (!UseSameVideoForAllMembers())
          {
            if(rmVisible) conn->RemoveVideoSource(userid, *memberToRemove);
            if(isVisible) memberToRemove->RemoveVideoSource(conn->GetID(), *conn);
          }
          if(isVisible) ++visibleMembers;
#       endif
      }
    }*/

#   if OPENMCU_VIDEO
      if (moderated==FALSE
#       if ENABLE_TEST_ROOMS
          || number == "testroom"
#       endif
      )
      { if (UseSameVideoForAllMembers())
        if (rmVisible)
        {
          PWaitAndSignal m(videoMixerListMutex);
          videoMixerList->mixer->RemoveVideoSource(userid, *memberToRemove);
        }
      }
      else
      {
        PWaitAndSignal m(videoMixerListMutex);
        VideoMixerRecord * vmr=videoMixerList; while(vmr!=NULL)
        {
          vmr->mixer->MyRemoveVideoSourceById(userid,FALSE);
          vmr=vmr->next;
        }
      }
#   endif

    // trigger H245 thread for leave message
//    if (memberToRemove->IsVisible())
//      new NotifyH245Thread(*this, FALSE, memberToRemove);

    terminalNumberMap.RemoveNumber(memberToRemove->GetTerminalNumber());


    // return TRUE if conference is empty 
    closeConference = (visibleMembers==0);
//fix it: just remove it:
//    PAssert(GetVisibleMemberCount() == visibleMembers, "Visible members counter failed");
  }

  // notify that member is not joined anymore
  memberToRemove->SetJoined(FALSE);

  // call the callback function
  OnMemberLeaving(memberToRemove);

  if((autoStopRecord>=0) && (visibleMembers <= autoStopRecord)) StopRecorder();
  else if((autoStartRecord>autoStopRecord) && (visibleMembers >= autoStartRecord)) StartRecorder();

  return closeConference;
}

#if OPENMCU_VIDEO

void Conference::ReadMemberVideo(ConferenceMember * member, void * buffer, int width, int height, PINDEX & amount)
{
  PWaitAndSignal m(videoMixerListMutex);
  if (videoMixerList == NULL)
    return;

// PTRACE(3, "Conference\tReadMemberVideo call 1" << width << "x" << height);
  unsigned mixerNumber; if(member==NULL) mixerNumber=0; else mixerNumber=member->GetVideoMixerNumber();
  MCUVideoMixer * mixer = VMLFind(mixerNumber);

  if(mixer==NULL)
  { if(mixerNumber != 0) mixer = VMLFind(0);
    if(mixer==NULL)
    { PTRACE(3,"Conference\tCould not get video");
      return;
    } else { PTRACE(6,"Conference\tCould not get video mixer " << mixerNumber << ", reading 0 instead"); }
  }

//  if (mixer!=NULL) {
    mixer->ReadFrame(*member, buffer, width, height, amount);
//    return;
//  }

/* commented by kay27 not really understanding what he is doing, 04.09.2013
  // find the other member and copy it's video
  PWaitAndSignal m(memberListMutex);
  MemberList::iterator r;
  for (r = memberList.begin(); r != memberList.end(); r++) {
    if ((r->second != member) && r->second->IsVisible()) {
      void * frameStore = r->second->OnExternalReadVideo(member->GetID(), width, height, amount);  
      if (frameStore != NULL) {
        memcpy(buffer, frameStore, amount);
        r->second->UnlockExternalVideo();
      }
    }
  }
*/
  
}

BOOL Conference::WriteMemberVideo(ConferenceMember * member, const void * buffer, int width, int height, PINDEX amount)
{
  if (UseSameVideoForAllMembers())
  {
    PWaitAndSignal m(videoMixerListMutex);
    if (videoMixerList != NULL) {
      VideoMixerRecord * vmr = videoMixerList; BOOL writeResult=FALSE;
      while(vmr!=NULL)
      { writeResult |= vmr->mixer->WriteFrame(member->GetID(), buffer, width, height, amount);
        vmr=vmr->next;
      }
      return writeResult;
    }
  }
  else
  {
    PWaitAndSignal m(memberListMutex);
    MemberList::iterator r;
    for (r = memberList.begin(); r != memberList.end(); ++r)
      r->second->OnExternalSendVideo(member->GetID(), buffer, width, height, amount);
  }
  return TRUE;
}

#endif

BOOL Conference::BeforeMemberJoining(ConferenceMember * member)
{ 
  return manager.BeforeMemberJoining(this, member); 
}

void Conference::OnMemberJoining(ConferenceMember * member)
{ 
  manager.OnMemberJoining(this, member); 
}

void Conference::OnMemberLeaving(ConferenceMember * member)
{ 
  manager.OnMemberLeaving(this, member); 
}

void Conference::FreezeVideo(ConferenceMemberId id)
{ 
  int i;
  PWaitAndSignal m(memberListMutex);
  MemberList::iterator r,s;
  if(id!=NULL)
  {
    r = memberList.find(id); if(r == memberList.end()) return;
    PWaitAndSignal m(videoMixerListMutex);
    VideoMixerRecord * vmr = videoMixerList;
    while(vmr!=NULL)
    { i=vmr->mixer->GetPositionStatus(id);
      if(i>=0) {
        r->second->SetFreezeVideo(FALSE);
        return;
      }
      vmr=vmr->next;
    }
    if(!UseSameVideoForAllMembers())for(s=memberList.begin();s!=memberList.end();++s) if(s->second->videoMixer!=NULL) if(s->second->videoMixer->GetPositionStatus(id)>=0) {r->second->SetFreezeVideo(FALSE); return;}
    r->second->SetFreezeVideo(TRUE);
    return;
  }
  for (r = memberList.begin(); r != memberList.end(); r++)
  {
    ConferenceMemberId mid=r->second->GetID();
    PWaitAndSignal m(videoMixerListMutex);
    VideoMixerRecord * vmr = videoMixerList;
    while(vmr!=NULL)
    {
      i=vmr->mixer->GetPositionStatus(mid);
      if(i>=0)
      {
        r->second->SetFreezeVideo(FALSE);
        break;
      }
      vmr=vmr->next;
    }
    if(vmr==NULL)
    { if(!UseSameVideoForAllMembers())
      { for(s=memberList.begin();s!=memberList.end();++s)
        { if(s->second->videoMixer!=NULL) if(s->second->videoMixer->GetPositionStatus(mid)>=0)
          { r->second->SetFreezeVideo(FALSE);
            break;
          }
        }
        if(s==memberList.end()) r->second->SetFreezeVideo(TRUE);
      }
      else r->second->SetFreezeVideo(TRUE);
    }
  }
}

BOOL Conference::PutChosenVan()
{
  BOOL put=FALSE;
  int i;
  PWaitAndSignal m(memberListMutex);
  MemberList::iterator r;
  for (r = memberList.begin(); r != memberList.end(); ++r)
  {
    if(r->second->chosenVan)
    {
      PWaitAndSignal m(videoMixerListMutex);
      VideoMixerRecord * vmr = videoMixerList;
      while(vmr!=NULL){
        i=vmr->mixer->GetPositionStatus(r->second->GetID());
        if(i < 0) put |= (NULL != vmr->mixer->SetVADPosition(r->second,r->second->chosenVan,VAtimeout));
        vmr = vmr->next;
      }
    }
  }
  return put;
}

void Conference::AddOfflineMemberToNameList(PString & name)
{
  ConferenceMember *zerop=NULL;
  memberNameList.insert(MemberNameList::value_type(name,zerop));
  PString username(name);
  username.Replace("&","&amp;",TRUE,0); username.Replace("\"","&quot;",TRUE,0);
  PString url=MCUURL(username).GetUrlId();
  MyProcess::Current().HttpWriteCmdRoom("addmmbr(0,0,'"+username+"',0,0,0,0,0,'"+url+"',0)",number);
}

void Conference::HandleFeatureAccessCode(ConferenceMember & member, PString fac){
  PTRACE(3,"Conference\tHandling feature access code " << fac << " from " << member.GetName());
  PStringArray s = fac.Tokenise("*");
  if(s[0]=="1")
  {
    int posTo=0;
    if(s.GetSize()>1) posTo=s[1].AsInteger();
    PTRACE(4,"Conference\t" << "*1*" << posTo << "#: jump into video position " << posTo);
    if(videoMixerCount==0) return;
    ConferenceMemberId id=member.GetID();
    if(id==NULL) return;

    {
      PWaitAndSignal m(videoMixerListMutex);
      if(videoMixerList==NULL) return;
      if(videoMixerList->mixer==NULL) return;
      int pos=videoMixerList->mixer->GetPositionNum(id);
      if(pos==posTo) return;
      videoMixerList->mixer->InsertVideoSource(&member,posTo);
    }
    FreezeVideo(NULL);

    //MyProcess::Current().HttpWriteCmdRoom(MyProcess::Current().GetEndpoint().GetConferenceOptsJavascript(*this),number);
    MyProcess::Current().HttpWriteCmdRoom("build_page()",number);
  }
}


///////////////////////////////////////////////////////////////////////////

ConferenceMember::    ConferenceMember(ConferenceManager & manager, OpalMixerNodeInfo * info, Conference * _conference, ConferenceMemberId _id, BOOL _isMCU)
  : OpalMixerNode(manager, info)
  , conference(_conference), id(_id), isMCU(_isMCU)
{
  channelCheck=0;
  audioLevel = 0;
  audioCounter = 0; previousAudioLevel = 65535; audioLevelIndicator = 0;
  currVolCoef = 1.0;
  kManualGain = 1.0; kManualGainDB = 0;
  kOutputGain = 1.0; kOutputGainDB = 0;
  terminalNumber = -1;
  memberIsJoined = FALSE;

#if OPENMCU_VIDEO
  videoMixer = NULL;
//  fsConverter = PColourConverter::Create("YUV420P", "YUV420P", CIF4_WIDTH, CIF4_HEIGHT);
//  MCUVideoMixer::FillCIF4YUVFrame(memberFrameStores.GetFrameStore(CIF4_WIDTH, CIF4_HEIGHT).data.GetPointer(), 0, 0, 0);
  totalVideoFramesReceived = 0;
  firstFrameReceiveTime = -1;
  totalVideoFramesSent = 0;
  firstFrameSendTime = -1;
  rxFrameWidth = 0; rxFrameHeight = 0;
  vad = 0;
  autoDial = FALSE;
  muteMask = 0;
  disableVAD = FALSE;
  chosenVan = 0;
  videoMixerNumber = 0;
#endif
}

ConferenceMember::~ConferenceMember()
{
  muteMask|=15;
#if OPENMCU_VIDEO
  delete videoMixer;
#endif
}   

void ConferenceMember::ChannelBrowserStateUpdate(BYTE bitMask, BOOL bitState)
{
  if(bitState)
  {
    channelCheck|=bitMask;
  }
  else
  {
    channelCheck&=~bitMask;
  }

  PStringStream msg;
  msg << "rtp_state(" << dec << (long)id << ", " << (unsigned)channelCheck << ")";
  PTRACE(1,"channelCheck change: " << (bitState?"+":"-") << (unsigned)bitMask << ". Result: " << (unsigned)channelCheck << ", conference=" << conference << ", msg=" << msg);

  if(!conference)
  {
    MyProcess::Current().HttpWriteCmd(msg);
    return;
  }

  MyProcess::Current().HttpWriteCmdRoom(msg,conference->GetNumber());
}

BOOL ConferenceMember::AddToConference(Conference * _conference)
{
  //if (conference != NULL)
  //  return FALSE;
  //conference = _conference;
#if OPENMCU_VIDEO
  if (!conference->UseSameVideoForAllMembers())
    videoMixer = new MCUSimpleVideoMixer();
#endif
  return TRUE;
}

void ConferenceMember::RemoveFromConference()
{
  if (conference != NULL) {
    if (conference->RemoveMember(this))
    {
      if(conference->autoDelete
#     if ENABLE_TEST_ROOMS
        || (conference->GetNumber().Left(8) == "testroom")
#     endif
#     if ENABLE_ECHO_MIXER
        || (conference->GetNumber().Left(4) *= "echo")
#     endif
      )
      conference->GetManager().RemoveConference(conference->GetID());
    }
  }
}

#if OPENMCU_VIDEO

// called whenever the connection needs a frame of video to send
void ConferenceMember::ReadVideo(void * buffer, int width, int height, PINDEX & amount)
{
  if(!(channelCheck&8)) ChannelBrowserStateUpdate(8,TRUE);
//  if(muteMask&8) { memset(buffer,0,amount); return; }
  if(muteMask&8) return;
  ++totalVideoFramesSent;
  if (!firstFrameSendTime.IsValid())
    firstFrameSendTime = PTime();
  if (lock.Wait()) {
    if (conference != NULL) {
      if (conference->UseSameVideoForAllMembers())
        conference->ReadMemberVideo(this, buffer, width, height, amount);
      else if (videoMixer != NULL)
        videoMixer->ReadFrame(*this, buffer, width, height, amount);
    }
    lock.Signal();
  }
}

// called whenever the connection receives a frame of video
void ConferenceMember::WriteVideo(const void * buffer, int width, int height, PINDEX amount)
{
  if(!(channelCheck&4)) ChannelBrowserStateUpdate(4,TRUE);
  if(muteMask&4) return;
  ++totalVideoFramesReceived;
  rxFrameWidth=width; rxFrameHeight=height;
  if (!firstFrameReceiveTime.IsValid())
    firstFrameReceiveTime = PTime();

  if (lock.Wait()) {
    if (conference != NULL) {
      if (!conference->WriteMemberVideo(this, buffer, width, height, amount)) {
        PWaitAndSignal m(memberFrameStoreMutex);
        VideoFrameStoreList::FrameStore & fs = memberFrameStores.GetFrameStore(width, height);
        memcpy(fs.data.GetPointer(), buffer, amount);
        memberFrameStores.InvalidateExcept(width, height);
      }
    }
    lock.Signal();
  }
}

void ConferenceMember::OnExternalSendVideo(ConferenceMemberId id, const void * buffer, int width, int height, PINDEX amount)
{
//  if (lock.Wait()) {
    videoMixer->WriteFrame(id, buffer, width, height, amount);
//    lock.Signal();
//  }
}

void * ConferenceMember::OnExternalReadVideo(ConferenceMemberId id, int width, int height, PINDEX & bytesReturned)
{
  if (!lock.Wait())
    return NULL;

  memberFrameStoreMutex.Wait();

  BOOL found;
  VideoFrameStoreList::FrameStore & nearestFs = memberFrameStores.GetNearestFrameStore(width, height, found);

  // if no valid framestores, nothing we can do
/*  if (!found) {
    memberFrameStoreMutex.Signal();
    lock.Signal();
    return NULL;
  }
*/
  // if the valid framestore is a perfect match, return it
  if(found)
  if ((nearestFs.width == width) && (nearestFs.height == height))
    return nearestFs.data.GetPointer();

  // create a new destinationf framestore
  VideoFrameStoreList::FrameStore & destFs = memberFrameStores.GetFrameStore(width, height);

  if(found)
  MCUVideoMixer::ResizeYUV420P(nearestFs.data.GetPointer(), destFs.data.GetPointer(), nearestFs.width, nearestFs.height, width, height);
  else
//  MyProcess::Current().GetPreMediaFrame(destFs.data.GetPointer(), width, height, bytesReturned);
  MCUVideoMixer::FillYUVFrame(destFs.data.GetPointer(), 0, 0, 0, width, height);
  destFs.valid = TRUE;

  return destFs.data.GetPointer();
}

void ConferenceMember::UnlockExternalVideo()
{ 
  memberFrameStoreMutex.Signal(); 
  lock.Signal();
}

BOOL ConferenceMember::AddVideoSource(ConferenceMemberId id, ConferenceMember & mbr)
{
  PAssert(videoMixer != NULL, "attempt to add video source to NULL video mixer");
  return videoMixer->AddVideoSource(id, mbr);
}

void ConferenceMember::RemoveVideoSource(ConferenceMemberId id, ConferenceMember & mbr)
{
  PAssert(videoMixer != NULL, "attempt to remove video source from NULL video mixer");
  videoMixer->RemoveVideoSource(id, mbr);
}

BOOL ConferenceMember::OnOutgoingVideo(void * buffer, int width, int height, PINDEX & amount)
{
  return FALSE;
}

BOOL ConferenceMember::OnIncomingVideo(const void * buffer, int width, int height, PINDEX amount)
{
  return FALSE;
}

#endif

PString ConferenceMember::GetMonitorInfo(const PString & /*hdr*/)
{ 
  return PString::Empty(); 
}

///////////////////////////////////////////////////////////////

MCULock::MCULock()
{
  closing = FALSE;
  count = 0;
}

BOOL MCULock::Wait(BOOL hard)
{
  mutex.Wait();
  if (hard)
    return TRUE;

  BOOL ret = TRUE;
  if (!closing)
    count++;
  else
    ret = FALSE;

  mutex.Signal();
  return ret;
}

void MCULock::Signal(BOOL hard)
{
  if (hard) {
    mutex.Signal();
    return;
  }

  mutex.Wait();
  if (count > 0)
    count--;
  if (closing)
    closeSync.Signal();
  mutex.Signal();
}

void MCULock::WaitForClose()
{
  mutex.Wait();
  closing = TRUE;
  BOOL wait = count > 0;
  mutex.Signal();
  while (wait) {
    closeSync.Wait();
    mutex.Wait();
    wait = count > 0;
    mutex.Signal();
  }
}
