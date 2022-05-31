/***************************************************************************
                       plotting_plot.cpp  -  GDL routines for plotting
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

static GDL_3DTRANSFORMDEVICE PlotDevice3d;

namespace lib {

  using namespace std;

  class plot_call : public plotting_routine_call {
    DDoubleGDL *yVal, *xVal, *xTemp, *yTemp;
    SizeT xEl, yEl, zEl;
    DDouble minVal, maxVal, xStart, xEnd, yStart, yEnd, zStart, zEnd, zValue;
    bool doMinMax;
    bool xLog, yLog, zLog;
    Guard<BaseGDL> xval_guard, yval_guard, xtemp_guard;
    bool iso;
    bool doT3d;
    COORDSYS coordinateSystem = DATA;
    DLongGDL *color;
    DLong psym;
    int calendar_codex;
    int calendar_codey;
    T3DEXCHANGECODE axisExchangeCode;

  private:

    bool handle_args(EnvT* e) {
      //T3D ?
      static int t3dIx = e->KeywordIx("T3D");
      doT3d = (e->BooleanKeywordSet(t3dIx) || T3Denabled());

      //note: Z (VALUE) will be used uniquely if Z is not effectively defined.
      static int zvIx = e->KeywordIx("ZVALUE");
      zValue = 0.0;
      e->AssureDoubleScalarKWIfPresent(zvIx, zValue);
      zValue = min(zValue, ZVALUEMAX); //to avoid problems with plplot
      zValue = max(zValue, 0.0);
      //zStart and zEnd are ALWAYS Zero. zValue will be registered in !Z.REGION
      zStart = 0;
      zEnd = 0;

      // system variable !P.NSUM first
      DLong nsum = (*static_cast<DLongGDL*> (SysVar::P()-> GetTag(SysVar::P()->Desc()->TagIndex("NSUM"), 0)))[0];
      static int NSUMIx = e->KeywordIx("NSUM");
      e->AssureLongScalarKWIfPresent(NSUMIx, nsum);

      static int polarIx = e->KeywordIx("POLAR");
      bool polar = (e->KeywordSet(polarIx));

      //test and transform eventually if POLAR and/or NSUM!
      if (nParam() == 1) {
        yTemp = e->GetParAs< DDoubleGDL > (0);
        if (yTemp->Rank() == 0)
          e->Throw("Expression must be an array in this context: " + e->GetParString(0));
        yEl = yTemp->N_Elements();
        xEl = yEl;
        xTemp = new DDoubleGDL(dimension(xEl), BaseGDL::INDGEN);
        xtemp_guard.Reset(xTemp); // delete upon exit
      } else {
        xTemp = e->GetParAs< DDoubleGDL > (0);
        if (xTemp->Rank() == 0)
          e->Throw("Expression must be an array in this context: " + e->GetParString(0));
        xEl = xTemp->N_Elements();
        yTemp = e->GetParAs< DDoubleGDL > (1);
        if (yTemp->Rank() == 0)
          e->Throw("Expression must be an array in this context: " + e->GetParString(1));
        yEl = yTemp->N_Elements();
        //silently drop unmatched values
        if (yEl != xEl) {
          SizeT size;
          size = min(xEl, yEl);
          xEl = size;
          yEl = size;
        }
      }
      //check nsum validity
      nsum = max(1, nsum);
      nsum = min(nsum, (DLong) xEl);

      if (nsum == 1) {
        if (polar) {
          xVal = new DDoubleGDL(dimension(xEl), BaseGDL::NOZERO);
          xval_guard.Reset(xVal); // delete upon exit
          yVal = new DDoubleGDL(dimension(yEl), BaseGDL::NOZERO);
          yval_guard.Reset(yVal); // delete upon exit
          for (int i = 0; i < xEl; i++) (*xVal)[i] = (*xTemp)[i] * cos((*yTemp)[i]);
          for (int i = 0; i < yEl; i++) (*yVal)[i] = (*xTemp)[i] * sin((*yTemp)[i]);
        } else { //careful about previously set autopointers!
          if (nParam() == 1) xval_guard.Init(xtemp_guard.release());
          xVal = xTemp;
          yVal = yTemp;
        }
      } else {
        int i, j, k;
        DLong size = (DLong) xEl / nsum;
        xVal = new DDoubleGDL(size, BaseGDL::ZERO); //SHOULD BE ZERO, IS NOT!
        xval_guard.Reset(xVal); // delete upon exit
        yVal = new DDoubleGDL(size, BaseGDL::ZERO); //IDEM
        yval_guard.Reset(yVal); // delete upon exit
        for (i = 0, k = 0; i < size; i++) {
          (*xVal)[i] = 0.0;
          (*yVal)[i] = 0.0;
          for (j = 0; j < nsum; j++, k++) {
            (*xVal)[i] += (*xTemp)[k];
            (*yVal)[i] += (*yTemp)[k];
          }
        }
        for (i = 0; i < size; i++) (*xVal)[i] /= nsum;
        for (i = 0; i < size; i++) (*yVal)[i] /= nsum;

        if (polar) {
          DDouble x, y;
          for (i = 0; i < size; i++) {
            x = (*xVal)[i] * cos((*yVal)[i]);
            y = (*xVal)[i] * sin((*yVal)[i]);
            (*xVal)[i] = x;
            (*yVal)[i] = y;
          }
        }
      }

      xLog = false;
      yLog = false;
      zLog = false; //and will not be checked.

      // handle Log options passing via Functions names PLOT_IO/OO/OI
      // the behavior can be superseed by [xy]log or [xy]type
      string ProName = e->GetProName();
      if (ProName != "PLOT") {
        if (ProName == "PLOT_IO") yLog = true;
        if (ProName == "PLOT_OI") xLog = true;
        if (ProName == "PLOT_OO") {
          xLog = true;
          yLog = true;
        }
      }

      // handle Log options passing via Keywords
      // note: undocumented keywords [xyz]type still exist and
      // have priority on [xyz]log !
      static int xLogIx = e->KeywordIx("XLOG");
      static int yLogIx = e->KeywordIx("YLOG");
      if (e->KeywordPresent(xLogIx)) xLog = e->KeywordSet(xLogIx);
      if (e->KeywordPresent(yLogIx)) yLog = e->KeywordSet(yLogIx);

      // note: undocumented keywords [xyz]type still exist and
      // have priority on [xyz]log ! In fact, it is the modulo (1, 3, 5 ... --> /log)   
      static int xTypeIx = e->KeywordIx("XTYPE");
      static int yTypeIx = e->KeywordIx("YTYPE");
      static int xType, yType;
      if (e->KeywordPresent(xTypeIx)) {
        e->AssureLongScalarKWIfPresent(xTypeIx, xType);
        if ((xType % 2) == 1) xLog = true;
        else xLog = false;
      }
      if (e->KeywordPresent(yTypeIx)) {
        e->AssureLongScalarKWIfPresent(yTypeIx, yType);
        if ((yType % 2) == 1) yLog = true;
        else yLog = false;
      }

      //  cout << xType<< " " <<xLog << " "<<yType <<" " << yLog << endl;

      static int xTickunitsIx = e->KeywordIx("XTICKUNITS");
      static int yTickunitsIx = e->KeywordIx("YTICKUNITS");

      calendar_codex = gdlGetCalendarCode(e, XAXIS);
      calendar_codey = gdlGetCalendarCode(e, YAXIS);
      if (e->KeywordSet(xTickunitsIx) && xLog) {
        Message("PLOT: LOG setting ignored for Date/Time TICKUNITS.");
        xLog = false;
      }
      if (e->KeywordSet(yTickunitsIx) && yLog) {
        Message("PLOT: LOG setting ignored for Date/Time TICKUNITS.");
        yLog = false;
      }


      //    cout << xLog << " " << yLog << endl;

      DLong minEl, maxEl;
      xVal->MinMax(&minEl, &maxEl, NULL, NULL, true, 0, xEl); //restrict minmax to xEl fist elements!!!!
      xStart = (*xVal)[minEl];
      xEnd = (*xVal)[maxEl];

      yVal->MinMax(&minEl, &maxEl, NULL, NULL, true, 0, yEl);
      yStart = (*yVal)[minEl];
      yEnd = (*yVal)[maxEl];

      //MIN_VALUE and MAX_VALUE overwrite yStart/yEnd eventually (note: the points will not be "seen" at all in plots)
      minVal = yStart; //to give a reasonable value...
      maxVal = yEnd; //idem
      doMinMax = false; //although we will not use it...
      static int MIN_VALUEIx = e->KeywordIx("MIN_VALUE");
      static int MAX_VALUEIx = e->KeywordIx("MAX_VALUE");
      if (e->KeywordSet(MIN_VALUEIx) || e->KeywordSet(MAX_VALUEIx))
        doMinMax = true; //...unless explicitely required
      e->AssureDoubleScalarKWIfPresent(MIN_VALUEIx, minVal);
      e->AssureDoubleScalarKWIfPresent(MAX_VALUEIx, maxVal);
      yStart = gdlPlot_Max(yStart, minVal);
      yEnd = gdlPlot_Min(yEnd, maxVal);
      if (yEnd <= yStart) yEnd = yStart + 1;
      //XRANGE and YRANGE overrides all that, but  Start/End should be recomputed accordingly
      DDouble xAxisStart, xAxisEnd, yAxisStart, yAxisEnd;
      bool setx = gdlGetDesiredAxisRange(e, XAXIS, xAxisStart, xAxisEnd);
      bool sety = gdlGetDesiredAxisRange(e, YAXIS, yAxisStart, yAxisEnd);
      if (setx && sety) {
        xStart = xAxisStart;
        xEnd = xAxisEnd;
        yStart = yAxisStart;
        yEnd = yAxisEnd;
      } else if (sety) {
        yStart = yAxisStart;
        yEnd = yAxisEnd;
      } else if (setx) {
        xStart = xAxisStart;
        xEnd = xAxisEnd;
        //must compute min-max for other axis!
        {
          gdlDoRangeExtrema(xVal, yVal, yStart, yEnd, xStart, xEnd, doMinMax, minVal, maxVal);
        }
      }
      //handle Nozero option after all that! (gdlAxisNoZero test if /ynozero option is valid (ex: no YRANGE)
      if (!gdlYaxisNoZero(e) && yStart > 0 && !yLog) yStart = 0.0;


      return false;
    }

    void old_body(EnvT* e, GDLGStream* actStream) {
      //check presence of DATA,DEVICE and NORMAL options
      static int DATAIx = e->KeywordIx("DATA");
      static int DEVICEIx = e->KeywordIx("DEVICE");
      static int NORMALIx = e->KeywordIx("NORMAL");
      coordinateSystem = DATA;
      //check presence of DATA,DEVICE and NORMAL options
      if (e->KeywordSet(DATAIx)) coordinateSystem = DATA;
      if (e->KeywordSet(DEVICEIx)) coordinateSystem = DEVICE;
      if (e->KeywordSet(NORMALIx)) coordinateSystem = NORMAL;

      //ISOTROPIC
      static int ISOTROPIC = e->KeywordIx("ISOTROPIC");
      iso = e->KeywordSet(ISOTROPIC);
      // background BEFORE next plot since it is the only place plplot may redraw the background...
      gdlSetGraphicsBackgroundColorFromKw(e, actStream);
      //start a plot
      gdlNextPlotHandlingNoEraseOption(e, actStream); //NOERASE

      //Box adjustement:
      gdlAdjustAxisRange(e, XAXIS, xStart, xEnd, xLog, calendar_codex);
      gdlAdjustAxisRange(e, YAXIS, yStart, yEnd, yLog, calendar_codey);

      // viewport and world coordinates
      // set the PLOT charsize before setting viewport (margin depend on charsize)
      gdlSetPlotCharsize(e, actStream);
      if (gdlSet2DViewPortAndWorldCoordinates(e, actStream, xStart, xEnd, xLog, yStart, yEnd, yLog, iso) == false) return; //no good: should catch an exception to get out of this mess.

      if (doT3d) { //call for driver to perform special transform for all further drawing
        gdlFillWithT3DMatrix(PlotDevice3d.T);
        PlotDevice3d.zValue=zValue;
        actStream->cmd( PLESC_3D,  &PlotDevice3d);
      } 
      gdlSetSymsize(e, actStream);
      actStream->setSymbolSizeConversionFactors();
      //current pen color...
      gdlSetGraphicsForegroundColorFromKw(e, actStream);
      gdlBox(e, actStream, xStart, xEnd, yStart, yEnd, xLog, yLog);
      
      // title and sub title
      gdlWriteTitleAndSubtitle(e, actStream);

      //NOW we work as for all other graphic procedures, in NORMmalized coordinates, as for PLOTS etc.
      actStream->OnePageSaveLayout(); // one page

      //CLIPPING (or not) is just defining the adequate viewport and world coordinates, all of them normalized since this is what plplot will get in the end.
      static int NOCLIPIx = e->KeywordIx("NOCLIP");
      bool noclip = e->BooleanKeywordSet(NOCLIPIx);
      // Clipping is enabled by default for OPLOT.
      int CLIP = e->KeywordIx("CLIP");
      bool doClip = (!(noclip) && !doT3d);

      PLFLT xnormmin = 0;
      PLFLT xnormmax = 1;
      PLFLT ynormmin = 0;
      PLFLT ynormmax = 1;

      if (doClip) { //redefine default viewport & world
        //define a default clipbox (DATA coords):
        PLFLT clipBox[4] = {xStart, yStart, xEnd, yEnd};
        DDoubleGDL* clipBoxGDL = e->IfDefGetKWAs<DDoubleGDL>(CLIP);
        if (clipBoxGDL != NULL && clipBoxGDL->N_Elements() < 4) for (auto i = 0; i < 4; ++i) clipBox[i] = 0; //set clipbox to 0 0 0 0 apparently this is what IDL does.
        if (clipBoxGDL != NULL && clipBoxGDL->N_Elements() == 4) for (auto i = 0; i < 4; ++i) clipBox[i] = (*clipBoxGDL)[i];
        //clipBox is defined accordingly to /NORM /DEVICE /DATA:
        //convert clipBox to normalized coordinates:
        //switch here is irrelevant since coordinates are DATA , but kepts here for sameness with othe rplotting_* routines.
        switch (coordinateSystem) {
        case DATA:
          actStream->WorldToNormedDevice(clipBox[0], clipBox[1], xnormmin, ynormmin);
          actStream->WorldToNormedDevice(clipBox[2], clipBox[3], xnormmax, ynormmax);
          break;
        case DEVICE:
          actStream->DeviceToNormedDevice(clipBox[0], clipBox[1], xnormmin, ynormmin);
          actStream->DeviceToNormedDevice(clipBox[2], clipBox[3], xnormmax, ynormmax);
          break;
        default:
          xnormmin = clipBox[0];
          xnormmax = clipBox[2];
          ynormmin = clipBox[1];
          ynormmax = clipBox[3];
        }
      }

      // it is important to fix symsize before changing vpor or win 
      gdlSetSymsize(e, actStream); //SYMSIZE
      actStream->vpor(xnormmin, xnormmax, ynormmin, ynormmax);
      actStream->wind(xnormmin, xnormmax, ynormmin, ynormmax); //transformed (plotted) coords will be in NORM. Conversion will be made on the data values.
      actStream->setSymbolSizeConversionFactors();
    }

    void call_plplot(EnvT* e, GDLGStream* actStream) {
      static int nodataIx = e->KeywordIx("NODATA");
      if (e->KeywordSet(nodataIx)) return;
      // start drawing. Graphic Keywords accepted:CLIP(YES), COLOR(YES), LINESTYLE(YES), NOCLIP(YES),
      //                                          PSYM(YES), SYMSIZE(YES), T3D(YES), ZVALUE(YES)
      static int colorIx = e->KeywordIx("COLOR");
      bool doColor = false;
      if (e->GetKW(colorIx) != NULL) {
        color = e->GetKWAs<DLongGDL>(colorIx);
        doColor = true;
      }

      //properties
      if (!doColor || color->N_Elements() == 1) {
        //if no KW or only 1 color, no need to complicate things
        //at draw_polyline level!
        gdlSetGraphicsForegroundColorFromKw(e, actStream); //COLOR
        doColor = false;
      }
      gdlSetPenThickness(e, actStream); //THICK
      gdlSetLineStyle(e, actStream); //LINESTYLE
      gdlGetPsym(e, psym); //PSYM

      bool mapSet = false;
      get_mapset(mapSet);
      mapSet = (mapSet && coordinateSystem == DATA); //always the case here

      if (mapSet) {
#ifdef USE_LIBPROJ
        ref = map_init();
        if (ref == NULL) e->Throw("Projection initialization failed.");

        DLongGDL *conn = NULL; //tricky as xVal and yVal will be probably replaced by connectivity
        bool doFill = false;
        bool doLines = (psym < 1);
        bool isRadians = false;
        //if doT3d and !flat3d, the projected polygon needs to keep track of Z.
        DDoubleGDL *lonlat = GDLgrGetProjectPolygon(actStream, ref, NULL, xVal, yVal, NULL, isRadians, doFill, doLines, conn);

        //lonlat is still in radians.
        //GDLgrPlotProjectedPolygon or draw_polyline() will make the 3d projection if flat3d=true through the use of stransform()
        //if doT3d and !flat3d, we need to apply the 3D rotation ourselves:

        if (lonlat != NULL) {
          SelfNormLonLat(lonlat); //lonlat is now converted to norm
          if (psym < 1) { //lines must be specially explored
            GDLgrPlotProjectedPolygon(actStream, lonlat, doFill, conn);
            psym = -psym;
          } //now that lines are plotted, do the points:
          if (psym > 0) {
            SizeT npts = lonlat->Dim(0); //lonlat is [npts,2]
            //temporary create x and y to pass to draw_polyline. Not very efficient!
            DDoubleGDL* x = new DDoubleGDL(dimension(npts), BaseGDL::NOZERO);
            for (auto i = 0; i < npts; ++i) (*x)[i] = (*lonlat)[i];
            DDoubleGDL* y = new DDoubleGDL(dimension(npts), BaseGDL::NOZERO);
            for (auto i = 0; i < npts; ++i) (*y)[i] = (*lonlat)[i + npts];
            draw_polyline(actStream, x, y, 0, 0, false, false, false, psym, false, doColor ? color : NULL);
            GDLDelete(x);
            GDLDelete(y);
          }
          GDLDelete(lonlat);
          if (doLines || doFill) GDLDelete(conn); //conn may be null if no line-drawing or fill was requested.
        }
#endif 
      } else { //just as if LIBPROJ WAS NOT present
        SelfConvertToNormXY(xVal, xLog, yVal, yLog, coordinateSystem); //DATA
        draw_polyline(actStream, xVal, yVal, 0, 0, false, xLog, yLog, psym, false, doColor ? color : NULL);
      }

    }

    void post_call(EnvT* e, GDLGStream* actStream) {
      if (doT3d) { //reset driver to 2D plotting routines.
        actStream->cmd(PLESC_2D, NULL);
      }
      actStream->stransform(NULL, NULL);
      actStream->lsty(1); //reset linestyle
      actStream->sizeChar(1.0);
    }

  };

  void plot(EnvT* e) {
    plot_call plot;
    plot.call(e, 1);
  }

  void plot_io(EnvT* e) {
    plot(e);
  }

  void plot_oo(EnvT* e) {
    plot(e);
  }

  void plot_oi(EnvT* e) {
    plot(e);
  }

} // namespace
