/*
 * videomixer.cxx
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

#include "main.h"

////////////////////////////////////////////////////////////////////////////////
extern "C" {
  #include <SDL.h>
};
#define P_MCU_VIDEO_DRIVER "MCU"
#define P_MCU_VIDEO_DEVICE "MCU"

#include <ptclib/vsdl.h>
class PSDL_System
{
  public:
    static PSDL_System & GetInstance()
    {
      static PSDL_System instance;
      return instance;
    }


    enum UserEvents {
      e_Open,
      e_Close,
      e_SetFrameSize,
      e_SetFrameData,
      e_ForceQuit
    };

  
    void ForceQuit()
    {
      PTRACE(3, "Forcing quit of event thread");
      
      SDL_Event sdlEvent;
      sdlEvent.type = SDL_USEREVENT;
      sdlEvent.user.code = e_ForceQuit;
      sdlEvent.user.data1 = NULL;
      ::SDL_PushEvent(&sdlEvent);
    }
  
#ifdef P_MACOSX
  void ReturnToApplicationMain()
  {
    PProcess::Current().InternalMain();
    ForceQuit();
  }
  
  bool ApplicationMain()
  {
    if (m_thread != NULL)
      return false;
    
    m_thread = PThread::Current();

    new PThreadObj<PSDL_System>(*this, &PSDL_System::ReturnToApplicationMain, true, PProcess::Current().GetName());
    MainLoop();
    return true;
  }
#endif

    void Run()
    {
      if (m_thread == NULL) {
        PTRACE(3, "Starting event thread.");
        m_thread = new PThreadObj<PSDL_System>(*this, &PSDL_System::MainLoop, true, "SDL");
        m_started.Wait();
      }
    }


  private:
    PThread     * m_thread;
    PSyncPoint    m_started;
    bool          m_initVideo;
  
    typedef std::set<PVideoOutputDevice_SDL *> DeviceList;
    DeviceList m_devices;

    PSDL_System()
      : m_thread(NULL)
      , m_initVideo(true)
    {
    }

    ~PSDL_System()
    {
      ForceQuit();
    }

    virtual void MainLoop()
    {
#if PTRACING
      PTRACE(4, "Start of event thread");

      SDL_version hdrVer, binVer;
      SDL_VERSION(&hdrVer);
      SDL_GetVersion(&binVer);
      PTRACE(3, "Compiled version: "
             << (unsigned)hdrVer.major << '.' << (unsigned)hdrVer.minor << '.' << (unsigned)hdrVer.patch
             << "  Run-Time version: "
             << (unsigned)binVer.major << '.' << (unsigned)binVer.minor << '.' << (unsigned)binVer.patch);
#endif

      // initialise the SDL library
      int err = ::SDL_Init(SDL_INIT_EVENTS);
      if (err != 0) {
        PTRACE(1, "Couldn't initialize SDL events: error=" << err << ' ' << ::SDL_GetError());
        return;
      }

      m_started.Signal();

      PTRACE(4, "Starting main event loop");
      SDL_Event sdlEvent;
      do {
        if (!SDL_WaitEvent(&sdlEvent)) {
          PTRACE(1, "Error in WaitEvent: " << ::SDL_GetError());
          break;
        }
      } while (HandleEvent(sdlEvent));

      PTRACE(3, "Quitting SDL");
      for (DeviceList::iterator it = m_devices.begin(); it != m_devices.end(); ++it)
        (*it)->InternalClose();
      
      m_devices.clear();
      
      ::SDL_Quit();

      PTRACE(4, "End of event thread");
    }


    bool HandleEvent(SDL_Event & sdlEvent)
    {
      switch (sdlEvent.type) {
        case SDL_USEREVENT :
        {
          PVideoOutputDevice_SDL * device = reinterpret_cast<PVideoOutputDevice_SDL *>(sdlEvent.user.data1);
          switch (sdlEvent.user.code) {
            case e_Open :
              if (m_initVideo) {
                int err = ::SDL_Init(SDL_INIT_VIDEO);
                if (err != 0)
                  PTRACE(1, "Couldn't initialize SDL video: error=" << err << ' ' << ::SDL_GetError());
                else
                  m_initVideo = false;
              }
              if (device->InternalOpen())
                m_devices.insert(device);
              break;

            case e_Close :
              device->InternalClose();
              if (m_devices.erase(device) == 0)
                PTRACE(2, "Close of unknown device: " << device);
              break;

            case e_SetFrameSize :
              if (m_devices.find(device) != m_devices.end())
                device->InternalSetFrameSize();
              break;

            case e_SetFrameData :
              if (m_devices.find(device) != m_devices.end())
                device->InternalSetFrameData();
              break;

            case e_ForceQuit :
              return false;

            default :
              PTRACE(5, "Unhandled user event " << sdlEvent.user.code);
          }
          break;
        }

        case SDL_WINDOWEVENT :
          switch (sdlEvent.window.event) {
            case SDL_WINDOWEVENT_RESIZED :
              PTRACE(4, "Resize window to " << sdlEvent.window.data1 << " x " << sdlEvent.window.data2 << " on " << sdlEvent.window.windowID);
              break;
            
            default :
              PTRACE(5, "Unhandled windows event " << (unsigned)sdlEvent.window.event);
          }
          break;
          
        default :
          PTRACE(5, "Unhandled event " << (unsigned)sdlEvent.type);
      }
      
      return true;
    }
};


///////////////////////////////////////////////////////////////////////

PVideoOutputDevice_MCU::PVideoOutputDevice_MCU(ConferenceManager & cmanager)
  : m_window(NULL)
  , m_renderer(NULL)
  , m_texture(NULL)
  , m_cmanager(cmanager)
{
  m_colourFormat = PVideoFrameInfo::YUV420P();
  PTRACE(5, "Constructed device: " << this);
}


PVideoOutputDevice_MCU::~PVideoOutputDevice_MCU()
{ 
  Close();
  PTRACE(5, "Destroyed device: " << this);
}


PStringArray PVideoOutputDevice_MCU::GetOutputDeviceNames()
{
  return P_MCU_VIDEO_DRIVER;
}


PStringArray PVideoOutputDevice_MCU::GetDeviceNames() const
{
  return GetOutputDeviceNames();
}


PBoolean PVideoOutputDevice_MCU::Open(const PString & name, PBoolean /*startImmediate*/)
{
  Close();

  m_deviceName = name;

  PTRACE(5, "Opening device " << this << ' ' << m_deviceName);
  PSDL_System::GetInstance().Run();
  PostEvent(PSDL_System::e_Open, true);
  return IsOpen();
}


bool PVideoOutputDevice_MCU::InternalOpen()
{
  PWaitAndSignal sync(m_operationComplete, false);

  m_window = SDL_CreateWindow(ParseDeviceNameTokenString("TITLE", "Video Output"),
                              ParseDeviceNameTokenInt("X", SDL_WINDOWPOS_UNDEFINED),
                              ParseDeviceNameTokenInt("Y", SDL_WINDOWPOS_UNDEFINED),
                              GetFrameWidth(), GetFrameHeight(), SDL_WINDOW_RESIZABLE);
  if (m_window == NULL) {
    PTRACE(1, "Couldn't create SDL window: " << ::SDL_GetError());
    return false;
  }
  
  m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_PRESENTVSYNC);
  if (m_renderer == NULL) {
    PTRACE(1, "Couldn't create SDL renderer: " << ::SDL_GetError());
    return false;
  }
  
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

  m_texture = SDL_CreateTexture(m_renderer,
                                SDL_PIXELFORMAT_IYUV,
                                SDL_TEXTUREACCESS_STREAMING,
                                GetFrameWidth(), GetFrameHeight());
  if (m_texture == NULL) {
    PTRACE(1, "Couldn't create SDL texture: " << ::SDL_GetError());
    return false;
  }
  
  PTRACE(3, "Opened window for device: " << this << ' ' << m_deviceName);
  return true;
}


PBoolean PVideoOutputDevice_MCU::IsOpen()
{
  return m_texture != NULL;
}


PBoolean PVideoOutputDevice_MCU::Close()
{
  if (!IsOpen())
    return false;
  
  PTRACE(5, "Closing device: " << this << ' ' << m_deviceName);
  PostEvent(PSDL_System::e_Close, true);
  return true;
}


void PVideoOutputDevice_MCU::InternalClose()
{
  SDL_DestroyTexture(m_texture);   m_texture  = NULL;
  SDL_DestroyRenderer(m_renderer); m_renderer = NULL;
  SDL_DestroyWindow(m_window);     m_window   = NULL;
  PTRACE(3, "Closed window on device " << this << ' ' << m_deviceName);
  m_operationComplete.Signal();
}


PBoolean PVideoOutputDevice_MCU::SetColourFormat(const PString & colourFormat)
{
  if (colourFormat *= PVideoFrameInfo::YUV420P())
    return PVideoOutputDevice::SetColourFormat(colourFormat);

  return false;
}


PBoolean PVideoOutputDevice_MCU::SetFrameSize(unsigned width, unsigned height)
{
  if (width == m_frameWidth && height == m_frameHeight)
    return true;

  /*if (!PVideoOutputDevice::SetFrameSize(width, height))
    return false;*/

  if (IsOpen())
    PostEvent(PSDL_System::e_SetFrameSize, true);

  return true;
}


void PVideoOutputDevice_MCU::InternalSetFrameSize()
{
  PWaitAndSignal sync(m_operationComplete, false);
  
  if (m_renderer == NULL)
    return;
  
  if (m_texture != NULL)
    SDL_DestroyTexture(m_texture);

  m_texture = SDL_CreateTexture(m_renderer,
                                SDL_PIXELFORMAT_IYUV,
                                SDL_TEXTUREACCESS_STREAMING,
                                GetFrameWidth(), GetFrameHeight());
  PTRACE_IF(1, m_texture == NULL, "Couldn't create SDL texture: " << ::SDL_GetError());

  if (m_window)
      SDL_SetWindowSize(m_window, GetFrameWidth(), GetFrameHeight());
}


PBoolean PVideoOutputDevice_MCU::SetFrameData(const FrameData & frameData) 
{
  if (!IsOpen())
    return false;
  
  if (frameData.x != 0 || frameData.y != 0 ||
      frameData.width != m_frameWidth || frameData.height != m_frameHeight ||
      frameData.pixels == NULL || frameData.partialFrame)
    return false;
     
  {
    void * ptr;
    int pitch;
    PWaitAndSignal lock(m_texture_mutex);
    SDL_LockTexture(m_texture, NULL, &ptr, &pitch);
    if (pitch == (int)frameData.width)
      memcpy(ptr, frameData.pixels, frameData.width*frameData.height*3/2);
    SDL_UnlockTexture(m_texture);
  }
  
  PostEvent(PSDL_System::e_SetFrameData, false);
  
  return true;
}


void PVideoOutputDevice_MCU::InternalSetFrameData()
{
  if (m_texture == NULL)
    return;
  
  PWaitAndSignal lock(m_texture_mutex);
  SDL_RenderClear(m_renderer);
  SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
  SDL_RenderPresent(m_renderer);
}

void PVideoOutputDevice_MCU::PostEvent(unsigned code, bool wait)
{
  SDL_Event sdlEvent;
  sdlEvent.type = SDL_USEREVENT;
  sdlEvent.user.code = code;
  sdlEvent.user.data1 = this;
  sdlEvent.user.data2 = NULL;
  if (::SDL_PushEvent(&sdlEvent) < 0) {
    PTRACE(1, "Couldn't post user event " << (unsigned)sdlEvent.user.code << ": " << ::SDL_GetError());
    return;
  }

  PTRACE(5, "Posted user event " << (unsigned)sdlEvent.user.code);
  if (wait)
    PAssert(m_operationComplete.Wait(100000),
            PSTRSTRM("Couldn't process user event " << (unsigned)sdlEvent.user.code));
}

//////////////////////////////////////////////////////////////////////////////////////////
VideoFrameStoreList::~VideoFrameStoreList()
{
  while (videoFrameStoreList.begin() != videoFrameStoreList.end()) {
    FrameStore * vf = videoFrameStoreList.begin()->second;
    delete vf;
    videoFrameStoreList.erase(videoFrameStoreList.begin());
  }
}

VideoFrameStoreList::FrameStore & VideoFrameStoreList::AddFrameStore(int width, int height)
{ 
  VideoFrameStoreListMapType::iterator r = videoFrameStoreList.find(WidthHeightToKey(width, height));
  if (r != videoFrameStoreList.end())
    return *(r->second);
  FrameStore * vf = new FrameStore(width, height);
  videoFrameStoreList.insert(VideoFrameStoreListMapType::value_type(WidthHeightToKey(width, height), vf)); 
  return *vf;
}

VideoFrameStoreList::FrameStore & VideoFrameStoreList::GetFrameStore(int width, int height) 
{
  VideoFrameStoreListMapType::iterator r = videoFrameStoreList.find(WidthHeightToKey(width, height));
  if (r != videoFrameStoreList.end())
    return *(r->second);
  FrameStore * vf = new FrameStore(width, height);
  videoFrameStoreList.insert(VideoFrameStoreListMapType::value_type(WidthHeightToKey(width, height), vf)); 
  return *vf;
}

void VideoFrameStoreList::InvalidateExcept(int w, int h)
{
  VideoFrameStoreListMapType::iterator r;
  for (r = videoFrameStoreList.begin(); r != videoFrameStoreList.end(); ++r) {
    unsigned int key = r->first;
    int kw, kh; KeyToWidthHeight(key, kw, kh);
    r->second->valid = (w == kw) && (h == kh);
  }
}

VideoFrameStoreList::FrameStore & VideoFrameStoreList::GetNearestFrameStore(int width, int height, bool & found)
{
  // see if exact match, and valid
  VideoFrameStoreListMapType::iterator r = videoFrameStoreList.find(WidthHeightToKey(width, height));
  if ((r != videoFrameStoreList.end()) && r->second->valid) {
    found = true;
    return *(r->second);
  }

  // return the first valid framestore
  for (r = videoFrameStoreList.begin(); r != videoFrameStoreList.end(); ++r) {
    if (r->second->valid) {
      found = true;
      return *(r->second);
    }
  }

  // return not found
  found = false;
  return *(videoFrameStoreList.end()->second);
}


VideoMixConfigurator::VideoMixConfigurator(long _w, long _h)
{
  bfw=_w; bfh=_h;
}

VideoMixConfigurator::~VideoMixConfigurator()
{
 for(unsigned ii=0;ii<vmconfs;ii++) { //attempt to delete all
  vmconf[ii].vmpcfg=(VMPCfgOptions *)realloc((void *)(vmconf[ii].vmpcfg),0);
  vmconf[ii].vmpcfg=NULL;
 }
 vmconfs=0;
 vmconf=(VMPCfgLayout *)realloc((void *)vmconf,0);
 vmconf=NULL;
}

void VideoMixConfigurator::go(unsigned frame_width, unsigned frame_height)
{
  bfw=frame_width; bfh=frame_height;
  FILE *fs; unsigned long f_size; char * f_buff;
#ifdef SYS_CONFIG_DIR
  fs = fopen(PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + VMPC_CONFIGURATION_NAME,"r");
#else
  fs = fopen(VMPC_CONFIGURATION_NAME,"r");
#endif
  if(!fs) { cout << "Video Mixer Configurator: ERROR! Can't read file \"" << VMPC_CONFIGURATION_NAME << "\"\n"; return; }
  fseek(fs,0L,SEEK_END); f_size=ftell(fs); rewind(fs);
  f_buff=new char[f_size+1]; 
  if(f_size != fread(f_buff,1,f_size,fs)) { cout << "Video Mixer Configurator: ERROR! Can't read file \"" << VMPC_CONFIGURATION_NAME << "\"\n"; return; }
  f_buff[f_size]=0;
  fclose(fs);

  for(long i=0;i<2;i++){
   fw[i]=bfw; if(fw[i]==0) fw[i]=2;
   fh[i]=bfh; if(fh[i]==0) fh[i]=2;
   strcpy(sopts[i].Id,VMPC_DEFAULT_ID);
   sopts[i].vidnum=VMPC_DEFAULT_VIDNUM;
   sopts[i].mode_mask=VMPC_DEFAULT_MODE_MASK;
   sopts[i].new_from_begin=VMPC_DEFAULT_NEW_FROM_BEGIN;
   sopts[i].reallocate_on_disconnect=VMPC_DEFAULT_REALLOCATE_ON_DISCONNECT;
   sopts[i].mockup_width=VMPC_DEFAULT_MOCKUP_WIDTH;
   sopts[i].mockup_height=VMPC_DEFAULT_MOCKUP_HEIGHT;
   opts[i].posx=VMPC_DEFAULT_POSX;
   opts[i].posy=VMPC_DEFAULT_POSY;
   opts[i].width=VMPC_DEFAULT_WIDTH;
   opts[i].height=VMPC_DEFAULT_HEIGHT;
   opts[i].border=VMPC_DEFAULT_BORDER;

  }
  parser(f_buff,f_size);
  delete f_buff;
  cout << "VideoMixConfigurator: " << VMPC_CONFIGURATION_NAME << " processed: " << vmconfs << " layout(s) loaded.\n";
}

void VideoMixConfigurator::parser(char* &f_buff,long f_size)
{
  lid=0; ldm=0;
  long pos1,pos=0;
  long line=0;
  bool escape=false;
  while(pos<f_size){
   pos1=pos;
   while(((f_buff[pos1]!='\n')||escape)&&(pos1<f_size)){
    if(escape&&(f_buff[pos1]=='\n')) line++;
    escape=((f_buff[pos1]=='\\')&&(!escape));
    pos1++;
   }
   line++;
   if(pos!=pos1)handle_line(f_buff,pos,pos1,line);
   pos=pos1+1;
  }
  if(ldm==1)finalize_layout_desc();
  geometry();
}

void VideoMixConfigurator::block_insert(VMPBlock * & b,long b_n,unsigned x,unsigned y,unsigned w,unsigned h)
{
 if(b_n==0) b=(VMPBlock *)malloc(sizeof(VMPBlock));
 else b=(VMPBlock *)realloc((void *)b,sizeof(VMPBlock)*(b_n+1));
/*
 b[b_n].posx=(x+1)&0xFFFFFE;
 b[b_n].posy=(y+1)&0XFFFFFE;
 b[b_n].width=(w+1)&0xFFFFFE;
 b[b_n].height=(h+1)&0xFFFFFE;
*/
 b[b_n].posx=x;
 b[b_n].posy=y;
 b[b_n].width=w;
 b[b_n].height=h;
// cout << "b.ins[" << x << "," << y << "," << w << "," << h << "]\n";
}

void VideoMixConfigurator::block_erase(VMPBlock * & b,long b_i,long b_n)
{
// cout << "e.bl[" << b_i << "," << b_n << "]\n";
 if(b_n<1) return;
 for(long i=b_i;i<b_n-1;i++)b[i]=b[i+1];
 if(b_n==1) {
  free((void *)b);
  b=NULL;
 }
 else b=(VMPBlock *)realloc((void *)b,sizeof(VMPBlock)*(b_n-1));
}

unsigned VideoMixConfigurator::frame_splitter(VMPBlock * & b,long b_i,long b_n,unsigned x0,unsigned y0,unsigned w0,unsigned h0,unsigned x1,unsigned y1,unsigned w1,unsigned h1)
{
// cout << "f.spl[" << b_i << "," << b_n << "," << x0 << "," << y0 << "," << w0 << "," << h0 << "," << x1 << "," << y1 << "," << w1 << "," << h1 << "]\n";
 unsigned x00=x0+w0; unsigned y00=y0+h0; unsigned x11=x1+w1; unsigned y11=y1+h1;
 geometry_changed=true;
 if((x1<=x0)&&(y1<=y0)&&(x11>=x00)&&(y11>=y00)){ // no visible blocks
//cout << "[16]";
  block_erase(b,b_i,b_n);
  return (b_n-1);
 }
 if((x1>x0)&&(y1>y0)&&(x11<x00)&&(y11<y00)){ // somewhere inside completely
//cout << "[1]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  //  //
  block_insert(b,b_n  ,x0   ,y1   ,x1-x0      ,h1         );  //////
  block_insert(b,b_n+1,x11  ,y1   ,x00-x11    ,h1         );
  block_insert(b,b_n+2,x0   ,y11  ,w0         ,y00-y11    );
  return b_n+3;
 }

 if((x1>x0)&&(x11<x00)&&(y1<=y0)&&(y11>y0)&&(y11<y00)){ // top middle intersection
//cout << "[2]";
  block_erase(b,b_i,b_n);                                     //  //
  block_insert(b,b_n-1,x0   ,y0   ,x1-x0      ,y11-y0     );  //////
  block_insert(b,b_n  ,x1+w1,y0   ,x00-x11    ,y11-y0     );  //////
  block_insert(b,b_n+1,x0   ,y11  ,w0         ,y00-y11    );
  return b_n+2;
 }
 if((x1>x0)&&(x11<x00)&&(y1>y0)&&(y1<y00)&&(y11>=y00)){ // bottom middle intersection
//cout << "[3]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  //////
  block_insert(b,b_n  ,x0   ,y1   ,x1-x0      ,y00-y1     );  //  //
  block_insert(b,b_n+1,x1+w1,y1   ,x00-x11    ,y00-y1     );
  return b_n+2;
 }
 if((x1<=x0)&&(x11>x0)&&(x11<x00)&&(y1>y0)&&(y11<y00)){ // middle left intersection
//cout << "[4]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );    ////
  block_insert(b,b_n  ,x11  ,y1   ,x00-x11    ,h1         );  //////
  block_insert(b,b_n+1,x0   ,y11  ,w0         ,y00-y11    );
  return b_n+2;
 }
 if((x11>=x00)&&(x1<x00)&&(x1>x0)&&(y1>y0)&&(y11<y00)){ // middle right intersection
//cout << "[5]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  ////
  block_insert(b,b_n  ,x0   ,y1   ,x1-x0      ,h1         );  //////
  block_insert(b,b_n+1,x0   ,y11  ,w0         ,y00-y11    );
  return b_n+2;
 }
 if((x1<=x0)&&(x11>x0)&&(x11<x00)&&(y1<=y0)&&(y11>y0)&&(y11<y00)){ // top left intersection
//cout << "[6]";
  block_erase(b,b_i,b_n);                                       ////
  block_insert(b,b_n-1,x11  ,y0   ,x00-x11    ,y11-y0     );  //////
  block_insert(b,b_n  ,x0   ,y11  ,w0         ,y00-y11    );  //////
  return b_n+1;
 }
 if((x11>=x00)&&(x1<x00)&&(x1>x0)&&(y1<=y0)&&(y11>y0)&&(y11<y00)){ // top right intersection
//cout << "[7]";
  block_erase(b,b_i,b_n);                                     ////
  block_insert(b,b_n-1,x0   ,y0   ,x1-x0      ,y11-y0     );  //////
  block_insert(b,b_n  ,x0   ,y11  ,w0         ,y00-y11    );  //////
  return b_n+1;
 }
 if((x1<=x0)&&(x11>x0)&&(x11<x00)&&(y11>=y00)&&(y1<y00)&&(y1>y0)){ // bottom left intersection
//cout << "[8]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  //////
  block_insert(b,b_n  ,x11  ,y1   ,x00-x11    ,y00-y1     );    ////
  return b_n+1;
 }
 if((x11>=x00)&&(x1<x00)&&(x1>x0)&&(y11>=y00)&&(y1<y00)&&(y1>y0)){ // bottom right intersection
//cout << "[9]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  //////
  block_insert(b,b_n  ,x0   ,y1   ,x1-x0      ,y00-y1     );  ////
  return b_n+1;
 }
 if((x1<=x0)&&(x11>=x00)&&(y1<=y0)&&(y11>y0)&&(y11<y00)){ // all-over top intersection
//cout << "[10]";
  block_erase(b,b_i,b_n);
  block_insert(b,b_n-1,x0   ,y11  ,w0         ,y00-y11    );  //////
  return b_n;                                                 //////
 }
 if((x1<=x0)&&(x11>=x00)&&(y11>=y00)&&(y1<y00)&&(y1>y0)){ // all-over bottom intersection
//cout << "[11]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );  //////
  return b_n;
 }
 if((x1<=x0)&&(x11>x0)&&(x11<x00)&&(y1<=y0)&&(y11>=y00)){ // all-over left intersection
//cout << "[12]";
  block_erase(b,b_i,b_n);                                       ////
  block_insert(b,b_n-1,x11  ,y0   ,x00-x11    ,h0         );    ////
  return b_n;                                                   ////
 }
 if((x11>=x00)&&(x1<x00)&&(x1>x0)&&(y1<=y0)&&(y11>=y00)){ // all-over left intersection
//cout << "[13]";
  block_erase(b,b_i,b_n);                                     ////
  block_insert(b,b_n-1,x0   ,y0   ,x1-x0      ,h0         );  ////
  return b_n;                                                 ////
 }
 if((x1<=x0)&&(x11>=x00)&&(y1>y0)&&(y11<y00)){ // all-over left-right intersection
//cout << "[14]";
  block_erase(b,b_i,b_n);                                     //////
  block_insert(b,b_n-1,x0   ,y0   ,w0         ,y1-y0      );        
  block_insert(b,b_n  ,x0   ,y11  ,w0         ,y00-y11    );  //////
  return b_n+1;
 }
 if((y1<=y0)&&(y11>=y00)&&(x1>x0)&&(x11<x00)){ // all-over top-bottom intersection
//cout << "[15]";
  block_erase(b,b_i,b_n);                                     //  //
  block_insert(b,b_n-1,x0   ,y0   ,x1-x0      ,h0         );  //  //
  block_insert(b,b_n  ,x11  ,y0   ,x00-x11    ,h0         );  //  //
  return b_n+1;
 }
// cout << "[0]";
 geometry_changed=false;
 return b_n;
}

void VideoMixConfigurator::geometry()
{ // find and store visible blocks of frames
 for(unsigned i=0;i<vmconfs;i++)
 {
   for(unsigned j=0;j<vmconf[i].splitcfg.vidnum;j++)
   { // Create single block 0 for each position first:
     block_insert(vmconf[i].vmpcfg[j].blk,0,vmconf[i].vmpcfg[j].posx,vmconf[i].vmpcfg[j].posy,vmconf[i].vmpcfg[j].width,vmconf[i].vmpcfg[j].height);
     vmconf[i].vmpcfg[j].blks=1;
//     cout << "*ctrl/i: i=" << i << " j=" << j << " posx=" << vmconf[i].vmpcfg[j].blk[0].posx << "\n";
   }
   for(unsigned j=0;j<vmconf[i].splitcfg.vidnum-1;j++) for (unsigned k=j+1; k<vmconf[i].splitcfg.vidnum;k++)
   {
     unsigned bn=vmconf[i].vmpcfg[j].blks; //remember initial value of blocks
     unsigned b0=0; // block index
     while ((b0<bn)&&(b0<vmconf[i].vmpcfg[j].blks)) {
       unsigned b1=frame_splitter(
         vmconf[i].vmpcfg[j].blk,b0,
         vmconf[i].vmpcfg[j].blks,
         vmconf[i].vmpcfg[j].blk[b0].posx,
         vmconf[i].vmpcfg[j].blk[b0].posy,
         vmconf[i].vmpcfg[j].blk[b0].width,
         vmconf[i].vmpcfg[j].blk[b0].height,
         vmconf[i].vmpcfg[k].blk[0].posx,
         vmconf[i].vmpcfg[k].blk[0].posy,
         vmconf[i].vmpcfg[k].blk[0].width,
         vmconf[i].vmpcfg[k].blk[0].height
       );
//       if(b1==vmconf[i].vmpcfg[j].blks)b0++;
       if(!geometry_changed)b0++;
       else vmconf[i].vmpcfg[j].blks=b1;
     }
   }
 }

}

void VideoMixConfigurator::handle_line(char* &f_buff,long pos,long pos1,long line)
{
   long i,pos0=pos;
   bool escape=false;
   for(i=pos;i<pos1;i++) {
    escape=((f_buff[i]=='\\')&&(!escape));
    if(!escape){
     if(f_buff[i]=='#') { // comment
      if(pos!=i)handle_atom(f_buff,pos,i,line,pos-pos0);
	  return;
	 }
	 if(f_buff[i]=='/') if(i<pos1-1) if(f_buff[i+1]=='/') { // comment
      if(pos!=i)handle_atom(f_buff,pos,i,line,pos-pos0);
	  return;
	 }
     if(f_buff[i]==';') {
      if(i!=pos)handle_atom(f_buff,pos,i,line,pos-pos0);
      pos=i+1;
     }
    }
   }
   if(pos!=pos1)handle_atom(f_buff,pos,pos1,line,pos-pos0);
}

void VideoMixConfigurator::handle_atom(char* &f_buff,long pos,long pos1,long line,long lo)
{
   while((f_buff[pos]<33)&&(pos<pos1)) {pos++; lo++;}
   if(pos==pos1) return; //empty atom
   while(f_buff[pos1-1]<33) pos1--; // atom is now trimed: [pos..pos1-1]
   if(f_buff[pos]=='[') handle_layout_descriptor(f_buff,pos,pos1,line,lo);
   else if(f_buff[pos]=='(') handle_position_descriptor(f_buff,pos,pos1,line,lo);
   else handle_parameter(f_buff,pos,pos1,line,lo);
}

void VideoMixConfigurator::handle_layout_descriptor(char* &f_buff,long pos,long pos1,long line,long lo)
{
   if(f_buff[pos1-1]==']'){
    pos++;
    pos1--;
    if(pos1-pos>32) {
     warning(f_buff,line,lo,"too long layout id, truncated",pos,pos+32);
     pos1=pos+32;
    };
    if(ldm==1)finalize_layout_desc();
    if(pos1<=pos) return; //empty layout Id
    initialize_layout_desc(f_buff,pos,pos1,line,lo);
   } else warning(f_buff,line,lo,"bad token",pos,pos1);
//   cout << line << "/" << lo << "\tLAYOUT\t"; for (long i=pos;i<pos1;i++) cout << (char)f_buff[i]; cout << "\n";
}

void VideoMixConfigurator::handle_position_descriptor(char* &f_buff,long pos,long pos1,long line,long lo)
{
   if(f_buff[pos1-1]==')'){
    pos++; pos1--;
    while((f_buff[pos]<33)&&(pos<pos1))pos++;
    if(pos==pos1) { warning(f_buff,line,lo,"incomplete position descriptor",1,0); return; }
    if((f_buff[pos]>'9')||(f_buff[pos]<'0')){ warning(f_buff,line,lo,"error in position description",pos,pos1); return; }
    long pos0=pos;
    while((pos<pos1)&&(f_buff[pos]>='0')&&(f_buff[pos]<='9'))pos++;
    long pos2=pos;
    while((pos<pos1)&&(f_buff[pos]<33))pos++;
    if(f_buff[pos]!=','){ warning(f_buff,line,lo,"unknown character in position descriptor",pos,pos1); return; }
    char t=f_buff[pos2];
    f_buff[pos2]=0;
    opts[1].posx=(atoi((const char*)(f_buff+pos0))*bfw)/fw[1];
//    opts[1].posx=((atoi((const char*)(f_buff+pos0))*bfw)/fw[1]+1)&0xFFFFFE;
    f_buff[pos2]=t;
    pos++;
    if(pos>=pos1){ warning(f_buff,line,lo,"Y-coordinate does not set",1,0); return; }
    while((f_buff[pos]<33)&&(pos<pos1))pos++;
    if(pos==pos1) { warning(f_buff,line,lo,"Y not set",1,0); return; }
    if((f_buff[pos]>'9')||(f_buff[pos]<'0')){ warning(f_buff,line,lo,"error in Y crd. description",pos,pos1); return; }
    pos0=pos;
    while((pos<pos1)&&(f_buff[pos]>='0')&&(f_buff[pos]<='9'))pos++;
    pos2=pos;
    while((pos<pos1)&&(f_buff[pos]<33))pos++;
    if(pos!=pos1){ warning(f_buff,line,lo,"unknown chars in position descriptor",pos,pos1); return; }
    t=f_buff[pos2];
    f_buff[pos2]=0;
    opts[1].posy=(atoi((const char*)(f_buff+pos0))*bfh)/fh[1];
//    opts[1].posy=((atoi((const char*)(f_buff+pos0))*bfh)/fh[1]+1)&0xFFFFFE;
    f_buff[pos2]=t;
   } else warning(f_buff,line,lo,"bad position descriptor",pos,pos1);
   if(pos_n==0)vmconf[lid].vmpcfg=(VMPCfgOptions *)malloc(sizeof(VMPCfgOptions));
   else vmconf[lid].vmpcfg=(VMPCfgOptions *)realloc((void *)(vmconf[lid].vmpcfg),sizeof(VMPCfgOptions)*(pos_n+1));
   vmconf[lid].vmpcfg[pos_n]=opts[1];
//   cout << "Position " << pos_n << ": " << opts[true].posx << "," << opts[true].posy << "\n";
   pos_n++;
}

void VideoMixConfigurator::handle_parameter(char* &f_buff,long pos,long pos1,long line,long lo)
{
   char p[64], v[256];
   bool escape=false;
   long pos0=pos;
   long pos00=pos;
   while((pos<pos1)&&(((f_buff[pos]>32)&&(f_buff[pos]!='='))||escape)&&(pos-pos0<63)){
    p[pos-pos0]=f_buff[pos];
    pos++;
   }
   p[pos-pos0]=0;
   if(pos-pos0>63){ warning(f_buff,line,lo,"parameter name is too long",pos0,pos1); return; }
   while((pos<pos1)&&(f_buff[pos]<33))pos++;
   if(pos==pos1){ warning(f_buff,line,lo,"unknown text",pos0,pos1); return; }
   if(f_buff[pos]!='='){ warning(f_buff,line,lo,"missing \"=\"",pos0,pos1); return; }
   pos++;
   while((pos<pos1)&&(f_buff[pos]<33))pos++;
   escape=false;
   pos0=pos;
   while((pos<pos1)&&((f_buff[pos]>31)||escape)&&(pos-pos0<255)){
    v[pos-pos0]=f_buff[pos];
    pos++;
   }
   v[pos-pos0]=0;
   if(pos-pos0>255){ warning(f_buff,line,lo,"parameter value is too long",pos0,pos1); return; }
   while((pos<pos1)&&(f_buff[pos]<33))pos++;
   if(pos!=pos1) warning(f_buff,line,lo,"unknown characters",pos,pos1);
   option_set((const char *)p,(const char *)v, f_buff, line, lo, pos00, pos0);
}

void VideoMixConfigurator::option_set(const char* p, const char* v, char* &f_buff, long line, long lo, long pos, long pos1)
{
   if(option_cmp((const char *)p,(const char *)"frame_width")){fw[ldm]=atoi(v);if(fw[ldm]<1)fw[ldm]=1;}
   else if(option_cmp((const char *)p,(const char *)"frame_height")){fh[ldm]=atoi(v);if(fh[ldm]<1)fh[ldm]=1;}
   else if(option_cmp((const char *)p,(const char *)"border"))opts[ldm].border=atoi(v);
   else if(option_cmp((const char *)p,(const char *)"mode_mask"))sopts[ldm].mode_mask=atoi(v);
//   else if(option_cmp((const char *)p,(const char *)"tags")){
//     if(strlen(v)<=128)strcpy(sopts[ldm].tags,v); else 
//     warning(f_buff,line,lo,"tags value too long (max 128 chars allowed)",pos,pos1);
//   }
   else if(option_cmp((const char *)p,(const char *)"position_width")) opts[ldm].width=(atoi(v)*bfw)/fw[ldm];
//   else if(option_cmp((const char *)p,(const char *)"position_width")) opts[ldm].width=((atoi(v)*bfw)/fw[ldm]+1)&0xFFFFFE;
   else if(option_cmp((const char *)p,(const char *)"position_height")) opts[ldm].height=(atoi(v)*bfh)/fh[ldm];
//   else if(option_cmp((const char *)p,(const char *)"position_height")) opts[ldm].height=((atoi(v)*bfh)/fh[ldm]+1)&0xFFFFFE;

   else if(option_cmp((const char *)p,(const char *)"reallocate_on_disconnect")) sopts[ldm].reallocate_on_disconnect=atoi(v);
   else if(option_cmp((const char *)p,(const char *)"new_members_first")) sopts[ldm].new_from_begin=atoi(v);
   else if(option_cmp((const char *)p,(const char *)"mockup_width")) sopts[ldm].mockup_width=atoi(v);
   else if(option_cmp((const char *)p,(const char *)"mockup_height")) sopts[ldm].mockup_height=atoi(v);
   else if(option_cmp((const char *)p,(const char *)"minimum_width_for_label"))
   { if(strlen(v)<11) strcpy(sopts[ldm].minimum_width_for_label,v);
     else warning(f_buff,line,lo,"minimum_width_for_label value too long (max 10 chars allowed)",pos,pos1);
   }
   else warning(f_buff,line,lo,"unknown parameter",pos,pos1);
}

bool VideoMixConfigurator::option_cmp(const char* p,const char* str)
{
   if(strlen(p)!=strlen(str))return false;
   for(unsigned i=0;i<strlen(str);i++)if(p[i]!=str[i]) return false;
   return true;
}

void VideoMixConfigurator::warning(char* &f_buff,long line,long lo,const char warn[64],long pos,long pos1)
{
  PStringStream w;
  w << "Warning! " << VMPC_CONFIGURATION_NAME << ":" << line << ":" << lo << ": "<< warn;
  if(pos1>pos)
  {
    w << ": \"";
    for(long i=pos;i<pos1;i++) w << (char)f_buff[i];
    w << "\"";
  }
  cout << w << "\n";
  PTRACE(1, w);
}

void VideoMixConfigurator::initialize_layout_desc(char* &f_buff,long pos,long pos1,long line,long lo)
{
   ldm=1;
   pos_n=0;
   opts[1]=opts[0]; sopts[1]=sopts[0]; fw[1]=fw[0]; fh[1]=fh[0];
   f_buff[pos1]=0;
//cout << " memcpy " << (pos1-pos+1) << "\n" << sopts[true].Id << "\n" << f_buff[pos] << "\n";
   strcpy((char *)(sopts[1].Id),(char *)(f_buff+pos));
   f_buff[pos1]=']';
   if(lid==0)vmconf=(VMPCfgLayout *)malloc(sizeof(VMPCfgLayout));
   else vmconf=(VMPCfgLayout *)realloc((void *)vmconf,sizeof(VMPCfgLayout)*(lid+1));
   vmconf[lid].vmpcfg=NULL;
}

void VideoMixConfigurator::finalize_layout_desc()
{
   ldm=0;
   vmconf[lid].splitcfg=sopts[1];
   vmconf[lid].splitcfg.vidnum=pos_n;
   lid++;
   vmconfs=lid;
}

void MCUVideoMixer::ConvertCIF4ToCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;

  // copy Y
  for (y = CIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    for (x = CIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy U
  for (y = CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    for (x = CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy V
  for (y = CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    for (x = CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }
}

void MCUVideoMixer::ConvertCIF16ToCIF4(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;

  // copy Y
  for (y = CIF4_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF16_WIDTH;
    for (x = CIF4_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy U
  for (y = CIF4_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    for (x = CIF4_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy V
  for (y = CIF4_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    for (x = CIF4_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }
}

void MCUVideoMixer::ConvertCIFToQCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;

  // copy Y
  for (y = QCIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    for (x = QCIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy U
  for (y = QCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    for (x = QCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy V
  for (y = QCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    for (x = QCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }
}

void MCUVideoMixer::ConvertCIFToQ3CIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = Q3CIF_HEIGHT; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = Q3CIF_WIDTH; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF_WIDTH] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF_WIDTH+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += Q3CIF_WIDTH;
    src += CIF_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF_WIDTH*CIF_HEIGHT;

  // copy U
  for (y = Q3CIF_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    for (x = Q3CIF_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF_WIDTH/2;
    src += QCIF_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF_WIDTH*CIF_HEIGHT+QCIF_WIDTH*QCIF_HEIGHT;

  // copy V
  for (y = Q3CIF_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    for (x = Q3CIF_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF_WIDTH/2;
    src += QCIF_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIF4ToQ3CIF4(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = Q3CIF4_HEIGHT; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF4_WIDTH; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF4_WIDTH] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF4_WIDTH+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += Q3CIF4_WIDTH;
    src += CIF4_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF4_WIDTH*CIF4_HEIGHT;

  // copy U
  for (y = Q3CIF4_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = Q3CIF4_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF4_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF4_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF4_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF4_WIDTH/2;
    src += CIF_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF4_WIDTH*CIF4_HEIGHT+CIF_WIDTH*CIF_HEIGHT;

  // copy V
  for (y = Q3CIF4_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = Q3CIF4_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF4_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF4_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF4_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF4_WIDTH/2;
    src += CIF_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIF16ToQ3CIF16(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = Q3CIF16_HEIGHT; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF16_WIDTH;
    srcRow2 = src + CIF16_WIDTH*2;
    for (x = Q3CIF16_WIDTH; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF16_WIDTH] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF16_WIDTH+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += Q3CIF16_WIDTH;
    src += CIF16_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF16_WIDTH*CIF16_HEIGHT;

  // copy U
  for (y = Q3CIF16_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF16_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF16_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF16_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF16_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF16_WIDTH/2;
    src += CIF4_WIDTH*3;
  }
  src=(unsigned char *)_src+CIF16_WIDTH*CIF16_HEIGHT+CIF4_WIDTH*CIF4_HEIGHT;

  // copy V
  for (y = Q3CIF16_HEIGHT/2; y > 1; y-=2) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF16_WIDTH/2; x > 1; x-=2) {
      val = (*srcRow0*4+*(srcRow0+1)*2+*srcRow1*2+*(srcRow1+1))*0.111111111;
      dst[0] = val;
      val = (*(srcRow0+1)*2+*(srcRow0+2)*4+*(srcRow1+1)+*(srcRow1+2)*2)*0.111111111;
      dst[1] = val;
      val = (*srcRow1*2+*(srcRow1+1)+*srcRow2*4+*(srcRow2+1)*2)*0.111111111;
      dst[Q3CIF16_WIDTH/2] = val;
      val = (*(srcRow1+1)+*(srcRow1+2)*2+*(srcRow2+1)*2+*(srcRow2+2)*4)*0.111111111;
      dst[Q3CIF16_WIDTH/2+1] = val;
      srcRow0 +=3; srcRow1 +=3; srcRow2 +=3;
      dst+=2;
    }
    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF16_WIDTH/2]=*srcRow2; dst++; }
    dst += Q3CIF16_WIDTH/2;
    src += CIF4_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIF16ToTCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = TCIF_HEIGHT; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF16_WIDTH;
    srcRow2 = src + CIF16_WIDTH*2;
    srcRow3 = src + CIF16_WIDTH*3;
    for (x = TCIF_WIDTH; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TCIF_WIDTH] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TCIF_WIDTH+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TCIF_WIDTH+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TCIF_WIDTH*2] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TCIF_WIDTH*2+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TCIF_WIDTH*2+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += TCIF_WIDTH*2;
    src += CIF16_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF16_WIDTH*CIF16_HEIGHT;

  // copy U
  for (y = TCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = TCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst += x;
    dst += TCIF_WIDTH;
    src += CIF4_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF16_WIDTH*CIF16_HEIGHT+CIF4_WIDTH*CIF4_HEIGHT;

  // copy V
  for (y = TCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = TCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF16_WIDTH/2]=*srcRow2; dst++; }
    dst += TCIF_WIDTH;
    src += CIF4_WIDTH*4;
  }
}

void MCUVideoMixer::ConvertCIF4ToTQCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = TQCIF_HEIGHT; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = TQCIF_WIDTH; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TQCIF_WIDTH] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TQCIF_WIDTH+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TQCIF_WIDTH+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TQCIF_WIDTH*2] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TQCIF_WIDTH*2+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TQCIF_WIDTH*2+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += TQCIF_WIDTH*2;
    src += CIF4_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF4_WIDTH*CIF4_HEIGHT;

  // copy U
  for (y = TQCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = TQCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TQCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TQCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TQCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TQCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TQCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TQCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst += x;
    dst += TQCIF_WIDTH;
    src += CIF_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF4_WIDTH*CIF4_HEIGHT+CIF_WIDTH*CIF_HEIGHT;

  // copy V
  for (y = TQCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = TQCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TQCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TQCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TQCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TQCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TQCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TQCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF16_WIDTH/2]=*srcRow2; dst++; }
    dst += TQCIF_WIDTH;
    src += CIF_WIDTH*4;
  }
}

void MCUVideoMixer::ConvertCIFToTSQCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = TSQCIF_HEIGHT; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = TSQCIF_WIDTH; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TSQCIF_WIDTH] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TSQCIF_WIDTH+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TSQCIF_WIDTH+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TSQCIF_WIDTH*2] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TSQCIF_WIDTH*2+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TSQCIF_WIDTH*2+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst[0]=*srcRow0; dst[Q3CIF_WIDTH]=*srcRow2; dst++;
    dst += TSQCIF_WIDTH*2;
    src += CIF_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF_WIDTH*CIF_HEIGHT;

  // copy U
  for (y = TSQCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    srcRow3 = src + QCIF_WIDTH*3;
    for (x = TSQCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TSQCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TSQCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TSQCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TSQCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TSQCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TSQCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    dst += x;
    dst += TSQCIF_WIDTH;
    src += QCIF_WIDTH*4;
  }
  src=(unsigned char *)_src+CIF_WIDTH*CIF_HEIGHT+QCIF_WIDTH*QCIF_HEIGHT;

  // copy V
  for (y = TSQCIF_HEIGHT/2; y > 2; y-=3) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    srcRow3 = src + QCIF_WIDTH*3;
    for (x = TSQCIF_WIDTH/2; x > 2; x-=3) {
      val = (*srcRow0*9+*(srcRow0+1)*3+*srcRow1*3+*(srcRow1+1))>>4;
      dst[0] = val;
      val = (*(srcRow0+1)*6+*(srcRow0+2)*6+*(srcRow1+1)*2+*(srcRow1+2)*2)>>4;
      dst[1] = val;
      val = (*(srcRow0+2)*3+*(srcRow0+3)*9+*(srcRow1+2)+*(srcRow1+3)*3)>>4;
      dst[2] = val;
      val = (*srcRow1*6+*(srcRow1+1)*2+*srcRow2*6+*(srcRow2+1)*2)>>4;
      dst[TSQCIF_WIDTH/2] = val;
      val = (*(srcRow1+1)*4+*(srcRow1+2)*4+*(srcRow2+1)*4+*(srcRow2+2)*4)>>4;
      dst[TSQCIF_WIDTH/2+1] = val;
      val = (*(srcRow1+2)*2+*(srcRow1+3)*6+*(srcRow2+2)*2+*(srcRow2+3)*6)>>4;
      dst[TSQCIF_WIDTH/2+2] = val;
      val = (*srcRow2*3+*(srcRow2+1)*1+*srcRow3*9+*(srcRow3+1)*3)>>4;
      dst[TSQCIF_WIDTH] = val;
      val = (*(srcRow2+1)*2+*(srcRow2+2)*2+*(srcRow3+1)*6+*(srcRow3+2)*6)>>4;
      dst[TSQCIF_WIDTH+1] = val;
      val = (*(srcRow2+2)*1+*(srcRow2+3)*3+*(srcRow3+2)*3+*(srcRow3+3)*9)>>4;
      dst[TSQCIF_WIDTH+2] = val;
      srcRow0 +=4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst+=3;
    }
//    if(x!=0) { dst[0]=*srcRow0; dst[Q3CIF16_WIDTH/2]=*srcRow2; dst++; }
    dst += TSQCIF_WIDTH;
    src += QCIF_WIDTH*4;
  }
}


void MCUVideoMixer::Convert2To1(const void * _src, void * _dst, unsigned int w, unsigned int h)
{
 if(w==CIF16_WIDTH && h==CIF16_HEIGHT) { ConvertCIF16ToCIF4(_src,_dst); return; }
 if(w==CIF4_WIDTH && h==CIF4_HEIGHT) { ConvertCIF4ToCIF(_src,_dst); return; }
 if(w==CIF_WIDTH && h==CIF_HEIGHT) { ConvertCIFToQCIF(_src,_dst); return; }
// if(w==QCIF_WIDTH && h=QCIF_HEIGHT) { ConvertQCIFToSQCIF(_src,_dst); return; }

  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;

  // copy Y
  for (y = h>>1; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + w;
    for (x = w>>1; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy U
  for (y = h>>2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + (w>>1);
    for (x = w>>2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }

  // copy V
  for (y = h>>2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + (w>>1);
    for (x = w>>2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*srcRow1+*(srcRow1+1))>>2;
      dst[0] = val;
      srcRow0 += 2; srcRow1 +=2;
      dst++;
    }
    src = srcRow1;
  }
}

void MCUVideoMixer::ConvertCIFToSQ3CIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = SQ3CIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = SQ3CIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF_WIDTH*3;
  }

  // copy U
  for (y = SQ3CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    for (x = SQ3CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += QCIF_WIDTH*3;
  }

  // copy V
  for (y = SQ3CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    for (x = SQ3CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += QCIF_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIF4ToQ3CIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = Q3CIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF4_WIDTH*3;
  }

  // copy U
  for (y = Q3CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = Q3CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF_WIDTH*3;
  }

  // copy V
  for (y = Q3CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    for (x = Q3CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIF16ToQ3CIF4(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;

  // copy Y
  for (y = Q3CIF4_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF16_WIDTH;
    srcRow2 = src + CIF16_WIDTH*2;
    for (x = Q3CIF4_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF16_WIDTH*3;
  }

  // copy U
  for (y = Q3CIF4_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF4_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF4_WIDTH*3;
  }

  // copy V
  for (y = Q3CIF4_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    for (x = Q3CIF4_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2))*0.11111111111;
      dst[0] = val;
      srcRow0 += 3; srcRow1 +=3; srcRow2 +=3;
      dst++;
    }
    src += CIF4_WIDTH*3;
  }
}

void MCUVideoMixer::ConvertCIFToSQCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = SQCIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = SQCIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF_WIDTH*4;
  }

  // copy U
  for (y = SQCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    srcRow3 = src + QCIF_WIDTH*3;
    for (x = SQCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += QCIF_WIDTH*4;
  }

  // copy V
  for (y = SQCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + QCIF_WIDTH;
    srcRow2 = src + QCIF_WIDTH*2;
    srcRow3 = src + QCIF_WIDTH*3;
    for (x = SQCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += QCIF_WIDTH*4;
  }
}

void MCUVideoMixer::ConvertCIF4ToQCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = QCIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = QCIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF4_WIDTH*4;
  }

  // copy U
  for (y = QCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = QCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF_WIDTH*4;
  }

  // copy V
  for (y = QCIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF_WIDTH;
    srcRow2 = src + CIF_WIDTH*2;
    srcRow3 = src + CIF_WIDTH*3;
    for (x = QCIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF_WIDTH*4;
  }
}

void MCUVideoMixer::ConvertCIF16ToCIF(const void * _src, void * _dst)
{
  unsigned char * src = (unsigned char *)_src;
  unsigned char * dst = (unsigned char *)_dst;

  unsigned int y, x, val;
  unsigned char * srcRow0;
  unsigned char * srcRow1;
  unsigned char * srcRow2;
  unsigned char * srcRow3;

  // copy Y
  for (y = CIF_HEIGHT; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF16_WIDTH;
    srcRow2 = src + CIF16_WIDTH*2;
    srcRow3 = src + CIF16_WIDTH*3;
    for (x = CIF_WIDTH; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF16_WIDTH*4;
  }

  // copy U
  for (y = CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF4_WIDTH*4;
  }

  // copy V
  for (y = CIF_HEIGHT/2; y > 0; y--) {
    srcRow0 = src;
    srcRow1 = src + CIF4_WIDTH;
    srcRow2 = src + CIF4_WIDTH*2;
    srcRow3 = src + CIF4_WIDTH*3;
    for (x = CIF_WIDTH/2; x > 0; x--) {
      val = (*srcRow0+*(srcRow0+1)+*(srcRow0+2)+*(srcRow0+3)
    	    +*srcRow1+*(srcRow1+1)+*(srcRow1+2)+*(srcRow1+3)
    	    +*srcRow2+*(srcRow2+1)+*(srcRow2+2)+*(srcRow2+3)
    	    +*srcRow3+*(srcRow3+1)+*(srcRow3+2)+*(srcRow3+3))>>4;
      dst[0] = val;
      srcRow0 += 4; srcRow1 +=4; srcRow2 +=4; srcRow3 +=4;
      dst++;
    }
    src += CIF4_WIDTH*4;
  }
}

void MCUVideoMixer::ConvertFRAMEToCUSTOM_FRAME(const void * _src, void * _dst, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh)
{
 BYTE * src = (BYTE *)_src;
 BYTE * dst = (BYTE *)_dst;

 //BYTE * dstEnd = dst + CIF_SIZE;
 int y, x, cx, cy;
 BYTE * srcRow;

  // copy Y
  cy=-dh;
  for (y = dh; y > 0; y--) {
    srcRow = src; cx=-dw;
    for (x = dw; x > 0; x--) {
      *dst = *srcRow;
      cx+=sw; while(cx>=0) { cx-=dw; srcRow++; }
      dst++;
    }
    cy+=sh; while(cy>=0) { cy-=dh; src+=sw; }
  }
  // copy U
  src=(BYTE *)_src+(sw*sh);
  cy=-dh;
  for (y = dh/2; y > 0; y--) {
    srcRow = src; cx=-dw;
    for (x = (dw>>1); x > 0; x--) {
      *dst = *srcRow;
      cx+=sw; while(cx>=0) { cx-=dw; srcRow++; }
      dst++;
    }
    cy+=sh; while(cy>=0) { cy-=dh; src+=(sw>>1); }
  }

  // copy V
  src=(BYTE *)_src+(sw*sh)+((sw/2)*(sh/2));
  cy=-dh;
  for (y = dh/2; y > 0; y--) {
    srcRow = src; cx=-dw;
    for (x = (dw>>1); x > 0; x--) {
      *dst = *srcRow;
      cx+=sw; while(cx>=0) { cx-=dw; srcRow++; }
      dst++;
    }
    cy+=sh; while(cy>=0) { cy-=dh; src+=(sw>>1); }
  }

}

void MCUVideoMixer::ConvertQCIFToCIF(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 1; y < QCIF_HEIGHT; y++) 
  {
    for (x = 1; x < QCIF_WIDTH; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[QCIF_WIDTH*2] = (srcRow[0]+srcRow[QCIF_WIDTH])>>1;
      dst[QCIF_WIDTH*2+1] = (srcRow[0]+srcRow[1]+srcRow[QCIF_WIDTH]+srcRow[QCIF_WIDTH+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[QCIF_WIDTH*2] = dst[QCIF_WIDTH*2+1] = (srcRow[0]+srcRow[QCIF_WIDTH])>>1;
    srcRow++; dst += 2; dst += QCIF_WIDTH*2;
  }
  for (x = 1; x < QCIF_WIDTH; x++) 
  {
   dst[0] = dst[QCIF_WIDTH*2] = srcRow[0];
   dst[1] = dst[QCIF_WIDTH*2+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[QCIF_WIDTH*2] = dst[QCIF_WIDTH*2+1] = srcRow[0];
  srcRow++; dst += 2; dst += QCIF_WIDTH*2;

  for (y = 1; y < QCIF_HEIGHT/2; y++) 
  {
    for (x = 1; x < QCIF_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[QCIF_WIDTH] = (srcRow[0]+srcRow[QCIF_WIDTH/2])>>1;
      dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[QCIF_WIDTH/2]+srcRow[QCIF_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[QCIF_WIDTH] = dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[QCIF_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += QCIF_WIDTH;
  }
  for (x = 1; x < QCIF_WIDTH/2; x++) 
  {
   dst[0] = dst[QCIF_WIDTH] = srcRow[0];
   dst[1] = dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[QCIF_WIDTH] = dst[QCIF_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += QCIF_WIDTH;

  for (y = 1; y < QCIF_HEIGHT/2; y++) 
  {
    for (x = 1; x < QCIF_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[QCIF_WIDTH] = (srcRow[0]+srcRow[QCIF_WIDTH/2])>>1;
      dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[QCIF_WIDTH/2]+srcRow[QCIF_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[QCIF_WIDTH] = dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[QCIF_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += QCIF_WIDTH;
  }
  for (x = 1; x < QCIF_WIDTH/2; x++) 
  {
   dst[0] = dst[QCIF_WIDTH] = srcRow[0];
   dst[1] = dst[QCIF_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[QCIF_WIDTH] = dst[QCIF_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += QCIF_WIDTH;
}


void MCUVideoMixer::ConvertCIFToCIF4(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 1; y < CIF_HEIGHT; y++) 
  {
    for (x = 1; x < CIF_WIDTH; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF_WIDTH*2] = (srcRow[0]+srcRow[CIF_WIDTH])>>1;
      dst[CIF_WIDTH*2+1] = (srcRow[0]+srcRow[1]+srcRow[CIF_WIDTH]+srcRow[CIF_WIDTH+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF_WIDTH*2] = dst[CIF_WIDTH*2+1] = (srcRow[0]+srcRow[CIF_WIDTH])>>1;
    srcRow++; dst += 2; dst += CIF_WIDTH*2;
  }
  for (x = 1; x < CIF_WIDTH; x++) 
  {
   dst[0] = dst[CIF_WIDTH*2] = srcRow[0];
   dst[1] = dst[CIF_WIDTH*2+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF_WIDTH*2] = dst[CIF_WIDTH*2+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF_WIDTH*2;

  for (y = 1; y < CIF_HEIGHT/2; y++) 
  {
    for (x = 1; x < CIF_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF_WIDTH] = (srcRow[0]+srcRow[CIF_WIDTH/2])>>1;
      dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[CIF_WIDTH/2]+srcRow[CIF_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF_WIDTH] = dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[CIF_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += CIF_WIDTH;
  }
  for (x = 1; x < CIF_WIDTH/2; x++) 
  {
   dst[0] = dst[CIF_WIDTH] = srcRow[0];
   dst[1] = dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF_WIDTH] = dst[CIF_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF_WIDTH;

  for (y = 1; y < CIF_HEIGHT/2; y++) 
  {
    for (x = 1; x < CIF_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF_WIDTH] = (srcRow[0]+srcRow[CIF_WIDTH/2])>>1;
      dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[CIF_WIDTH/2]+srcRow[CIF_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF_WIDTH] = dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[CIF_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += CIF_WIDTH;
  }
  for (x = 1; x < CIF_WIDTH/2; x++) 
  {
   dst[0] = dst[CIF_WIDTH] = srcRow[0];
   dst[1] = dst[CIF_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF_WIDTH] = dst[CIF_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF_WIDTH;
}

void MCUVideoMixer::ConvertCIF4ToCIF16(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 1; y < CIF4_HEIGHT; y++) 
  {
    for (x = 1; x < CIF4_WIDTH; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF4_WIDTH*2] = (srcRow[0]+srcRow[CIF4_WIDTH])>>1;
      dst[CIF4_WIDTH*2+1] = (srcRow[0]+srcRow[1]+srcRow[CIF4_WIDTH]+srcRow[CIF4_WIDTH+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF4_WIDTH*2] = dst[CIF4_WIDTH*2+1] = (srcRow[0]+srcRow[CIF4_WIDTH])>>1;
    srcRow++; dst += 2; dst += CIF4_WIDTH*2;
  }
  for (x = 1; x < CIF4_WIDTH; x++) 
  {
   dst[0] = dst[CIF4_WIDTH*2] = srcRow[0];
   dst[1] = dst[CIF4_WIDTH*2+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF4_WIDTH*2] = dst[CIF4_WIDTH*2+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF4_WIDTH*2;

  for (y = 1; y < CIF4_HEIGHT/2; y++) 
  {
    for (x = 1; x < CIF4_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF4_WIDTH] = (srcRow[0]+srcRow[CIF4_WIDTH/2])>>1;
      dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[CIF4_WIDTH/2]+srcRow[CIF4_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF4_WIDTH] = dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[CIF4_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += CIF4_WIDTH;
  }
  for (x = 1; x < CIF4_WIDTH/2; x++) 
  {
   dst[0] = dst[CIF4_WIDTH] = srcRow[0];
   dst[1] = dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF4_WIDTH] = dst[CIF4_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF4_WIDTH;

  for (y = 1; y < CIF4_HEIGHT/2; y++) 
  {
    for (x = 1; x < CIF4_WIDTH/2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[CIF4_WIDTH] = (srcRow[0]+srcRow[CIF4_WIDTH/2])>>1;
      dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[1]+srcRow[CIF4_WIDTH/2]+srcRow[CIF4_WIDTH/2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[CIF4_WIDTH] = dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[CIF4_WIDTH/2])>>1;
    srcRow++; dst += 2; dst += CIF4_WIDTH;
  }
  for (x = 1; x < CIF4_WIDTH/2; x++) 
  {
   dst[0] = dst[CIF4_WIDTH] = srcRow[0];
   dst[1] = dst[CIF4_WIDTH+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[CIF4_WIDTH] = dst[CIF4_WIDTH+1] = srcRow[0];
  srcRow++; dst += 2; dst += CIF4_WIDTH;
}

void MCUVideoMixer::Convert1To2(const void * _src, void * _dst, unsigned int w, unsigned int h)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  if(w==QCIF_WIDTH && h==QCIF_HEIGHT) ConvertQCIFToCIF(_src,_dst);
  if(w==CIF_WIDTH && h==CIF_HEIGHT) ConvertCIFToCIF4(_src,_dst);
  if(w==CIF4_WIDTH && h==CIF4_HEIGHT) ConvertCIF4ToCIF16(_src,_dst);

  unsigned int y,x,w2=w*2;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 1; y < h; y++) 
  {
    for (x = 1; x < w; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[w2] = (srcRow[0]+srcRow[w])>>1;
      dst[w2+1] = (srcRow[0]+srcRow[1]+srcRow[w]+srcRow[w+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[w2] = dst[w2+1] = (srcRow[0]+srcRow[w])>>1;
    srcRow++; dst += 2; dst += w2;
  }
  for (x = 1; x < w; x++) 
  {
   dst[0] = dst[w2] = srcRow[0];
   dst[1] = dst[w2+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[w2] = dst[w2+1] = srcRow[0];
  srcRow++; dst += 2; dst += w2;

  w2=w>>1;
  for (y = 1; y < (h>>1); y++) 
  {
    for (x = 1; x < w2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[w] = (srcRow[0]+srcRow[w2])>>1;
      dst[w+1] = (srcRow[0]+srcRow[1]+srcRow[w2]+srcRow[w2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[w] = dst[w+1] = (srcRow[0]+srcRow[w2])>>1;
    srcRow++; dst += 2; dst += w;
  }
  for (x = 1; x < w2; x++) 
  {
   dst[0] = dst[w] = srcRow[0];
   dst[1] = dst[w+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[w] = dst[w+1] = srcRow[0];
  srcRow++; dst += 2; dst += w;

  for (y = 1; y < (h>>1); y++) 
  {
    for (x = 1; x < w2; x++) 
    {
      dst[0] = srcRow[0];
      dst[1] = (srcRow[0]+srcRow[1])>>1;
      dst[w] = (srcRow[0]+srcRow[w2])>>1;
      dst[w+1] = (srcRow[0]+srcRow[1]+srcRow[w2]+srcRow[w2+1])>>2;
      dst+=2; srcRow++;
    }
    dst[0] = dst[1] = srcRow[0];
    dst[w] = dst[w+1] = (srcRow[0]+srcRow[w2])>>1;
    srcRow++; dst += 2; dst += w;
  }
  for (x = 1; x < w2; x++) 
  {
   dst[0] = dst[w] = srcRow[0];
   dst[1] = dst[w+1] = (srcRow[0]+srcRow[1])>>1;
   dst+=2; srcRow++;
  }
  dst[0] = dst[1] = dst[w] = dst[w+1] = srcRow[0];
  srcRow++; dst += 2; dst += w;
}


void MCUVideoMixer::ConvertCIFToTQCIF(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  unsigned int sum;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 0; y < CIF_HEIGHT/2; y++) {
    for (x = 0; x < CIF_WIDTH/2; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF_WIDTH*3/2;
    for (x = 0; x < CIF_WIDTH/2; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF_WIDTH*9/2;
    for (x = 0; x < CIF_WIDTH*3/2; x++) {
      sum = dst[0]+dst[CIF_WIDTH*3];
      dst[CIF_WIDTH*3/2] = (sum >> 1);
      dst++;
    }
   dst+=CIF_WIDTH*3;
  }

  // copy U
  for (y = 0; y < CIF_HEIGHT/4; y++) {
    for (x = 0; x < CIF_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF_WIDTH*3/4;
    for (x = 0; x < CIF_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF_WIDTH*9/4;
    for (x = 0; x < CIF_WIDTH*3/4; x++) {
      sum = dst[0]+dst[CIF_WIDTH*3/2];
      dst[CIF_WIDTH*3/4] = (sum >> 1);
      dst++;
    }
   dst+=CIF_WIDTH*3/2;
  }

  // copy V
  for (y = 0; y < CIF_HEIGHT/4; y++) {
    for (x = 0; x < CIF_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF_WIDTH*3/4;
    for (x = 0; x < CIF_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF_WIDTH*9/4;
    for (x = 0; x < CIF_WIDTH*3/4; x++) {
      sum = dst[0]+dst[CIF_WIDTH*3/2];
      dst[CIF_WIDTH*3/4] = (sum >> 1);
      dst++;
    }
   dst+=CIF_WIDTH*3/2;
  }
}

void MCUVideoMixer::ConvertCIF4ToTCIF(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  unsigned int sum;
  BYTE * srcRow;

  // copy Y
  srcRow = src;
  for (y = 0; y < CIF4_HEIGHT/2; y++) {
    for (x = 0; x < CIF4_WIDTH/2; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF4_WIDTH*3/2;
    for (x = 0; x < CIF4_WIDTH/2; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF4_WIDTH*9/2;
    for (x = 0; x < CIF4_WIDTH*3/2; x++) {
      sum = dst[0]+dst[CIF4_WIDTH*3];
      dst[CIF4_WIDTH*3/2] = (sum >> 1);
      dst++;
    }
   dst+=CIF4_WIDTH*3;
  }

  // copy U
  for (y = 0; y < CIF4_HEIGHT/4; y++) {
    for (x = 0; x < CIF4_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF4_WIDTH*3/4;
    for (x = 0; x < CIF4_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF4_WIDTH*9/4;
    for (x = 0; x < CIF4_WIDTH*3/4; x++) {
      sum = dst[0]+dst[CIF4_WIDTH*3/2];
      dst[CIF4_WIDTH*3/4] = (sum >> 1);
      dst++;
    }
   dst+=CIF4_WIDTH*3/2;
  }

  // copy V
  for (y = 0; y < CIF4_HEIGHT/4; y++) {
    for (x = 0; x < CIF4_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst += CIF4_WIDTH*3/4;
    for (x = 0; x < CIF4_WIDTH/4; x++) {
      sum = *srcRow; dst[0] = *srcRow++;
      sum+= *srcRow; dst[2] = *srcRow++;
      dst[1] = (sum >> 1);
      dst += 3;
    }
    dst -= CIF4_WIDTH*9/4;
    for (x = 0; x < CIF4_WIDTH*3/4; x++) {
      sum = dst[0]+dst[CIF4_WIDTH*3/2];
      dst[CIF4_WIDTH*3/4] = (sum >> 1);
      dst++;
    }
   dst+=CIF4_WIDTH*3/2;
  }
}

void MCUVideoMixer::ConvertQCIFToCIF4(const void * _src, void * _dst)
{
  BYTE * src = (BYTE *)_src;
  BYTE * dst = (BYTE *)_dst;

  //BYTE * dstEnd = dst + CIF_SIZE;
  int y, x;
  BYTE * srcRow;

  // copy Y
  for (y = 0; y < QCIF_HEIGHT; y++) {
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    src += QCIF_WIDTH;
  }

  // copy U
  for (y = 0; y < QCIF_HEIGHT/2; y++) {
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    src += QCIF_WIDTH/2;
  }

  // copy V
  for (y = 0; y < QCIF_HEIGHT/2; y++) {
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    srcRow = src;
    for (x = 0; x < QCIF_WIDTH/2; x++) {
      dst[0] = dst[1] = dst[2] = dst[3] = *srcRow++;
      dst += 4;
    }
    src += QCIF_WIDTH/2;
  }
}
