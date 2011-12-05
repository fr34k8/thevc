/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2011, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TDecSlice.cpp
    \brief    slice decoder class
*/

#include "TDecSlice.h"

//! \ingroup TLibDecoder
//! \{

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TDecSlice::TDecSlice()
{
#if OL_USE_WPP
  m_pcBufferSbacDecoders = NULL;
  m_pcBufferBinCABACs    = NULL;
#endif
}

TDecSlice::~TDecSlice()
{
}

Void TDecSlice::create( TComSlice* pcSlice, Int iWidth, Int iHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth )
{
}

Void TDecSlice::destroy()
{
#if OL_USE_WPP
  if ( m_pcBufferSbacDecoders )
  {
    delete[] m_pcBufferSbacDecoders;
    m_pcBufferSbacDecoders = NULL;
  }
  if ( m_pcBufferBinCABACs )
  {
    delete[] m_pcBufferBinCABACs;
    m_pcBufferBinCABACs = NULL;
  }
#endif
}

Void TDecSlice::init(TDecEntropy* pcEntropyDecoder, TDecCu* pcCuDecoder)
{
  m_pcEntropyDecoder  = pcEntropyDecoder;
  m_pcCuDecoder       = pcCuDecoder;
}

#if OL_USE_WPP
Void TDecSlice::decompressSlice(TComInputBitstream* pcBitstream, TComInputBitstream** ppcSubstreams, TComPic*& rpcPic, TDecSbac* pcSbacDecoder, TDecSbac* pcSbacDecoders)
#else
Void TDecSlice::decompressSlice(TComInputBitstream* pcBitstream, TComPic*& rpcPic, TDecSbac* pcSbacDecoder)
#endif
{
  TComDataCU* pcCU;
  UInt        uiIsLast = 0;
#if FINE_GRANULARITY_SLICES
#if TILES
  Int   iStartCUEncOrder = max(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr()/rpcPic->getNumPartInCU(), rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getEntropySliceCurStartCUAddr()/rpcPic->getNumPartInCU());
  Int   iStartCUAddr = rpcPic->getPicSym()->getCUOrderMap(iStartCUEncOrder);
#else
  Int   iStartCUAddr = max(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr()/rpcPic->getNumPartInCU(), rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getEntropySliceCurStartCUAddr()/rpcPic->getNumPartInCU());
#endif
#else
#if TILES
  Int   iStartCUEncOrder = max(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr(), rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getEntropySliceCurStartCUAddr());
  Int   iStartCUAddr = rpcPic->getPicSym()->getCUOrderMap(iStartCUEncOrder);
#else
  Int   iStartCUAddr = max(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr(), rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getEntropySliceCurStartCUAddr());
#endif
#endif

  // decoder don't need prediction & residual frame buffer
  rpcPic->setPicYuvPred( 0 );
  rpcPic->setPicYuvResi( 0 );
  
#if ENC_DEC_TRACE
  g_bJustDoIt = g_bEncDecTraceEnable;
#endif
  DTRACE_CABAC_VL( g_nSymbolCounter++ );
  DTRACE_CABAC_T( "\tPOC: " );
  DTRACE_CABAC_V( rpcPic->getPOC() );
  DTRACE_CABAC_T( "\n" );

#if ENC_DEC_TRACE
  g_bJustDoIt = g_bEncDecTraceDisable;
#endif

#if OL_USE_WPP
  UInt uiTilesAcross   = 1;
  TComSlice*  pcSlice = rpcPic->getSlice(rpcPic->getCurrSliceIdx());
  UInt iSymbolMode    = pcSlice->getPPS()->getEntropyCodingMode();
  Int  iNumSubstreams = pcSlice->getPPS()->getNumSubstreams();

  if( iSymbolMode )
  {
    m_pcBufferSbacDecoders = new TDecSbac    [uiTilesAcross];  
    m_pcBufferBinCABACs    = new TDecBinCABAC[uiTilesAcross];
    for (UInt ui = 0; ui < uiTilesAcross; ui++)
      m_pcBufferSbacDecoders[ui].init(&m_pcBufferBinCABACs[ui]);
    //save init. state
    for (UInt ui = 0; ui < uiTilesAcross; ui++)
      m_pcBufferSbacDecoders[ui].load(pcSbacDecoder);
  }

  UInt uiWidthInLCUs  = rpcPic->getPicSym()->getFrameWidthInCU();
  //UInt uiHeightInLCUs = rpcPic->getPicSym()->getFrameHeightInCU();
  UInt uiCol=0, uiLin=0, uiSubStrm=0;
#if TILES
  UInt uiTileStartLCU;
#if !FINE_GRANULARITY_SLICES
  UInt uiSliceStartLCU;
#endif
  UInt uiTileLCUX;
  UInt uiTileLCUY;
  UInt uiTileWidth;
  UInt uiTileHeight;
  Int iNumSubstreamsPerTile = 1; // if independent.
#endif
#endif

#if TILES
  for( Int iCUAddr = iStartCUAddr; !uiIsLast && iCUAddr < rpcPic->getNumCUsInFrame(); iCUAddr = rpcPic->getPicSym()->xCalculateNxtCUAddr(iCUAddr) )
#else
  // for all CUs in slice
  UInt  uiLastCUAddr = iStartCUAddr;
  for( Int iCUAddr = iStartCUAddr; !uiIsLast && iCUAddr < rpcPic->getNumCUsInFrame(); iCUAddr++, uiLastCUAddr++ )
#endif
  {
    pcCU = rpcPic->getCU( iCUAddr );
    pcCU->initCU( rpcPic, iCUAddr );

#if OL_USE_WPP
#if TILES
    uiTileStartLCU = rpcPic->getPicSym()->getTComTile(0)->getFirstCUAddr();
    uiTileLCUX = uiTileStartLCU % uiWidthInLCUs;
    uiTileLCUY = uiTileStartLCU / uiWidthInLCUs;
    uiTileWidth = rpcPic->getPicSym()->getTComTile(0)->getTileWidth();
    uiTileHeight = rpcPic->getPicSym()->getTComTile(0)->getTileHeight();
#if !FINE_GRANULARITY_SLICES
    uiSliceStartLCU = pcSlice->getSliceCurStartCUAddr();
#endif
    uiCol     = iCUAddr % uiWidthInLCUs;
    uiLin     = iCUAddr / uiWidthInLCUs;
#endif
    // inherit from TR if necessary, select substream to use.
    if( iSymbolMode && pcSlice->getPPS()->getEntropyCodingSynchro() )
    {
#if TILES
      // dependent tiles => substreams are "per frame".
      uiSubStrm = uiLin % iNumSubstreams;
      m_pcEntropyDecoder->setBitstream( ppcSubstreams[uiSubStrm] );
      // Synchronize cabac probabilities with upper-right LCU if it's available and we're at the start of a line.
      if (pcSlice->getPPS()->getEntropyCodingSynchro() && uiCol == uiTileLCUX)
      {
        // We'll sync if the TR is available.
        TComDataCU *pcCUUp = pcCU->getCUAbove();
        UInt uiWidthInCU = rpcPic->getFrameWidthInCU();
        TComDataCU *pcCUTR = NULL;
        if ( pcCUUp && ((iCUAddr%uiWidthInCU+pcSlice->getPPS()->getEntropyCodingSynchro()) < uiWidthInCU)  )
          pcCUTR = rpcPic->getCU( iCUAddr - uiWidthInCU + pcSlice->getPPS()->getEntropyCodingSynchro() );
        UInt uiMaxParts = 1<<(pcSlice->getSPS()->getMaxCUDepth()<<1);

#if FINE_GRANULARITY_SLICES
        if ( (true/*bEnforceSliceRestriction*/ &&
             ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || 
             ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getSliceCurStartCUAddr())
             ))||
             (true/*bEnforceEntropySliceRestriction*/ &&
             ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || 
             ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getEntropySliceCurStartCUAddr())
             ))
           )
#else
        if( (true/*bEnforceSliceRestriction*/ &&
            ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || (rpcPic->getPicSym()->getInverseCUOrderMap( pcCUTR->getAddr()) < rpcPic->getPicSym()->getTempInverseCUOrderMap(uiSliceStartLCU)) ||
            (rpcPic->getPicSym()->getTileBoundaryIndependenceIdr() && (rpcPic->getPicSym()->getTileIdxMap( pcCUTR->getAddr() ) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr)) ))) ||
            (true/*bEnforceEntropySliceRestriction*/ &&
            ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || (rpcPic->getPicSym()->getInverseCUOrderMap( pcCUTR->getAddr()) < rpcPic->getPicSym()->getTempInverseCUOrderMap(pcSlice->getEntropySliceCurStartCUAddr()))
    ||
            (rpcPic->getPicSym()->getTileBoundaryIndependenceIdr() && (rpcPic->getPicSym()->getTileIdxMap( pcCUTR->getAddr() ) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr)) )))
          )
#endif
        {
          // TR not available.
        }
        else
        {
          // TR is available, we use it.
            pcSbacDecoders[uiSubStrm].loadContexts( &m_pcBufferSbacDecoders[0] );
        }
      }
#else
      uiCol     = iCUAddr % uiWidthInLCUs;
      uiLin     = iCUAddr / uiWidthInLCUs;
      uiSubStrm = uiLin   % iNumSubstreams;    //index of {substream/entropy coder}
       
      m_pcEntropyDecoder->setBitstream( ppcSubstreams[uiSubStrm] );
      if (pcSlice->getPPS()->getEntropyCodingSynchro() && uiCol == 0 && iCUAddr != 0)
      {
        // We'll sync if the TR is available.
        TComDataCU *pcCUUp = pcCU->getCUAbove();
        UInt uiWidthInCU = rpcPic->getFrameWidthInCU();
        UInt uiMaxParts = 1<<(pcSlice->getSPS()->getMaxCUDepth()<<1);
        TComDataCU *pcCUTR = NULL;
        if ( pcCUUp && ((iCUAddr%uiWidthInCU+pcSlice->getPPS()->getEntropyCodingSynchro()) < uiWidthInCU)  )
          pcCUTR = rpcPic->getCU( iCUAddr - uiWidthInCU + pcSlice->getPPS()->getEntropyCodingSynchro() );
#if FINE_GRANULARITY_SLICES
        if ((true/*bEnforceSliceRestriction*/ &&
            ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getSliceCurStartCUAddr())))
          ||(true/*bEnforceEntropySliceRestriction*/ &&
            ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getEntropySliceCurStartCUAddr()))))
#else
        if ((pcCUTR == NULL)
            || (pcCUTR->getSlice() == NULL)
            || (pcCUTR->getAddr() < pcSlice->getSliceCurStartCUAddr())
            || (pcCUTR->getAddr() < pcSlice->getEntropySliceCurStartCUAddr()))
#endif
        {
          // TR is not available.
        }
        else
        {
          // TR is available, we use it.
          pcSbacDecoders[uiSubStrm].loadContexts( &m_pcBufferSbacDecoders[0] );
        }
      }
#endif // TILES
      pcSbacDecoder->load(&pcSbacDecoders[uiSubStrm]);  //this load is used to simplify the code (avoid to change all the call to pcSbacDecoders)
    }
    else if ( iSymbolMode && !pcSlice->getPPS()->getEntropyCodingSynchro() )
    {
      // Set variables to appropriate values to avoid later code change.
#if TILES
      iNumSubstreamsPerTile = 1;
#endif
    }
#endif // OL_USE_WPP

#if ENC_DEC_TRACE
    g_bJustDoIt = g_bEncDecTraceEnable;
#endif

    m_pcCuDecoder->decodeCU     ( pcCU, uiIsLast );
    m_pcCuDecoder->decompressCU ( pcCU );
    
#if ENC_DEC_TRACE
    g_bJustDoIt = g_bEncDecTraceDisable;
#endif
#if OL_USE_WPP
    if( iSymbolMode )
    {
#if OL_FLUSH
      /*If at the end of a LCU line but not at the end of a substream, perform CABAC flush*/
      if (!uiIsLast && pcSlice->getPPS()->getCabacIstateReset())
      {
        if ((uiCol == uiWidthInLCUs-1) && (uiLin+iNumSubstreams < pcCU->getPic()->getFrameHeightInCU()) )
        {
          m_pcEntropyDecoder->decodeFlush();
        }
      }
#endif
      pcSbacDecoders[uiSubStrm].load(pcSbacDecoder);

      //Store probabilities of second LCU in line into buffer
#if TILES
      if (pcSlice->getPPS()->getEntropyCodingSynchro() && (uiCol == uiTileLCUX+pcSlice->getPPS()->getEntropyCodingSynchro()))
      {
        m_pcBufferSbacDecoders[0].loadContexts( &pcSbacDecoders[uiSubStrm] );
      }
#else
      if (pcSlice->getPPS()->getEntropyCodingSynchro() && (uiCol == pcSlice->getPPS()->getEntropyCodingSynchro()))
      {
        //printf("saving context into 0 for ss%d at %d (col %d)\n", uiSubStrm, iCUAddr, uiCol);
        m_pcBufferSbacDecoders[0].loadContexts( &pcSbacDecoders[uiSubStrm] );
      }
#endif

    }
#endif
  }

}
//! \}
