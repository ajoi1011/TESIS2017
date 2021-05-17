/*
 * conference.h
 * 
*/


#include "precompile.h"
#include "managers.h"
typedef void * StreamID;
//////////////////////////////////////////////////////////////////////////////////////////
class VideoFrameStoreList 
{
  public:
    class FrameStore 
    {
      public:
        FrameStore(int w, int h)
          : width(w)
          , height(h)
        { valid = false; 
		  PAssert(w != 0 && h != 0, "Cannot create zero size framestore"); 
		  data.SetSize(w*h*3/2); 
		}

      bool valid;
      int width;
      int height;
      time_t lastRead;
      PBYTEArray data;
    };

    inline unsigned WidthHeightToKey(int width, int height)
    { return width << 16 | height; }

    inline void KeyToWidthHeight(unsigned key, int & width, int & height)
    { width = (key >> 16) & 0xffff; height = (key & 0xffff); }

    ~VideoFrameStoreList();
    FrameStore & AddFrameStore(int width, int height);
    FrameStore & GetFrameStore(int width, int height);
    FrameStore & GetNearestFrameStore(int width, int height, bool & found);
    void InvalidateExcept(int w, int h);

    typedef std::map<unsigned, FrameStore *> VideoFrameStoreListMapType;
    VideoFrameStoreListMapType videoFrameStoreList;
};
//////////////////////////////////////////////////////////////////////////////////////////

class MCUVideoMixer : public OpalVideoStreamMixer
{
  PCLASSINFO(MCUVideoMixer, OpalVideoStreamMixer)
  public:
    MCUVideoMixer(const OpalMixerNodeInfo & info);
    
    class VideoMixPosition 
    {
      public:
        VideoMixPosition(StreamID id, 
                         unsigned x = 0, 
                         unsigned y = 0, 
                         unsigned w = 0, 
                         unsigned h = 0);
                                  
        virtual ~VideoMixPosition();

        VideoMixPosition *next;
        VideoMixPosition *prev;
        StreamID id;
	    int n;
        unsigned xpos, ypos, width, height;
        int status;        // static | vad visibility
        volatile int type; // static, vad, vad2, vad3
        int chosenVan;     // always visible vad members (can switched between vad and vad2)
	    PString endpointName;
        bool border;
    };
    
    
    struct VideoStream : public Stream
    {
      VideoStream(MCUVideoMixer & mixer);
      virtual void QueuePacket(const RTP_DataFrame & rtp);
      virtual void WriteSubFrame(PINDEX pos);
      void InsertVideoFrame(unsigned x, unsigned y, unsigned w, unsigned h);
      
      MCUVideoMixer & m_mixer;
      
    };
    
    virtual VideoMixPosition * CreateVideoMixPosition(StreamID id, unsigned x, unsigned y, unsigned w, unsigned h)
    { return new VideoMixPosition(id, x, y, w, h); }
    
    void VMPListInit(); 
    void VMPListInsVMP(VideoMixPosition *pVMP);
    PString VMPListScanJS(); // returns associative javascript "{0:id1, 1:-1, 2:id2, ...}"
    void VMPListAddVMP(VideoMixPosition *pVMP);
    void VMPListDelVMP(VideoMixPosition *pVMP);
    void VMPListClear();
    VideoMixPosition * VMPListFindVMP(Id id);
    VideoMixPosition * VMPListFindVMP(int pos);
    virtual bool WriteFrame(Id id, const void * buffer, int width, int height, PINDEX amount);
    virtual Stream * CreateStream();
    virtual bool MixStreams(RTP_DataFrame & frame);
    
    
    virtual bool MixVideo();
    virtual bool AddVideoSource(Id id, OpalMixerNode * node);
    virtual void Shuffle();
    virtual void Scroll(bool reverse);
    /*virtual void RemoveVideoSource(ConferenceMemberId id, ConferenceMember & mbr);*/
    virtual void PositionSetup(int pos, int type, Id id);
    
    virtual void RemoveVideoSourceByPos(int pos, bool flag);
    // void MyRemoveVideoSourceById(ConferenceMemberId id, bool flag);
    virtual void RemoveAllVideoSource();
    virtual int GetPositionSet();
    /*virtual int GetPositionNum(ConferenceMemberId id);
    virtual int GetPositionStatus(ConferenceMemberId id);
    virtual int GetPositionType(ConferenceMemberId id);*/
    virtual void SetPositionType(int pos, int type);
    /*virtual void InsertVideoSource(ConferenceMember * member, int pos);
    virtual void SetPositionStatus(ConferenceMemberId id,int newStatus);*/
    
    virtual Id GetHonestId(int pos);
    virtual void Exchange(int pos1, int pos2);
    virtual void CalcVideoSplitSize(unsigned int imageCount, int & subImageWidth, int & subImageHeight, int & cols, int & rows);
    virtual void ReallocatePositions();
    virtual void Revert();
    
    void InsertVideoSubFrame(const StreamMap_T::iterator & it, PINDEX pos);
    virtual bool WriteSubFrame(VideoMixPosition & vmp, const void * buffer, int width, int height, PINDEX amount);
    virtual bool ReadFrame(void * buffer, int width, int height, PINDEX & amount);
    
    virtual void imageStores_operational_size(long w, long h, BYTE mask){
      long s=w*h*3/2;
      if(mask&_IMGST)if(s>imageStore_size){imageStore.SetSize(s);imageStore_size=s;}
      if(mask&_IMGST1)if(s>imageStore1_size){imageStore1.SetSize(s);imageStore1_size=s;}
      if(mask&_IMGST2)if(s>imageStore2_size){imageStore2.SetSize(s);imageStore2_size=s;}
     }
    static void VideoSplitLines(void * dst, unsigned fw, unsigned fh);
    static void ResizeYUV420P(const void * _src, void * _dst, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh);
    static void CopyRectFromFrame(const void * _src, void * _dst, int xpos, int ypos, int width, int height, int fw, int fh);
    static void CopyRFromRIntoR(const void *_s, void * _d, int xp, int yp, int w, int h, int rx_abs, int ry_abs, int rw, int rh, int fw, int fh, int lim_w, int lim_h);
    
    static void ConvertQCIFToCIF(const void * _src, void * _dst);
    static void ConvertCIFToCIF4(const void * _src, void * _dst);
    static void ConvertCIF4ToCIF16(const void * _src, void * _dst);
    static void ConvertFRAMEToCUSTOM_FRAME(const void * _src, void * _dst, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh);
    static void ConvertCIFToTQCIF(const void * _src, void * _dst);
    static void ConvertCIF4ToTCIF(const void * _src, void * _dst);
    static void ConvertCIF16ToTCIF(const void * _src, void * _dst);
    static void ConvertCIF4ToTQCIF(const void * _src, void * _dst);
    static void ConvertCIFToTSQCIF(const void * _src, void * _dst);
    static void ConvertQCIFToCIF4(const void * _src, void * _dst);
    static void ConvertCIF4ToCIF(const void * _src, void * _dst);
    static void ConvertCIF16ToCIF4(const void * _src, void * _dst);
    static void ConvertCIFToQCIF(const void * _src, void * _dst);
    static void Convert2To1(const void * _src, void * _dst, unsigned int w, unsigned int h);
    static void Convert1To2(const void * _src, void * _dst, unsigned int w, unsigned int h);
    static void ConvertCIFToQ3CIF(const void * _src, void * _dst);
    static void ConvertCIF4ToQ3CIF4(const void * _src, void * _dst);
    static void ConvertCIF16ToQ3CIF16(const void * _src, void * _dst);
    static void ConvertCIFToSQ3CIF(const void*, void*);
    static void ConvertCIF4ToQ3CIF(const void*, void*);
    static void ConvertCIF16ToQ3CIF4(const void*, void*);
    static void ConvertCIF16ToCIF(const void * _src, void * _dst);
    static void ConvertCIF4ToQCIF(const void * _src, void * _dst);
    static void ConvertCIFToSQCIF(const void * _src, void * _dst);
    
    virtual StreamID GetPositionId(int pos);
    virtual void NullRectangle(int x, int y, int w, int h, bool border);
    virtual void NullAllFrameStores();
    virtual bool AddVideoSourceToLayout(StreamID id, OpalMixerNode *node);
    void InsertVideoFrame(const StreamMap_T::iterator & it, unsigned x, unsigned y, unsigned w, unsigned h);
    
    virtual void ChangeLayout(unsigned newLayout);
    virtual bool ReadMixedFrame(void * buffer, int width, int height, PINDEX & amount);
    bool ReadSrcFrame(VideoFrameStoreList & srcFrameStores, void * buffer, int width, int height, PINDEX & amount);
    static void CopyRectIntoFrame(const void * _src, void * _dst, int xpos, int ypos, int width, int height, int fw, int fh);
    static void ConvertRGBToYUV(BYTE R, BYTE G, BYTE B, BYTE & Y, BYTE & U, BYTE & V);
    static void FillYUVFrame(void * buffer, BYTE R, BYTE G, BYTE B, int w, int h);
    
    PBYTEArray    myjpeg;
    PINDEX        jpegSize;
    unsigned long jpegTime;
   
    
    PBYTEArray imageStore,            
               imageStore1,           
               imageStore2;           // temporary conversion store
    
    long   imageStore_size, 
           imageStore1_size, 
           imageStore2_size;
    
    unsigned specialLayout,
             vmpNum;
    VideoFrameStoreList frameStores;  // list of framestores for data
    
    VideoMixPosition                * vmpList;
    VideoFrameStoreList::FrameStore * m_videoFrameStore;
    
    PDECLARE_MUTEX(m_videoMixerMutex);
    OpalMixerNodeInfo & m_info;
    
    
    
};

class ConferenceNode : public OpalMixerNode
{
	PCLASSINFO(ConferenceNode, OpalMixerNode);
	public:
	  ConferenceNode(ConferenceManager & manager, OpalMixerNodeInfo * info);
	  
	  
	  class VideoMixerRecord
	  {
		public:
		  VideoMixerRecord(){ prev=NULL; next=NULL; };
		  ~VideoMixerRecord(){ delete mixer; };
			
		  VideoMixerRecord      * prev;
		  VideoMixerRecord      * next;
		  MCUVideoMixer    * mixer;
		  unsigned           id;
	  };
	  
	  virtual bool AttachStream(
      OpalMixerMediaStream * stream     ///< Stream to attach
      );
      virtual void SetModerated(bool set) { m_moderated = set; }
      virtual PString IsModerated() const;
      void     VMLInit(MCUVideoMixer * mixer);
      void     VMLClear();
      unsigned VMLAdd(); 
      unsigned VMLDel(unsigned n);
      MCUVideoMixer * VMLFind(unsigned i) const;
      
      VideoMixerRecord & GetVideoMixerList() { return *m_videoMixerList;}
      
      OpalVideoStreamMixer * m_videoMixer;
      VideoMixerRecord     * m_videoMixerList;
      PINDEX                 m_maxMemberCount;
      unsigned               m_videoMixerCount;
      bool                   m_moderated;
};

//////////////////////////////////////////////////////////////////////////////////////////

class ConferenceConnection : public OpalMixerConnection 
{  
  PCLASSINFO(ConferenceConnection, OpalMixerConnection);
  public:
    ConferenceConnection(
      PSafePtr<OpalMixerNode> node,
      OpalCall & call,
      ConferenceManager & endpoint,
      void * userData,
      unsigned options,
      OpalConnection::StringOptions * stringOptions
    ); 
    
    /*virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat,
      unsigned sessionID,
      PBoolean isSource
    );*/ 
    
    
    ConferenceManager & GetConferenceManager() { return m_cmanager; }
 
  protected:
    ConferenceManager & m_cmanager;
    

}; 

/*class ConferenceMediaStream : public OpalMixerMediaStream
{
  PCLASSINFO(ConferenceMediaStream, OpalMixerMediaStream);
  public:
    ConferenceMediaStream(
      ConferenceConnection & conn,
      const OpalMediaFormat & mediaFormat,
      unsigned sessionID,
      bool isSource,
      PSafePtr<OpalMixerNode> node,
      bool listenOnly,
      PVideoOutputDevice * outputDevice,
      bool autoDeleteOutput = true
    );

    virtual PBoolean WritePacket(  
      RTP_DataFrame & packet
    );
    
    virtual PBoolean WriteData(
      const BYTE * data,
      PINDEX length,
      PINDEX & written
    );       
    
    virtual PBoolean Open();

  protected:    
    virtual void InternalClose(); 
    ConferenceConnection & m_connection;
    PVideoOutputDevice * m_outputDevice; 
    bool                 m_autoDeleteOutput;

    PDECLARE_MUTEX(m_devicesMutex);
};*/                    
