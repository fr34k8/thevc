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

/** \file     TEncTop.cpp
    \brief    encoder class
*/

#include "TLibCommon/CommonDef.h"
#include "TEncTop.h"
#if QP_ADAPTATION
#include "TEncPic.h"
#endif
#if FAST_BIT_EST
#include "TLibCommon/ContextModel.h"
#endif

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TEncTop::TEncTop()
{
  m_iPOCLast          = -1;
  m_iNumPicRcvd       =  0;
  m_uiNumAllPicCoded  =  0;
  m_pppcRDSbacCoder   =  NULL;
  m_pppcBinCoderCABAC =  NULL;
  m_cRDGoOnSbacCoder.init( &m_cRDGoOnBinCoderCABAC );
#if ENC_DEC_TRACE
  g_hTrace = fopen( "TraceEnc.txt", "wb" );
  g_bJustDoIt = g_bEncDecTraceDisable;
  g_nSymbolCounter = 0;
#endif

  m_iMaxRefPicNum     = 0;

#if FAST_BIT_EST
  ContextModel::buildNextStateTable();
#endif

}

TEncTop::~TEncTop()
{
#if ENC_DEC_TRACE
  fclose( g_hTrace );
#endif
}

Void TEncTop::create ()
{
  // initialize global variables
  initROM();
  
  // create processing unit classes
  m_cGOPEncoder.        create( getSourceWidth(), getSourceHeight(), g_uiMaxCUWidth, g_uiMaxCUHeight );
  m_cSliceEncoder.      create( getSourceWidth(), getSourceHeight(), g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
  m_cCuEncoder.         create( g_uiMaxCUDepth, g_uiMaxCUWidth, g_uiMaxCUHeight );
#if ADAPTIVE_QP_SELECTION
  if (m_bUseAdaptQpSelect)
  {
    m_cTrQuant.initSliceQpDelta();
  }
#endif
  
  // if SBAC-based RD optimization is used
  if( m_bUseSBACRD )
  {
    m_pppcRDSbacCoder = new TEncSbac** [g_uiMaxCUDepth+1];
#if FAST_BIT_EST
    m_pppcBinCoderCABAC = new TEncBinCABACCounter** [g_uiMaxCUDepth+1];
#else
    m_pppcBinCoderCABAC = new TEncBinCABAC** [g_uiMaxCUDepth+1];
#endif
    
    for ( Int iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      m_pppcRDSbacCoder[iDepth] = new TEncSbac* [CI_NUM];
#if FAST_BIT_EST
      m_pppcBinCoderCABAC[iDepth] = new TEncBinCABACCounter* [CI_NUM];
#else
      m_pppcBinCoderCABAC[iDepth] = new TEncBinCABAC* [CI_NUM];
#endif
      
      for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
      {
        m_pppcRDSbacCoder[iDepth][iCIIdx] = new TEncSbac;
#if FAST_BIT_EST
        m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABACCounter;
#else
        m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABAC;
#endif
        m_pppcRDSbacCoder   [iDepth][iCIIdx]->init( m_pppcBinCoderCABAC [iDepth][iCIIdx] );
      }
    }
  }
}

Void TEncTop::destroy ()
{
  // destroy processing unit classes
  m_cGOPEncoder.        destroy();
  m_cSliceEncoder.      destroy();
  m_cCuEncoder.         destroy();
#if G1002_RPS
  m_cRPSList.               destroy();
#endif
  
  // SBAC RD
  if( m_bUseSBACRD )
  {
    Int iDepth;
    for ( iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
      {
        delete m_pppcRDSbacCoder[iDepth][iCIIdx];
        delete m_pppcBinCoderCABAC[iDepth][iCIIdx];
      }
    }
    
    for ( iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      delete [] m_pppcRDSbacCoder[iDepth];
      delete [] m_pppcBinCoderCABAC[iDepth];
    }
    
    delete [] m_pppcRDSbacCoder;
    delete [] m_pppcBinCoderCABAC;
  }
  
  // destroy ROM
  destroyROM();
  
  return;
}

Void TEncTop::init()
{
  UInt *aTable4=NULL, *aTable8=NULL;
  UInt* aTableLastPosVlcIndex=NULL; 
  // initialize SPS
  xInitSPS();
  
  // initialize PPS
  m_cPPS.setSPS(&m_cSPS);
#if G1002_RPS
  m_cPPS.setRPSList(&m_cRPSList);
#endif
  xInitPPS();
#if G1002_RPS
  xInitRPS();
#endif

  // initialize processing unit classes
  m_cGOPEncoder.  init( this );
  m_cSliceEncoder.init( this );
  m_cCuEncoder.   init( this );
  
  // initialize transform & quantization class
  m_pcCavlcCoder = getCavlcCoder();
  aTable4 = m_pcCavlcCoder->GetLP4Table();
  aTableLastPosVlcIndex=m_pcCavlcCoder->GetLastPosVlcIndexTable();
  
  m_cTrQuant.init( g_uiMaxCUWidth, g_uiMaxCUHeight, 1 << m_uiQuadtreeTULog2MaxSize,
                  aTable4, aTable8, 
                  aTableLastPosVlcIndex, true 
#if ADAPTIVE_QP_SELECTION                  
                  , m_bUseAdaptQpSelect
#endif
                  );
  
  // initialize encoder search class
  m_cSearch.init( this, &m_cTrQuant, m_iSearchRange, m_bipredSearchRange, m_iFastSearch, &m_cEntropyCoder, &m_cRdCost, getRDSbacCoder(), getRDGoOnSbacCoder() );

  m_iMaxRefPicNum = 0;
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void TEncTop::deletePicBuffer()
{
  TComList<TComPic*>::iterator iterPic = m_cListPic.begin();
  Int iSize = Int( m_cListPic.size() );
  
  for ( Int i = 0; i < iSize; i++ )
  {
    TComPic* pcPic = *(iterPic++);
    
    pcPic->destroy();
    delete pcPic;
    pcPic = NULL;
  }
}

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \param   bEos                true if end-of-sequence is reached
 \param   pcPicYuvOrg         original YUV picture
 \retval  rcListPicYuvRecOut  list of reconstruction YUV pictures
 \retval  rcListBitstreamOut  list of output bitstreams
 \retval  iNumEncoded         number of encoded pictures
 */
Void TEncTop::encode( bool bEos, TComPicYuv* pcPicYuvOrg, TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded )
{
  TComPic* pcPicCurr = NULL;
  
  // get original YUV
  xGetNewPicBuffer( pcPicCurr );
  pcPicYuvOrg->copyToPic( pcPicCurr->getPicYuvOrg() );
  
#if QP_ADAPTATION
  // compute image characteristics
  if ( getUseAdaptiveQP() )
  {
    m_cPreanalyzer.xPreanalyze( dynamic_cast<TEncPic*>( pcPicCurr ) );
  }
#endif
  
  if ( m_iPOCLast != 0 && ( m_iNumPicRcvd != m_iGOPSize && m_iGOPSize ) && !bEos )
  {
    iNumEncoded = 0;
    return;
  }
  
  // compress GOP
  m_cGOPEncoder.compressGOP(m_iPOCLast, m_iNumPicRcvd, m_cListPic, rcListPicYuvRecOut, accessUnitsOut);
  
  iNumEncoded         = m_iNumPicRcvd;
  m_iNumPicRcvd       = 0;
  m_uiNumAllPicCoded += iNumEncoded;
  
  if (bEos)
  {
    m_cGOPEncoder.printOutSummary (m_uiNumAllPicCoded);
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \retval rpcPic obtained picture buffer
 */
Void TEncTop::xGetNewPicBuffer ( TComPic*& rpcPic )
{
  TComSlice::sortPicList(m_cListPic);
  
#if G1002_RPS
  if (m_cListPic.size() >= (UInt)(m_iGOPSize + getMaxNumberOfReferencePictures() + 2) )
  {
    TComList<TComPic*>::iterator iterPic  = m_cListPic.begin();
    Int iSize = Int( m_cListPic.size() );
    for ( Int i = 0; i < iSize; i++ )
    {
      rpcPic = *(++iterPic);
      if(rpcPic->getSlice(0)->isReferenced() == false)
         break;
    }
  }
  else
  {
#else
  // bug-fix - erase frame memory (previous GOP) which is not used for reference any more
  if (m_cListPic.size() >= (UInt)(m_iGOPSize + 2 * getNumOfReference() + 1) )  // 2)   //  K. Lee bug fix - for multiple reference > 2
  {
#if REF_SETTING_FOR_LD
    if ( m_bUseNewRefSetting )
    {
      Bool bFound = false;
      TComList<TComPic*>::iterator  it = m_cListPic.begin();
      while ( it != m_cListPic.end() )
      {
        if ( (*it)->getReconMark() == false )
        {
          bFound = true;
          rpcPic = *it;
          m_cListPic.erase( it );
          break;
        }
        if ( !(*it)->getSlice(0)->isReferenced() )
        {
          bFound = true;
          (*it)->setReconMark( false );
          (*it)->getPicYuvRec()->setBorderExtension( false );
          rpcPic = *it;
          m_cListPic.erase( it );
          break;
        }

        it++;
      }
      if ( !bFound )
      {
        assert(0);
      }
    }
    else
    {
      rpcPic = m_cListPic.popFront();
    }
#else
    rpcPic = m_cListPic.popFront();
#endif
    
    // is it necessary without long-term reference?
    if ( rpcPic->getERBIndex() > 0 && abs(rpcPic->getPOC() - m_iPOCLast) <= 0 )
    {
      m_cListPic.pushFront(rpcPic);
      
      TComList<TComPic*>::iterator iterPic  = m_cListPic.begin();
      rpcPic = *(++iterPic);
      if ( abs(rpcPic->getPOC() - m_iPOCLast) <= m_iGOPSize )
      {
#endif
#if QP_ADAPTATION
        if ( getUseAdaptiveQP() )
        {
          TEncPic* pcEPic = new TEncPic;
          pcEPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth, m_cPPS.getMaxCuDQPDepth()+1 );
          rpcPic = pcEPic;
        }
        else
        {
          rpcPic = new TComPic;
          rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
        }
#else
        rpcPic = new TComPic;
        rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
#endif
#if G1002_RPS
    m_cListPic.pushBack( rpcPic );
  }
#else
      }
      else
      {
        m_cListPic.erase( iterPic );
        TComSlice::sortPicList( m_cListPic );
      }
    }
  }
  else
  {
#if QP_ADAPTATION
    if ( getUseAdaptiveQP() )
    {
      TEncPic* pcEPic = new TEncPic;
      pcEPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth, m_cPPS.getMaxCuDQPDepth()+1 );
      rpcPic = pcEPic;
    }
    else
    {
      rpcPic = new TComPic;
      rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
    }
#else
    rpcPic = new TComPic;
    rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
#endif
  }
  
  m_cListPic.pushBack( rpcPic );
#endif
  rpcPic->setReconMark (false);
  
  m_iPOCLast++;
  m_iNumPicRcvd++;
  
  rpcPic->getSlice(0)->setPOC( m_iPOCLast );
  // mark it should be extended
  rpcPic->getPicYuvRec()->setBorderExtension(false);
}

Void TEncTop::xInitSPS()
{
  m_cSPS.setWidth         ( m_iSourceWidth      );
  m_cSPS.setHeight        ( m_iSourceHeight     );
  m_cSPS.setMaxCUWidth    ( g_uiMaxCUWidth      );
  m_cSPS.setMaxCUHeight   ( g_uiMaxCUHeight     );
  m_cSPS.setMaxCUDepth    ( g_uiMaxCUDepth      );
  m_cSPS.setMinTrDepth    ( 0                   );
  m_cSPS.setMaxTrDepth    ( 1                   );
  
#if G1002_RPS
  m_cSPS.setMaxNumberOfReferencePictures(m_uiMaxNumberOfReferencePictures);
  m_cSPS.setNumReorderFrames(m_numReorderFrames);
#endif

  m_cSPS.setQuadtreeTULog2MaxSize( m_uiQuadtreeTULog2MaxSize );
  m_cSPS.setQuadtreeTULog2MinSize( m_uiQuadtreeTULog2MinSize );
  m_cSPS.setQuadtreeTUMaxDepthInter( m_uiQuadtreeTUMaxDepthInter    );
  m_cSPS.setQuadtreeTUMaxDepthIntra( m_uiQuadtreeTUMaxDepthIntra    );
  
#if !G1002_RPS
  m_cSPS.setUseLDC        ( m_bUseLDC           );
#endif
  
  m_cSPS.setUseMRG        ( m_bUseMRG           ); // SOPH:

  m_cSPS.setMaxTrSize   ( 1 << m_uiQuadtreeTULog2MaxSize );
  
  m_cSPS.setUseLComb    ( m_bUseLComb           );
  m_cSPS.setLCMod       ( m_bLCMod   );
#if NSQT
  m_cSPS.setUseNSQT( m_useNSQT );
#endif
  
  Int i;
#if HHI_AMVP_OFF
  for ( i = 0; i < g_uiMaxCUDepth; i++ )
  {
    m_cSPS.setAMVPMode( i, AM_NONE );
  }
#else
  for ( i = 0; i < g_uiMaxCUDepth; i++ )
  {
    m_cSPS.setAMVPMode( i, AM_EXPL );
  }
#endif
  
#if AMP
  for (i = 0; i < g_uiMaxCUDepth-1; i++ )
  {
    m_cSPS.setAMPAcc( i, m_useAMP );
    //m_cSPS.setAMPAcc( i, 1 );
  }

  m_cSPS.setUseAMP ( m_useAMP );

  for (i = g_uiMaxCUDepth-1; i < g_uiMaxCUDepth; i++ )
  {
    m_cSPS.setAMPAcc(i, 0);
  }
#endif

  if ( m_bTLayering )
  {
    Int iMaxTLayers = 1;
    for ( i = 1; ; i++)
    {
      iMaxTLayers = i;
#if G1002_RPS
      if ( (m_iGOPSize >> i) == 0 ) 
#else
      if ( (m_iRateGOPSize >> i) == 0 ) 
#endif
      {
        break;
      }
    }
  
    m_cSPS.setMaxTLayers( (UInt)iMaxTLayers );

    Bool bTemporalIdNestingFlag = true;
    for ( i = 0; i < m_cSPS.getMaxTLayers()-1; i++ )
    {
      if ( !m_abTLayerSwitchingFlag[i] )
      {
        bTemporalIdNestingFlag = false;
        break;
      }
    }

    m_cSPS.setTemporalIdNestingFlag( bTemporalIdNestingFlag );
  }
  else
  {
    m_cSPS.setMaxTLayers( 1 );
    m_cSPS.setTemporalIdNestingFlag( false );
  }

#if !G1002_RPS
#if REF_SETTING_FOR_LD
  m_cSPS.setUseNewRefSetting( m_bUseNewRefSetting );
  if ( m_bUseNewRefSetting )
  {
    m_cSPS.setMaxNumRefFrames( m_iNumOfReference );
  }
#endif
#endif
}

Void TEncTop::xInitPPS()
{
  if ( m_cSPS.getTemporalIdNestingFlag() ) 
  {
    m_cPPS.setNumTLayerSwitchingFlags( 0 );
    for ( UInt i = 0; i < m_cSPS.getMaxTLayers() - 1; i++ )
    {
      m_cPPS.setTLayerSwitchingFlag( i, true );
    }
  }
  else
  {
    m_cPPS.setNumTLayerSwitchingFlags( m_cSPS.getMaxTLayers() - 1 );
    for ( UInt i = 0; i < m_cPPS.getNumTLayerSwitchingFlags(); i++ )
    {
      m_cPPS.setTLayerSwitchingFlag( i, m_abTLayerSwitchingFlag[i] );
    }
  }   
#if G1002_RPS
  Int max_temporal_layers = m_cPPS.getSPS()->getMaxTLayers();
  if(max_temporal_layers > 4)
     m_cPPS.setBitsForTemporalId(3);
  else if(max_temporal_layers > 2)
     m_cPPS.setBitsForTemporalId(2);
  else if(max_temporal_layers > 1)
     m_cPPS.setBitsForTemporalId(1);
  else
     m_cPPS.setBitsForTemporalId(0);

#endif

#if NO_TMVP_MARKING
  m_cPPS.setEnableTMVPFlag( m_bEnableTMVP );
#endif
}


#if G1002_RPS
Void TEncTop::xInitRPS()
{
  TComReferencePictureSet*      pcRPS;
  // In this function 
  // a number of Reference Picture Sets
  // are defined for different coding structures.
  // This is the place 
  // where you need to do changes in
  // order to try different Reference Picture Sets.
  // In a future implementation the 
  // Reference Picture Sets will be 
  // configured directly from the config file.
  // Here we check what BD is appropriate
  
  m_cRPSList.create(getGOPSize()+m_iExtraRPSs);
  for( Int i = 0; i < getGOPSize()+m_iExtraRPSs; i++) 
  {
    GOPEntry pGE = getGOPEntry(i);
    pcRPS = m_cRPSList.getReferencePictureSet(i);
    pcRPS->setNumberOfPictures(pGE.m_iNumRefPics);
#if INTER_RPS_PREDICTION
    pcRPS->setNumRefIdc(pGE.m_iNumRefIdc);
#endif
    int iNumNeg = 0;
    int iNumPos = 0;
    for( Int j = 0; j < pGE.m_iNumRefPics; j++)
    {
      pcRPS->setDeltaPOC(j,pGE.m_aiReferencePics[j]);
      pcRPS->setUsed(j,pGE.m_aiUsedByCurrPic[j]);
      if(pGE.m_aiReferencePics[j]>0)
        iNumPos++;
      else
        iNumNeg++;
    }
    pcRPS->setNumberOfNegativePictures(iNumNeg);
    pcRPS->setNumberOfPositivePictures(iNumPos);
#if INTER_RPS_PREDICTION
    pcRPS->setInterRPSPrediction(pGE.m_bInterRPSPrediction);
    if (pGE.m_bInterRPSPrediction)
    {
      pcRPS->setDeltaRIdxMinus1(pGE.m_iDeltaRIdxMinus1);
      pcRPS->setDeltaRPS(pGE.m_iDeltaRPS);
      pcRPS->setNumRefIdc(pGE.m_iNumRefIdc);
      for (Int j = 0; j < pGE.m_iNumRefIdc; j++ )
      {
        pcRPS->setRefIdc(j, pGE.m_aiRefIdc[j]);
      }
#if WRITE_BACK
      // the folowing code overwrite the deltaPOC and Used by current values read from the config file with the ones
      // computed from the RefIdc.  This is not necessary if both are identical. Currently there is no check to see if they are identical.
      iNumNeg = 0;
      iNumPos = 0;
      TComReferencePictureSet*     pcRPSRef = m_cRPSList.getReferencePictureSet(i-(pGE.m_iDeltaRIdxMinus1+1));
      for (Int j = 0; j < pGE.m_iNumRefIdc; j++ )
      {
        if (pGE.m_aiRefIdc[j])
        {
          int deltaPOC = pGE.m_iDeltaRPS + ((j < pcRPSRef->getNumberOfPictures())? pcRPSRef->getDeltaPOC(j) : 0);
          pcRPS->setDeltaPOC((iNumNeg+iNumPos),deltaPOC);
          pcRPS->setUsed((iNumNeg+iNumPos),pGE.m_aiRefIdc[j]==1?1:0);
          if (deltaPOC<0)
            iNumNeg++;
          else
            iNumPos++;
        }
      }
      pcRPS->setNumberOfNegativePictures(iNumNeg);
      pcRPS->setNumberOfPositivePictures(iNumPos);
      pcRPS->sortDeltaPOC();
#endif
    }
#endif
  }
  
}

Void TEncTop::selectReferencePictureSet(TComSlice* pcSlice, UInt uiPOCCurr, UInt iGOPid,TComList<TComPic*>& rcListPic )
{

   // This is a function that 
   // decides what Reference Picture Set to use 
   // for a specific picture (with POC = uiPOCCurr)

  pcSlice->setRPSidx(iGOPid);

  for(Int extraNum=m_iGOPSize; extraNum<m_iExtraRPSs+m_iGOPSize; extraNum++)
  {    
    if(m_uiIntraPeriod > 0)
    {
      if(uiPOCCurr%m_uiIntraPeriod==m_pcGOPList[extraNum].m_iPOC)
      {
        pcSlice->setRPSidx(extraNum);
      }
    }
    else
    {
      if(uiPOCCurr==m_pcGOPList[extraNum].m_iPOC)
      {
        pcSlice->setRPSidx(extraNum);
      }
    }
  }

  pcSlice->setRPS(getRPSList()->getReferencePictureSet(pcSlice->getRPSidx()));
  pcSlice->getRPS()->setNumberOfPictures(pcSlice->getRPS()->getNumberOfNegativePictures()+pcSlice->getRPS()->getNumberOfPositivePictures());

}
#endif

//! \}
