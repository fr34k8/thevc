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

/** \file     TEncGOP.cpp
    \brief    GOP encoder class
*/

#include <list>
#include <algorithm>

#include "TEncTop.h"
#include "TEncGOP.h"
#include "TEncAnalyze.h"
#include "libmd5/MD5.h"
#include "TLibCommon/SEI.h"
#include "TLibCommon/NAL.h"
#include "NALwrite.h"

#include <math.h>

using namespace std;

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

TEncGOP::TEncGOP()
{
  m_iLastIDR            = 0;
  m_iNumPicCoded        = 0; //Niko
  m_bFirst              = true;
  
  m_pcCfg               = NULL;
  m_pcSliceEncoder      = NULL;
  m_pcListPic           = NULL;
  
  m_pcEntropyCoder      = NULL;
  m_pcCavlcCoder        = NULL;
  m_pcSbacCoder         = NULL;
  m_pcBinCABAC          = NULL;
  
  m_bSeqFirst           = true;
  
  m_bRefreshPending     = 0;
  m_uiPOCCDR            = 0;

  return;
}

TEncGOP::~TEncGOP()
{
}

/** Create list to contain pointers to LCU start addresses of slice.
 * \param iWidth, iHeight are picture width, height. iMaxCUWidth, iMaxCUHeight are LCU width, height.
 */
Void  TEncGOP::create( Int iWidth, Int iHeight, UInt iMaxCUWidth, UInt iMaxCUHeight )
{
  UInt uiWidthInCU       = ( iWidth %iMaxCUWidth  ) ? iWidth /iMaxCUWidth  + 1 : iWidth /iMaxCUWidth;
  UInt uiHeightInCU      = ( iHeight%iMaxCUHeight ) ? iHeight/iMaxCUHeight + 1 : iHeight/iMaxCUHeight;
  UInt uiNumCUsInFrame   = uiWidthInCU * uiHeightInCU;
  m_bLongtermTestPictureHasBeenCoded = 0;
  m_bLongtermTestPictureHasBeenCoded2 = 0;
}

Void  TEncGOP::destroy()
{
}

Void TEncGOP::init ( TEncTop* pcTEncTop )
{
  m_pcEncTop     = pcTEncTop;
  m_pcCfg                = pcTEncTop;
  m_pcSliceEncoder       = pcTEncTop->getSliceEncoder();
  m_pcListPic            = pcTEncTop->getListPic();
  
  m_pcEntropyCoder       = pcTEncTop->getEntropyCoder();
  m_pcCavlcCoder         = pcTEncTop->getCavlcCoder();
  m_pcSbacCoder          = pcTEncTop->getSbacCoder();
  m_pcBinCABAC           = pcTEncTop->getBinCABAC();
  m_pcBitCounter         = pcTEncTop->getBitCounter();
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================
Void TEncGOP::compressGOP( Int iPOCLast, Int iNumPicRcvd, TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsInGOP)
{
  TComPic*        pcPic;
  TComPicYuv*     pcPicYuvRecOut;
  TComSlice*      pcSlice;

  xInitGOP( iPOCLast, iNumPicRcvd, rcListPic, rcListPicYuvRecOut );
  
  m_iNumPicCoded = 0;

  for ( Int iGOPid=0; iGOPid < 1; iGOPid++ )
    {
      UInt uiColDir = 1;
      
      //select uiColDir
      Int iCloseLeft=1, iCloseRight=-1;
      {
        Int iRef = -1;
        if(iRef>iCloseLeft||iCloseLeft==1)
        {
          iCloseLeft=iRef;
        }
      }
      if(iCloseRight>-1)
      {
        iCloseRight=iCloseRight;
      }
      if(iCloseLeft<1) 
      {
        iCloseLeft=iCloseLeft;
        while(iCloseLeft<0)
        {
          iCloseLeft+=1;
        }
      }
      int iLeftQP=0, iRightQP=0;
      {
          iLeftQP= 1;
          iRightQP=1;
      }
      if(iCloseRight>-1&&iRightQP<iLeftQP)
      {
        uiColDir=0;
      }

      /////////////////////////////////////////////////////////////////////////////////////////////////// Initial to start encoding
      UInt uiPOCCurr = iPOCLast -iNumPicRcvd+ 1;
      Int iTimeOffset = 1;
      if(uiPOCCurr>=m_pcCfg->getFrameToBeEncoded())
      {
        continue;
      }
      if(iPOCLast == 0)
      {
        uiPOCCurr=0;
        iTimeOffset = 1;
      }
        
      if(getNalUnitType(uiPOCCurr) == NAL_UNIT_CODED_SLICE_IDR)
      {
        m_iLastIDR = uiPOCCurr;
      }        
      /* start a new access unit: create an entry in the list of output
       * access units */
      accessUnitsInGOP.push_back(AccessUnit());
      AccessUnit& accessUnit = accessUnitsInGOP.back();
      xGetBuffer( rcListPic, rcListPicYuvRecOut, iNumPicRcvd, iTimeOffset, pcPic, pcPicYuvRecOut, uiPOCCurr );
      
      //  Slice data initialization
      pcPic->clearSliceBuffer();
      assert(pcPic->getNumAllocatedSlice() == 1);

      m_pcSliceEncoder->initEncSlice ( pcPic, iPOCLast, uiPOCCurr, iNumPicRcvd, iGOPid, pcSlice, m_pcEncTop->getSPS(), m_pcEncTop->getPPS() );
      pcSlice->setLastIDR(m_iLastIDR);

      m_pcEncTop->getSPS()->setDisInter4x4(m_pcEncTop->getDisInter4x4());

      if(pcSlice->getSliceType()==B_SLICE)
      {
        pcSlice->setSliceType(P_SLICE);
      }
      // Set the nal unit type
      pcSlice->setNalUnitType(getNalUnitType(uiPOCCurr));
      // Do decoding refresh marking if any 
      pcSlice->decodingRefreshMarking(m_uiPOCCDR, m_bRefreshPending, rcListPic);

      if ( !pcSlice->getPPS()->getEnableTMVPFlag() )
      {
        pcSlice->decodingMarkingForNoTMVP( rcListPic, pcSlice->getPOC() );
      }

      m_pcEncTop->selectReferencePictureSet(pcSlice, uiPOCCurr, iGOPid,rcListPic);
      pcSlice->getRPS()->setNumberOfLongtermPictures(0);

      if(pcSlice->checkThatAllRefPicsAreAvailable(rcListPic, pcSlice->getRPS(), false) != 0)
      {
         pcSlice->createExplicitReferencePictureSetFromReference(rcListPic, pcSlice->getRPS());
      }
      pcSlice->applyReferencePictureSet(rcListPic, pcSlice->getRPS());

      TComRefPicListModification* refPicListModification = pcSlice->getRefPicListModification();
      refPicListModification->setRefPicListModificationFlagL0(0);
      refPicListModification->setNumberOfRefPicListModificationsL0(0);
      refPicListModification->setRefPicListModificationFlagL1(0);
      refPicListModification->setNumberOfRefPicListModificationsL1(0);

      pcSlice->setNumRefIdx(REF_PIC_LIST_0,min((UInt)1,pcSlice->getRPS()->getNumberOfPictures()));
      pcSlice->setNumRefIdx(REF_PIC_LIST_1,min((UInt)1,pcSlice->getRPS()->getNumberOfPictures()));

      //  Set reference list
      pcSlice->setRefPicList ( rcListPic );
      
      //  Slice info. refinement
      if ( (pcSlice->getSliceType() == B_SLICE) && (pcSlice->getNumRefIdx(REF_PIC_LIST_1) == 0) )
      {
        pcSlice->setSliceType ( P_SLICE );
      }
      
      if (pcSlice->getSliceType() != B_SLICE || !pcSlice->getSPS()->getUseLComb())
      {
        pcSlice->setNumRefIdx(REF_PIC_LIST_C, 0);
        pcSlice->setRefPicListCombinationFlag(false);
        pcSlice->setRefPicListModificationFlagLC(false);
      }
      else
      {
        pcSlice->setRefPicListCombinationFlag(pcSlice->getSPS()->getUseLComb());
        pcSlice->setRefPicListModificationFlagLC(pcSlice->getSPS()->getLCMod());
        pcSlice->setNumRefIdx(REF_PIC_LIST_C, pcSlice->getNumRefIdx(REF_PIC_LIST_0));
      }
      
      if (pcSlice->getSliceType() == B_SLICE)
      {
        pcSlice->setColDir(uiColDir);
        Bool bLowDelay = true;
        Int  iCurrPOC  = pcSlice->getPOC();
        Int iRefIdx = 0;

        for (iRefIdx = 0; iRefIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_0) && bLowDelay; iRefIdx++)
        {
          if ( pcSlice->getRefPic(REF_PIC_LIST_0, iRefIdx)->getPOC() > iCurrPOC )
          {
            bLowDelay = false;
          }
        }
        for (iRefIdx = 0; iRefIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_1) && bLowDelay; iRefIdx++)
        {
          if ( pcSlice->getRefPic(REF_PIC_LIST_1, iRefIdx)->getPOC() > iCurrPOC )
          {
            bLowDelay = false;
          }
        }

        pcSlice->setCheckLDC(bLowDelay);  
      }
      
      uiColDir = 1-uiColDir;
      
      //-------------------------------------------------------------
      pcSlice->setRefPOCList();
      
      pcSlice->setNoBackPredFlag( false );
      if ( pcSlice->getSliceType() == B_SLICE && !pcSlice->getRefPicListCombinationFlag())
      {
        if ( pcSlice->getNumRefIdx(RefPicList( 0 ) ) == pcSlice->getNumRefIdx(RefPicList( 1 ) ) )
        {
          pcSlice->setNoBackPredFlag( true );
          int i;
          for ( i=0; i < pcSlice->getNumRefIdx(RefPicList( 1 ) ); i++ )
          {
            if ( pcSlice->getRefPOC(RefPicList(1), i) != pcSlice->getRefPOC(RefPicList(0), i) ) 
            {
              pcSlice->setNoBackPredFlag( false );
              break;
            }
          }
        }
      }

      if(pcSlice->getNoBackPredFlag())
      {
        pcSlice->setNumRefIdx(REF_PIC_LIST_C, 0);
      }
      pcSlice->generateCombinedList();
      
      /////////////////////////////////////////////////////////////////////////////////////////////////// Compress a slice
      //  Slice compression
      UInt uiNumSlices = 1;

      UInt uiInternalAddress = pcPic->getNumPartInCU()-4;
      UInt uiExternalAddress = pcPic->getPicSym()->getNumberOfCUsInFrame()-1;
      UInt uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
      UInt uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
      UInt uiWidth = pcSlice->getSPS()->getWidth();
      UInt uiHeight = pcSlice->getSPS()->getHeight();
      while(uiPosX>=uiWidth||uiPosY>=uiHeight) 
      {
        uiInternalAddress--;
        uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
        uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
      }
      uiInternalAddress++;
      if(uiInternalAddress==pcPic->getNumPartInCU()) 
      {
        uiInternalAddress = 0;
        uiExternalAddress++;
      }
      UInt uiRealEndAddress = uiExternalAddress*pcPic->getNumPartInCU()+uiInternalAddress;

      UInt uiNextCUAddr = 0;

      {
        pcSlice->setNextSlice       ( false );
        m_pcSliceEncoder->compressSlice   ( pcPic );
        assert( uiRealEndAddress <= pcSlice->getSliceCurEndCUAddr() );
      }

      /////////////////////////////////////////////////////////////////////////////////////////////////// File writing
      // Set entropy coder
      m_pcEntropyCoder->setEntropyCoder   ( m_pcCavlcCoder, pcSlice );

      /* write various header sets. */
      if ( m_bSeqFirst )
      {
        OutputNALUnit nalu(NAL_UNIT_SPS, NAL_REF_IDC_PRIORITY_HIGHEST);
        m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        m_pcEntropyCoder->encodeSPS(pcSlice->getSPS());
        writeRBSPTrailingBits(nalu.m_Bitstream);
        accessUnit.push_back(new NALUnitEBSP(nalu));

        nalu = NALUnit(NAL_UNIT_PPS, NAL_REF_IDC_PRIORITY_HIGHEST);
        m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        m_pcEntropyCoder->encodePPS(pcSlice->getPPS());
        writeRBSPTrailingBits(nalu.m_Bitstream);
        accessUnit.push_back(new NALUnitEBSP(nalu));

        m_bSeqFirst = false;
      }

      /* use the main bitstream buffer for storing the marshalled picture */
      m_pcEntropyCoder->setBitstream(NULL);

      uiNextCUAddr                 = 0;
      pcSlice = pcPic->getSlice();

      static Int iCurrAPSIdx = 0;
      Int iCodedAPSIdx = 0;
      TComSlice* pcSliceForAPS = NULL;

      bool skippedSlice=false;
      // Iterate over all slices
      {
        {
          pcSlice = pcPic->getSlice();
          pcSlice->setNextSlice       ( true );
        } 

      pcSlice->setRPS(pcPic->getSlice()->getRPS());
      pcSlice->setRPSidx(pcPic->getSlice()->getRPSidx());
        UInt uiDummyBoundingCUAddr;
        m_pcSliceEncoder->xDetermineStartAndBoundingCUAddr(uiDummyBoundingCUAddr,pcPic,true);

        uiInternalAddress = (pcSlice->getSliceCurEndCUAddr()-1) % pcPic->getNumPartInCU();
        uiExternalAddress = (pcSlice->getSliceCurEndCUAddr()-1) / pcPic->getNumPartInCU();
        uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
        uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
        uiWidth = pcSlice->getSPS()->getWidth();
        uiHeight = pcSlice->getSPS()->getHeight();
        while(uiPosX>=uiWidth||uiPosY>=uiHeight)
        {
          uiInternalAddress--;
          uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
          uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
        }
        uiInternalAddress++;
        if(uiInternalAddress==pcPic->getNumPartInCU())
        {
          uiInternalAddress = 0;
          uiExternalAddress++;
        }
        UInt uiEndAddress = uiExternalAddress*pcPic->getNumPartInCU()+uiInternalAddress;
        if(skippedSlice) 
        {
          pcSlice->setNextSlice       ( true );
        }
        skippedSlice=false;

          m_pcEntropyCoder->setEntropyCoder   ( m_pcCavlcCoder, pcSlice );
          m_pcEntropyCoder->resetEntropy      ();
        /* start slice NALunit */
        OutputNALUnit nalu(pcSlice->getNalUnitType(), pcSlice->isReferenced() ? NAL_REF_IDC_PRIORITY_HIGHEST: NAL_REF_IDC_PRIORITY_LOWEST, true);
        m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        pcSlice->setCABACinitIDC(pcSlice->getSliceType());
        m_pcEntropyCoder->encodeSliceHeader(pcSlice);

        // CHECK_ME: for bitstream check only
        nalu.m_Bitstream.write(0, 1); //xWriteFlag( 0 ); // encodeTileMarkerFlag
        nalu.m_Bitstream.write(0, 1); // iTransmitTileLocationInSliceHeader

        // is it needed?
        {
          nalu.m_Bitstream.writeAlignOne(); // Byte-alignment before CABAC data
          m_pcSbacCoder->init( (TEncBinIf*)m_pcBinCABAC );
          m_pcEntropyCoder->setEntropyCoder ( m_pcSbacCoder, pcSlice );
          m_pcEntropyCoder->resetEntropy    ();
        }

        if(pcSlice->isNextSlice())
        {
          // set entropy coder for writing
          m_pcSbacCoder->init( (TEncBinIf*)m_pcBinCABAC );
            m_pcEntropyCoder->setEntropyCoder ( m_pcSbacCoder, pcSlice );
          m_pcEntropyCoder->resetEntropy    ();
          m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        }
        pcSlice->setFinalized(true);

        m_pcSliceEncoder->encodeSlice(pcPic, &nalu.m_Bitstream);

        // CHECK_ME: I think we don't need this bit, because Decoder read TerminatingBit only
        {
          // Construct the final bitstream by flushing and concatenating substreams.
          // The final bitstream is either nalu.m_Bitstream or pcBitstreamRedirect;
            nalu.m_Bitstream.write( 1, 1 ); // stop bit.
        }

        writeRBSPTrailingBits(nalu.m_Bitstream);
        accessUnit.push_back(new NALUnitEBSP(nalu));

        uiNextCUAddr               = pcSlice->getSliceCurEndCUAddr();
      } // end iteration over slices

      pcPic->compressMotion(); 
      
      const char* digestStr = NULL;
      if (m_pcCfg->getPictureDigestEnabled())
      {
        /* calculate MD5sum for entire reconstructed picture */
        SEIpictureDigest sei_recon_picture_digest;
        sei_recon_picture_digest.method = SEIpictureDigest::MD5;
        calcMD5(*pcPic->getPicYuvRec(), sei_recon_picture_digest.digest);
        digestStr = digestToString(sei_recon_picture_digest.digest);

        OutputNALUnit nalu(NAL_UNIT_SEI, NAL_REF_IDC_PRIORITY_LOWEST);

        /* write the SEI messages */
        m_pcEntropyCoder->setEntropyCoder(m_pcCavlcCoder, pcSlice);
        m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        m_pcEntropyCoder->encodeSEI(sei_recon_picture_digest);
        writeRBSPTrailingBits(nalu.m_Bitstream);

        /* insert the SEI message NALUnit before any Slice NALUnits */
        AccessUnit::iterator it = find_if(accessUnit.begin(), accessUnit.end(), mem_fun(&NALUnit::isSlice));
        accessUnit.insert(it, new NALUnitEBSP(nalu));
      }

      xCalculateAddPSNR( pcPic, pcPic->getPicYuvRec(), accessUnit );
      if (digestStr)
        printf(" [MD5:%s]", digestStr);

#if FIXED_ROUNDING_FRAME_MEMORY
      /* TODO: this should happen after copyToPic(pcPicYuvRecOut) */
      pcPic->getPicYuvRec()->xFixedRoundingPic();
#endif
      pcPic->getPicYuvRec()->copyToPic(pcPicYuvRecOut);
      
      pcPic->setReconMark   ( true );

      pcPic->setUsedForTMVP ( true );

      m_bFirst = false;
      m_iNumPicCoded++;

      /* logging: insert a newline at end of picture period */
      printf("\n");
      fflush(stdout);
  }
  
  assert ( m_iNumPicCoded == iNumPicRcvd );
}

Void TEncGOP::printOutSummary(UInt uiNumAllPicCoded)
{
  assert (uiNumAllPicCoded == m_gcAnalyzeAll.getNumPic());
  
    
  //--CFG_KDY
  m_gcAnalyzeAll.setFrmRate( m_pcCfg->getFrameRate() );
  m_gcAnalyzeI.setFrmRate( m_pcCfg->getFrameRate() );
  m_gcAnalyzeP.setFrmRate( m_pcCfg->getFrameRate() );
  m_gcAnalyzeB.setFrmRate( m_pcCfg->getFrameRate() );
  
  //-- all
  printf( "\n\nSUMMARY --------------------------------------------------------\n" );
  m_gcAnalyzeAll.printOut('a');
  
  printf( "\n\nI Slices--------------------------------------------------------\n" );
  m_gcAnalyzeI.printOut('i');
  
  printf( "\n\nP Slices--------------------------------------------------------\n" );
  m_gcAnalyzeP.printOut('p');
  
  printf( "\n\nB Slices--------------------------------------------------------\n" );
  m_gcAnalyzeB.printOut('b');
  
#if _SUMMARY_OUT_
  m_gcAnalyzeAll.printSummaryOut();
#endif
#if _SUMMARY_PIC_
  m_gcAnalyzeI.printSummary('I');
  m_gcAnalyzeP.printSummary('P');
  m_gcAnalyzeB.printSummary('B');
#endif

  printf("\nRVM: %.3lf\n" , xCalculateRVM());
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

Void TEncGOP::xInitGOP( Int iPOCLast, Int iNumPicRcvd, TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRecOut )
{
  assert( iNumPicRcvd > 0 );
  //  Exception for the first frame
  
  return;
}

Void TEncGOP::xGetBuffer( TComList<TComPic*>&       rcListPic,
                         TComList<TComPicYuv*>&    rcListPicYuvRecOut,
                         Int                       iNumPicRcvd,
                         Int                       iTimeOffset,
                         TComPic*&                 rpcPic,
                         TComPicYuv*&              rpcPicYuvRecOut,
                         UInt                      uiPOCCurr )
{
  Int i;
  //  Rec. output
  TComList<TComPicYuv*>::iterator     iterPicYuvRec = rcListPicYuvRecOut.end();
  for ( i = 0; i < iNumPicRcvd - iTimeOffset + 1; i++ )
  {
    iterPicYuvRec--;
  }
  
  rpcPicYuvRecOut = *(iterPicYuvRec);
  
  //  Current pic.
  TComList<TComPic*>::iterator        iterPic       = rcListPic.begin();
  while (iterPic != rcListPic.end())
  {
    rpcPic = *(iterPic);
    if (rpcPic->getPOC() == (Int)uiPOCCurr)
    {
      break;
    }
    iterPic++;
  }
  
  assert (rpcPic->getPOC() == (Int)uiPOCCurr);
  
  return;
}

#if VERBOSE_RATE
static const char* nalUnitTypeToString(NalUnitType type)
{
  switch (type)
  {
  case NAL_UNIT_CODED_SLICE: return "SLICE";
  case NAL_UNIT_CODED_SLICE_CDR: return "CDR";
  case NAL_UNIT_CODED_SLICE_IDR: return "IDR";
  case NAL_UNIT_SEI: return "SEI";
  case NAL_UNIT_SPS: return "SPS";
  case NAL_UNIT_PPS: return "PPS";
  case NAL_UNIT_FILLER_DATA: return "FILLER";
  default: return "UNK";
  }
}
#endif

Void TEncGOP::xCalculateAddPSNR( TComPic* pcPic, TComPicYuv* pcPicD, const AccessUnit& accessUnit )
{
  Int     x, y;
  UInt64 uiSSDY  = 0;
  UInt64 uiSSDU  = 0;
  UInt64 uiSSDV  = 0;
  
  Double  dYPSNR  = 0.0;
  Double  dUPSNR  = 0.0;
  Double  dVPSNR  = 0.0;
  
  //===== calculate PSNR =====
  Pel*  pOrg    = pcPic ->getPicYuvOrg()->getLumaAddr();
  Pel*  pRec    = pcPicD->getLumaAddr();
  Int   iStride = pcPicD->getStride();
  
  Int   iWidth;
  Int   iHeight;
  
  iWidth  = pcPicD->getWidth ();
  iHeight = pcPicD->getHeight();
  
  Int   iSize   = iWidth*iHeight;
  
  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      Int iDiff = (Int)( pOrg[x] - pRec[x] );
      uiSSDY   += iDiff * iDiff;
    }
    pOrg += iStride;
    pRec += iStride;
  }
  
  iHeight >>= 1;
  iWidth  >>= 1;
  iStride >>= 1;
  pOrg  = pcPic ->getPicYuvOrg()->getCbAddr();
  pRec  = pcPicD->getCbAddr();
  
  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      Int iDiff = (Int)( pOrg[x] - pRec[x] );
      uiSSDU   += iDiff * iDiff;
    }
    pOrg += iStride;
    pRec += iStride;
  }
  
  pOrg  = pcPic ->getPicYuvOrg()->getCrAddr();
  pRec  = pcPicD->getCrAddr();
  
  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      Int iDiff = (Int)( pOrg[x] - pRec[x] );
      uiSSDV   += iDiff * iDiff;
    }
    pOrg += iStride;
    pRec += iStride;
  }
  
  unsigned int maxval = 255;
  Double fRefValueY = (double) maxval * maxval * iSize;
  Double fRefValueC = fRefValueY / 4.0;
  dYPSNR            = ( uiSSDY ? 10.0 * log10( fRefValueY / (Double)uiSSDY ) : 99.99 );
  dUPSNR            = ( uiSSDU ? 10.0 * log10( fRefValueC / (Double)uiSSDU ) : 99.99 );
  dVPSNR            = ( uiSSDV ? 10.0 * log10( fRefValueC / (Double)uiSSDV ) : 99.99 );

  /* calculate the size of the access unit, excluding:
   *  - any AnnexB contributions (start_code_prefix, zero_byte, etc.,)
   *  - SEI NAL units
   */
  unsigned numRBSPBytes = 0;
  for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
  {
    unsigned numRBSPBytes_nal = unsigned((*it)->m_nalUnitData.str().size());
#if VERBOSE_RATE
    printf("*** %6s numBytesInNALunit: %u\n", nalUnitTypeToString((*it)->m_UnitType), numRBSPBytes_nal);
#endif
    if ((*it)->m_UnitType != NAL_UNIT_SEI)
      numRBSPBytes += numRBSPBytes_nal;
  }

  unsigned uibits = numRBSPBytes * 8;
  m_vRVM_RP.push_back( uibits );

  //===== add PSNR =====
  m_gcAnalyzeAll.addResult (dYPSNR, dUPSNR, dVPSNR, (Double)uibits);
  TComSlice*  pcSlice = pcPic->getSlice();
  if (pcSlice->isIntra())
  {
    m_gcAnalyzeI.addResult (dYPSNR, dUPSNR, dVPSNR, (Double)uibits);
  }
  if (pcSlice->isInterP())
  {
    m_gcAnalyzeP.addResult (dYPSNR, dUPSNR, dVPSNR, (Double)uibits);
  }
  if (pcSlice->isInterB())
  {
    m_gcAnalyzeB.addResult (dYPSNR, dUPSNR, dVPSNR, (Double)uibits);
  }

  Char c = (pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B');
  if (!pcSlice->isReferenced()) c += 32;

  printf("POC %4d ( %c-SLICE, QP %d ) %10d bits",
         pcSlice->getPOC(),
         c,
         pcSlice->getSliceQp(),
         uibits );

  printf(" [Y %6.4lf dB    U %6.4lf dB    V %6.4lf dB]", dYPSNR, dUPSNR, dVPSNR );
  
  for (Int iRefList = 0; iRefList < 2; iRefList++)
  {
    printf(" [L%d ", iRefList);
    for (Int iRefIndex = 0; iRefIndex < pcSlice->getNumRefIdx(RefPicList(iRefList)); iRefIndex++)
    {
      printf ("%d ", pcSlice->getRefPOC(RefPicList(iRefList), iRefIndex)-pcSlice->getLastIDR());
    }
    printf("]");
  }
  if(pcSlice->getNumRefIdx(REF_PIC_LIST_C)>0 && !pcSlice->getNoBackPredFlag())
  {
    printf(" [LC ");
    for (Int iRefIndex = 0; iRefIndex < pcSlice->getNumRefIdx(REF_PIC_LIST_C); iRefIndex++)
    {
      printf ("%d ", pcSlice->getRefPOC((RefPicList)pcSlice->getListIdFromIdxOfLC(iRefIndex), pcSlice->getRefIdxFromIdxOfLC(iRefIndex))-pcSlice->getLastIDR());
    }
    printf("]");
  }
}

/** Function for deciding the nal_unit_type.
 * \param uiPOCCurr POC of the current picture
 * \returns the nal_unit type of the picture
 * This function checks the configuration and returns the appropriate nal_unit_type for the picture.
 */
NalUnitType TEncGOP::getNalUnitType(UInt uiPOCCurr)
{
  if (uiPOCCurr == 0)
  {
    return NAL_UNIT_CODED_SLICE_IDR;
  }
  if (uiPOCCurr % m_pcCfg->getIntraPeriod() == 0)
  {
    if (m_pcCfg->getDecodingRefreshType() == 1)
    {
      return NAL_UNIT_CODED_SLICE_CDR;
    }
    else if (m_pcCfg->getDecodingRefreshType() == 2)
    {
      return NAL_UNIT_CODED_SLICE_IDR;
    }
  }
  return NAL_UNIT_CODED_SLICE;
}

Double TEncGOP::xCalculateRVM()
{
  Double dRVM = 0;
  
  if( m_pcCfg->getIntraPeriod() != 1 && m_pcCfg->getFrameToBeEncoded() > RVM_VCEGAM10_M * 2 )
  {
    // calculate RVM only for lowdelay configurations
    std::vector<Double> vRL , vB;
    size_t N = m_vRVM_RP.size();
    vRL.resize( N );
    vB.resize( N );
    
    Int i;
    Double dRavg = 0 , dBavg = 0;
    vB[RVM_VCEGAM10_M] = 0;
    for( i = RVM_VCEGAM10_M + 1 ; i < N - RVM_VCEGAM10_M + 1 ; i++ )
    {
      vRL[i] = 0;
      for( Int j = i - RVM_VCEGAM10_M ; j <= i + RVM_VCEGAM10_M - 1 ; j++ )
        vRL[i] += m_vRVM_RP[j];
      vRL[i] /= ( 2 * RVM_VCEGAM10_M );
      vB[i] = vB[i-1] + m_vRVM_RP[i] - vRL[i];
      dRavg += m_vRVM_RP[i];
      dBavg += vB[i];
    }
    
    dRavg /= ( N - 2 * RVM_VCEGAM10_M );
    dBavg /= ( N - 2 * RVM_VCEGAM10_M );
    
    double dSigamB = 0;
    for( i = RVM_VCEGAM10_M + 1 ; i < N - RVM_VCEGAM10_M + 1 ; i++ )
    {
      Double tmp = vB[i] - dBavg;
      dSigamB += tmp * tmp;
    }
    dSigamB = sqrt( dSigamB / ( N - 2 * RVM_VCEGAM10_M ) );
    
    double f = sqrt( 12.0 * ( RVM_VCEGAM10_M - 1 ) / ( RVM_VCEGAM10_M + 1 ) );
    
    dRVM = dSigamB / dRavg * f;
  }
  
  return( dRVM );
}


//! \}
