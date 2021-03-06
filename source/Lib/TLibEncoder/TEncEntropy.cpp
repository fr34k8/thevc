/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2012, ITU/ISO/IEC
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

/** \file     TEncEntropy.cpp
    \brief    entropy encoder class
*/

#include "TEncEntropy.h"
#include "TLibCommon/TypeDef.h"
#include "TLibCommon/TComAdaptiveLoopFilter.h"
#include "TLibCommon/TComSampleAdaptiveOffset.h"

//! \ingroup TLibEncoder
//! \{

Void TEncEntropy::setEntropyCoder ( TEncEntropyIf* e, TComSlice* pcSlice )
{
  m_pcEntropyCoderIf = e;
  m_pcEntropyCoderIf->setSlice ( pcSlice );
}

Void TEncEntropy::encodeSliceHeader ( TComSlice* pcSlice )
{
  if (pcSlice->getSPS()->getUseSAO())
  {
#if REMOVE_APS
    SAOParam *saoParam = pcSlice->getPic()->getPicSym()->getSaoParam();
#else
    SAOParam *saoParam = pcSlice->getAPS()->getSaoParam();
#endif
    pcSlice->setSaoEnabledFlag     (saoParam->bSaoFlag[0]);
    if (pcSlice->getSaoEnabledFlag())
    {
#if SAO_TYPE_SHARING
      pcSlice->setSaoEnabledFlagChroma   (saoParam->bSaoFlag[1]);
#else
      pcSlice->setSaoEnabledFlagCb   (saoParam->bSaoFlag[1]);
      pcSlice->setSaoEnabledFlagCr   (saoParam->bSaoFlag[2]);
#endif
    }
    else
    {
#if SAO_TYPE_SHARING
      pcSlice->setSaoEnabledFlagChroma (0);
#else
      pcSlice->setSaoEnabledFlagCb   (0);
      pcSlice->setSaoEnabledFlagCr   (0);
#endif
    }
  }

  m_pcEntropyCoderIf->codeSliceHeader( pcSlice );
  return;
}

Void  TEncEntropy::encodeTilesWPPEntryPoint( TComSlice* pSlice )
{
  m_pcEntropyCoderIf->codeTilesWPPEntryPoint( pSlice );
}

Void TEncEntropy::encodeTerminatingBit      ( UInt uiIsLast )
{
  m_pcEntropyCoderIf->codeTerminatingBit( uiIsLast );
  
  return;
}

Void TEncEntropy::encodeSliceFinish()
{
  m_pcEntropyCoderIf->codeSliceFinish();
}

Void TEncEntropy::encodeFlush()
{
  m_pcEntropyCoderIf->codeFlush();
}

Void TEncEntropy::encodeStart()
{
  m_pcEntropyCoderIf->encodeStart();
}

Void TEncEntropy::encodeSEI(const SEI& sei)
{
  m_pcEntropyCoderIf->codeSEI(sei);
  return;
}

Void TEncEntropy::encodePPS( TComPPS* pcPPS )
{
  m_pcEntropyCoderIf->codePPS( pcPPS );
  return;
}

Void TEncEntropy::encodeSPS( TComSPS* pcSPS )
{
  m_pcEntropyCoderIf->codeSPS( pcSPS );
  return;
}

Void TEncEntropy::encodeCUTransquantBypassFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  m_pcEntropyCoderIf->codeCUTransquantBypassFlag( pcCU, uiAbsPartIdx );
}

Void TEncEntropy::encodeVPS( TComVPS* pcVPS )
{
  m_pcEntropyCoderIf->codeVPS( pcVPS );
  return;
}

Void TEncEntropy::encodeSkipFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if ( pcCU->getSlice()->isIntra() )
  {
    return;
  }
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  if( !bRD )
  {
    if( pcCU->getLastCUSucIPCMFlag() && pcCU->getIPCMFlag(uiAbsPartIdx) )
    {
      return;
    }
  }
  m_pcEntropyCoderIf->codeSkipFlag( pcCU, uiAbsPartIdx );
}

/** encode merge flag
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiPUIdx
 * \returns Void
 */
Void TEncEntropy::encodeMergeFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiPUIdx )
{ 
  // at least one merge candidate exists
  m_pcEntropyCoderIf->codeMergeFlag( pcCU, uiAbsPartIdx );
}

/** encode merge index
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiPUIdx
 * \param bRD
 * \returns Void
 */
Void TEncEntropy::encodeMergeIndex( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiPUIdx, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
    assert( pcCU->getPartitionSize(uiAbsPartIdx) == SIZE_2Nx2N );
  }

  UInt uiNumCand = MRG_MAX_NUM_CANDS;
  if ( uiNumCand > 1 )
  {
    m_pcEntropyCoderIf->codeMergeIndex( pcCU, uiAbsPartIdx );
  }
}

/** encode prediction mode
 * \param pcCU
 * \param uiAbsPartIdx
 * \param bRD
 * \returns Void
 */
Void TEncEntropy::encodePredMode( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  if( !bRD )
  {
    if( pcCU->getLastCUSucIPCMFlag() && pcCU->getIPCMFlag(uiAbsPartIdx) )
    {
      return;
    }
  }

  if ( pcCU->getSlice()->isIntra() )
  {
    return;
  }

  m_pcEntropyCoderIf->codePredMode( pcCU, uiAbsPartIdx );
}

// Split mode
Void TEncEntropy::encodeSplitFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  if( !bRD )
  {
    if( pcCU->getLastCUSucIPCMFlag() && pcCU->getIPCMFlag(uiAbsPartIdx) )
    {
      return;
    }
  }

  m_pcEntropyCoderIf->codeSplitFlag( pcCU, uiAbsPartIdx, uiDepth );
}

/** encode partition size
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \param bRD
 * \returns Void
 */
Void TEncEntropy::encodePartSize( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  if( !bRD )
  {
    if( pcCU->getLastCUSucIPCMFlag() && pcCU->getIPCMFlag(uiAbsPartIdx) )
    {
      return;
    }
  }
  m_pcEntropyCoderIf->codePartSize( pcCU, uiAbsPartIdx, uiDepth );
}

/** Encode I_PCM information. 
 * \param pcCU pointer to CU 
 * \param uiAbsPartIdx CU index
 * \param bRD flag indicating estimation or encoding
 * \returns Void
 */
Void TEncEntropy::encodeIPCMInfo( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if(!pcCU->getSlice()->getSPS()->getUsePCM()
    || pcCU->getWidth(uiAbsPartIdx) > (1<<pcCU->getSlice()->getSPS()->getPCMLog2MaxSize())
    || pcCU->getWidth(uiAbsPartIdx) < (1<<pcCU->getSlice()->getSPS()->getPCMLog2MinSize()))
  {
    return;
  }
  
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  
  Int numIPCM = 0;
  Bool firstIPCMFlag = false;

  if( pcCU->getIPCMFlag(uiAbsPartIdx) )
  {
    numIPCM = 1;
    firstIPCMFlag = true;

    if( !bRD )
    {
      numIPCM = pcCU->getNumSucIPCM();
      firstIPCMFlag = !pcCU->getLastCUSucIPCMFlag();
    }
  }
  m_pcEntropyCoderIf->codeIPCMInfo ( pcCU, uiAbsPartIdx, numIPCM, firstIPCMFlag);

}

Void TEncEntropy::xEncodeTransform( TComDataCU* pcCU,UInt offsetLuma, UInt offsetChroma, UInt uiAbsPartIdx, UInt absTUPartIdx, UInt uiDepth, UInt width, UInt height, UInt uiTrIdx, UInt uiInnerQuadIdx, Bool& bCodeDQP )
{
  const UInt uiSubdiv = pcCU->getTransformIdx( uiAbsPartIdx ) + pcCU->getDepth( uiAbsPartIdx ) > uiDepth;
  const UInt uiLog2TrafoSize = g_aucConvertToBit[pcCU->getSlice()->getSPS()->getMaxCUWidth()]+2 - uiDepth;
  UInt cbfY = pcCU->getCbf( uiAbsPartIdx, TEXT_LUMA    , uiTrIdx );
  UInt cbfU = pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_U, uiTrIdx );
  UInt cbfV = pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_V, uiTrIdx );

  if(uiTrIdx==0)
  {
    m_bakAbsPartIdxCU = uiAbsPartIdx;
  }
  if( uiLog2TrafoSize == 2 )
  {
    UInt partNum = pcCU->getPic()->getNumPartInCU() >> ( ( uiDepth - 1 ) << 1 );
    if( ( uiAbsPartIdx % partNum ) == 0 )
    {
      m_uiBakAbsPartIdx   = uiAbsPartIdx;
      m_uiBakChromaOffset = offsetChroma;
    }
    else if( ( uiAbsPartIdx % partNum ) == (partNum - 1) )
    {
      cbfU = pcCU->getCbf( m_uiBakAbsPartIdx, TEXT_CHROMA_U, uiTrIdx );
      cbfV = pcCU->getCbf( m_uiBakAbsPartIdx, TEXT_CHROMA_V, uiTrIdx );
    }
  }
  
  if( pcCU->getPredictionMode(uiAbsPartIdx) == MODE_INTRA && pcCU->getPartitionSize(uiAbsPartIdx) == SIZE_NxN && uiDepth == pcCU->getDepth(uiAbsPartIdx) )
  {
    assert( uiSubdiv );
  }
  else if( pcCU->getPredictionMode(uiAbsPartIdx) == MODE_INTER && (pcCU->getPartitionSize(uiAbsPartIdx) != SIZE_2Nx2N) && uiDepth == pcCU->getDepth(uiAbsPartIdx) &&  (pcCU->getSlice()->getSPS()->getQuadtreeTUMaxDepthInter() == 1) )
  {
    if ( uiLog2TrafoSize > pcCU->getQuadtreeTULog2MinSizeInCU(uiAbsPartIdx) )
    {
      assert( uiSubdiv );
    }
    else
    {
      assert(!uiSubdiv );
    }
  }
  else if( uiLog2TrafoSize > pcCU->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() )
  {
    assert( uiSubdiv );
  }
  else if( uiLog2TrafoSize == pcCU->getSlice()->getSPS()->getQuadtreeTULog2MinSize() )
  {
    assert( !uiSubdiv );
  }
  else if( uiLog2TrafoSize == pcCU->getQuadtreeTULog2MinSizeInCU(uiAbsPartIdx) )
  {
    assert( !uiSubdiv );
  }
  else
  {
    assert( uiLog2TrafoSize > pcCU->getQuadtreeTULog2MinSizeInCU(uiAbsPartIdx) );
#if TRANS_SPLIT_FLAG_CTX_REDUCTION
    m_pcEntropyCoderIf->codeTransformSubdivFlag( uiSubdiv, 5 - uiLog2TrafoSize );
#else
    m_pcEntropyCoderIf->codeTransformSubdivFlag( uiSubdiv, uiDepth );
#endif
  }

  const UInt uiTrDepthCurr = uiDepth - pcCU->getDepth( uiAbsPartIdx );
  const Bool bFirstCbfOfCU = uiTrDepthCurr == 0;
  if( bFirstCbfOfCU || uiLog2TrafoSize > 2 )
  {
    if( bFirstCbfOfCU || pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_U, uiTrDepthCurr - 1 ) )
    {
      m_pcEntropyCoderIf->codeQtCbf( pcCU, uiAbsPartIdx, TEXT_CHROMA_U, uiTrDepthCurr );
    }
    if( bFirstCbfOfCU || pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_V, uiTrDepthCurr - 1 ) )
    {
      m_pcEntropyCoderIf->codeQtCbf( pcCU, uiAbsPartIdx, TEXT_CHROMA_V, uiTrDepthCurr );
    }
  }
  else if( uiLog2TrafoSize == 2 )
  {
    assert( pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_U, uiTrDepthCurr ) == pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_U, uiTrDepthCurr - 1 ) );
    assert( pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_V, uiTrDepthCurr ) == pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_V, uiTrDepthCurr - 1 ) );
  }
  
  if( uiSubdiv )
  {
    UInt size;
    width  >>= 1;
    height >>= 1;
    size = width*height;
    uiTrIdx++;
    ++uiDepth;
    const UInt partNum = pcCU->getPic()->getNumPartInCU() >> (uiDepth << 1);
    
#if REMOVE_NSQT
    UInt nsAddr = uiAbsPartIdx;
#else
    UInt nsAddr = 0;
    nsAddr = pcCU->getNSAbsPartIdx( uiLog2TrafoSize-1, uiAbsPartIdx, absTUPartIdx, 0, uiDepth - pcCU->getDepth( uiAbsPartIdx ) );
#endif
    xEncodeTransform( pcCU, offsetLuma, offsetChroma, uiAbsPartIdx, nsAddr, uiDepth, width, height, uiTrIdx, 0, bCodeDQP );

    uiAbsPartIdx += partNum;  offsetLuma += size;  offsetChroma += (size>>2);
#if REMOVE_NSQT
    nsAddr = uiAbsPartIdx;
#else
    nsAddr = pcCU->getNSAbsPartIdx( uiLog2TrafoSize-1, uiAbsPartIdx, absTUPartIdx, 1, uiDepth - pcCU->getDepth( uiAbsPartIdx ) );
#endif
    xEncodeTransform( pcCU, offsetLuma, offsetChroma, uiAbsPartIdx, nsAddr, uiDepth, width, height, uiTrIdx, 1, bCodeDQP );

    uiAbsPartIdx += partNum;  offsetLuma += size;  offsetChroma += (size>>2);
#if REMOVE_NSQT
    nsAddr = uiAbsPartIdx;
#else
    nsAddr = pcCU->getNSAbsPartIdx( uiLog2TrafoSize-1, uiAbsPartIdx, absTUPartIdx, 2, uiDepth - pcCU->getDepth( uiAbsPartIdx ) );
#endif
    xEncodeTransform( pcCU, offsetLuma, offsetChroma, uiAbsPartIdx, nsAddr, uiDepth, width, height, uiTrIdx, 2, bCodeDQP );

    uiAbsPartIdx += partNum;  offsetLuma += size;  offsetChroma += (size>>2);
#if REMOVE_NSQT
    nsAddr = uiAbsPartIdx;
#else
    nsAddr = pcCU->getNSAbsPartIdx( uiLog2TrafoSize-1, uiAbsPartIdx, absTUPartIdx, 3, uiDepth - pcCU->getDepth( uiAbsPartIdx ) );
#endif
    xEncodeTransform( pcCU, offsetLuma, offsetChroma, uiAbsPartIdx, nsAddr, uiDepth, width, height, uiTrIdx, 3, bCodeDQP );
  }
  else
  {
    {
      DTRACE_CABAC_VL( g_nSymbolCounter++ );
      DTRACE_CABAC_T( "\tTrIdx: abspart=" );
      DTRACE_CABAC_V( uiAbsPartIdx );
      DTRACE_CABAC_T( "\tdepth=" );
      DTRACE_CABAC_V( uiDepth );
      DTRACE_CABAC_T( "\ttrdepth=" );
      DTRACE_CABAC_V( pcCU->getTransformIdx( uiAbsPartIdx ) );
      DTRACE_CABAC_T( "\n" );
    }
    UInt uiLumaTrMode, uiChromaTrMode;
    pcCU->convertTransIdx( uiAbsPartIdx, pcCU->getTransformIdx( uiAbsPartIdx ), uiLumaTrMode, uiChromaTrMode );
#if !REMOVE_NSQT
    if(pcCU->getPredictionMode( uiAbsPartIdx ) == MODE_INTER && pcCU->useNonSquarePU( uiAbsPartIdx ) )
    {
      pcCU->setNSQTIdxSubParts( uiLog2TrafoSize, uiAbsPartIdx, absTUPartIdx, uiLumaTrMode );
    }
#endif
    
    if( pcCU->getPredictionMode(uiAbsPartIdx) != MODE_INTRA && uiDepth == pcCU->getDepth( uiAbsPartIdx ) && !pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_U, 0 ) && !pcCU->getCbf( uiAbsPartIdx, TEXT_CHROMA_V, 0 ) )
    {
      assert( pcCU->getCbf( uiAbsPartIdx, TEXT_LUMA, 0 ) );
      //      printf( "saved one bin! " );
    }
    else
    {
      m_pcEntropyCoderIf->codeQtCbf( pcCU, uiAbsPartIdx, TEXT_LUMA, uiLumaTrMode );
    }


    if ( cbfY || cbfU || cbfV )
    {
      // dQP: only for LCU once
      if ( pcCU->getSlice()->getPPS()->getUseDQP() )
      {
        if ( bCodeDQP )
        {
          encodeQP( pcCU, m_bakAbsPartIdxCU );
          bCodeDQP = false;
        }
      }
    }
    if( cbfY )
    {
      Int trWidth = width;
      Int trHeight = height;
#if !REMOVE_NSQT
      pcCU->getNSQTSize( uiTrIdx, uiAbsPartIdx, trWidth, trHeight );
#endif
      m_pcEntropyCoderIf->codeCoeffNxN( pcCU, (pcCU->getCoeffY()+offsetLuma), uiAbsPartIdx, trWidth, trHeight, uiDepth, TEXT_LUMA );
    }
    if( uiLog2TrafoSize > 2 )
    {
      Int trWidth = width >> 1;
      Int trHeight = height >> 1;
#if !REMOVE_NSQT
      pcCU->getNSQTSize( uiTrIdx, uiAbsPartIdx, trWidth, trHeight );
#endif
      if( cbfU )
      {
        m_pcEntropyCoderIf->codeCoeffNxN( pcCU, (pcCU->getCoeffCb()+offsetChroma), uiAbsPartIdx, trWidth, trHeight, uiDepth, TEXT_CHROMA_U );
      }
      if( cbfV )
      {
        m_pcEntropyCoderIf->codeCoeffNxN( pcCU, (pcCU->getCoeffCr()+offsetChroma), uiAbsPartIdx, trWidth, trHeight, uiDepth, TEXT_CHROMA_V );
      }
    }
    else
    {
      UInt partNum = pcCU->getPic()->getNumPartInCU() >> ( ( uiDepth - 1 ) << 1 );
      if( ( uiAbsPartIdx % partNum ) == (partNum - 1) )
      {
        Int trWidth = width;
        Int trHeight = height;
#if !REMOVE_NSQT
        pcCU->getNSQTSize( uiTrIdx - 1, uiAbsPartIdx, trWidth, trHeight );
#endif
        if( cbfU )
        {
          m_pcEntropyCoderIf->codeCoeffNxN( pcCU, (pcCU->getCoeffCb()+m_uiBakChromaOffset), m_uiBakAbsPartIdx, trWidth, trHeight, uiDepth, TEXT_CHROMA_U );
        }
        if( cbfV )
        {
          m_pcEntropyCoderIf->codeCoeffNxN( pcCU, (pcCU->getCoeffCr()+m_uiBakChromaOffset), m_uiBakAbsPartIdx, trWidth, trHeight, uiDepth, TEXT_CHROMA_V );
        }
      }
    }
  }
}

// Intra direction for Luma
Void TEncEntropy::encodeIntraDirModeLuma  ( TComDataCU* pcCU, UInt absPartIdx, Bool isMultiplePU )
{
  m_pcEntropyCoderIf->codeIntraDirLumaAng( pcCU, absPartIdx , isMultiplePU);
}

// Intra direction for Chroma
Void TEncEntropy::encodeIntraDirModeChroma( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  
  m_pcEntropyCoderIf->codeIntraDirChroma( pcCU, uiAbsPartIdx );
}

Void TEncEntropy::encodePredInfo( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  if( pcCU->isIntra( uiAbsPartIdx ) )                                 // If it is Intra mode, encode intra prediction mode.
  {
    encodeIntraDirModeLuma  ( pcCU, uiAbsPartIdx,true );
    encodeIntraDirModeChroma( pcCU, uiAbsPartIdx, bRD );
  }
  else                                                                // if it is Inter mode, encode motion vector and reference index
  {
    encodePUWise( pcCU, uiAbsPartIdx, bRD );
  }
}

/** encode motion information for every PU block
 * \param pcCU
 * \param uiAbsPartIdx
 * \param bRD
 * \returns Void
 */
Void TEncEntropy::encodePUWise( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if ( bRD )
  {
    uiAbsPartIdx = 0;
  }
  
  PartSize ePartSize = pcCU->getPartitionSize( uiAbsPartIdx );
  UInt uiNumPU = ( ePartSize == SIZE_2Nx2N ? 1 : ( ePartSize == SIZE_NxN ? 4 : 2 ) );
  UInt uiDepth = pcCU->getDepth( uiAbsPartIdx );
  UInt uiPUOffset = ( g_auiPUOffset[UInt( ePartSize )] << ( ( pcCU->getSlice()->getSPS()->getMaxCUDepth() - uiDepth ) << 1 ) ) >> 4;

  for ( UInt uiPartIdx = 0, uiSubPartIdx = uiAbsPartIdx; uiPartIdx < uiNumPU; uiPartIdx++, uiSubPartIdx += uiPUOffset )
  {
    encodeMergeFlag( pcCU, uiSubPartIdx, uiPartIdx );
    if ( pcCU->getMergeFlag( uiSubPartIdx ) )
    {
      encodeMergeIndex( pcCU, uiSubPartIdx, uiPartIdx );
    }
    else
    {
      encodeInterDirPU( pcCU, uiSubPartIdx );
      for ( UInt uiRefListIdx = 0; uiRefListIdx < 2; uiRefListIdx++ )
      {
        if ( pcCU->getSlice()->getNumRefIdx( RefPicList( uiRefListIdx ) ) > 0 )
        {
          encodeRefFrmIdxPU ( pcCU, uiSubPartIdx, RefPicList( uiRefListIdx ) );
          encodeMvdPU       ( pcCU, uiSubPartIdx, RefPicList( uiRefListIdx ) );
          encodeMVPIdxPU    ( pcCU, uiSubPartIdx, RefPicList( uiRefListIdx ) );
        }
      }
    }
  }

  return;
}

Void TEncEntropy::encodeInterDirPU( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  if ( !pcCU->getSlice()->isInterB() )
  {
    return;
  }

  m_pcEntropyCoderIf->codeInterDir( pcCU, uiAbsPartIdx );
  return;
}

/** encode reference frame index for a PU block
 * \param pcCU
 * \param uiAbsPartIdx
 * \param eRefList
 * \returns Void
 */
Void TEncEntropy::encodeRefFrmIdxPU( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  assert( !pcCU->isIntra( uiAbsPartIdx ) );
  {
    if ( ( pcCU->getSlice()->getNumRefIdx( eRefList ) == 1 ) )
    {
      return;
    }

    if ( pcCU->getInterDir( uiAbsPartIdx ) & ( 1 << eRefList ) )
    {
      m_pcEntropyCoderIf->codeRefFrmIdx( pcCU, uiAbsPartIdx, eRefList );
    }
  }

  return;
}

/** encode motion vector difference for a PU block
 * \param pcCU
 * \param uiAbsPartIdx
 * \param eRefList
 * \returns Void
 */
Void TEncEntropy::encodeMvdPU( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  assert( !pcCU->isIntra( uiAbsPartIdx ) );

  if ( pcCU->getInterDir( uiAbsPartIdx ) & ( 1 << eRefList ) )
  {
    m_pcEntropyCoderIf->codeMvd( pcCU, uiAbsPartIdx, eRefList );
  }
  return;
}

Void TEncEntropy::encodeMVPIdxPU( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  if ( (pcCU->getInterDir( uiAbsPartIdx ) & ( 1 << eRefList )) && (pcCU->getAMVPMode(uiAbsPartIdx) == AM_EXPL) )
  {
    m_pcEntropyCoderIf->codeMVPIdx( pcCU, uiAbsPartIdx, eRefList );
  }

  return;
}

Void TEncEntropy::encodeQtCbf( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eType, UInt uiTrDepth )
{
  m_pcEntropyCoderIf->codeQtCbf( pcCU, uiAbsPartIdx, eType, uiTrDepth );
}

Void TEncEntropy::encodeTransformSubdivFlag( UInt uiSymbol, UInt uiCtx )
{
  m_pcEntropyCoderIf->codeTransformSubdivFlag( uiSymbol, uiCtx );
}

Void TEncEntropy::encodeQtRootCbf( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  m_pcEntropyCoderIf->codeQtRootCbf( pcCU, uiAbsPartIdx );
}

#if TU_ZERO_CBF_RDO
Void TEncEntropy::encodeQtCbfZero( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eType, UInt uiTrDepth )
{
  m_pcEntropyCoderIf->codeQtCbfZero( pcCU, uiAbsPartIdx, eType, uiTrDepth );
}
Void TEncEntropy::encodeQtRootCbfZero( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  m_pcEntropyCoderIf->codeQtRootCbfZero( pcCU, uiAbsPartIdx );
}
#endif

// dQP
Void TEncEntropy::encodeQP( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool bRD )
{
  if( bRD )
  {
    uiAbsPartIdx = 0;
  }
  
  if ( pcCU->getSlice()->getPPS()->getUseDQP() )
  {
    m_pcEntropyCoderIf->codeDeltaQP( pcCU, uiAbsPartIdx );
  }
}


// texture
/** encode coefficients
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \param uiWidth
 * \param uiHeight
 */
Void TEncEntropy::encodeCoeff( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, UInt uiWidth, UInt uiHeight, Bool& bCodeDQP )
{
  UInt uiMinCoeffSize = pcCU->getPic()->getMinCUWidth()*pcCU->getPic()->getMinCUHeight();
  UInt uiLumaOffset   = uiMinCoeffSize*uiAbsPartIdx;
  UInt uiChromaOffset = uiLumaOffset>>2;
  
  UInt uiLumaTrMode, uiChromaTrMode;
  pcCU->convertTransIdx( uiAbsPartIdx, pcCU->getTransformIdx(uiAbsPartIdx), uiLumaTrMode, uiChromaTrMode );
  
  if( pcCU->isIntra(uiAbsPartIdx) )
  {
    DTRACE_CABAC_VL( g_nSymbolCounter++ )
    DTRACE_CABAC_T( "\tdecodeTransformIdx()\tCUDepth=" )
    DTRACE_CABAC_V( uiDepth )
    DTRACE_CABAC_T( "\n" )
  }
  else
  {
    if( !(pcCU->getMergeFlag( uiAbsPartIdx ) && pcCU->getPartitionSize(uiAbsPartIdx) == SIZE_2Nx2N ) )
    {
      m_pcEntropyCoderIf->codeQtRootCbf( pcCU, uiAbsPartIdx );
    }
    if ( !pcCU->getQtRootCbf( uiAbsPartIdx ) )
    {
#if !REMOVE_NSQT
      pcCU->setNSQTIdxSubParts( uiAbsPartIdx, uiDepth );
#endif
      return;
    }
  }
  
  xEncodeTransform( pcCU, uiLumaOffset, uiChromaOffset, uiAbsPartIdx, uiAbsPartIdx, uiDepth, uiWidth, uiHeight, 0, 0, bCodeDQP);
}

Void TEncEntropy::encodeCoeffNxN( TComDataCU* pcCU, TCoeff* pcCoeff, UInt uiAbsPartIdx, UInt uiTrWidth, UInt uiTrHeight, UInt uiDepth, TextType eType )
{
  // This is for Transform unit processing. This may be used at mode selection stage for Inter.
  m_pcEntropyCoderIf->codeCoeffNxN( pcCU, pcCoeff, uiAbsPartIdx, uiTrWidth, uiTrHeight, uiDepth, eType );
}

Void TEncEntropy::estimateBit (estBitsSbacStruct* pcEstBitsSbac, Int width, Int height, TextType eTType)
{  
  eTType = eTType == TEXT_LUMA ? TEXT_LUMA : TEXT_CHROMA;
  
  m_pcEntropyCoderIf->estBit ( pcEstBitsSbac, width, height, eTType );
}

/** Encode SAO Offset
 * \param  saoLcuParam SAO LCU paramters
 */
#if SAO_TYPE_SHARING 
Void TEncEntropy::encodeSaoOffset(SaoLcuParam* saoLcuParam, UInt compIdx)
#else
Void TEncEntropy::encodeSaoOffset(SaoLcuParam* saoLcuParam)
#endif
{
  UInt uiSymbol;
  Int i;

  uiSymbol = saoLcuParam->typeIdx + 1;
#if SAO_TYPE_SHARING
  if (compIdx!=2)
  {
    m_pcEntropyCoderIf->codeSaoTypeIdx(uiSymbol);
  }
#else
  m_pcEntropyCoderIf->codeSaoTypeIdx(uiSymbol);
#endif
  if (uiSymbol)
  {
#if SAO_TYPE_CODING
#if SAO_TYPE_SHARING
    if (saoLcuParam->typeIdx < 4 && compIdx != 2)
#else
    if (saoLcuParam->typeIdx < 4)
#endif
    {
      saoLcuParam->subTypeIdx = saoLcuParam->typeIdx;
    }
#endif
#if FULL_NBIT
    Int offsetTh = 1 << ( min((Int)(g_uiBitDepth + (g_uiBitDepth-8)-5),5) );
#else
    Int offsetTh = 1 << ( min((Int)(g_uiBitDepth + g_uiBitIncrement-5),5) );
#endif
    if( saoLcuParam->typeIdx == SAO_BO )
    {
#if !SAO_TYPE_CODING
      // Code Left Band Index
      uiSymbol = (UInt) (saoLcuParam->bandPosition);
      m_pcEntropyCoderIf->codeSaoUflc(uiSymbol);
#endif
      for( i=0; i< saoLcuParam->length; i++)
      {
        UInt absOffset = ( (saoLcuParam->offset[i] < 0) ? -saoLcuParam->offset[i] : saoLcuParam->offset[i]);
        m_pcEntropyCoderIf->codeSaoMaxUvlc(absOffset, offsetTh-1);
      }  
      for( i=0; i< saoLcuParam->length; i++)
      {
        if (saoLcuParam->offset[i] != 0)
        {
          UInt sign = (saoLcuParam->offset[i] < 0) ? 1 : 0 ;
          m_pcEntropyCoderIf->codeSAOSign(sign);
        }
      }
#if SAO_TYPE_CODING
      uiSymbol = (UInt) (saoLcuParam->subTypeIdx);
      m_pcEntropyCoderIf->codeSaoUflc(5, uiSymbol);
#endif
    }
    else if( saoLcuParam->typeIdx < 4 )
    {
      m_pcEntropyCoderIf->codeSaoMaxUvlc( saoLcuParam->offset[0], offsetTh-1);
      m_pcEntropyCoderIf->codeSaoMaxUvlc( saoLcuParam->offset[1], offsetTh-1);
      m_pcEntropyCoderIf->codeSaoMaxUvlc(-saoLcuParam->offset[2], offsetTh-1);
      m_pcEntropyCoderIf->codeSaoMaxUvlc(-saoLcuParam->offset[3], offsetTh-1);
#if SAO_TYPE_CODING
#if SAO_TYPE_SHARING
      if (compIdx!=2)
      {
        uiSymbol = (UInt) (saoLcuParam->subTypeIdx);
        m_pcEntropyCoderIf->codeSaoUflc(2, uiSymbol);
      }
#else
     uiSymbol = (UInt) (saoLcuParam->subTypeIdx);
     m_pcEntropyCoderIf->codeSaoUflc(2, uiSymbol);
#endif
#endif
    }
  }
}

/** Encode SAO unit interleaving
* \param  rx
* \param  ry
* \param  pSaoParam
* \param  pcCU
* \param  iCUAddrInSlice
* \param  iCUAddrUpInSlice
* \param  bLFCrossSliceBoundaryFlag
 */
Void TEncEntropy::encodeSaoUnitInterleaving(Int compIdx, Bool saoFlag, Int rx, Int ry, SaoLcuParam* saoLcuParam, Int cuAddrInSlice, Int cuAddrUpInSlice, Int allowMergeLeft, Int allowMergeUp)
{
  if (saoFlag)
  {
    if (rx>0 && cuAddrInSlice!=0 && allowMergeLeft)
    {
#if SAO_MERGE_ONE_CTX
      m_pcEntropyCoderIf->codeSaoMerge(saoLcuParam->mergeLeftFlag);
#else
      m_pcEntropyCoderIf->codeSaoMergeLeft(saoLcuParam->mergeLeftFlag,compIdx);
#endif
    }
    else
    {
      saoLcuParam->mergeLeftFlag = 0;
    }
    if (saoLcuParam->mergeLeftFlag == 0)
    {
      if ( (ry > 0) && (cuAddrUpInSlice>=0) && allowMergeUp )
      {
#if SAO_MERGE_ONE_CTX
        m_pcEntropyCoderIf->codeSaoMerge(saoLcuParam->mergeUpFlag);
#else
        m_pcEntropyCoderIf->codeSaoMergeUp(saoLcuParam->mergeUpFlag);
#endif
      }
      else
      {
        saoLcuParam->mergeUpFlag = 0;
      }
      if (!saoLcuParam->mergeUpFlag)
      {
#if SAO_TYPE_SHARING 
        encodeSaoOffset(saoLcuParam, compIdx);
#else
        encodeSaoOffset(saoLcuParam);
#endif
      }
    }
  }
}

Int TEncEntropy::countNonZeroCoeffs( TCoeff* pcCoef, UInt uiSize )
{
  Int count = 0;
  
  for ( Int i = 0; i < uiSize; i++ )
  {
    count += pcCoef[i] != 0;
  }
  
  return count;
}

/** encode quantization matrix
 * \param scalingList quantization matrix information
 */
Void TEncEntropy::encodeScalingList( TComScalingList* scalingList )
{
  m_pcEntropyCoderIf->codeScalingList( scalingList );
}

//! \}
