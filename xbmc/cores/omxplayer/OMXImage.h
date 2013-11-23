#pragma once
/*
 *      Copyright (C) 2010-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#if defined(HAVE_OMXLIB)

#include "OMXCore.h"

#include <IL/OMX_Video.h>

#include "OMXClock.h"
#if defined(STANDALONE)
#define XB_FMT_A8R8G8B8 1
#include "File.h"
#else
#include "filesystem/File.h"
#include "guilib/XBTF.h"
#endif

#include "system_gl.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "threads/Thread.h"

using namespace XFILE;
using namespace std;

class COMXImageFile;

class COMXImage : public CThread
{
enum TextureAction {TEXTURE_ALLOC, TEXTURE_DELETE };

struct textureinfo {
  TextureAction action;
  int width, height;
  GLuint texture;
  EGLImageKHR egl_image;
  void *parent;
  const char *filename;
  CEvent sync;
};

protected:
  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process();
public:
  COMXImage();
  virtual ~COMXImage();
  void Initialize();
  void Deinitialize();
  static COMXImageFile *LoadJpeg(const CStdString& texturePath);
  static void CloseJpeg(COMXImageFile *file);

  static bool DecodeJpeg(COMXImageFile *file, unsigned int maxWidth, unsigned int maxHeight, unsigned int stride, void *pixels);
  static bool CreateThumbnailFromSurface(unsigned char* buffer, unsigned int width, unsigned int height,
      unsigned int format, unsigned int pitch, const CStdString& destFile);
  static bool ClampLimits(unsigned int &width, unsigned int &height, unsigned int m_width, unsigned int m_height, bool transposed = false);
  static bool CreateThumb(const CStdString& srcFile, unsigned int width, unsigned int height, std::string &additional_info, const CStdString& destFile);
  bool DecodeJpegToTexture(COMXImageFile *file, unsigned int width, unsigned int height, void **userdata);
  void DestroyTexture(void *userdata);
  void GetTexture(void *userdata, GLuint *texture);
private:
  EGLDisplay m_egl_display;
  EGLContext m_egl_context;

  void CreateContext();
  CCriticalSection               m_texqueue_lock;
  XbmcThreads::ConditionVariable m_texqueue_cond;
  std::queue <struct textureinfo *> m_texqueue;
  void AllocTextureInternal(struct textureinfo *tex);
  void DestroyTextureInternal(struct textureinfo *tex);
};

class COMXImageFile
{
public:
  COMXImageFile();
  virtual ~COMXImageFile();
  bool ReadFile(const CStdString& inputFile);
  int  GetOrientation() { return m_orientation; };
  unsigned int GetWidth()  { return m_width; };
  unsigned int GetHeight() { return m_height; };
  unsigned long GetImageSize() { return m_image_size; };
  const uint8_t *GetImageBuffer() { return (const uint8_t *)m_image_buffer; };
  const char *GetFilename() { return m_filename; };
protected:
  OMX_IMAGE_CODINGTYPE GetCodingType(unsigned int &width, unsigned int &height);
  uint8_t           *m_image_buffer;
  unsigned long     m_image_size;
  unsigned int      m_width;
  unsigned int      m_height;
  int               m_orientation;
  const char *      m_filename;
};

class COMXImageDec
{
public:
  COMXImageDec();
  virtual ~COMXImageDec();

  // Required overrides
  void Close();
  bool Decode(const uint8_t *data, unsigned size, unsigned int width, unsigned int height, unsigned stride, void *pixels);
  unsigned int GetDecodedWidth() { return (unsigned int)m_decoded_format.format.image.nFrameWidth; };
  unsigned int GetDecodedHeight() { return (unsigned int)m_decoded_format.format.image.nFrameHeight; };
  unsigned int GetDecodedStride() { return (unsigned int)m_decoded_format.format.image.nStride; };
protected:
  bool HandlePortSettingChange(unsigned int resize_width, unsigned int resize_height);
  // Components
  COMXCoreComponent             m_omx_decoder;
  COMXCoreComponent             m_omx_resize;
  COMXCoreTunel                 m_omx_tunnel_decode;
  OMX_BUFFERHEADERTYPE          *m_decoded_buffer;
  OMX_PARAM_PORTDEFINITIONTYPE  m_decoded_format;
  CCriticalSection              m_OMXSection;
};

class COMXImageEnc
{
public:
  COMXImageEnc();
  virtual ~COMXImageEnc();

  // Required overrides
  bool CreateThumbnailFromSurface(unsigned char* buffer, unsigned int width, unsigned int height,
      unsigned int format, unsigned int pitch, const CStdString& destFile);
protected:
  bool Encode(unsigned char *buffer, int size, unsigned int width, unsigned int height, unsigned int pitch);
  // Components
  COMXCoreComponent             m_omx_encoder;
  OMX_BUFFERHEADERTYPE          *m_encoded_buffer;
  OMX_PARAM_PORTDEFINITIONTYPE  m_encoded_format;
  CCriticalSection              m_OMXSection;
};

class COMXImageReEnc
{
public:
  COMXImageReEnc();
  virtual ~COMXImageReEnc();

  // Required overrides
  void Close();
  bool ReEncode(COMXImageFile &srcFile, unsigned int width, unsigned int height, void * &pDestBuffer, unsigned int &nDestSize);
protected:
  bool HandlePortSettingChange(unsigned int resize_width, unsigned int resize_height, bool port_settings_changed);
  // Components
  COMXCoreComponent             m_omx_decoder;
  COMXCoreComponent             m_omx_resize;
  COMXCoreComponent             m_omx_encoder;
  COMXCoreTunel                 m_omx_tunnel_decode;
  COMXCoreTunel                 m_omx_tunnel_resize;
  OMX_BUFFERHEADERTYPE          *m_encoded_buffer;
  CCriticalSection              m_OMXSection;
  void                          *m_pDestBuffer;
  unsigned int                  m_nDestAllocSize;
};

class COMXTexture
{
public:
  COMXTexture();
  virtual ~COMXTexture();

  // Required overrides
  void Close(void);
  bool Decode(const uint8_t *data, unsigned size, unsigned int width, unsigned int height, void *egl_image, void *egl_display);
protected:
  bool HandlePortSettingChange(unsigned int resize_width, unsigned int resize_height, void *egl_image, void *egl_display, bool port_settings_changed);

  // Components
  COMXCoreComponent m_omx_decoder;
  COMXCoreComponent m_omx_resize;
  COMXCoreComponent m_omx_egl_render;

  COMXCoreTunel     m_omx_tunnel_decode;
  COMXCoreTunel     m_omx_tunnel_egl;

  OMX_BUFFERHEADERTYPE *m_egl_buffer;
  CCriticalSection              m_OMXSection;
};

extern COMXImage g_OMXImage;
#endif
