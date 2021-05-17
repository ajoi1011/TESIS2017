/*
 * videomixer.h
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

#include "precompile.h"
#include "config.h"
#include "conference.h"


class PVideoOutputDevice_MCU : public PVideoOutputDevice
{
  PCLASSINFO(PVideoOutputDevice_MCU, PVideoOutputDevice);
  
  public:
    
    PVideoOutputDevice_MCU(ConferenceManager & cmanager);
  
      
    ~PVideoOutputDevice_MCU();

    
    virtual PStringArray GetDeviceNames() const;
    static PStringArray GetOutputDeviceNames();
  
    
    virtual PBoolean Open(
      const PString & /*deviceName*/,   ///< Device name to open
      PBoolean /*startImmediate*/ = true    ///< Immediately start device
    );
  
    
    virtual PBoolean Close();
  
    
    virtual PBoolean IsOpen();
  
    
    virtual PBoolean SetColourFormat(
      const PString & colourFormat ///< New colour format for device.
    );

    
    virtual PBoolean SetFrameSize(
      unsigned width,   ///< New width of frame
      unsigned height   ///< New height of frame
    );

    virtual PBoolean SetFrameData(const FrameData & frameData);  
    
  private:
    struct SDL_Window * m_window;
    struct SDL_Renderer * m_renderer;
    struct SDL_Texture * m_texture;
    PDECLARE_MUTEX(      m_texture_mutex);
    PSyncPoint           m_operationComplete;
    ConferenceManager    & m_cmanager;
    bool InternalOpen();
    void InternalClose();
    void InternalSetFrameSize();
    void InternalSetFrameData();
    void PostEvent(unsigned code, bool wait);

    
  friend class PSDL_System;
};

typedef PVideoOutputDevice_MCU PMCUVideoDevice; // Backward compatibility

//////////////////////////////////////////////////////////////////////////////////////////


struct VMPBlock {
 unsigned posx,
          posy,
          width,
          height;
};

struct VMPCfgOptions {
 unsigned posx, 
          posy, 
          width, 
          height, 
          border, 
          label_mask, /* label_color, */ 
          label_bgcolor, 
          scale_mode, 
          blks;
          
 bool cut_before_bracket;
 
 char fontsize[10],
      border_left[10], 
      border_right[10], 
      border_top[10], 
      border_bottom[10],
      h_pad[10], 
      v_pad[10],
      dropshadow_l[10], 
      dropshadow_r[10], 
      dropshadow_t[10], 
      dropshadow_b[10];
 
 VMPBlock *blk;
};

struct VMPCfgSplitOptions 
{ unsigned vidnum, 
	       mode_mask, 
	       reallocate_on_disconnect, 
	       new_from_begin, 
	       mockup_width, 
	       mockup_height; 
	       
  char Id[32], 
       minimum_width_for_label[10]; 
};

struct VMPCfgLayout {
  VMPCfgSplitOptions splitcfg;
  VMPCfgOptions *vmpcfg;
};

class VideoMixConfigurator 
{
  public:
    VideoMixConfigurator(long _w = CIF4_WIDTH, long _h = CIF4_HEIGHT);
    ~VideoMixConfigurator();
    VMPCfgLayout *vmconf;  // *configuration*
    unsigned vmconfs;      // count of vmconf
    char fontfile[256];
    unsigned bfw,bfh;      // base frame values for "resize" frames
    virtual void go(unsigned frame_width, unsigned frame_height);
  
  protected:
    long lid;              // layout number
    long pos_n;            // positions found for current lid
    unsigned char ldm;     // layout descriptor mode flag (1/0)
    bool geometry_changed;
    VMPCfgOptions opts[2]; // local & global VMP options
    VMPCfgSplitOptions sopts[2]; // local & global layout options
    unsigned fw[2],fh[2];  // local & global base frame values
    virtual void parser(char* &f_buff,long f_size);
    virtual void block_insert(VMPBlock * & b,long b_n,unsigned x,unsigned y,unsigned w,unsigned h);
    virtual void block_erase(VMPBlock * & b,long b_i,long b_n);
    virtual unsigned frame_splitter(VMPBlock * & b,long b_i,long b_n,unsigned x0,unsigned y0,unsigned w0,unsigned h0,unsigned x1,unsigned y1,unsigned w1,unsigned h1);
    virtual void geometry();
    virtual void handle_line(char* &f_buff,long pos,long pos1,long line);
    virtual void handle_atom(char* &f_buff,long pos,long pos1,long line,long lo);
    virtual void handle_layout_descriptor(char* &f_buff,long pos,long pos1,long line,long lo);
    virtual void handle_position_descriptor(char* &f_buff,long pos,long pos1,long line,long lo);
    virtual void handle_parameter(char* &f_buff,long pos,long pos1,long line,long lo);
    virtual void option_set(const char* p, const char* v, char* &f_buff, long line, long lo, long pos, long pos1);
    virtual bool option_cmp(const char* p,const char* str);
    virtual void warning(char* &f_buff,long line,long lo,const char warn[64],long pos,long pos1);
    virtual void initialize_layout_desc(char* &f_buff,long pos,long pos1,long line,long lo);
    virtual void finalize_layout_desc();
};
