/*
libdmtx - Data Matrix Encoding/Decoding Library

Copyright (C) 2010 Mike Laughton

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Contact: mblaughton@users.sourceforge.net
*/

/* $Id$ */

#include <string.h>
#include <assert.h>
#include <math.h>
#include "multi_test.h"

/**
 *
 *
 */
DmtxPassFail
HoughGridPopulate(DmtxDecode2 *dec)
{
   int row, col;
   DmtxHoughGrid *grid;

   dec->houghGrid = (DmtxHoughGrid *)calloc(1, sizeof(DmtxHoughGrid));
   if(dec->houghGrid == NULL)
      return DmtxFail;

   grid = dec->houghGrid;
   grid->rows = 1;
   grid->cols = 1;
   grid->count = grid->rows * grid->cols;
   grid->local = (DmtxHoughLocal *)malloc(grid->count * sizeof(DmtxHoughLocal));
   if(grid->local == NULL)
   {
      free(dec->houghGrid);
      dec->houghGrid = NULL;
      return DmtxFail;
   }

   for(row = 0; row < grid->rows; row++)
   {
      for(col = 0; col < grid->cols; col++)
      {
         HoughLocalAccumulate(dec, col, row);
      }
   }

   dec->fn.dmtxHoughLocalCallback(dec->houghGrid->local, 0);

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
HoughGridDestroy(DmtxHoughGrid **grid)
{
   if(grid == NULL || *grid == NULL)
      return DmtxFail;

   if((*grid)->local != NULL)
      free((*grid)->local);

   free(*grid);
   *grid = NULL;

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
HoughLocalAccumulate(DmtxDecode2 *dec, int gCol, int gRow)
{
   int rRow, rCol;
   int iRow, iCol;
   int iWidth, iHeight;
   int phi;
   DmtxHoughLocal *hRegion;
   ZeroCrossing vvZXing, bbZXing, hhZXing, ssZXing;
   DmtxPassFail vvPassFail, bbPassFail, hhPassFail, ssPassFail;

   hRegion = &(dec->houghGrid->local[0]); /* [hCol * width + hRol]; */
   memset(hRegion, 0x00, sizeof(DmtxHoughLocal));

   /* Global coordinate system */
   iWidth = dmtxImageGetProp(dec->image, DmtxPropWidth);
   iHeight = dmtxImageGetProp(dec->image, DmtxPropHeight);

   hRegion->xOrigin = gState.localOffsetX;
   hRegion->yOrigin = gState.localOffsetY;

   /* calculate dOffset ? */

   for(rRow = 0; rRow < 64; rRow++)
   {
      iRow = hRegion->yOrigin + rRow;

      if(iRow >= iHeight)
         continue;

      for(rCol = 0; rCol < 64; rCol++)
      {
         iCol = hRegion->xOrigin + rCol;

         if(iCol >= iWidth)
            continue;

         vvZXing = GetZeroCrossing(dec->vvAccel, iCol, iRow, &vvPassFail);
         bbZXing = GetZeroCrossing(dec->bbAccel, iCol, iRow, &bbPassFail);
         hhZXing = GetZeroCrossing(dec->hhAccel, iCol, iRow, &hhPassFail);
         ssZXing = GetZeroCrossing(dec->ssAccel, iCol, iRow, &ssPassFail);

         if(vvZXing.mag > 0 && vvPassFail == DmtxPass)
         {
            for(phi = 0; phi < 16; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, vvZXing);
            for(phi = 112; phi < 128; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, vvZXing);

            if(gState.displayEdge == 1)
               dec->fn.zeroCrossingCallback(vvZXing, 0);
         }

         if(bbZXing.mag > 0 && bbPassFail == DmtxPass)
         {
            for(phi = 16; phi < 48; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, bbZXing);

            if(gState.displayEdge == 2)
               dec->fn.zeroCrossingCallback(bbZXing, 0);
         }

         if(hhZXing.mag > 0 && hhPassFail == DmtxPass)
         {
            for(phi = 48; phi < 80; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, hhZXing);

            if(gState.displayEdge == 3)
               dec->fn.zeroCrossingCallback(hhZXing, 0);
         }

         if(ssZXing.mag > 0 && ssPassFail == DmtxPass)
         {
            for(phi = 80; phi < 112; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, ssZXing);

            if(gState.displayEdge == 4)
               dec->fn.zeroCrossingCallback(ssZXing, 0);
         }
      }
   }

   return DmtxPass;
}

/**
 *
 *
 */
ZeroCrossing
GetZeroCrossing(DmtxValueGrid *accel, int iCol, int iRow, DmtxPassFail *passFail)
{
   int aInc, aIdx, aIdxNext;
   int aRow, aCol;
   int aWidth, aHeight;
   int aHere, aNext, aPrev;
   double smidge;
   ZeroCrossing edge = { 0, 0.0, 0.0 };
   DmtxPassFail childPassFail;

   *passFail = DmtxFail;

   aWidth = dmtxValueGridGetWidth(accel);
   aHeight = dmtxValueGridGetHeight(accel);
   aRow = iRow - 1;
   aCol = iCol - 1;

   /* XXX add better bounds checking of aIdxNext now that we're comparing diagonals */

   switch(accel->type) {
      case DmtxEdgeVertical:
         aInc = 1;
         break;
      case DmtxEdgeBackslash:
         aInc = aWidth + 1;
         break;
      case DmtxEdgeHorizontal:
         aInc = aWidth;
         break;
      case DmtxEdgeSlash:
         aCol++; /* Special case: "Slash" comparison starts at bottom-right */
         aInc = aWidth - 1;
         break;
      default:
         return edge; /* Fail: Illegal edge direction */
   }

   aIdx = aRow * aWidth + aCol;
   aIdxNext = aIdx + aInc;

   aHere = accel->value[aIdx];
   aNext = accel->value[aIdxNext];

   if(OPPOSITE_SIGNS(aHere, aNext))
   {
      /* Zero crossing: Neighbors with opposite signs [-10,+10] */
      smidge = abs(aHere/(aHere - aNext));
      edge = SetZeroCrossingFromIndex(accel, aCol, aRow, smidge, &childPassFail);
   }
   else if(aHere == 0 && aNext != 0)
   {
      /* No previous value for comparison (beginning of row/col) */
      if(aIdx < aInc)
         return edge;

      aPrev = accel->value[aIdx-aInc];
      if(OPPOSITE_SIGNS(aPrev, aNext))
      {
         /* Zero crossing: Opposite signs separated by zero [-10,0,+10] */
         edge = SetZeroCrossingFromIndex(accel, aCol, aRow, 0.0, &childPassFail);
      }
   }

   *passFail = childPassFail;
   return edge;
}

/**
 * 0 < smidge < 1
 *
 */
ZeroCrossing
SetZeroCrossingFromIndex(DmtxValueGrid *accel, int aCol, int aRow, double smidge, DmtxPassFail *passFail)
{
   int sCol, sRow, sIdx;
   const ZeroCrossing emptyEdge = { 0, 0.0, 0.0 };
   ZeroCrossing edge;
   DmtxValueGrid *sobel = accel->ref;

   *passFail = DmtxFail;

   switch(accel->type) {
      case DmtxEdgeVertical:
         edge.x = (double)aCol + 2.0 + smidge;
         edge.y = (double)aRow + 1.5;
         sRow = aRow;
         sCol = aCol + 1;
         break;

      case DmtxEdgeBackslash:
         edge.x = (double)aCol + 2.0 + smidge; /* XXX need sqrt(2) here */
         edge.y = (double)aRow + 2.0 + smidge; /* XXX need sqrt(2) here */
         sRow = aRow + 1;
         sCol = aCol + 1;
         break;

      case DmtxEdgeHorizontal:
         edge.x = (double)aCol + 1.5;
         edge.y = (double)aRow + 2.0 + smidge;
         sRow = aRow + 1;
         sCol = aCol;
         break;

      case DmtxEdgeSlash:
         /* Special case: Slash accel comparison starts at bottom right */
         edge.x = (double)aCol + 1.0 - smidge; /* XXX need sqrt(2) here */
         edge.y = (double)aRow + 2.0 + smidge; /* XXX need sqrt(2) here */
         sRow = aRow + 1;
         sCol = aCol;
         break;

      default:
         return emptyEdge; /* Fail: Illegal edge type */
   }

   /* Fail:  Sobel location out of bounds */
   if(sCol < 0 || sCol >= sobel->width || sRow < 0 || sRow >= sobel->height)
      return emptyEdge;

   /* Use Sobel value that falls directly between 2 accel locations */
   sIdx = sRow * dmtxValueGridGetWidth(sobel) + sCol;
   edge.mag = abs(sobel->value[sIdx]);

   *passFail = DmtxPass;
   return edge;
}

/**
 *
 *
 */
DmtxPassFail
HoughLocalAccumulateEdge(DmtxHoughLocal *local, int phi, ZeroCrossing edge)
{
   double d;
   int dInt;

   d = HoughGetLocalOffset(edge.x - local->xOrigin, edge.y - local->yOrigin, phi);
   dInt = (int)d;

if(dInt > 63)
   return DmtxFail;

   assert(dInt >= 0);
   assert(dInt < 64);
   assert(phi >= 0 && phi < 128);

   local->bucket[dInt][phi] += edge.mag;

   return DmtxPass;
}

/**
 *
 *
 */
double
HoughGetLocalOffset(double xLoc, double yLoc, int phi)
{
   double phiRad, sinPhi, cosPhi;
   double scale, d;

   phiRad = (phi * M_PI)/128.0;
   sinPhi = sin(phiRad);
   cosPhi = cos(phiRad);

   if(phi <= 64)
   {
      scale = 1.0 / (sinPhi + cosPhi);
      d = (xLoc * cosPhi + yLoc * sinPhi) * scale;
   }
   else
   {
      scale = 1.0 / (sinPhi - cosPhi);
      d = ((xLoc * cosPhi + yLoc * sinPhi) - (cosPhi * 64.0)) * scale;
   }

   return d;
}
