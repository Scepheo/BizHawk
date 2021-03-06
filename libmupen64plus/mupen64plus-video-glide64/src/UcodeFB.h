/*
*   Glide64 - Glide video plugin for Nintendo 64 emulators.
*   Copyright (c) 2002  Dave2001
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public
*   License along with this program; if not, write to the Free
*   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
*   Boston, MA  02110-1301, USA
*/

//****************************************************************
//
// Glide64 - Glide Plugin for Nintendo 64 emulators (tested mostly with Project64)
// Project started on December 29th, 2001
//
// To modify Glide64:
// * Write your name and (optional)email, commented by your work, so I know who did it, and so that you can find which parts you modified when it comes time to send it to me.
// * Do NOT send me the whole project or file that you modified.  Take out your modified code sections, and tell me where to put them.  If people sent the whole thing, I would have many different versions, but no idea how to combine them all.
//
// Official Glide64 development channel: #Glide64 on EFnet
//
// Original author: Dave2001 (Dave2999@hotmail.com)
// Other authors: Gonetz, Gugaman
//
//****************************************************************
//
// Creation 13 August 2003               Gonetz
//
//****************************************************************

#ifndef _WIN32
#include <stdlib.h>
#endif

static void fb_uc0_moveword()
{
  if ((rdp.cmd0 & 0xFF) == 0x06)  // segment
  {
    rdp.segment[(rdp.cmd0 >> 10) & 0x0F] = rdp.cmd1;
  }
}

static void fb_uc2_moveword()
{
  if (((rdp.cmd0 >> 16) & 0xFF) == 0x06)  // segment
  {
    rdp.segment[((rdp.cmd0 & 0xFFFF) >> 2)&0xF] = rdp.cmd1;
  }
}

static void fb_bg_copy ()
{
  if (rdp.main_ci == 0)
    return;
  CI_STATUS status = rdp.frame_buffers[rdp.ci_count-1].status;
  if (status == ci_copy)
    return;
  
  DWORD addr = segoffset(rdp.cmd1) >> 1;
  BYTE imageFmt = ((BYTE *)gfx.RDRAM)[(((addr+11)<<1)+0)^3];
  BYTE imageSiz = ((BYTE *)gfx.RDRAM)[(((addr+11)<<1)+1)^3];    
  DWORD imagePtr    = segoffset(((DWORD*)gfx.RDRAM)[(addr+8)>>1]);  
  FRDP ("fb_bg_copy. fmt: %d, size: %d, imagePtr %08lx, main_ci: %08lx, cur_ci: %08lx \n", imageFmt, imageSiz, imagePtr, rdp.main_ci, rdp.frame_buffers[rdp.ci_count-1].addr);
  
  if (status == ci_main)
  {
    WORD frameW = ((WORD *)gfx.RDRAM)[(addr+3)^1] >> 2; 
    WORD frameH = ((WORD *)gfx.RDRAM)[(addr+7)^1] >> 2; 
    if ( (frameW == rdp.frame_buffers[rdp.ci_count-1].width) && (frameH == rdp.frame_buffers[rdp.ci_count-1].height) )
      rdp.main_ci_bg = imagePtr;
  }
  else if (imagePtr >= rdp.main_ci && imagePtr < rdp.main_ci_end) //addr within main frame buffer
  {
    rdp.copy_ci_index = rdp.ci_count-1;
    rdp.frame_buffers[rdp.copy_ci_index].status = ci_copy;
    FRDP("rdp.frame_buffers[%d].status = ci_copy\n", rdp.copy_ci_index);
    
    if (rdp.frame_buffers[rdp.copy_ci_index].addr != rdp.main_ci_bg)
    {
      rdp.scale_x = 1.0f;
      rdp.scale_y = 1.0f;
    }
    else 
    {
      RDP("motion blur!\n");
      rdp.motionblur = TRUE;
    }
    
    FRDP ("Detect FB usage. texture addr is inside framebuffer: %08lx - %08lx \n", imagePtr, rdp.main_ci);
  }
  else if (imagePtr == rdp.zimg)
  {
    printf("toto !\n");
    if (status == ci_unknown)
    {
      rdp.frame_buffers[rdp.ci_count-1].status = ci_zimg;
      rdp.tmpzimg = rdp.frame_buffers[rdp.ci_count-1].addr;
      FRDP("rdp.frame_buffers[%d].status = ci_zimg\n", rdp.copy_ci_index);
    }
  }
}

static void fb_setscissor()
{
  rdp.scissor_o.lr_y = (((rdp.cmd1 & 0x00000FFF) >> 2));
  if (rdp.ci_count)
  {
    rdp.scissor_o.ul_x = (((rdp.cmd0 & 0x00FFF000) >> 14));
    rdp.scissor_o.lr_x = (((rdp.cmd1 & 0x00FFF000) >> 14));
    COLOR_IMAGE & cur_fb = rdp.frame_buffers[rdp.ci_count-1];
    if (rdp.scissor_o.lr_x - rdp.scissor_o.ul_x > (cur_fb.width >> 1))
    {
      if (cur_fb.height == 0 || (cur_fb.width >= rdp.scissor_o.lr_x-1 && cur_fb.width <= rdp.scissor_o.lr_x+1)) 
        cur_fb.height = rdp.scissor_o.lr_y;
    }
    FRDP("fb_setscissor. lr_x = %d, lr_y = %d, fb_width = %d, fb_height = %d\n", rdp.scissor_o.lr_x, rdp.scissor_o.lr_y, cur_fb.width, cur_fb.height); 
  }
}

static void fb_rect()
{
  if (rdp.frame_buffers[rdp.ci_count-1].width == 32)
    return;
  int ul_x = ((rdp.cmd1 & 0x00FFF000) >> 14);
  int lr_x = ((rdp.cmd0 & 0x00FFF000) >> 14);
  int width = lr_x-ul_x;
  DWORD lr_y = ((rdp.cmd0 & 0x00000FFF) >> 2);
  int diff = abs((int)rdp.frame_buffers[rdp.ci_count-1].width - width);
  if (diff < 4)
    if (rdp.frame_buffers[rdp.ci_count-1].height < lr_y)
    {
      FRDP("fb_rect. ul_x: %d, lr_x: %d, fb_height: %d -> %d\n", ul_x, lr_x, rdp.frame_buffers[rdp.ci_count-1].height, lr_y);
      rdp.frame_buffers[rdp.ci_count-1].height = lr_y;
    }
}

static void fb_settextureimage()
{
  if (rdp.main_ci == 0)
    return;
  COLOR_IMAGE & cur_fb = rdp.frame_buffers[rdp.ci_count-1];
  if ( cur_fb.status >= ci_copy )
    return;
  if (((rdp.cmd0 >> 19) & 0x03) >= 2)  //check that texture is 16/32bit
  {
    int tex_format = ((rdp.cmd0 >> 21) & 0x07);
    DWORD addr = segoffset(rdp.cmd1);
    if ( tex_format == 0 )
    {
    FRDP ("fb_settextureimage. fmt: %d, size: %d, imagePtr %08lx, main_ci: %08lx, cur_ci: %08lx \n", ((rdp.cmd0 >> 21) & 0x07), ((rdp.cmd0 >> 19) & 0x03), addr, rdp.main_ci, rdp.frame_buffers[rdp.ci_count-1].addr);
      if (cur_fb.status == ci_main) 
    {
      rdp.main_ci_last_tex_addr = addr;
        if (cur_fb.height == 0)
        {
          cur_fb.height = rdp.scissor_o.lr_y;
          rdp.main_ci_end = cur_fb.addr + ((cur_fb.width * cur_fb.height) << cur_fb.size >> 1);
        }
    }
    if ((addr >= rdp.main_ci) && (addr < rdp.main_ci_end)) //addr within main frame buffer
    {
        if (cur_fb.status == ci_main)
          {
            rdp.copy_ci_index = rdp.ci_count-1;
          cur_fb.status = ci_copy_self;
          rdp.scale_x = rdp.scale_x_bak;
          rdp.scale_y = rdp.scale_y_bak;
            FRDP("rdp.frame_buffers[%d].status = ci_copy_self\n", rdp.ci_count-1);
          }
          else
          {
          if (cur_fb.width == rdp.frame_buffers[rdp.main_ci_index].width)
          {
            rdp.copy_ci_index = rdp.ci_count-1;
            cur_fb.status = ci_copy;
            FRDP("rdp.frame_buffers[%d].status = ci_copy\n", rdp.copy_ci_index);
            if ((rdp.main_ci_last_tex_addr >= cur_fb.addr) && 
              (rdp.main_ci_last_tex_addr < (cur_fb.addr + cur_fb.width*cur_fb.height*cur_fb.size)))
            {
              RDP("motion blur!\n");
              rdp.motionblur = TRUE;
            }
            else 
            {
              rdp.scale_x = 1.0f;
              rdp.scale_y = 1.0f;
            }
          }
          else if (!settings.fb_ignore_aux_copy && cur_fb.width < rdp.frame_buffers[rdp.main_ci_index].width)
          {
            rdp.copy_ci_index = rdp.ci_count-1;
            cur_fb.status = ci_aux_copy;
            FRDP("rdp.frame_buffers[%d].status = ci_aux_copy\n", rdp.copy_ci_index);
            rdp.scale_x = 1.0f;
            rdp.scale_y = 1.0f;
          }
          else
          {
            cur_fb.status = ci_aux;
            FRDP("rdp.frame_buffers[%d].status = ci_aux\n", rdp.copy_ci_index);
          }
        }
        FRDP ("Detect FB usage. texture addr is inside framebuffer: %08lx - %08lx \n", addr, rdp.main_ci);
    }
///*
      else if ((cur_fb.status != ci_main) && (addr >= rdp.zimg && addr < rdp.zimg_end))
    {
        cur_fb.status = ci_zcopy;
      FRDP("fb_settextureimage. rdp.frame_buffers[%d].status = ci_zcopy\n", rdp.ci_count-1);
    }
//*/
      else if ((addr >= rdp.maincimg[0].addr) && (addr < (rdp.maincimg[0].addr + rdp.maincimg[0].width*rdp.maincimg[0].height*2))) 
    {
        if (cur_fb.status != ci_main)
      {
          cur_fb.status = ci_old_copy;
          FRDP("rdp.frame_buffers[%d].status = ci_old_copy 1, addr:%08lx\n", rdp.ci_count-1, rdp.last_drawn_ci_addr);
      }
      rdp.read_previous_ci = TRUE;
        RDP("read_previous_ci = TRUE\n");
      }
      else if ((addr >= rdp.last_drawn_ci_addr) && (addr < (rdp.last_drawn_ci_addr + rdp.maincimg[0].width*rdp.maincimg[0].height*2))) 
      {
        if (cur_fb.status != ci_main)
        {
          cur_fb.status = ci_old_copy;
          FRDP("rdp.frame_buffers[%d].status = ci_old_copy 2, addr:%08lx\n", rdp.ci_count-1, rdp.last_drawn_ci_addr);
        }
        rdp.read_previous_ci = TRUE;
        RDP("read_previous_ci = TRUE\n");
      }
    }   
    else if (settings.fb_hires && (cur_fb.status == ci_main))
    {
      if ((addr >= rdp.main_ci) && (addr < rdp.main_ci_end)) //addr within main frame buffer
      {
        rdp.copy_ci_index = rdp.ci_count-1;
        rdp.black_ci_index = rdp.ci_count-1;
        cur_fb.status = ci_copy_self;
        FRDP("rdp.frame_buffers[%d].status = ci_copy_self\n", rdp.ci_count-1);
    }
  }
  }
  if (cur_fb.status == ci_unknown)
  {
    cur_fb.status = ci_aux;
      FRDP("fb_settextureimage. rdp.frame_buffers[%d].status = ci_aux\n", rdp.ci_count-1);
  }
}

static void fb_loadtxtr()
{
  if (rdp.frame_buffers[rdp.ci_count-1].status == ci_unknown)
  {
      rdp.frame_buffers[rdp.ci_count-1].status = ci_aux;
      FRDP("rdp.frame_buffers[%d].status = ci_aux\n", rdp.ci_count-1);
  }
}

static void fb_setdepthimage()
{
  rdp.zimg = segoffset(rdp.cmd1) & BMASK;
  rdp.zimg_end = rdp.zimg + rdp.ci_width*rdp.ci_height*2;
  FRDP ("fb_setdepthimage. addr %08lx - %08lx\n", rdp.zimg, rdp.zimg_end);
  if (rdp.zimg == rdp.main_ci)  //strange, but can happen
  {
    rdp.frame_buffers[rdp.main_ci_index].status = ci_unknown;
    if (rdp.main_ci_index < rdp.ci_count)
    {
      rdp.frame_buffers[rdp.main_ci_index].status = ci_zimg;
      FRDP("rdp.frame_buffers[%d].status = ci_zimg\n", rdp.main_ci_index);
      rdp.main_ci_index++;
      rdp.frame_buffers[rdp.main_ci_index].status = ci_main;
      FRDP("rdp.frame_buffers[%d].status = ci_main\n", rdp.main_ci_index);
      rdp.main_ci = rdp.frame_buffers[rdp.main_ci_index].addr;
      rdp.main_ci_end = rdp.main_ci + (rdp.frame_buffers[rdp.main_ci_index].width * rdp.frame_buffers[rdp.main_ci_index].height * rdp.frame_buffers[rdp.main_ci_index].size);
    }
    else
    {
      rdp.main_ci = 0;
    }
  }
  for (int i = 0; i < rdp.ci_count; i++)
  {
    COLOR_IMAGE & fb = rdp.frame_buffers[i];
    if ((fb.addr == rdp.zimg) && (fb.status == ci_aux || fb.status == ci_useless))
    {
      fb.status = ci_zimg;
      FRDP("rdp.frame_buffers[%d].status = ci_zimg\n", i);
    }
  }
}

static void fb_setcolorimage()
{
  rdp.ocimg = rdp.cimg;
  rdp.cimg = segoffset(rdp.cmd1) & BMASK;
  COLOR_IMAGE & cur_fb = rdp.frame_buffers[rdp.ci_count];
  cur_fb.width = (rdp.cmd0 & 0xFFF) + 1;
  if (cur_fb.width == 32 ) 
    cur_fb.height = 32;
  else if (cur_fb.width == 16 ) 
    cur_fb.height = 16;
  else if (rdp.ci_count > 0)
    cur_fb.height = rdp.scissor_o.lr_y;
  else
    cur_fb.height = 0;
  cur_fb.format = (rdp.cmd0 >> 21) & 0x7;
  cur_fb.size = (rdp.cmd0 >> 19) & 0x3;
  cur_fb.addr = rdp.cimg;
  cur_fb.changed = 1;
/*
  if (rdp.ci_count > 0)
    if (rdp.frame_buffers[0].addr == rdp.cimg)
      rdp.frame_buffers[0].height = rdp.scissor_o.lr_y;
*/
  FRDP ("fb_setcolorimage. width: %d,  height: %d,  fmt: %d, size: %d, addr %08lx\n", cur_fb.width, cur_fb.height, cur_fb.format, cur_fb.size, cur_fb.addr);
  if ((rdp.cimg == rdp.zimg) || (rdp.cimg == rdp.tmpzimg))
  {
    cur_fb.status = ci_zimg;
    if (rdp.zimg_end == rdp.zimg)
      rdp.zimg_end = rdp.zimg + cur_fb.width*rdp.scissor_o.lr_y*2;
    FRDP("rdp.frame_buffers[%d].status = ci_zimg\n", rdp.ci_count);
  }
  else if (rdp.main_ci != 0)
  {
    if (rdp.cimg == rdp.main_ci) //switched to main fb again 
    {
            cur_fb.height = max(cur_fb.height, rdp.frame_buffers[rdp.main_ci_index].height);
      rdp.main_ci_index = rdp.ci_count;
        rdp.main_ci_end = rdp.cimg + ((cur_fb.width * cur_fb.height) << cur_fb.size >> 1);
        cur_fb.status = ci_main;
      FRDP("rdp.frame_buffers[%d].status = ci_main\n", rdp.ci_count);
    }
    else // status is not known yet
    {
      cur_fb.status = ci_unknown;
    }
  }
  else
  {
    if ((rdp.zimg != rdp.cimg))//&& (rdp.ocimg != rdp.cimg))
    {
      rdp.main_ci = rdp.cimg;
      rdp.main_ci_end = rdp.cimg + ((cur_fb.width * cur_fb.height) << cur_fb.size >> 1);
      rdp.main_ci_index = rdp.ci_count;
      cur_fb.status = ci_main;
      FRDP("rdp.frame_buffers[%d].status = ci_main\n", rdp.ci_count);
    }
    else
    {
      cur_fb.status = ci_unknown;
    }
    
  } 
  if (rdp.frame_buffers[rdp.ci_count-1].status == ci_unknown) //status of previous fb was not changed - it is useless
  {
    if (settings.fb_hires && !settings.PM)
    {
      rdp.frame_buffers[rdp.ci_count-1].status = ci_aux;
      rdp.frame_buffers[rdp.ci_count-1].changed = 0;
      FRDP("rdp.frame_buffers[%d].status = ci_aux\n", rdp.ci_count-1);
    }
    else
    {
      rdp.frame_buffers[rdp.ci_count-1].status = ci_useless;
      /*
      DWORD addr = rdp.frame_buffers[rdp.ci_count-1].addr;
      for (int i = 0; i < rdp.ci_count - 1; i++)
      {
      if (rdp.frame_buffers[i].addr == addr)
      {
      rdp.frame_buffers[rdp.ci_count-1].status = rdp.frame_buffers[i].status;
      break;
      }
      }
      //*/
      FRDP("rdp.frame_buffers[%d].status = %s\n", rdp.ci_count-1, CIStatus[rdp.frame_buffers[rdp.ci_count-1].status]);
    }
  }
  if (cur_fb.status == ci_main)
  {
    BOOL viSwapOK = ((settings.swapmode == 2) && (rdp.vi_org_reg == *gfx.VI_ORIGIN_REG)) ? FALSE : TRUE;
    if ((rdp.maincimg[0].addr != cur_fb.addr) && SwapOK && viSwapOK)
    {
      SwapOK = FALSE;
      rdp.swap_ci_index = rdp.ci_count;
    }
  }
  rdp.ci_count++;
  if (rdp.ci_count > NUMTEXBUF) //overflow
    rdp.halt = 1;
}

// RDP graphic instructions pointer table used in DetectFrameBufferUsage

static rdp_instr gfx_instruction_lite[9][256] =
{
  {
    // uCode 0 - RSP SW 2.0X
    // 00-3f
    // games: Super Mario 64, Tetrisphere, Demos
    0,                     0,             0,              0,
      0,             0,              uc0_displaylist,        0,
      0,              0,           0,                      0,
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      // 40-7f: Unused
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      // 80-bf: Immediate commands
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      uc0_enddl,              0,                        0,                      0,
      fb_uc0_moveword,           0,                     uc0_culldl,             0,
      // c0-ff: RDP commands
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                      0,                      0,                      0,        
      0,                  0,                  0,                  0,    
      0,                  0,                  0,                  0,    
      0,                  0,                  0,                  0,    
      0,                  0,                  0,                  0,    
      0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
  },
  
  // uCode 1 - F3DEX 1.XX
  // 00-3f
  // games: Mario Kart, Star Fox
  {
      0,                      0,                      0,                      0,        
        0,             0,              uc0_displaylist,        0,
        0,              0,           0,                      0,
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        // 40-7f: unused
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        // 80-bf: Immediate commands
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      uc6_loaducode,        
        uc1_branch_z,           0,               0,        0,
        uc1_rdphalf_1,          0,             0,  0,
        uc0_enddl,              0,     0,     0,
        fb_uc0_moveword,           0,          uc2_culldl,             0,
        // c0-ff: RDP commands
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
      },
      
      // uCode 2 - F3DEX 2.XX
      // games: Zelda 64
      {
        // 00-3f
        0,                  0,              0,          uc2_culldl,
          uc1_branch_z,         0,              0,          0,
          0,                fb_bg_copy,         fb_bg_copy,         0,
          0,                    0,                  0,                  0,
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // 40-7f: unused
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // 80-bf: unused
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // c0-ff: RDP commands mixed with uc2 commands
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,            uc2_dlist_cnt,              0,                  0,
          0,            0,          0,              fb_uc2_moveword,
          0/*fb_uc2_movemem*/,          uc2_load_ucode,         uc0_displaylist,    uc0_enddl,
          0,                    uc1_rdphalf_1,          0,      0,
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
        },
        
        // uCode 3 - "RSP SW 2.0D", but not really
        // 00-3f
        // games: Wave Race
        // ** Added by Gonetz **
        {
          0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            // 40-7f: unused
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            // 80-bf: Immediate commands
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                      0,                      0,                      0,        
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            uc0_enddl,              0,     0,     0,
            fb_uc0_moveword,           0,          uc0_culldl,             0,
            // c0-ff: RDP commands
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
            0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
          },
          
          {
            // uCode 4 - RSP SW 2.0D EXT
            // 00-3f
            // games: Star Wars: Shadows of the Empire
            0,                  0,                  0,                  0,    
              0,             0,              uc0_displaylist,        0,
              0,                  0,                  0,                  0,    
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              // 40-7f: Unused
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              // 80-bf: Immediate commands
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                      0,                      0,                      0,        
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              uc0_enddl,              0,     0,     0,
              fb_uc0_moveword,           0,          uc0_culldl,             0,
              // c0-ff: RDP commands
              rdp_noop,               0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
              0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
            },
            
            {
              // uCode 5 - RSP SW 2.0 Diddy
              // 00-3f
              // games: Diddy Kong Racing
                0,                      0,                      0,                      0,
                0,                      0,                 uc0_displaylist,          uc5_dl_in_mem,
                0,                      0,                      0,                      0,
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                // 40-7f: Unused
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                // 80-bf: Immediate commands
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,        
                0,                      0,                      0,                      0,    
                uc0_enddl,              0,                      0,                      0,
                fb_uc0_moveword,        0,                      uc0_culldl,             0,
                // c0-ff: RDP commands
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
                0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
              },
              
              // uCode 6 - S2DEX 1.XX
              // games: Yoshi's Story
              {
                0,                  0,                  0,                  0,    
                  0,             0,         uc0_displaylist,        0,
                  0,                  0,                  0,                  0,    
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  // 40-7f: unused
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  // 80-bf: Immediate commands
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      0,        
                  0,                      0,                      0,                      uc6_loaducode,        
                  uc6_select_dl,          0,         0,     0,
                  0,                  0,                  0,                  0,    
                  uc0_enddl,              0,     0,     0,
                  fb_uc0_moveword,           0,          uc2_culldl,             0,
                  // c0-ff: RDP commands
                  0,               fb_loadtxtr,       fb_loadtxtr,    fb_loadtxtr,
                  fb_loadtxtr,        0,                  0,                  0,
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
                  0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
                },
                
  {
        0,                      0,                      0,                      0,        
        0,                      0,                      uc0_displaylist,        0,
        0,                      0,                      0,                      0,
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        // 40-7f: unused
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        // 80-bf: Immediate commands
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,
        0,                      0,                      0,                      0,
        uc0_enddl,              0,                      0,                      0,
        fb_uc0_moveword,        0,                      uc0_culldl,             0,
        // c0-ff: RDP commands
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                      0,                      0,                      0,        
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
        0,                  0,                  0,                  0,    
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
      },

      {
        // 00-3f
        0,                  0,              0,          uc2_culldl,
          uc1_branch_z,         0,              0,          0,
          0,                fb_bg_copy,         fb_bg_copy,         0,
          0,                    0,                  0,                  0,
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // 40-7f: unused
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // 80-bf: unused
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          0,                    0,                  0,                  0,
          
          // c0-ff: RDP commands mixed with uc2 commands
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,                  0,                  0,                  0,    
          0,            uc2_dlist_cnt,              0,                  0,
          0,            0,          0,              fb_uc2_moveword,
          0,            uc2_load_ucode,         uc0_displaylist,    uc0_enddl,
          0,            uc1_rdphalf_1,            0,                  0,
          fb_rect,         fb_rect,               0,                  0,    
          0,                  0,                  0,                  0,    
          0,         fb_setscissor,         0,       0,
          0,                  0,                  0,                  0,    
          0,                  0,                fb_rect,              0,    
          0,                  0,                  0,                  0,    
          0,         fb_settextureimage,    fb_setdepthimage,      fb_setcolorimage
        }
};

