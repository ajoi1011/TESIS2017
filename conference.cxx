/*
 * conference.cxx
 *
*/

#include "main.h"
static inline int ABS(int v)
{  return (v >= 0) ? v : -v; }
///////////////////////////////////////////////////////////////
ConferenceNode::ConferenceNode(ConferenceManager & manager, OpalMixerNodeInfo * info)
  : OpalMixerNode(manager, info)
  , m_videoMixer(manager.CreateVideoMixer(*info))
{
  m_moderated = false;
  m_maxMemberCount = 0;
  VMLInit((MCUVideoMixer*)m_videoMixer);
}

void ConferenceNode::VMLInit(MCUVideoMixer * mixer)
{
  
  if(mixer==NULL) { 
    m_videoMixerList  = NULL; 
    m_videoMixerCount = 0; 
    cout << "DONT FORCE SCREEN SPLIT: NO CONFERENCE MIXER" << endl;
    return; 
  } 
  m_videoMixerList = new VideoMixerRecord();
  m_videoMixerList->id = 0;
  m_videoMixerList->mixer = mixer;
  m_videoMixerCount = 1;
}

unsigned ConferenceNode::VMLAdd()
{ 
  
  if(m_videoMixerCount >= 100) 
    return 0; // limitation
  
  VideoMixerRecord * vmr = m_videoMixerList;
  unsigned id = vmr->id;
  while (vmr->next != NULL) { 
    vmr = vmr->next; 
    if(vmr->id > id)
      id = vmr->id; 
  }
      
  VideoMixerRecord * vmrnew = new VideoMixerRecord();
  vmrnew->mixer=new MCUVideoMixer(GetNodeInfo());
  id++;
  vmr->next    = vmrnew; 
  vmrnew->prev = vmr;
  vmrnew->id   = id; 
  m_videoMixerCount++;
  return id;
}

void ConferenceNode::VMLClear()
{
  
  VideoMixerRecord * vmr = m_videoMixerList;
  if(vmr->next != NULL) 
    while(vmr->next->next != NULL) 
      vmr = vmr->next; //LIFO: points to last but one, delete the next == last
  while(vmr != NULL) {
	if(vmr->next != NULL) { 
	  if(vmr->next->mixer != NULL) { 
	    delete vmr->next->mixer; 
	    vmr->next->mixer = NULL;
	  }
	  delete vmr->next; 
	}
	vmr = vmr->prev;
  }
  delete m_videoMixerList;
  m_videoMixerCount = 0;
}

unsigned ConferenceNode::VMLDel(unsigned n)
{
  
  PTRACE(6,"MixerCtrl\tVMLDel(" << n << ") videoMixerCount=" << m_videoMixerCount);
  if(m_videoMixerCount == 1)
    return false; //prevent last mixer deletion
  
  VideoMixerRecord * vmr = m_videoMixerList;
  unsigned newIndex = 0; // reordering index
  while (vmr != NULL) { 
    if (vmr->id == n) { 
	  PTRACE(6,"MixerCtrl\tVMLDel(" << n << ") mixer record with vmr->id=" << vmr->id << "found, removing");
	  VideoMixerRecord * vmrprev = vmr->prev; 
	  VideoMixerRecord * vmrnext = vmr->next; // keep prev & next
	  if(vmr->mixer != NULL) { delete vmr->mixer; vmr->mixer = NULL; }
	  if(m_videoMixerList == vmr) m_videoMixerList = vmrnext; // special case of deletion of mixer 0
	  if(vmrprev != NULL) vmrprev->next = vmrnext; 
	  if(vmrnext != NULL) vmrnext->prev = vmrprev;
	  delete vmr; 
	  m_videoMixerCount--;
	  n=65535; // prevent further deletions
	  vmr = vmrnext;
	} 
	else { 
	  vmr->id = newIndex; 
	  newIndex++; 
	  vmr = vmr->next; 
	}
  }
  if(newIndex != m_videoMixerCount) { 
    PTRACE(1,"MixerCtrl\tVideo mixer counter " << m_videoMixerCount << " doesn't match last mixer id++ " << newIndex); 
  }
  return newIndex;
}

MCUVideoMixer * ConferenceNode::VMLFind(unsigned i) const
{
  
  VideoMixerRecord *vmr = m_videoMixerList;
  while (vmr->next != NULL && vmr->id != i) 
    vmr=vmr->next;
  
  if(vmr->id == i && vmr != NULL) 
    return vmr->mixer;
  return NULL;
}

PString ConferenceNode::IsModerated() const
{
   PString yes = "+";
   PString no  = "-";
   if(!m_moderated) 
     return no; 
   else 
     return yes;
}

bool ConferenceNode::AttachStream(OpalMixerMediaStream * stream)
{
  PString id = stream->GetID();

  PTRACE(4, "Attaching " << stream->GetMediaFormat()
         << ' ' << (stream->IsSource() ? "source" : "sink")
         << " stream with id " << id << " to " << *this);

#if OPAL_VIDEO
  if (stream->GetMediaFormat().GetMediaType() == OpalMediaType::Video()) {
    OpalVideoFormat::ContentRole role = stream->GetMediaFormat().GetOptionEnum(OpalVideoFormat::ContentRoleOption(), OpalVideoFormat::eNoRole);
    OpalVideoStreamMixer * videoMixer;
    VideoMixerMap::iterator it = m_videoMixers.find(role);
    if (it != m_videoMixers.end())
      videoMixer = it->second;
    else {
      videoMixer = m_videoMixer;
      m_videoMixers[role] = videoMixer;
      cout << "Testing" << endl;
    }

    m_mixerById[id] = videoMixer;

    if (stream->IsSink())
      return videoMixer->AddStream(id);

    videoMixer->Append(stream);
    return true;
  }
#endif // OPAL_VIDEO

  m_mixerById[id] = m_audioMixer;

  if (stream->IsSink())
    return m_audioMixer->AddStream(id);

  m_audioMixer->Append(stream);
  return true;
}

MCUVideoMixer::MCUVideoMixer(const OpalMixerNodeInfo & info)
  : OpalVideoStreamMixer(info)
  , m_info(const_cast<OpalMixerNodeInfo &>(info))
{
  PTRACE(1,"MCUVideoMixer created");
  cout << "CREATE" << m_info.m_name << this << endl;
  VMPListInit();
  imageStore_size  =0;
  imageStore1_size =0;
  imageStore2_size =0;
  specialLayout    =0;
  
  jpegTime=0; jpegSize=0;
}

void MCUVideoMixer::ChangeLayout(unsigned newLayout)
{
  int newCount=OpalMCUEIE::vmcfg.vmconf[newLayout].splitcfg.vidnum;
  //PWaitAndSignal m(mutex); 
  
  specialLayout=newLayout; NullAllFrameStores();
  VideoMixPosition *r = vmpList->next; while(r!=NULL)
  {
    VideoMixPosition * vmp = r;
    if(vmp->n < newCount){
      vmp->xpos=OpalMCUEIE::vmcfg.vmconf[newLayout].vmpcfg[vmp->n].posx;
      vmp->ypos=OpalMCUEIE::vmcfg.vmconf[newLayout].vmpcfg[vmp->n].posy;
      vmp->width=OpalMCUEIE::vmcfg.vmconf[newLayout].vmpcfg[vmp->n].width;
      vmp->height=OpalMCUEIE::vmcfg.vmconf[newLayout].vmpcfg[vmp->n].height;
      vmp->border=OpalMCUEIE::vmcfg.vmconf[newLayout].vmpcfg[vmp->n].border;
    }
    else
    {
      r=r->prev;
      VMPListDelVMP(vmp);
      delete vmp;
    }
    r=r->next;
  }
}

StreamID MCUVideoMixer::GetPositionId(int pos)
{
  PWaitAndSignal m(m_videoMixerMutex);

  VideoMixPosition * r = vmpList->next;
  while(r != NULL)
  {
    VideoMixPosition & vmp = *r;
    if (vmp.n == pos )
    {
      if(vmp.type > 1) return (void *)(long)(1-vmp.type);
      else return vmp.id;
    }
    r = r->next;
  }

  return NULL;
}

bool MCUVideoMixer::MixVideo()
{
	

  // create output frame
  /*unsigned x, y, w, h, left;
  if (!StartMix(x, y, w, h, left))
    return false;

  // This makes sure subimage are on 32 bit boundary, some parts of the
  // system can get mightily upset if this is not the case.
  w &= 0xfffffffc;
  h &= 0xfffffffc;

  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter) {
    InsertVideoFrame(iter, x, y, w, h);
    if (!NextMix(x, y, w, h, left))
      break;
  }

  return true;*/
  PINDEX pos = 0;
  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter, ++pos) {
    
    InsertVideoSubFrame(iter,pos);
    
  }
  return true;
}

bool MCUVideoMixer::ReadFrame(void * buffer, int width, int height, PINDEX & amount)
{
  //PWaitAndSignal m(mutex);
  
  // special case of one member means fill with black
//  if (!forceScreenSplit && rows == 0) {
  if (vmpNum == 0) {
	
    VideoFrameStoreList::FrameStore & fs = frameStores.GetFrameStore(width, height);
    if (!fs.valid) {
	  	
      if (!OpalMCUEIE::Current().GetPreMediaFrame(fs.data.GetPointer(), width, height, amount)){
        FillYUVFrame(fs.data.GetPointer(), 0, 0, 0, width, height);
        
      }
      fs.valid = TRUE;
      fs.lastRead = time(NULL);
    }
    memcpy(buffer, fs.data.GetPointer(), amount);
    
  }

  return ReadMixedFrame(buffer, width, height, amount);
}


bool MCUVideoMixer::ReadSrcFrame(VideoFrameStoreList & srcFrameStores, void * buffer, int width, int height, PINDEX & amount)
{
  //PWaitAndSignal m(mutex);

  VideoFrameStoreList::FrameStore & Fs = srcFrameStores.GetFrameStore(width, height);
  if (!Fs.valid) 
  {
//   if (!OpalMCUEIE::Current().GetPreMediaFrame(Fs.data.GetPointer(), width, height, amount))
    MCUVideoMixer::FillYUVFrame(Fs.data.GetPointer(), 0, 0, 0, width, height);
    
    if(width>=2 && height >=2) // grid
    for (unsigned i=0; i<OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum; i++)
    if(OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].border)
    { 
      VMPCfgOptions & vmpcfg=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i];
      int pw=vmpcfg.width*width/CIF4_WIDTH; // pixel w&h of vmp-->fs
      int ph=vmpcfg.height*height/CIF4_HEIGHT;
      if(pw<2 || ph<2) continue;
      imageStores_operational_size(pw,ph,_IMGST);
      
      const void *ist = imageStore.GetPointer();
      FillYUVFrame(imageStore.GetPointer(), 0, 0, 0, pw, ph);
      
      VideoSplitLines(imageStore.GetPointer(), pw, ph);
      int px=vmpcfg.posx*width/CIF4_WIDTH; // pixel x&y of vmp-->fs
      int py=vmpcfg.posy*height/CIF4_HEIGHT;
      CopyRectIntoFrame(ist,Fs.data.GetPointer(),px,py,pw,ph,width,height);
    }
    Fs.valid = TRUE;
    
  }
  memcpy(buffer, Fs.data.GetPointer(), amount);
  Fs.lastRead=time(NULL);
  
  return TRUE;
}

bool MCUVideoMixer::ReadMixedFrame(void * buffer, int width, int height, PINDEX & amount)
{
  return ReadSrcFrame(frameStores, buffer, width, height, amount);
}

void MCUVideoMixer::ReallocatePositions()
{
  NullAllFrameStores();
  unsigned i = 0;
  VideoMixPosition *r = vmpList->next;
  while(r != NULL)
  {
    VideoMixPosition & vmp = *r;
    vmp.n = i;
#if USE_FREETYPE
    RemoveSubtitles(vmp);
#endif
    vmp.xpos=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posx;
    vmp.ypos=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posy;
    vmp.width=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].width;
    vmp.height=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].height;
    vmp.border=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].border;
    ++i;
    r = r->next;
  }
}

void MCUVideoMixer::PositionSetup(int pos, int type, Id id) //types 1000, 1001, 1002, 1003 means type will not changed
{
  if((unsigned)pos>=OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum) return; // n out of range

  //PWaitAndSignal m(mutex);

  VideoMixPosition * old;
  //ConferenceMemberId id;
  if(id!=NULL)
  {
    
    old = VMPListFindVMP(id);
  }
  else
  {
    id = NULL;
    old = NULL;
  }

  if(old!=NULL)
  {
    if(old->n == pos)
    {
      if(type<1000)
      {
        old->type=type;
      }
      return;
    }
    if(old->type == 1)
    {
      RemoveVideoSourceByPos(old->n, TRUE);
    }
    else
    {
      RemoveVideoSourceByPos(old->n, FALSE);
    }
  }

  VideoMixPosition * v = vmpList->next;

  while(v!=NULL)
  {
    if(v->n==pos) // we found it
    {
      if(type<1000) v->type=type;

      if((v->type==1) && (id==NULL)) // special case: VMP needs to be removed
      {
        NullRectangle(v->xpos, v->ypos, v->width, v->height, v->border);
        { VMPListDelVMP(v); delete v; }
        return;
      }

      if((v->type==2)||(v->type==3))
      {
        if(v->chosenVan) return;
        NullRectangle(v->xpos, v->ypos, v->width, v->height, v->border);
        v->id=(void*)(long)v->n;
#if USE_FREETYPE
        RemoveSubtitles(*v);
#endif
        v->endpointName="Voice-activated " + PString(type-1);
        return;
      }

      if(v->id == id) return;

      v->id=id;
#if USE_FREETYPE
      RemoveSubtitles(*v);
#endif
      //v->endpointName=member->GetName();
      return;
    }

    v=v->next;
  }

// creating new video mix position:

  if(type>1000) type-=1000;
  if((type==1) && (id==NULL)) return;
  PString name;
  if(id==NULL) id=(void *)(long)pos; //vad
  //else name=member->GetName();

  VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[pos];

  VideoMixPosition * newPosition = CreateVideoMixPosition(id, o.posx, o.posy, o.width, o.height);
  newPosition->type=type;
  newPosition->n=pos;
  //newPosition->endpointName = name;
#if USE_FREETYPE
  RemoveSubtitles(*newPosition);
#endif
  newPosition->border=o.border;

  if(OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.new_from_begin)
    VMPListInsVMP(newPosition);
  else VMPListAddVMP(newPosition);
}

void MCUVideoMixer::SetPositionType(int pos, int type)
{ //PWaitAndSignal m(mutex);
  VideoMixPosition *r = vmpList->next;
  while(r!=NULL) if (r->n == pos)
  {  r->type=type;
     return;
  } else r=r->next;

  if((unsigned)pos>=OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum) return;
  VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[pos];

  if(type==1) return;

  Id id=(void*)(long)pos;
  VideoMixPosition * newPosition = CreateVideoMixPosition(id, o.posx, o.posy, o.width, o.height);
  newPosition->type=type;
  newPosition->n=pos;
#if USE_FREETYPE
  RemoveSubtitles(*newPosition);
#endif
  newPosition->endpointName = "Voice-activated " + PString(type-1);
  newPosition->border=o.border;

  if(OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.new_from_begin)
    VMPListInsVMP(newPosition);
  else VMPListAddVMP(newPosition);
}


bool MCUVideoMixer::AddVideoSource(Id id, OpalMixerNode * node)
{
  //PWaitAndSignal m(mutex);
  
  if (vmpNum == MAX_SUBFRAMES)
  {
    PTRACE(2, "AddVideoSource " << id << " " << vmpNum << " maximum exceeded (" << MAX_SUBFRAMES << ")");
    return FALSE;
  }

  // make sure this source is not already in the list
  VideoMixPosition *newPosition = VMPListFindVMP(id); if(newPosition != NULL)
  {
    PTRACE(2, "AddVideoSource " << id << " " << vmpNum << " already in list (" << newPosition << ")");
    return TRUE;
  }

// finding best matching layout (newsL):
  int newsL=-1; int maxL=-1; unsigned maxV=99999;
  
  for(unsigned i=0;i<OpalMCUEIE::vmcfg.vmconfs;i++) 
    if(OpalMCUEIE::vmcfg.vmconf[i].splitcfg.mode_mask&1)
  {
    if(OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum==vmpNum+1) { newsL=i; break; }
    else if((OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum>vmpNum)&&(OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum<maxV))
    {
     maxV=OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum;
     maxL=(int)i;
    }
  }
  if(newsL==-1) {
    if(maxL!=-1) newsL=maxL; else newsL=specialLayout;
  }

  if ((newsL != specialLayout)||(vmpNum==0)) // split changed or first vmp
  {
	
    specialLayout=newsL;
    newPosition = CreateVideoMixPosition(id, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmpNum].posx, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmpNum].posy, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmpNum].width, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmpNum].height);
    newPosition->type=1;
    newPosition->n=vmpNum;
    //newPosition->endpointName=mbr.GetName();
    newPosition->border=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmpNum].border;
    if(OpalMCUEIE::vmcfg.vmconf[newsL].splitcfg.new_from_begin) {
      VMPListInsVMP(newPosition);
    } 
    else { VMPListAddVMP(newPosition);}
//    cout << "AddVideoSource " << id << " " << vmpNum << " done (" << newPosition << ")\n";
    ReallocatePositions();
  }
  else  // otherwise find an empty position
  {
    for(unsigned i=0;i<OpalMCUEIE::vmcfg.vmconf[newsL].splitcfg.vidnum;i++)
    {
      newPosition = vmpList->next;
      while (newPosition != NULL) { if (newPosition->n != (int)i) newPosition=newPosition->next; else break; }
      if(newPosition==NULL) // empty position found
      {
        newPosition = CreateVideoMixPosition(id, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posx, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posy, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].width, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].height);
        newPosition->n=i;
        newPosition->type=1;
#if USE_FREETYPE
        RemoveSubtitles(*newPosition);
#endif
        newPosition->border=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].border;
        if(OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.new_from_begin) 
          VMPListInsVMP(newPosition); else VMPListAddVMP(newPosition);
//        cout << "AddVideoSource1 " << id << " " << vmpNum << " added as " << i << " (" << newPosition << ")\n";
        break;
      }
    }
  }

  if (newPosition != NULL) {
    PBYTEArray fs(CIF4_SIZE);
    PINDEX amount = newPosition->width*newPosition->height*3/2;
    if (!OpalMCUEIE::Current().GetPreMediaFrame(fs.GetPointer(), newPosition->width, newPosition->height, amount))
      FillYUVFrame(fs.GetPointer(), 0, 0, 0, newPosition->width, newPosition->height);
    WriteSubFrame(*newPosition, fs.GetPointer(), newPosition->width, newPosition->height, amount);
  }
  else
  {
    PTRACE(2, "AddVideoSource " << id << " " << vmpNum << " could not find empty video position");
  }
  return TRUE;
}

bool MCUVideoMixer::AddVideoSourceToLayout(StreamID id, OpalMixerNode * node)
{
  //PWaitAndSignal m(mutex);
  cout << OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum << endl;
  //cout << "specialLayout " << specialLayout << endl;
  
  
  
  if (vmpNum == OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum)
  {
    
    PTRACE(3, "AddVideoSource " << id << " " << vmpNum << " layout capacity exceeded (" << OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum << ")");
    return FALSE;
  }

  // make sure this source is not already in the list
  VideoMixPosition *newPosition = VMPListFindVMP(id); if(newPosition != NULL)
  {
    
    PTRACE(3, "AddVideoSource " << id << " " << vmpNum << " already in list (" << newPosition << ")");
    return TRUE;
  }

  for(unsigned i=0;i<OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum;i++)
  {
    
    newPosition = vmpList->next;
    while (newPosition != NULL) { if (newPosition->n != (int)i) newPosition=newPosition->next; else break; }
    if(newPosition==NULL) // empty position found
    {
      //cout << i << endl;
      newPosition = CreateVideoMixPosition(id, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posx, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].posy, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].width, OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].height);
      newPosition->n=i;
      newPosition->type=1;
#if USE_FREETYPE
      RemoveSubtitles(*newPosition);
#endif
      //newPosition->endpointName = mbr.GetName();
      newPosition->border=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].border;
      if(OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.new_from_begin) VMPListInsVMP(newPosition); else VMPListAddVMP(newPosition);
      PTRACE(3, "AddVideoSource " << id << " " << vmpNum << " added as " << i << " (" << newPosition << ")");
//      cout <<"AddVideoSourceToLayout " << id << " " << vmpNum << " added as " << i << " (" << newPosition << ")" << endl;
      break;
    }
  }

  if (newPosition != NULL) {
    PBYTEArray fs(CIF4_SIZE);
    PINDEX amount = newPosition->width*newPosition->height*3/2;
    if (!OpalMCUEIE::Current().GetPreMediaFrame(fs.GetPointer(), newPosition->width, newPosition->height, amount))
      FillYUVFrame(fs.GetPointer(), 0, 0, 0, newPosition->width, newPosition->height);
    WriteSubFrame(*newPosition, fs.GetPointer(), newPosition->width, newPosition->height, amount);
    
    return TRUE;
  }
  else return FALSE;
  
}

bool MCUVideoMixer::WriteFrame(Id id, const void * buffer, int width, int height, PINDEX amount)
{
  //PWaitAndSignal m(mutex);
    
  // write data into sub frame of mixed image
  VideoMixPosition *pVMP = VMPListFindVMP(id);
  
  if(pVMP==NULL) { cout << "MIXED" << id << endl; return FALSE;}
  
  WriteSubFrame(*pVMP, buffer, width, height, amount);

  return TRUE;
}

bool MCUVideoMixer::WriteSubFrame(VideoMixPosition & vmp, const void * buffer, int width, int height, PINDEX amount)
{
  
  VMPCfgOptions & vmpcfg=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[vmp.n];
  time_t inactiveSign = time(NULL) - FRAMESTORE_TIMEOUT;
  //PWaitAndSignal m(mutex);
  VideoFrameStoreList::VideoFrameStoreListMapType theCopy(frameStores.videoFrameStoreList);
  for (VideoFrameStoreList::VideoFrameStoreListMapType::iterator r=theCopy.begin(), e=theCopy.end(); r!=e; ++r)
  {
    
    VideoFrameStoreList::FrameStore & vf = *(r->second);
    
    if(vf.lastRead<inactiveSign)
    {
#if USE_FREETYPE
      DeleteSubtitlesByFS(vf.width,vf.height);
#endif
      delete r->second;
      r->second=NULL;
      frameStores.videoFrameStoreList.erase(r->first);
      cout << vf.lastRead << "-" << inactiveSign << endl;
      continue;
    }
    if(vf.width<2 || vf.height<2) continue; // minimum size 2*2
     
    // pixel w&h of vmp-->fs:
    int pw=vmp.width*vf.width/CIF4_WIDTH; int ph=vmp.height*vf.height/CIF4_HEIGHT; if(pw<2 || ph<2) continue;
    
    const void *ist;
    if(pw==width && ph==height) //same size
    {

        ist = buffer; 

       
    }
    else if(pw*height<ph*width){
      imageStores_operational_size(ph*width/height,ph,_IMGST+_IMGST1);
      ResizeYUV420P((const BYTE *)buffer,    imageStore1.GetPointer(), width, height, ph*width/height, ph);
      CopyRectFromFrame         (imageStore1.GetPointer(),imageStore.GetPointer() , (ph*width/height-pw)/2, 0, pw, ph, ph*width/height, ph);
      ist=imageStore.GetPointer();
       
    }
    else if(pw*height>ph*width){
      imageStores_operational_size(pw,pw*height/width,_IMGST+_IMGST1);
      ResizeYUV420P((const BYTE *)buffer,    imageStore1.GetPointer(), width, height, pw, pw*height/width);
      CopyRectFromFrame         (imageStore1.GetPointer(),imageStore.GetPointer() , 0, (pw*height/width-ph)/2, pw, ph, pw, pw*height/width);
      ist=imageStore.GetPointer();
       
    }
    else { // fit. scale
      imageStores_operational_size(pw,ph,_IMGST);
      ResizeYUV420P((const BYTE *)buffer,    imageStore.GetPointer() , width, height, pw, ph);
      ist=imageStore.GetPointer();
       
    }

    // border (split lines):
    if (vmpcfg.border) VideoSplitLines((void *)ist, pw, ph);

#if USE_FREETYPE
    if(!(vmpcfg.label_mask&FT_P_DISABLED)) PrintSubtitles(vmp,(void *)ist,pw,ph,vmpcfg.label_mask);
#endif

    int px=vmp.xpos*vf.width/CIF4_WIDTH; // pixel x&y of vmp-->fs
    int py=vmp.ypos*vf.height/CIF4_HEIGHT;
    
    for(unsigned i=0;i<vmpcfg.blks;i++) {
	  //cout << "blks " << i << endl;	
      CopyRFromRIntoR( ist, vf.data.GetPointer(), px, py, pw, ph,
        vmpcfg.blk[i].posx*vf.width/CIF4_WIDTH, vmpcfg.blk[i].posy*vf.height/CIF4_HEIGHT,
        vmpcfg.blk[i].width*vf.width/CIF4_WIDTH, vmpcfg.blk[i].height*vf.height/CIF4_HEIGHT,
        vf.width, vf.height, pw, ph );
      
        
      }  
      
      
      //FillYUVFrame(vf.data.GetPointer(), 54, 30, 250, pw, ph);

//    frameStores.InvalidateExcept(vf.width, vf.height);
    vf.valid=1;
  }
    
  return TRUE;
}

void MCUVideoMixer::RemoveAllVideoSource()
{
  VMPListClear();
  NullAllFrameStores();
}

void MCUVideoMixer::RemoveVideoSourceByPos(int pos, bool flag)
{
  //PWaitAndSignal m(mutex);

  VideoMixPosition *r = vmpList->next;
  while(r != NULL)
  {
    VideoMixPosition & vmp = *r;
    if (vmp.n == pos ) 
    {
     NullRectangle(vmp.xpos,vmp.ypos,vmp.width,vmp.height,vmp.border);
     if(flag) { VMPListDelVMP(r); delete r; } // static pos
     else { vmp.status = 0; vmp.id = (void *)(long)pos; } // vad pos
     return;
    }
    r = r->next;
  }
}

void MCUVideoMixer::Shuffle()
{
  //PWaitAndSignal m(mutex);
  if(vmpList->next==NULL) return;
  VideoMixPosition * v = vmpList->next;
  unsigned n=OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum;
  unsigned * used = new unsigned [n];
  unsigned done=0;

  while(v!=NULL)
  {
    unsigned ec;
    for(ec=0;ec<10000;ec++)
    {

      unsigned r = rand()%n;

      unsigned i;
      for(i=0;i<done;i++) if(used[i]==r) break;
      if(used[i]!=r)
      {
        used[done]=r;
        v->n=r;
        VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[r];
        v->xpos=o.posx;
        v->ypos=o.posy;
        v->width=o.width;
        v->height=o.height;
        v->border=o.border;
#if USE_FREETYPE
        RemoveSubtitles(*v);
#endif
        done++;
        v=v->next;
        break;
      }
    }
    if(ec>=10000)
    {
      PTRACE(1,"Conference\tRandom generator failed");
      delete[] used;
      return;
    }
  }
  delete[] used;
  NullAllFrameStores();
}

void MCUVideoMixer::Scroll(bool reverse)
{
  //PWaitAndSignal m(mutex);
  if(vmpList->next==NULL) return;
  unsigned n=OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum;
  if(n<2) return;
  VideoMixPosition * v = vmpList->next;
  while(v!=NULL)
  {
    if(reverse)v->n=(v->n+n-1)%n; else v->n=(v->n+1)%n;
    VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[v->n];
    v->xpos=o.posx; v->ypos=o.posy; v->width=o.width; v->height=o.height; v->border=o.border;
#if USE_FREETYPE
    RemoveSubtitles(*v);
#endif
    v=v->next;
  }
  NullAllFrameStores();
}

Id MCUVideoMixer::GetHonestId(int pos)
{ //PWaitAndSignal m(mutex);
  VideoMixPosition *r = vmpList->next;
  while(r!=NULL)
  { VideoMixPosition & vmp = *r;
    if (vmp.n == pos ) return vmp.id;
    r=r->next;
  }
  return NULL;
}

void MCUVideoMixer::Exchange(int pos1, int pos2)
{
  //PWaitAndSignal m(mutex);

  unsigned layoutCapacity = OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum;
  if(((unsigned)pos1>=layoutCapacity)||((unsigned)pos2>=layoutCapacity)) return;

  VideoMixPosition * v1 = VMPListFindVMP(pos1);
  VideoMixPosition * v2 = VMPListFindVMP(pos2);
  if((v1==NULL)&&(v2==NULL)) return;

  if(v2==NULL) {pos1=pos2; v2=v1; v1=NULL;} // lazy to type the following twice

  if(v1==NULL)
  {
    VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[pos1];
    NullRectangle(v2->xpos, v2->ypos, v2->width, v2->height, v2->border);
    v2->xpos=o.posx;
    v2->ypos=o.posy;
    v2->width=o.width;
    v2->height=o.height;
    v2->border=o.border;
    v2->n=pos1;
#if USE_FREETYPE
    RemoveSubtitles(*v2);
#endif
    return;
  }

  Id id0=v1->id;
  PString tn0=v1->endpointName;
  int t=v1->type, st=v1->status;

  v1->id           = v2->id;
  v1->type         = v2->type;
  v1->status       = v2->status;
  v1->endpointName = v2->endpointName;
#if USE_FREETYPE
  RemoveSubtitles(*v1);
#endif
  if( (((unsigned long)v1->id)&(~(unsigned long)255)) < 100) NullRectangle(v1->xpos, v1->ypos, v1->width, v1->height, v1->border);

  v2->id=id0;
  v2->type         = t;
  v2->status       = st;
  v2->endpointName=tn0;
#if USE_FREETYPE
  RemoveSubtitles(*v2);
#endif
  if( (((unsigned long)v2->id)&(~(unsigned long)255)) < 100) NullRectangle(v2->xpos, v2->ypos, v2->width, v2->height, v2->border);
}


void MCUVideoMixer::Revert()
{
  //PWaitAndSignal m(mutex);
  if(vmpList->next==NULL) return;
  unsigned n=OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum;
  if(n<2) return;
  VideoMixPosition * v = vmpList->next;
  while(v!=NULL)
  { v->n=n-v->n-1;
    VMPCfgOptions & o = OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[v->n];
    v->xpos=o.posx; v->ypos=o.posy; v->width=o.width; v->height=o.height; v->border=o.border;
#if USE_FREETYPE
    RemoveSubtitles(*v);
#endif
    v=v->next;
  }
  NullAllFrameStores();
}

OpalBaseMixer::Stream * MCUVideoMixer::CreateStream()
{
  return new VideoStream(*this);
}

void MCUVideoMixer::InsertVideoFrame(const StreamMap_T::iterator & it, unsigned x, unsigned y, unsigned w, unsigned h)
{
  VideoStream * vid = dynamic_cast<VideoStream *>(it->second);
  if (vid != NULL)
    vid->InsertVideoFrame(x, y, w, h);
}


bool MCUVideoMixer::MixStreams(RTP_DataFrame & frame)
{
  
  if (!MixVideo())
    return false;

  frame.SetPayloadSize(GetOutputSize());
  PluginCodec_Video_FrameHeader * video = (PluginCodec_Video_FrameHeader *)frame.GetPayloadPtr();
  video->width = m_width;
  video->height = m_height;
  PINDEX amount = video->width*video->height*3/2;
  ConferenceManager & cm = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  cm.OnOutgoingVideo(m_info.m_name, OpalVideoFrameDataPtr(video),video->width,video->height,amount);

  //memcpy(OpalVideoFrameDataPtr(video), m_frameStore, m_frameStore.GetSize());
  
  return true;
}


void MCUVideoMixer::InsertVideoSubFrame(const StreamMap_T::iterator & it,PINDEX pos)
{
  VideoStream * vid = dynamic_cast<VideoStream *>(it->second);
  if (vid != NULL){
    vid->WriteSubFrame(pos);
    
  }
}

void MCUVideoMixer::VideoSplitLines(void * dst, unsigned fw, unsigned fh){
 unsigned int i;
 BYTE * d = (BYTE *)dst;
 for(i=1;i<fh-1;i++){
  if(d[i*fw]>127)d[i*fw]=255;else if(d[i*fw]<63)d[i*fw]=64; else d[i*fw]<<=1;
  d[i*fw+fw-1]>>=1;
 }
 for(i=1;i<fw-1;i++){
  if(d[i]>127)d[i]=255;else if(d[i]<63)d[i]=64; else d[i]<<=1;
  d[(fh-1)*fw+i]>>=1;
 }
 if(d[0]>127)d[0]=255;else if(d[0]<63)d[0]=64; else d[0]<<=1;
 d[fw-1]>>=2;
 d[(fh-1)*fw]>>=2;
 d[(fh-1)*fw+fw-1]>>=1;
 return;
}

void MCUVideoMixer::ResizeYUV420P(const void * _src, void * _dst, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh)
{
  if(sw==dw && sh==dh) // same size
    memcpy(_dst,_src,dw*dh*3/2);

  else if(sw==CIF16_WIDTH && sh==CIF16_HEIGHT && dw==TCIF_WIDTH    && dh==TCIF_HEIGHT)   // CIF16 -> TCIF
    ConvertCIF16ToTCIF(_src,_dst);
  else if(sw==CIF16_WIDTH && sh==CIF16_HEIGHT && dw==Q3CIF16_WIDTH && dh==Q3CIF16_HEIGHT)// CIF16 -> Q3CIF16
    ConvertCIF16ToQ3CIF16(_src,_dst);
  else if(sw==CIF16_WIDTH && sh==CIF16_HEIGHT && dw==CIF4_WIDTH    && dh==CIF4_HEIGHT)   // CIF16 -> CIF4
    ConvertCIF16ToCIF4(_src,_dst);
  else if(sw==CIF16_WIDTH && sh==CIF16_HEIGHT && dw==Q3CIF4_WIDTH  && dh==Q3CIF4_HEIGHT) // CIF16 -> Q3CIF4
    ConvertCIF16ToQ3CIF4(_src,_dst);
  else if(sw==CIF16_WIDTH && sh==CIF16_HEIGHT && dw==CIF_WIDTH     && dh==CIF_HEIGHT)    // CIF16 -> CIF
    ConvertCIF16ToCIF(_src,_dst);

  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==CIF16_WIDTH  && dh==CIF16_HEIGHT)  // CIF4 -> CIF16
    ConvertCIF4ToCIF16(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==TCIF_WIDTH   && dh==TCIF_HEIGHT)   // CIF4 -> TCIF
    ConvertCIF4ToTCIF(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==TQCIF_WIDTH  && dh==TQCIF_HEIGHT)  // CIF4 -> TQCIF
    ConvertCIF4ToTQCIF(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==CIF_WIDTH    && dh==CIF_HEIGHT)    // CIF4 -> CIF
    ConvertCIF4ToCIF(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==Q3CIF4_WIDTH && dh==Q3CIF4_HEIGHT) // CIF4 -> Q3CIF4
    ConvertCIF4ToQ3CIF4(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==QCIF_WIDTH   && dh==QCIF_HEIGHT)   // CIF4 -> QCIF
    ConvertCIF4ToQCIF(_src,_dst);
  else if(sw==CIF4_WIDTH && sh==CIF4_HEIGHT && dw==Q3CIF_WIDTH  && dh==Q3CIF_HEIGHT)  // CIF4 -> CIF16
    ConvertCIF4ToQ3CIF(_src,_dst);

  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==CIF4_WIDTH   && dh==CIF4_HEIGHT)   // CIF -> CIF4
    ConvertCIFToCIF4(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==TQCIF_WIDTH  && dh==TQCIF_HEIGHT)  // CIF -> TQCIF
    ConvertCIFToTQCIF(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==TQCIF_WIDTH  && dh==TQCIF_HEIGHT)  // CIF -> TSQCIF
    ConvertCIFToTSQCIF(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==Q3CIF_WIDTH  && dh==Q3CIF_HEIGHT)  // CIF -> Q3CIF
    ConvertCIFToQ3CIF(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==QCIF_WIDTH   && dh==QCIF_HEIGHT)   // CIF -> QCIF
    ConvertCIFToQCIF(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==SQ3CIF_WIDTH && dh==SQ3CIF_HEIGHT) // CIF -> SQ3CIF
    ConvertCIFToSQ3CIF(_src,_dst);
  else if(sw==CIF_WIDTH && sh==CIF_HEIGHT && dw==SQCIF_WIDTH  && dh==SQCIF_HEIGHT)  // CIF -> SQCIF
    ConvertCIFToSQCIF(_src,_dst);

  else if(sw==QCIF_WIDTH && sh==QCIF_HEIGHT && dw==CIF4_WIDTH && dh==CIF4_HEIGHT) // QCIF -> CIF4
    ConvertQCIFToCIF4(_src,_dst);
  else if(sw==QCIF_WIDTH && sh==QCIF_HEIGHT && dw==CIF_WIDTH && dh==CIF_HEIGHT)   // QCIF -> CIF
    ConvertQCIFToCIF(_src,_dst);

  else if((sw<<1)==dw && (sh<<1)==dh) // needs 2x zoom
    Convert1To2(_src, _dst, sw, sh);
  else if((dw<<1)==sw && (dh<<1)==sh) // needs 2x reduce
    Convert2To1(_src, _dst, sw, sh);

  else ConvertFRAMEToCUSTOM_FRAME(_src,_dst,sw,sh,dw,dh);

}

void MCUVideoMixer::ConvertRGBToYUV(BYTE R, BYTE G, BYTE B, BYTE & Y, BYTE & U, BYTE & V)
{
  Y = (BYTE)PMIN(ABS(R *  2104 + G *  4130 + B *  802 + 4096 +  131072) / 8192, 235);
  U = (BYTE)PMIN(ABS(R * -1214 + G * -2384 + B * 3598 + 4096 + 1048576) / 8192, 240);
  V = (BYTE)PMIN(ABS(R *  3598 + G * -3013 + B * -585 + 4096 + 1048576) / 8192, 240);
}

void MCUVideoMixer::FillYUVFrame(void * buffer, BYTE R, BYTE G, BYTE B, int w, int h)
{
  BYTE Y, U, V;
  ConvertRGBToYUV(R, G, B, Y, U, V);

  const int ysize = w*h;
  const int usize = (w>>1)*(h>>1);
  const int vsize = usize;

  memset((BYTE *)buffer + 0,             Y, ysize);
  memset((BYTE *)buffer + ysize,         U, usize);
  memset((BYTE *)buffer + ysize + usize, V, vsize);
}

void MCUVideoMixer::CopyRectFromFrame(const void * _src, void * _dst, int xpos, int ypos, int width, int height, int fw, int fh)
{
 if(xpos+width > fw || ypos+height > fh) return;
 
 BYTE * dst = (BYTE *)_dst;
 BYTE * src = (BYTE *)_src + (ypos * fw) + xpos;

 int y;

  // copy Y
  for (y = 0; y < height; ++y) 
   { memcpy(dst, src, width); dst += width; src += fw; }

  // copy U
//  src = (BYTE *)_src + (fw * fh) + (ypos * fw >> 2) + (xpos >> 1);
  src = (BYTE *)_src + (fw * fh) + ((ypos>>1) * (fw >> 1)) + (xpos >> 1);
  for (y = 0; y < height/2; ++y) 
   { memcpy(dst, src, width/2); dst += width/2; src += fw/2; }

  // copy V
//  src = (BYTE *)_src + (fw * fh) + (fw * fh >> 2) + (ypos * fw >> 2) + (xpos >> 1);
  src = (BYTE *)_src + (fw * fh) + ((fw>>1) * (fh>>1)) + ((ypos>>1) * (fw>>1)) + (xpos >> 1);
  for (y = 0; y < height/2; ++y) 
   { memcpy(dst, src, width/2); dst += width/2; src += fw/2; }
}

void MCUVideoMixer::CopyRFromRIntoR(const void *_s, void * _d, int xp, int yp, int w, int h, int rx_abs, int ry_abs, int rw, int rh, int fw, int fh, int lim_w, int lim_h)
{
 int rx=rx_abs-xp;
 int ry=ry_abs-yp;
 int w0=w/2;
 int ry0=ry/2;
 int rx0=rx/2;
 int fw0=fw/2;
 int rh0=rh/2;
 int rw0=rw/2;
 BYTE * s = (BYTE *)_s + w*ry + rx;
 BYTE * d = (BYTE *)_d + (yp+ry)*fw + xp + rx;
 BYTE * sU = (BYTE *)_s + w*h + ry0*w0 + rx0;
 BYTE * dU = (BYTE *)_d + fw*fh + (yp/2+ry0)*fw0 + xp/2 + rx0;
 BYTE * sV = sU + w0*(h/2);
 BYTE * dV = dU + fw0*(fh/2);

 if(rx+rw>lim_w)rw=lim_w-rx;
 if(rx0+rw0>lim_w/2)rw0=lim_w/2-rx0;
 if(ry+rh>lim_h)rh=lim_h-ry;
 if(ry0+rh0>lim_h/2)rh0=lim_h/2-ry0;

 if(rx&1){ dU++; sU++; dV++; sV++; }
 for(int i=ry;i<ry+rh;i++){
   memcpy(d,s,rw); s+=w; d+=fw;
   if(!(i&1)){
     memcpy(dU,sU,rw0); sU+=w0; dU+=fw0;
     memcpy(dV,sV,rw0); sV+=w0; dV+=fw0;
   }
 }

}

void MCUVideoMixer::CopyRectIntoFrame(const void * _src, void * _dst, int xpos, int ypos, int width, int height, int fw, int fh)
{
 if(xpos+width > fw || ypos+height > fh) return;
 
 BYTE * src = (BYTE *)_src;
 BYTE * dst = (BYTE *)_dst + (ypos * fw) + xpos;

 int y;

  // copy Y
  for (y = 0; y < height; ++y) 
   { memcpy(dst, src, width); src += width; dst += fw; }

  // copy U
//  dst = (BYTE *)_dst + (fw * fh) + (ypos * fw >> 2) + (xpos >> 1);
  dst = (BYTE *)_dst + (fw * fh) + ((ypos>>1) * (fw>>1)) + (xpos >> 1);
  for (y = 0; y < height/2; ++y) 
   { memcpy(dst, src, width/2); src += width/2; dst += fw/2; }

  // copy V
//  dst = (BYTE *)_dst + (fw * fh) + (fw * fh >> 2) + (ypos * fw >> 2) + (xpos >> 1);
  dst = (BYTE *)_dst + (fw * fh) + ((fw>>1) * (fh>>1)) + ((ypos>>1) * (fw>>1)) + (xpos >> 1);
  for (y = 0; y < height/2; ++y) 
   { memcpy(dst, src, width/2); src += width/2; dst += fw/2; }
}

int MCUVideoMixer::GetPositionSet()
{
  return specialLayout;

}

void MCUVideoMixer::NullAllFrameStores()
{
  //PWaitAndSignal m(mutex);

  time_t inactiveSign = time(NULL) - FRAMESTORE_TIMEOUT;
  VideoFrameStoreList::VideoFrameStoreListMapType theCopy(frameStores.videoFrameStoreList);
  for (VideoFrameStoreList::VideoFrameStoreListMapType::iterator r=theCopy.begin(), e=theCopy.end(); r!=e; ++r)
  { VideoFrameStoreList::FrameStore & vf = *(r->second);
    if(vf.lastRead<inactiveSign)
    {
#if USE_FREETYPE
      DeleteSubtitlesByFS(vf.width,vf.height);
#endif
      delete r->second; r->second=NULL;
      frameStores.videoFrameStoreList.erase(r->first);
      continue;
    }
    if(vf.width<2 || vf.height<2) continue; // minimum size 2*2
    FillYUVFrame(vf.data.GetPointer(), 0, 0, 0, vf.width, vf.height);
    vf.valid=1;
  }

  for (unsigned i=0; i<OpalMCUEIE::vmcfg.vmconf[specialLayout].splitcfg.vidnum; i++) //slow, fix it
  if(OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i].border)
  {
    VMPCfgOptions & vmpcfg=OpalMCUEIE::vmcfg.vmconf[specialLayout].vmpcfg[i];
    NullRectangle(vmpcfg.posx, vmpcfg.posy, vmpcfg.width, vmpcfg.height, vmpcfg.border);
  }
}
void MCUVideoMixer::CalcVideoSplitSize(unsigned int imageCount, int & subImageWidth, int & subImageHeight, int & cols, int & rows)
{
}

void MCUVideoMixer::NullRectangle(int x, int y, int w, int h, bool border)
{ //PWaitAndSignal m(mutex);
  time_t inactiveSign = time(NULL) - FRAMESTORE_TIMEOUT;
  VideoFrameStoreList::VideoFrameStoreListMapType theCopy(frameStores.videoFrameStoreList);
  for (VideoFrameStoreList::VideoFrameStoreListMapType::iterator r=theCopy.begin(), e=theCopy.end(); r!=e; ++r)
  { VideoFrameStoreList::FrameStore & vf = *(r->second);
    if(vf.lastRead<inactiveSign)
    {
#if USE_FREETYPE
      DeleteSubtitlesByFS(vf.width,vf.height);
#endif
      delete r->second; r->second=NULL;
      frameStores.videoFrameStoreList.erase(r->first);
      continue;
    }
    if(vf.width<2 || vf.height<2) continue; // minimum size 2*2
    int pw=w*vf.width/CIF4_WIDTH; // pixel w&h of vmp-->fs
    int ph=h*vf.height/CIF4_HEIGHT;
    if(pw<2 || ph<2) continue; //PINDEX amount=pw*ph*3/2;
    imageStores_operational_size(pw,ph,_IMGST);
    const void *ist = imageStore.GetPointer();
//    if (!OpalMCUEIE::Current().GetPreMediaFrame(imageStore.GetPointer(), pw, ph, amount))
    FillYUVFrame(imageStore.GetPointer(), 0, 0, 0, pw, ph);
    if (border) VideoSplitLines(imageStore.GetPointer(), pw, ph);
    int px=x*vf.width/CIF4_WIDTH; // pixel x&y of vmp-->fs
    int py=y*vf.height/CIF4_HEIGHT;
    CopyRectIntoFrame(ist,vf.data.GetPointer(),px,py,pw,ph,vf.width,vf.height);
//    frameStores.InvalidateExcept(vf.width, vf.height);
    vf.valid=1;
  }
}

MCUVideoMixer::VideoMixPosition::VideoMixPosition(StreamID id,  unsigned x,  unsigned y, unsigned w, unsigned h)
  : id(id)
  , xpos(x)
  , ypos(y)
  , width(w)
  , height(h)
{ 
  status = 0;
  type = 0;
  chosenVan = 0;
  prev = NULL;
  next = NULL;
  border = true;
}

MCUVideoMixer::VideoMixPosition::~VideoMixPosition()
{
}

void MCUVideoMixer::VMPListInit()
{ 
  vmpList = new VideoMixPosition(0); 
  vmpNum = 0; 
}

void MCUVideoMixer::VMPListInsVMP(VideoMixPosition *pVMP)
{
  VideoMixPosition *vmpListMember = vmpList->next;
  while(vmpListMember!=NULL)
  {
	if(vmpListMember == pVMP) 
	{
	  PTRACE(3, "VMP " << pVMP->n << " already in list"); 
	  return;
	}
   vmpListMember = vmpListMember->next;
  }

 vmpListMember = vmpList->next;
 while(vmpListMember!=NULL)
 {
   if(vmpListMember->id == pVMP->id) 
   {
	 PTRACE(1, "VMP " << pVMP->n << " duplicate key error: " << pVMP->id); 
	 return;
   }
   vmpListMember = vmpListMember->next;
 }

 pVMP->prev = vmpList; 
 pVMP->next = vmpList->next;
 if(vmpList->next != NULL) pVMP->next->prev = pVMP;
 vmpList->next = pVMP;
 vmpNum++;
}

PString MCUVideoMixer::VMPListScanJS() 
{
  PStringStream r, q;
  r << "{";
  q=r;
  VideoMixPosition *vmp = vmpList->next;
  while(vmp!=NULL)
  {
	r << vmp->n << ":" << (long)(vmp->id);
	q << vmp->n << ":" << vmp->type;
	vmp = vmp->next;
	if(vmp!=NULL) { r << ","; q << ","; }
  }
  r << "}," << q << "}";
  return r;
}

void MCUVideoMixer::VMPListAddVMP(VideoMixPosition *pVMP)
{
  VideoMixPosition *vmpListMember;
  VideoMixPosition *vmpListLastMember=NULL;
  vmpListMember = vmpList->next;
  while(vmpListMember!=NULL)
  {
	if(vmpListMember == pVMP)
	{
	  PTRACE(3, "VMP " << pVMP->n << " already in list"); 
	  return;
	}
	vmpListMember = vmpListMember->next;
  }
  vmpListMember = vmpList->next;
  while(vmpListMember!=NULL)
  {
	if(vmpListMember->id == pVMP->id)
	{
	  PTRACE(1, "VMP " << pVMP->n << " duplicate key error: " << pVMP->id); 
	  return;
	}
	vmpListLastMember=vmpListMember;
	vmpListMember = vmpListMember->next; 
  }
  if(vmpListLastMember!=NULL)
  {
	vmpListLastMember->next=pVMP;
	pVMP->prev = vmpListLastMember;
  }
  if(vmpList->next == NULL)
  {
	vmpList->next=pVMP;
	pVMP->prev = vmpList;
  }
  pVMP->next = NULL;
  vmpNum++;
}

void MCUVideoMixer::VMPListDelVMP(VideoMixPosition *pVMP)
{
 if(pVMP->next!=NULL) pVMP->next->prev=pVMP->prev;
 pVMP->prev->next=pVMP->next;
 vmpNum--;
}

void MCUVideoMixer::VMPListClear()
{
 VideoMixPosition *vmpListMember;
 while(vmpList->next!=NULL)
 {
  vmpListMember = vmpList->next;
  vmpList->next=vmpListMember->next;
  delete vmpListMember;
 }
 vmpNum = 0;
}

MCUVideoMixer::VideoMixPosition * MCUVideoMixer::VMPListFindVMP(Id id)
{
 
 VideoMixPosition *vmpListMember = vmpList->next;
 while(vmpListMember!=NULL)
 {
  if(vmpListMember->id == id) { return vmpListMember;}
  vmpListMember = vmpListMember->next;
 }
 
 return NULL;
}

MCUVideoMixer::VideoMixPosition * MCUVideoMixer::VMPListFindVMP(int pos)
{
 VideoMixPosition *vmpListMember = vmpList->next;
 while(vmpListMember!=NULL)
 {
  if(vmpListMember->n == pos) return vmpListMember;
  vmpListMember = vmpListMember->next;
 }
 return NULL;
}

MCUVideoMixer::VideoStream::VideoStream(MCUVideoMixer & mixer)
  : m_mixer(mixer)
{
}

void MCUVideoMixer::VideoStream::QueuePacket(const RTP_DataFrame & rtp)
{
  m_queue.push(rtp);
}


void MCUVideoMixer::VideoStream::InsertVideoFrame(unsigned x, unsigned y, unsigned w, unsigned h)
{
  if (m_queue.empty())
    return;

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)m_queue.front().GetPayloadPtr();

  /*PTRACE(DETAIL_LOG_LEVEL, "Copying video: " << header->width << 'x' << header->height
         << " -> " << x << ',' << y << '/' << w << 'x' << h);*/

  PColourConverter::CopyYUV420P(0, 0, header->width, header->height,
                                header->width, header->height, OpalVideoFrameDataPtr(header),
                                x, y, w, h,
                                m_mixer.m_width, m_mixer.m_height, m_mixer.m_frameStore.GetPointer(),
                                PVideoFrameInfo::eScale);

  /* To avoid continual build up of frames in queue if input frame rate
     greater than mixer frame, we flush the queue, but keep one to allow for
     slight mismatches in timing when frame rates are identical. */
  do {
    m_queue.pop();
  } while (m_queue.size() > 1);
}

void MCUVideoMixer::VideoStream::WriteSubFrame(PINDEX pos)
{
  
   if (m_queue.empty())
    return;
  
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)m_queue.front().GetPayloadPtr();
  PINDEX amount = header->width*header->height*3/2;
  
  ConferenceManager & cm = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  cm.OnIncomingVideo(m_mixer.m_info.m_name,(void*)pos,OpalVideoFrameDataPtr(header),header->width,header->height,amount);
  do {
    m_queue.pop();
  } while (m_queue.size() > 1);
}


//////////////////////////////////////////////////////////////////////////////////////////
ConferenceConnection::ConferenceConnection(PSafePtr<OpalMixerNode> node,
                                                        OpalCall & call,
                                                 ConferenceManager & ep,
                                                        void * userData,
                                                       unsigned options,
                          OpalConnection::StringOptions * stringOptions)
  : OpalMixerConnection(node, call, ep, userData, options, stringOptions)
  , m_cmanager(ep)
{
}

/*OpalMediaStream * ConferenceConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                         unsigned sessionID,
                                                         PBoolean isSource)
{
  bool autoDelete;
  PVideoOutputDevice * outDevice;	  
  CreateVideoOutputDevice(mediaFormat, false, outDevice, autoDelete);
  
  return new ConferenceMediaStream(*this, mediaFormat, sessionID, isSource, m_node, false,outDevice,autoDelete);
  
}*/
//////////////////////////////////////////////////////////////////////////////////////////

/*ConferenceMediaStream::ConferenceMediaStream(ConferenceConnection  & conn,
                                             const OpalMediaFormat & format,
                                             unsigned sessionID,
                                             bool isSource,
                                             PSafePtr<OpalMixerNode> node,
                                             bool listenOnly,
                                             PVideoOutputDevice * out,
                                             bool delOut)
  : OpalMixerMediaStream(conn, format, sessionID, isSource, node, listenOnly)
  , m_connection(conn)
  , m_outputDevice(out)
  , m_autoDeleteOutput(delOut)
  
{
}

PBoolean ConferenceMediaStream::WritePacket(RTP_DataFrame & packet)
{   
  //OpalMediaStream::WritePacket(packet);
  return IsOpen() && m_node->WritePacket(*this, packet);
}

PBoolean ConferenceMediaStream::Open()
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
  
  SetPaused(IsSink() && m_listenOnly);
  
  if (!IsPaused() && !m_node->AttachStream(this))
    return false;

  return OpalMediaStream::Open();
}

PBoolean ConferenceMediaStream::WriteData(const BYTE * data, PINDEX length, PINDEX & written)
{
  if (!IsOpen())
    return false;
  
  if (IsSource()) {
    PTRACE(1, "Tried to write to source media stream");
    return false;
  }

  PWaitAndSignal mutex(m_devicesMutex);

  written = length;
  
  if (data == NULL)
    return true;

  const OpalVideoTranscoder::FrameHeader * frame = (const OpalVideoTranscoder::FrameHeader *)data;

  if (m_outputDevice == NULL) {
    PTRACE(1, "Output device is null");
    return false;
  }

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
  
  return true;
}

void ConferenceMediaStream::InternalClose()
{
  if (m_outputDevice != NULL) {
    if (m_autoDeleteOutput)
      m_outputDevice->Close();
    else
      m_outputDevice->Stop();
  }
  m_node->DetachStream(this);
}*/
//////////////////////////////////////////////////////////////////////////////////////////
