/* Java wrapper to use libprojectm
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2022 Christoph Zimmermann.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE' included within this release
 */
 
package org.brain4free.jprojectm;

import java.io.IOException;

/**
 * <p>
 * JOGL2 OpenGL ES 2 demo to expose and learn what the RAW OpenGL ES 2 API looks like.
 *
 * Compile, run and enjoy:
   wget http://jogamp.org/deployment/jogamp-current/archive/jogamp-all-platforms.7z
   7z x jogamp-all-platforms.7z
   cd jogamp-all-platforms
   mkdir -p demos/es2
   cd demos/es2
   wget https://raw.github.com/xranby/jogl-demos/master/src/demos/es2/RawGL2ES2demo.java
   cd ../..
   javac -cp jar/jogl-all.jar:jar/gluegen-rt.jar demos/es2/RawGL2ES2demo.java
   java -cp jar/jogl-all.jar:jar/gluegen-rt.jar:. demos.es2.RawGL2ES2demo
 * </p>
 *
 *
 * @author Christoph Zimmermann (nuess0r)
 */

public class ProjectM {

    static {
        System.loadLibrary("projectmjni");
    }
    
    public static void open() {
        new ProjectM().initJackPorts();
        new ProjectM().initShaders();
        new ProjectM().initTexture(256);
        new ProjectM().initProjectm();
    }
    
    public static void close() {
        new ProjectM().destroyJack();
        new ProjectM().destroyGl();
        new ProjectM().destroyProjectm();
    }
        
    
    public static void main(String[] args) {
        System.out.println("Start");
        new ProjectM().initGlWindow();
        new ProjectM().open();
        //new ProjectM().initJackPorts();
        //
        //new ProjectM().initShaders();
        //new ProjectM().initTexture(256);
        //new ProjectM().initProjectm();
        
        new ProjectM().render();
        new ProjectM().loadPreset("/home/chrigi/Projects/milkdrop/MilkdropVLCPresets/yin - 311 - Ocean of Light (bouncing off mix).milk");
        new ProjectM().render();
        
        try {
            System.out.println("Wait, press any key to continue");
            char c = (char) System.in.read();
        }
        catch(IOException e)
        {
        System.err.println("IOException has been caught");
        }
            
        
        System.out.println("Stop");
        new ProjectM().close();
        //new ProjectM().destroyJack();
        //new ProjectM().destroyGl();
        //new ProjectM().destroyProjectm();
    }

    // Declare a native method init() that receives no arguments and returns void
    public native void init();
    
    // Declare a native method destroy() that receives no arguments and returns void
    public native void destroy();
    
    //
    public native boolean initJackPorts();
    
    //
    public native boolean initGlWindow();
    
    //
    public native boolean initTexture(int size);
    
    //
    public native boolean initProjectm();
    
    //
    public native boolean initShaders();
    
    //
    public native boolean loadPreset(String presetUrl);
    
    //
    public native void destroyJack();
    
    //
    public native void destroyGl();
    
    //
    public native void destroyProjectm();
    
    //
    public native void render();
    
    //
    public native void renderTexture();
    
    //
    public native void reshape(int w, int h);
    
    
    // TODO: Add methods to send audio from Java when no JACK connection is used
    //public native void addAudio(float samples[]);
    
    //
    private native boolean startMainLoop();
} 
