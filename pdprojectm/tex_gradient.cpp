////////////////////////////////////////////////////////
//
// GEM - Graphics Environment for Multimedia
//
// zmoelnig@iem.at
//
// Implementation file
//
//    Copyright (c) 1997-1999 Mark Danks.
//    Copyright (c) Günther Geiger.
//    Copyright (c) 2001-2011 IOhannes m zmölnig. forum::für::umläute. IEM. zmoelnig@iem.at
//    Copyright (c) 2002-2006 James Tittle & Chris Clepper
//    For information on usage and redistribution, and for a DISCLAIMER OF ALL
//    WARRANTIES, see the file, "GEM.LICENSE.TERMS" in this distribution.
//
/////////////////////////////////////////////////////////

#include "tex_gradient.h"

#include "Gem/Settings.h"
#include "Gem/Image.h"
#include "Utils/Functions.h"
#include <string.h>

#ifdef debug_post
# undef debug_post
#endif

//#define DEBUG_ME

#ifdef DEBUG_ME
# define debug_post post
#else
# include "Utils/nop.h"
# define debug_post nop_post
#endif

CPPEXTERN_NEW(tex_gradient);

/////////////////////////////////////////////////////////
//
// tex_gradient
//
/////////////////////////////////////////////////////////
// Constructor
//
/////////////////////////////////////////////////////////
tex_gradient :: tex_gradient()
  : m_textureOnOff(1),
    m_textureMinQuality(GL_LINEAR), m_textureMagQuality(GL_LINEAR),
    m_wantMipmap(false), m_canMipmap(false), m_hasMipmap(false),
    m_repeat(GL_REPEAT),
    m_didTexture(false), m_rebuildList(false),
    m_textureObj(0),
    m_realTextureObj(0),
    m_oldTexCoords(NULL), m_oldNumCoords(0), m_oldTexture(0),
    m_oldBaseCoord(TexCoord(1.,1.)), m_oldOrientation(true),
    m_textureType( GL_TEXTURE_2D ),
    m_clientStorage(0), //have to do this due to texture corruption issues
    m_yuv(1),
    m_texunit(0),
    m_numTexUnits(0),
    m_numPbo(0), m_oldNumPbo(0), m_curPbo(0), m_pbo(NULL),
    m_upsidedown(false)
{
  m_dataSize[0] = m_dataSize[1] = m_dataSize[2] = -1;
  m_buffer.xsize = m_buffer.ysize = m_buffer.csize = -1;
  m_buffer.data = NULL;

  int ival=1;
  gem::Settings::get("texture.repeat", ival);
  repeatMess(ival);

  ival=1;
  gem::Settings::get("texture.quality", ival);
  textureQuality(ival);

  gem::Settings::get("texture.pbo", m_numPbo);

  // create an outlet to send texture ID
  m_outTexID = outlet_new(this->x_obj, &s_float);
}

////////////////////////////////////////////////////////
// Destructor
//
/////////////////////////////////////////////////////////
tex_gradient :: ~tex_gradient()
{
  if(m_outTexID) {
    outlet_free(m_outTexID);
  }

  m_outTexID=NULL;
}

////////////////////////////////////////////////////////
// setUpTextureState
//
/////////////////////////////////////////////////////////
void tex_gradient :: setUpTextureState()
{
  GLuint doRepeat=m_repeat;

  if (GLEW_APPLE_client_storage) {
    if(m_clientStorage) {
      glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
      debug_post("using client storage");
    } else {
      glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
      debug_post("not using client storage");
    }
  } else {
    glPixelStoref(GL_UNPACK_ALIGNMENT, 1);
  }

  setTexFilters(m_textureMinQuality != GL_LINEAR_MIPMAP_LINEAR
                || (m_wantMipmap && m_canMipmap));
  glTexParameterf(m_textureType, GL_TEXTURE_WRAP_S, doRepeat);
  glTexParameterf(m_textureType, GL_TEXTURE_WRAP_T, doRepeat);
}

void tex_gradient :: setTexFilters(bool mipmap)
{
  glTexParameterf(m_textureType, GL_TEXTURE_MAG_FILTER, m_textureMagQuality);
  if (mipmap) {
    glTexParameterf(m_textureType, GL_TEXTURE_MIN_FILTER, m_textureMinQuality);
  } else {
    glTexParameterf(m_textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }
}

////////////////////////////////////////////////////////
// setTexCoords
//
// upsidedown is derived from the imageStruct.upsidedown
// use this when loading images...
//
/////////////////////////////////////////////////////////
inline void setTexCoords(TexCoord *coords, float xRatio, float yRatio,
                         GLboolean upsidedown=false)
{
  if(!upsidedown) {
    coords[0].s = 0.f;
    coords[0].t = 0.f;
    coords[1].s = xRatio;
    coords[1].t = 0.f;
    coords[2].s = xRatio;
    coords[2].t = yRatio;
    coords[3].s = 0.f;
    coords[3].t = yRatio;
  } else {
    coords[3].s = 0.f;
    coords[3].t = 0.f;
    coords[2].s = xRatio;
    coords[2].t = 0.f;
    coords[1].s = xRatio;
    coords[1].t = yRatio;
    coords[0].s = 0.f;
    coords[0].t = yRatio;
  }
}

static inline void tex2state(GemState*state, TexCoord*coords, int size)
{
  state->set(GemState::_GL_TEX_COORDS, coords);
  state->set(GemState::_GL_TEX_NUMCOORDS, size);
}


////////////////////////////////////////////////////////
// extension check
//
/////////////////////////////////////////////////////////
bool tex_gradient :: isRunnable(void)
{
  /* for simplicity's sake, i have dropped support for very old openGL-versions */
  if(!GLEW_VERSION_3_3) {
    error("need at least openGL-3.3 for libprojectm refusing to work");
    return false;
  }

  GLint numTexUnits=0;
  if(GLEW_ARB_multitexture) {
    glGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &numTexUnits );
  }
  m_numTexUnits=numTexUnits;

  m_canMipmap=(GLEW_ARB_framebuffer_object!=0);

  return true;
}

void tex_gradient :: pushTexCoords(GemState*state)
{
  state->get(GemState::_GL_TEX_COORDS, m_oldTexCoords);
  state->get(GemState::_GL_TEX_NUMCOORDS, m_oldNumCoords);
  state->get(GemState::_GL_TEX_TYPE, m_oldTexture);
  state->get(GemState::_GL_TEX_ORIENTATION, m_oldOrientation);
  state->get(GemState::_GL_TEX_BASECOORD, m_oldBaseCoord);
}

void tex_gradient :: popTexCoords(GemState*state)
{
  tex2state(state, m_oldTexCoords, m_oldNumCoords);
  state->set(GemState::_GL_TEX_TYPE, m_oldTexture);
  state->set(GemState::_GL_TEX_ORIENTATION, m_oldOrientation);
  state->set(GemState::_GL_TEX_BASECOORD, m_oldBaseCoord);
}


void tex_gradient :: sendExtTexture(GLuint texobj, GLfloat xRatio,
                                   GLfloat yRatio, GLint texType, GLboolean upsidedown)
{
  // send textureID to outlet
  if(texobj) {
    t_atom ap[5];
    SETFLOAT(ap, (t_float)texobj);
    SETFLOAT(ap+1, (t_float)xRatio);
    SETFLOAT(ap+2, (t_float)yRatio);
    SETFLOAT(ap+3, (t_float)texType);
    SETFLOAT(ap+4, (t_float)upsidedown);
    outlet_list(m_outTexID, &s_list, 5, ap);
  }
}

////////////////////////////////////////////////////////
// render
//
/////////////////////////////////////////////////////////
void tex_gradient :: render(GemState *state)
{
  m_didTexture=false;
  pushTexCoords(state);

  if(!m_textureOnOff) {
    return;
  }

  bool upsidedown=false;
  bool normalized=true;
  bool canMipmap=m_canMipmap;

  int texType = m_textureType;
  int x_2=1, y_2=1;
  int newfilm = 0;
  pixBlock*img=NULL;
  GLint internalformat = GL_RGBA;

  if(m_pbo && (m_numPbo != m_oldNumPbo)) {
    /* the PBO settings have changed, invalidate the old PBO */
    GLuint*pbo=m_pbo;
    glDeleteBuffersARB(m_numPbo, pbo);
    delete[]pbo;
    m_pbo=NULL;
  }

  state->get(GemState::_PIX, img);
  if(img) {
    newfilm = img->newfilm;
  }

//todo place texture drawing here
  if (!img || !img->image.data) {
    if(0>0) {
      
    } else
      /* neither do we have an image nor an external texture */
    {
      return;
    }
  }
  tex2state(state, m_coords, 4);

    upsidedown = img->image.upsidedown;
    if (img->newimage) {
      m_rebuildList = true;
    }

    img->image.copy2ImageStruct(&m_imagebuf);
    switch(m_imagebuf.type) {
    case GL_FLOAT:
    case GL_DOUBLE:
      internalformat =  GL_RGBA32F;
      break;
    default:
      internalformat = GL_RGBA;
    }

    x_2 = powerOfTwo(m_imagebuf.xsize);
    y_2 = powerOfTwo(m_imagebuf.ysize);

    normalized = ((m_imagebuf.xsize==x_2) && (m_imagebuf.ysize==y_2));

    debug_post("normalized=%d\t%d - %d\t%d - %d", normalized, m_imagebuf.xsize,
               x_2, m_imagebuf.ysize, y_2);

      m_textureType = GL_TEXTURE_2D;
      debug_post("using mode 0:GL_TEXTURE_2D");
      normalized = 0;

    debug_post("normalized=%d", normalized);
  

  if (m_textureType!=texType) {
    debug_post("texType != m_textureType");
    stopRendering();
    startRendering();
  }

  if(GLEW_VERSION_1_3) {
    glActiveTexture(GL_TEXTURE0_ARB + m_texunit);
  }
  glEnable(m_textureType);
  glBindTexture(m_textureType, m_textureObj);

  if (newfilm ) {
    //  tigital:  shouldn't we also allow TEXTURE_2D here?
    if(NULL!=glTextureRangeAPPLE) {
      if ( GLEW_APPLE_texture_range ) {
        if(glTextureRangeAPPLE == NULL) {
          glTextureRangeAPPLE( m_textureType,
                               m_imagebuf.xsize * m_imagebuf.ysize * m_imagebuf.csize,
                               m_imagebuf.data );
          debug_post("using glTextureRangeAPPLE()");
        } else {
          glTextureRangeAPPLE( m_textureType, 0, NULL );
        }
      }
    }

    /* hmm, GL_TEXTURE_STORAGE_HINT_APPLE throws a GL-error on linux (and probably on w32 too)
     * how to do a run-time check for it?
     *
     * according to http://developer.apple.com/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/chapter_10_section_2.html
     * this seems to be a part of the texture_range extension, so we check for that!
     */
    if(GLEW_APPLE_texture_range) {
      glTexParameteri( m_textureType, GL_TEXTURE_STORAGE_HINT_APPLE,
                       GL_STORAGE_SHARED_APPLE );
    }
    // GL_STORAGE_SHARED_APPLE -  AGP texture path
    // GL_STORAGE_CACHED_APPLE - VRAM texture path
    // GL_STORAGE_PRIVATE_APPLE - normal texture path
    if(m_clientStorage) {
      glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
    }
  }



  /* here comes the work: a new image has to be transferred from main memory to GPU and attached to a texture object */

  if (m_rebuildList) {
    // if YUV is not supported on this platform, we have to convert it to RGB
    //(skip Alpha since it isn't used)
    const bool do_yuv = m_yuv && GLEW_APPLE_ycbcr_422;
    if (!do_yuv && m_imagebuf.format == GEM_YUV) {
      m_imagebuf.format=GL_RGB;
      m_imagebuf.csize=3;
      m_imagebuf.reallocate();
      if(img) {
        m_imagebuf.fromYUV422(img->image.data);
      }
    }
    if (normalized) {
      m_buffer.xsize = m_imagebuf.xsize;
      m_buffer.ysize = m_imagebuf.ysize;
      m_buffer.csize  = m_imagebuf.csize;
      m_buffer.format = m_imagebuf.format;
      m_buffer.type   = m_imagebuf.type;
      m_buffer.reallocate();
      m_xRatio=1.0;
      m_yRatio=1.0;
      m_upsidedown=upsidedown;

      tex2state(state, m_coords, 4);
      if (m_buffer.csize != m_dataSize[0] ||
          m_buffer.xsize != m_dataSize[1] ||
          m_buffer.ysize != m_dataSize[2]) {
        m_dataSize[0] = m_buffer.csize;
        m_dataSize[1] = m_buffer.xsize;
        m_dataSize[2] = m_buffer.ysize;

      }
      //if the texture is a power of two in size then there is no need to subtexture
      glTexImage2D(m_textureType, /* target */
                   0, /* level */
                   internalformat, /* internalformat */
                   m_imagebuf.xsize, m_imagebuf.ysize,
                   0, /* border */
                   m_imagebuf.format,
                   m_imagebuf.type,
                   m_imagebuf.data);
      m_hasMipmap = false;

    } else { // !normalized
      m_xRatio = (float)m_imagebuf.xsize;
      m_yRatio = (float)m_imagebuf.ysize;
        m_xRatio /= (float)x_2;
        m_yRatio /= (float)y_2;
        m_buffer.xsize = x_2;
        m_buffer.ysize = y_2;

      m_buffer.csize  = m_imagebuf.csize;
      m_buffer.format = m_imagebuf.format;
      m_buffer.type   = m_imagebuf.type;
      m_buffer.reallocate();
      m_upsidedown=upsidedown;
      tex2state(state, m_coords, 4);

      if (m_buffer.csize != m_dataSize[0] ||
          m_buffer.xsize != m_dataSize[1] ||
          m_buffer.ysize != m_dataSize[2]) {
        newfilm = 1;

      } //end of loop if size has changed

      // okay, load in the actual pixel data

      //when doing rectangle textures the buffer changes after every film is loaded this call makes sure the
      //texturing is updated as well to prevent crashes
      if(newfilm) {
        m_dataSize[0] = m_buffer.csize;
        m_dataSize[1] = m_buffer.xsize;
        m_dataSize[2] = m_buffer.ysize;

        if (m_buffer.format == GEM_YUV && !m_rectangle) {
          m_buffer.setBlack();
        }

        if(m_numPbo>0) {
          if(GLEW_ARB_pixel_buffer_object) {
            GLuint*pbo=m_pbo;
            if(pbo) {
              delete[]pbo;
              pbo=NULL;
            }
            pbo=new GLuint[m_numPbo];
            m_oldNumPbo=m_numPbo;
            m_pbo=pbo;
            glGenBuffersARB(m_numPbo, pbo);
            int i=0;
            for(i=0; i<m_numPbo; i++) {
              glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo[i]);
              glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                              m_buffer.xsize*m_buffer.ysize*m_buffer.csize,
                              0, GL_STREAM_DRAW_ARB);
            }
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

          } else {
            verbose(1, "PBOs not supported! disabling");
            m_numPbo=0;
          }
        }

        //this is for dealing with power of 2 textures which need a buffer that's 2^n
          glTexImage2D( m_textureType, 0,
                        //m_buffer.csize,
                        internalformat,
                        m_buffer.xsize,
                        m_buffer.ysize, 0,
                        m_buffer.format,
                        m_buffer.type,
                        m_buffer.data);
          m_hasMipmap = false;
          debug_post("TexImage2D non rectangle");

        // just to make sure...
        img->newfilm = 0;
      }

      if(m_pbo && m_numPbo) {
        GLuint*pbo=m_pbo;
        m_curPbo=(m_curPbo+1)%m_numPbo;
        GLuint index=m_curPbo;
        GLuint nextIndex=(m_curPbo+1)%m_numPbo;

        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo[index]);
        glTexSubImage2D(m_textureType, 0,
                        0, 0,
                        m_imagebuf.xsize,
                        m_imagebuf.ysize,
                        m_imagebuf.format,
                        m_imagebuf.type,
                        NULL); /* <-- that's the key */
        m_hasMipmap = false;

        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo[nextIndex]);
        glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                        m_imagebuf.xsize * m_imagebuf.ysize * m_imagebuf.csize, 0,
                        GL_STREAM_DRAW_ARB);

        GLubyte* ptr = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                                                GL_WRITE_ONLY_ARB);
        if(ptr) {
          // update data off the mapped buffer
          memcpy(ptr, m_imagebuf.data,
                 m_imagebuf.xsize * m_imagebuf.ysize * m_imagebuf.csize);
          glUnmapBufferARB(
            GL_PIXEL_UNPACK_BUFFER_ARB); // release pointer to mapping buffer
        }

        /* unbind the current buffer */
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

      } else {
        glTexSubImage2D(m_textureType, 0,
                        0, 0,                           // position
                        m_imagebuf.xsize,
                        m_imagebuf.ysize,
                        m_imagebuf.format,
                        m_imagebuf.type,
                        m_imagebuf.data);
        m_hasMipmap = false;
      }
    }
  } // rebuildlist

  if (m_wantMipmap && canMipmap && !m_hasMipmap) {
    glGenerateMipmap(m_textureType);
    m_hasMipmap = true;
  }
  setTexFilters(m_textureMinQuality != GL_LINEAR_MIPMAP_LINEAR
                || (m_wantMipmap && canMipmap));

  setTexCoords(m_coords, m_xRatio, m_yRatio, m_upsidedown);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, m_env);

  /* cleanup */
  m_rebuildList = false;
  m_didTexture=true;

  int numTexUnits=m_numTexUnits;
  state->set(GemState::_GL_TEX_UNITS, numTexUnits);

  // if we are using rectangle textures, this is a way to inform the downstream objects
  // (this is important for things like [pix_coordinate]

  // we don't use switch/case as _ARB and _EXT might be the same...
  if(m_textureType==GL_TEXTURE_RECTANGLE_ARB
      || m_textureType==GL_TEXTURE_RECTANGLE_EXT) {
    state->set(GemState::_GL_TEX_TYPE, 2);
  } else {
    state->set(GemState::_GL_TEX_TYPE, 1);
  }

  m_baseCoord.s=m_xRatio;
  m_baseCoord.t=m_yRatio;
  state->set(GemState::_GL_TEX_BASECOORD, m_baseCoord);
  state->set(GemState::_GL_TEX_ORIENTATION, upsidedown);

  sendExtTexture(m_textureObj, m_xRatio, m_yRatio, m_textureType,
                 upsidedown);
}

////////////////////////////////////////////////////////
// postrender
//
/////////////////////////////////////////////////////////
void tex_gradient :: postrender(GemState *state)
{
  popTexCoords(state);

  if (m_didTexture) {
    if(GLEW_VERSION_1_3) {
      glActiveTexture(GL_TEXTURE0_ARB + m_texunit);  //needed?
    }
    glDisable(m_textureType);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // to avoid matrix stack confusion, we reset the upstream texunit to 0
    if(GLEW_VERSION_1_3) {
      glActiveTexture(GL_TEXTURE0_ARB);
    }
  }

}

////////////////////////////////////////////////////////
// startRendering
//
////////////////////////////////////////////////////////
void tex_gradient :: startRendering()
{
  GLuint obj=0;
  glGenTextures(1, &obj); // this crashes sometimes!!!! (jmz)
  m_realTextureObj=obj;
  if(GLEW_VERSION_1_3) {
    glActiveTexture(GL_TEXTURE0_ARB + m_texunit);
  }
  glBindTexture(m_textureType, m_realTextureObj);
  m_textureObj=m_realTextureObj;
  setUpTextureState();

  m_dataSize[0] = m_dataSize[1] = m_dataSize[2] = -1;

  if (!m_realTextureObj)        {
    error("Unable to allocate texture object");
    return;
  }
}

////////////////////////////////////////////////////////
// stopRendering
//
/////////////////////////////////////////////////////////
void tex_gradient :: stopRendering()
{
  if(m_realTextureObj) {
    GLuint obj=m_realTextureObj;
    glDeleteTextures(1, &obj);

    m_realTextureObj = 0;
    m_dataSize[0] = m_dataSize[1] = m_dataSize[2] = -1;
  }

  if(m_pbo) {
    GLuint*pbo=m_pbo;
    glDeleteBuffersARB(m_numPbo, pbo);
    delete[]pbo;
    m_pbo=NULL;
  }

}


////////////////////////////////////////////////////////
// textureQuality
//
/////////////////////////////////////////////////////////
void tex_gradient :: setModified()
{
  m_rebuildList=true;
  GemBase::setModified();
}

////////////////////////////////////////////////////////
// textureOnOff
//
/////////////////////////////////////////////////////////
void tex_gradient :: textureOnOff(int on)
{
  m_textureOnOff = on;
  setModified();
}

/////////////////////////////////////////////////////////
// textureQuality
//
/////////////////////////////////////////////////////////
void tex_gradient :: textureQuality(int type)
{
  if (type == 2) {
    m_textureMinQuality = GL_LINEAR_MIPMAP_LINEAR;
    m_textureMagQuality = GL_LINEAR;
    m_wantMipmap = true;
  } else {
    m_wantMipmap = false;
    if (type) {
      m_textureMinQuality = GL_LINEAR;
      m_textureMagQuality = GL_LINEAR;
    } else {
      m_textureMinQuality = GL_NEAREST;
      m_textureMagQuality = GL_NEAREST;
    }
  }

  if (m_textureObj) {
    if(GLEW_VERSION_1_3) {
      glActiveTexture(GL_TEXTURE0_ARB + m_texunit);
    }
    glBindTexture(m_textureType, m_textureObj);
    setTexFilters(m_textureMinQuality != GL_LINEAR_MIPMAP_LINEAR
                  || (m_wantMipmap && m_canMipmap));
  }
  setModified();
}

////////////////////////////////////////////////////////
// texture repeat message
//
/////////////////////////////////////////////////////////
void tex_gradient :: repeatMess(int type)
{
  if (type) {
    m_repeat = GL_REPEAT;
  } else {
    if( (getState()!=INIT) && GLEW_EXT_texture_edge_clamp) {
      m_repeat = GL_CLAMP_TO_EDGE;
    } else {
      m_repeat = GL_CLAMP;
    }
  }
  GLuint doRepeat=m_repeat;
  if ( m_textureType ==  GL_TEXTURE_RECTANGLE_ARB
       || m_textureType == GL_TEXTURE_RECTANGLE_EXT) {
    doRepeat=GL_CLAMP_TO_EDGE;
  }

  if (m_textureObj) {
    if(GLEW_VERSION_1_1) {
      glBindTexture(m_textureType, m_textureObj);
      glTexParameterf(m_textureType, GL_TEXTURE_WRAP_S, doRepeat);
      glTexParameterf(m_textureType, GL_TEXTURE_WRAP_T, doRepeat);
    } else {
      glBindTextureEXT(m_textureType, m_textureObj);
      glTexParameteri(m_textureType, GL_TEXTURE_WRAP_S, doRepeat);
      glTexParameteri(m_textureType, GL_TEXTURE_WRAP_T, doRepeat);
    }
  }
  setModified();
}

////////////////////////////////////////////////////////
// texture environment mode
//
/////////////////////////////////////////////////////////
void tex_gradient :: envMess(int num)
{
  switch (num) {
  case 0:
    m_env = GL_REPLACE;
    break;
  case 1:
    m_env = GL_DECAL;
    break;
  case 2:
    m_env = GL_BLEND;
    break;
  case 3:
    m_env = GL_ADD;
    break;
  case 4:
    m_env = GL_COMBINE;
    break;
  default:
    m_env = GL_MODULATE;
  }
  setModified();
}



////////////////////////////////////////////////////////
// Pixel Buffer Object message
//
/////////////////////////////////////////////////////////
void tex_gradient :: pboMess(int num)
{
  if(num<0) {
    return;
  }

  if(m_pbo) {
    GLuint*pbo=m_pbo;
    glDeleteBuffersARB(m_numPbo, pbo);
    delete[]pbo;
    m_pbo=NULL;
    m_numPbo=0;
  }

  m_numPbo=num;
  setModified();
}

void tex_gradient :: clientStorage(int mode)
{
  m_clientStorage=mode;
}
void tex_gradient :: yuvMess(int mode)
{
  m_yuv=mode;
}
void tex_gradient :: texunitMess(int unit)
{
  m_texunit=unit;
}

////////////////////////////////////////////////////////
// static member functions
//
/////////////////////////////////////////////////////////
void tex_gradient :: obj_setupCallback(t_class *classPtr)
{
  CPPEXTERN_MSG1(classPtr, "float", textureOnOff, int);
  CPPEXTERN_MSG1(classPtr, "quality", textureQuality, int);
  CPPEXTERN_MSG1(classPtr, "repeat", repeatMess, int);
  CPPEXTERN_MSG1(classPtr, "env", envMess, int);

  CPPEXTERN_MSG1(classPtr, "client_storage", clientStorage, int);

  CPPEXTERN_MSG1(classPtr, "yuv", yuvMess, int);
  CPPEXTERN_MSG1(classPtr, "pbo", pboMess, int);

  CPPEXTERN_MSG1(classPtr, "texunit", texunitMess, int);

  class_addcreator(reinterpret_cast<t_newmethod>(create_tex_gradient),
                   gensym("tex_gradient2"), A_GIMME, A_NULL);
}
