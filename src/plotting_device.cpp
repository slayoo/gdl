/***************************************************************************
                       plotting.cpp  -  GDL routines for plotting
                             -------------------
    begin                : July 22 2002
    copyright            : (C) 2002-2011 by Marc Schellens et al.
    email                : m_schellens@users.sf.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "includefirst.hpp"
#include "plotting.hpp"
#include <gsl/gsl_const_mksa.h> // GSL_CONST_MKSA_INCH

namespace lib {

  using namespace std;

  void device( EnvT* e)
  {
    // CLOSE for z-buffer device
    static int closeFileIx = e->KeywordIx( "CLOSE_FILE"); 
    static int fileNameIx = e->KeywordIx( "FILENAME"); 
    static int decomposedIx = e->KeywordIx( "DECOMPOSED"); 
    static int get_decomposedIx = e->KeywordIx( "GET_DECOMPOSED"); 
    static int z_bufferingIx = e->KeywordIx( "Z_BUFFERING"); 
    static int set_resolutionIx = e->KeywordIx( "SET_RESOLUTION"); 
    static int get_visual_depthIx = e->KeywordIx( "GET_VISUAL_DEPTH");
    //    static int landscapeIx = e->KeywordIx( "LANDSCAPE"); 
    //    static int portraitIx = e->KeywordIx( "PORTRAIT");
    static int xSizeIx = e->KeywordIx( "XSIZE");
    static int ySizeIx = e->KeywordIx( "YSIZE");
    static int colorIx = e->KeywordIx( "COLOR");
    static int inchesIx = e->KeywordIx( "INCHES");
    static int get_screen_sizeIx = e->KeywordIx( "GET_SCREEN_SIZE");
    static int window_stateIx = e->KeywordIx( "WINDOW_STATE");

    Graphics* actDevice = Graphics::GetDevice();
    
    if( e->KeywordPresent( get_screen_sizeIx))
      {
#ifndef HAVE_X
        e->Throw("GDL was compiled without support for X-windows");
#else
	// see below in Function "get_scren_size()" explanations ...
	Display* display = XOpenDisplay(NULL);
	if (display == NULL)
	  e->Throw("Cannot connect to X server");
	
	int screen_num;
	int screen_width;
	int screen_height;
	screen_num = DefaultScreen(display);
	screen_width = DisplayWidth(display, screen_num);
	screen_height = DisplayHeight(display, screen_num);

	DIntGDL* res;
	res = new DIntGDL(2, BaseGDL::NOZERO);
    
	(*res)[0]= screen_width;
	(*res)[1]= screen_height;
	e->SetKW( get_screen_sizeIx, res);
#endif
      }

    if (e->KeywordPresent(window_stateIx))
    {
      // check if X (could be more elegant...)
      DStructGDL* dStruct = SysVar::D();
      static unsigned nameTag = dStruct->Desc()->TagIndex( "NAME");
      DString d_name = (*static_cast<DStringGDL*>( dStruct->GetTag( nameTag, 0)))[0];
      // if PS and not noErase (ie, erase) then set !p.noerase=0    
      if (d_name != "X") e->Throw("WINDOW_STATE not supported for the current device (" + d_name + "), it works for X only");
      int maxwin = actDevice->MaxWin();
      assert(maxwin > 0);
      DByteGDL* ret = new DByteGDL(dimension( maxwin), BaseGDL::NOZERO);
      for (int i = 0; i < maxwin; i++) (*ret)[i] = actDevice->WState(i);
      e->SetKW( window_stateIx, ret);
    }

    if( e->KeywordSet( closeFileIx))
      {
	bool success = actDevice->CloseFile();
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword CLOSE_FILE.");
      }

    BaseGDL* z_buffering = e->GetKW( z_bufferingIx);
    if( z_buffering != NULL)
      {
	bool success = actDevice->ZBuffering( e->KeywordSet( z_bufferingIx));
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword Z_BUFFERING.");
      }

    BaseGDL* set_resolution = e->GetKW( set_resolutionIx);
    if( set_resolution != NULL)
      {
	DLongGDL* resolution = e->GetKWAs<DLongGDL>( set_resolutionIx);
	if( resolution->N_Elements() != 2)
	  e->Throw( "Keyword array parameter SET_RESOLUTION must have "
		    "2 elements.");
	DLong x = (*resolution)[0];
	DLong y = (*resolution)[1];

	if( x<0 || y<0)
	  e->Throw( "Value of Resolution is out of allowed range.");

	bool success = actDevice->SetResolution( x, y);
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword SET_RESOLUTION.");
      }


    BaseGDL* decomposed = e->GetKW( decomposedIx);
    if( decomposed != NULL)
	{
	  bool success = actDevice->Decomposed( e->KeywordSet( decomposedIx));
	  if( !success)
	    e->Throw( "Current device does not support "
		      "keyword DECOMPOSED.");
 	}


    if( e->KeywordPresent( get_decomposedIx)) {
      DLong value = actDevice->GetDecomposed();
      if(value == -1)
	e->Throw( "Current device does not support "
		  "keyword GET_DECOMPOSED.");
      else {
	e->SetKW( get_decomposedIx, new DLongGDL( value));
      }
    }

    if (e->KeywordPresent( get_visual_depthIx))
    {
      {
        DStructGDL* dStruct = SysVar::D(); 
        static unsigned nameTag = dStruct->Desc()->TagIndex( "NAME");
        if ((*static_cast<DStringGDL*>( dStruct->GetTag( nameTag, 0)))[0] != "X")
          e->Throw("GET_VISUAL_DEPTH is not supported by current device");
      }
#ifndef HAVE_X
      e->Throw("GDL was compiled without support for X-windows");
#else
      Display* display = XOpenDisplay(NULL);
      if (display == NULL) 
        e->Throw("Cannot connect to X server");
      int depth = DefaultDepth(display, DefaultScreen(display));
      XCloseDisplay(display);
      e->SetKW( get_visual_depthIx, new DLongGDL( depth));
#endif
    }

    BaseGDL* fileName = e->GetKW( fileNameIx);
    if( fileName != NULL)
      {
	DString fName;
	e->AssureStringScalarKW( fileNameIx, fName);
	if( fName == "")
	  e->Throw( "Null filename not allowed.");
	bool success = actDevice->SetFileName( fName);
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword FILENAME.");
      }


    BaseGDL* xSizeKW = e->GetKW( xSizeIx);
    if( xSizeKW != NULL)
      {
	DFloat xSizeValue;
	e->AssureFloatScalarKW( xSizeIx, xSizeValue);
	bool success = actDevice->SetXPageSize( xSizeValue 
          * (e->KeywordPresent(inchesIx) ? 100. * GSL_CONST_MKSA_INCH : 1.)
        );
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword XSIZE.");
      } 

    BaseGDL* ySizeKW = e->GetKW( ySizeIx);
    if( ySizeKW != NULL)
      {
	DFloat ySizeValue;
	e->AssureFloatScalarKW( ySizeIx, ySizeValue);
	bool success = actDevice->SetYPageSize( ySizeValue
          * (e->KeywordPresent(inchesIx) ? 100. * GSL_CONST_MKSA_INCH : 1.)
        );
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword YSIZE.");
      } 

    BaseGDL* colorKW = e->GetKW( colorIx);
    if( colorKW != NULL)
      {
	bool success = actDevice->SetColor();
	if( !success)
	  e->Throw( "Current device does not support "
		    "keyword COLOR.");
      } 


  }

} // namespace

