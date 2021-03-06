/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           George Sedov <radist.morse@gmail.com>                               #
#                  - Threaded encoding                                          #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

#ifndef LAVC_COMMON_H
#define LAVC_COMMON_H

#include "../config.h"
#include "defs.h"
#include "sound.h"
#include "video_format.h"

#ifdef HAS_AVCODEC_H
  #include <avcodec.h>
  #include <audioconvert.h>
#else
  #ifdef HAS_LIBAVCODEC_AVCODEC_H
    #include <libavcodec/avcodec.h>
    #include <libavutil/audioconvert.h> //channel_layout definitions
  #else
    #ifdef HAS_FFMPEG_AVCODEC_H
      #include <ffmpeg/avcodec.h>
      #include <ffmpeg/audioconvert.h>
    #else
      #include <libavcodec/avcodec.h>
      #include <libavutil/audioconvert.h>
    #endif
  #endif
#endif

#define LIBAVCODEC_VER_AT_LEAST(major,minor)  (LIBAVCODEC_VERSION_MAJOR > major || \
                                              (LIBAVCODEC_VERSION_MAJOR == major && \
                                               LIBAVCODEC_VERSION_MINOR >= minor))
                                               
#if !LIBAVCODEC_VER_AT_LEAST(53,0)
  #define AV_SAMPLE_FMT_S16 SAMPLE_FMT_S16
  #define AV_SAMPLE_FMT_FLT SAMPLE_FMT_FLT 
#endif

#define MAX_DELAYED_FRAMES 50  //Maximum supported delayed frames

/*video*/
struct lavcData
{
	AVCodec *codec;
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	AVDictionary *private_options;
#endif
	AVCodecContext *codec_context;
	AVFrame *picture;
	AVPacket *outpkt;
	
	int delayed_frames;
	int index_of_df; //index of delayed frame pts in use;
	INT64 delayed_pts[MAX_DELAYED_FRAMES]; //delayed frames pts
	int flush_delayed_frames;
	int flushed_buffers;
	int flush_done;
	
	BYTE* tmpbuf;
	int outbuf_size;
	BYTE* outbuf;
	
	int codec_id;
	int monotonic_pts;
};

/*Audio*/
struct lavcAData
{
	AVCodec *codec;
	AVCodecContext *codec_context;
	AVFrame *frame;
	AVPacket *outpkt;
	
	int outbuf_size;
	BYTE* outbuf;
	
	int monotonic_pts;
};

int encode_lavc_frame (BYTE *picture_buf, struct lavcData* data, int format, struct VideoFormatData *videoF);

int encode_lavc_audio_frame (void *audio_buf, struct lavcAData* data, struct VideoFormatData *videoF);

/* arg = pointer to lavcData struct =>
 * *arg = struct lavcData**
 */
int clean_lavc (void *arg);

int clean_lavc_audio (void* arg);

struct lavcData* init_lavc(int width, int height, int fps_num, int fps_den, int codec_ind);

struct lavcAData* init_lavc_audio(struct paRecordData *pdata, int codec_ind);

#endif
