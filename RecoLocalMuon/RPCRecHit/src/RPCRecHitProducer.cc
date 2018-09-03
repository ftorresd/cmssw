/** \file
 *
 *  \author M. Maggi -- INFN Bari
*/

#include "RPCRecHitProducer.h"

#include "FWCore/Framework/interface/ESHandle.h"

#include "Geometry/RPCGeometry/interface/RPCRoll.h"
#include "Geometry/RPCGeometry/interface/RPCGeometry.h"
#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "DataFormats/MuonDetId/interface/RPCDetId.h"
#include "DataFormats/RPCRecHit/interface/RPCRecHit.h"

#include "RecoLocalMuon/RPCRecHit/interface/RPCRecHitAlgoFactory.h"
#include "DataFormats/RPCRecHit/interface/RPCRecHitCollection.h"

#include "CondFormats/DataRecord/interface/RPCMaskedStripsRcd.h"
#include "CondFormats/DataRecord/interface/RPCDeadStripsRcd.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <string>
#include <fstream>

using namespace edm;
using namespace std;

RPCRecHitProducer::RPCRecHitProducer(const ParameterSet& config):
  theRPCDigiLabel(consumes<RPCDigiCollection>(config.getParameter<InputTag>("rpcDigiLabel"))),
  theRPCTwinMuxDigiLabel(consumes<RPCDigiCollection>(config.getParameter<InputTag>("rpcTwinMuxDigiLabel"))),
  theRPCOMTFDigiLabel(consumes<RPCDigiCollection>(config.getParameter<InputTag>("rpcOMTFDigiLabel"))),
  theRPCCPPFDigiLabel(consumes<RPCDigiCollection>(config.getParameter<InputTag>("rpcCPPFDigiLabel"))),
  maskSource_(MaskSource::EventSetup), deadSource_(MaskSource::EventSetup),
  isLegacy(config.getParameter<bool>("isLegacy"))
{
  // Set verbose output
  produces<RPCRecHitCollection>();

  // Get the concrete reconstruction algo from the factory
  const string theAlgoName = config.getParameter<string>("recAlgo");
  theAlgo.reset(RPCRecHitAlgoFactory::get()->create(theAlgoName,
                config.getParameter<ParameterSet>("recAlgoConfig")));

  // Get masked- and dead-strip information
  theRPCMaskedStripsObj = std::make_unique<RPCMaskedStrips>();
  theRPCDeadStripsObj = std::make_unique<RPCDeadStrips>();

  const string maskSource = config.getParameter<std::string>("maskSource");
  if (maskSource == "File") {
    maskSource_ = MaskSource::File;
    edm::FileInPath fp = config.getParameter<edm::FileInPath>("maskvecfile");
    std::ifstream inputFile(fp.fullPath().c_str(), std::ios::in);
    if ( !inputFile ) {
      std::cerr << "Masked Strips File cannot not be opened" << std::endl;
      exit(1);
    }
    while ( inputFile.good() ) {
      RPCMaskedStrips::MaskItem Item;
      inputFile >> Item.rawId >> Item.strip;
      if ( inputFile.good() ) MaskVec.push_back(Item);
    }
    inputFile.close();
  }

  const string deadSource = config.getParameter<std::string>("deadSource");
  if (deadSource == "File") {
    deadSource_ = MaskSource::File;
    edm::FileInPath fp = config.getParameter<edm::FileInPath>("deadvecfile");
    std::ifstream inputFile(fp.fullPath().c_str(), std::ios::in);
    if ( !inputFile ) {
      std::cerr << "Dead Strips File cannot not be opened" << std::endl;
      exit(1);
    }
    while ( inputFile.good() ) {
      RPCDeadStrips::DeadItem Item;
      inputFile >> Item.rawId >> Item.strip;
      if ( inputFile.good() ) DeadVec.push_back(Item);
    }
    inputFile.close();
  }
}


void RPCRecHitProducer::beginRun(const edm::Run& r, const edm::EventSetup& setup){
  // Getting the masked-strip information
  if ( maskSource_ == MaskSource::EventSetup ) {
    edm::ESHandle<RPCMaskedStrips> readoutMaskedStrips;
    setup.get<RPCMaskedStripsRcd>().get(readoutMaskedStrips);
    const RPCMaskedStrips* tmp_obj = readoutMaskedStrips.product();
    theRPCMaskedStripsObj->MaskVec = tmp_obj->MaskVec;
    delete tmp_obj;
  }
  else if ( maskSource_ == MaskSource::File ) {
    std::vector<RPCMaskedStrips::MaskItem>::iterator posVec;
    for ( posVec = MaskVec.begin(); posVec != MaskVec.end(); ++posVec ) {
      RPCMaskedStrips::MaskItem Item; 
      Item.rawId = (*posVec).rawId;
      Item.strip = (*posVec).strip;
      theRPCMaskedStripsObj->MaskVec.push_back(Item);
    }
  }

  // Getting the dead-strip information
  if ( deadSource_ == MaskSource::EventSetup ) {
    edm::ESHandle<RPCDeadStrips> readoutDeadStrips;
    setup.get<RPCDeadStripsRcd>().get(readoutDeadStrips);
    const RPCDeadStrips* tmp_obj = readoutDeadStrips.product();
    theRPCDeadStripsObj->DeadVec = tmp_obj->DeadVec;
    delete tmp_obj;
  }
  else if ( deadSource_ == MaskSource::File ) {
    std::vector<RPCDeadStrips::DeadItem>::iterator posVec;
    for ( posVec = DeadVec.begin(); posVec != DeadVec.end(); ++posVec ) {
      RPCDeadStrips::DeadItem Item;
      Item.rawId = (*posVec).rawId;
      Item.strip = (*posVec).strip;
      theRPCDeadStripsObj->DeadVec.push_back(Item);
    }
  }

}

void RPCRecHitProducer::produce(Event& event, const EventSetup& setup) {
  // Get the RPC Geometry
  ESHandle<RPCGeometry> rpcGeom;
  setup.get<MuonGeometryRecord>().get(rpcGeom);

  // Get the digis from the event
  // Legacy
  Handle<RPCDigiCollection> Legacy_digis; 
  // Handle<RPCDigiCollection> digis; 
  event.getByToken(theRPCDigiLabel,Legacy_digis);
  // event.getByToken(theRPCDigiLabel,digis);
  // TwinMux
  Handle<RPCDigiCollection> TwinMux_digis; 
  event.getByToken(theRPCTwinMuxDigiLabel,TwinMux_digis);
  // OMTF
  Handle<RPCDigiCollection> OMTF_digis; 
  event.getByToken(theRPCOMTFDigiLabel,OMTF_digis);
  // CPFF
  Handle<RPCDigiCollection> CPPF_digis; 
  event.getByToken(theRPCCPPFDigiLabel,CPPF_digis);

  // // set legacy as default
  // auto digis = &Legacy_digis;
  // if (!isLegacy) {
  //   // do something
  // }

  // Pass the EventSetup to the algo
  theAlgo->setES(setup);

  // Create the pointer to the collection which will store the rechits
  auto recHitCollection = std::make_unique<RPCRecHitCollection>();

  // Iterate through all digi collections ordered by LayerId   
  // for ( auto rpcdgIt = digis->begin(); rpcdgIt != digis->end(); ++rpcdgIt ) {
  //   // The layerId
  //   const RPCDetId& rpcId = (*rpcdgIt).first;

  //   // Get the GeomDet from the setup
  //   const RPCRoll* roll = rpcGeom->roll(rpcId);
  //   if (roll == nullptr){
  //     edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
  //     continue;
  //   }

  //   // Get the iterators over the digis associated with this LayerId
  //   const RPCDigiCollection::Range& range = (*rpcdgIt).second;

  //   // Getting the roll mask, that includes dead strips, for the given RPCDet
  //   RollMask mask;
  //   const int rawId = rpcId.rawId();
  //   for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
  //     if ( tomask.rawId == rawId ) {
  //       const int bit = tomask.strip;
  //       mask.set(bit-1);
  //     }
  //   }

  //   for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
  //     if ( tomask.rawId == rawId ) {
  //       const int bit = tomask.strip;
  //       mask.set(bit-1);
  //     }
  //   }

  //   // Call the reconstruction algorithm    
  //   OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
    
  //   if(!recHits.empty()) //FIXME: is it really needed?
  //     recHitCollection->put(rpcId, recHits.begin(), recHits.end());
  // }

  if (isLegacy) {
    //////////////////////////////
    // legacy  
    for ( auto rpcdgIt = Legacy_digis->begin(); rpcdgIt != Legacy_digis->end(); ++rpcdgIt ) {
      // The layerId
      const RPCDetId& rpcId = (*rpcdgIt).first;

      // Get the GeomDet from the setup
      const RPCRoll* roll = rpcGeom->roll(rpcId);
      if (roll == nullptr){
        edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
        continue;
      }

      // Get the iterators over the digis associated with this LayerId
      const RPCDigiCollection::Range& range = (*rpcdgIt).second;

      // Getting the roll mask, that includes dead strips, for the given RPCDet
      RollMask mask;
      const int rawId = rpcId.rawId();
      for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      // Call the reconstruction algorithm    
      OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
      
      if(!recHits.empty()) //FIXME: is it really needed?
        recHitCollection->put(rpcId, recHits.begin(), recHits.end());
    }
  } else {
    //////////////////////////////
    // TwinMux 
    for ( auto rpcdgIt = TwinMux_digis->begin(); rpcdgIt != TwinMux_digis->end(); ++rpcdgIt ) {
      // The layerId
      const RPCDetId& rpcId = (*rpcdgIt).first;

      // Get the GeomDet from the setup
      const RPCRoll* roll = rpcGeom->roll(rpcId);
      if (roll == nullptr){
        edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
        continue;
      }

      // Get the iterators over the digis associated with this LayerId
      const RPCDigiCollection::Range& range = (*rpcdgIt).second;

      // Getting the roll mask, that includes dead strips, for the given RPCDet
      RollMask mask;
      const int rawId = rpcId.rawId();
      for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      // Call the reconstruction algorithm    
      OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
      
      if(!recHits.empty()) //FIXME: is it really needed?
        recHitCollection->put(rpcId, recHits.begin(), recHits.end());
    }

    //////////////////////////////
    // OMTF 
    for ( auto rpcdgIt = OMTF_digis->begin(); rpcdgIt != OMTF_digis->end(); ++rpcdgIt ) {
      // The layerId
      const RPCDetId& rpcId = (*rpcdgIt).first;
      
      // clear overlaps
      if ( !((rpcId.region() == -1 || rpcId.region() == 1) &&  (rpcId.ring() == 3) && (rpcId.station() == 1 || rpcId.station() == 2)) ) 
        continue; // accpets only rings: RE-2_R3 ; RE-1_R3 ; RE+1_R3 ; RE+2_R3 ; 
      // if ( rpcId.region() == 0 && (rpcId.ring() == -2 || rpcId.ring() == 2) ) // W_-2 ; W_+2
      //   continue;
      // if (rpcId.region() == -1 || rpcId.region() == 1) {
      //   if (rpcId.ring() == 2 && (rpcId.station() == -1 || rpcId.station() == -1) ) // RE+1_R2 ; RE-1_R2 
      //     continue;
      //   if (rpcId.ring() == 3 && (rpcId.station() == -3 || rpcId.station() == -3) ) // RE+3_R3 ; RE-3_R3
      //     continue;
      // }

      // Get the GeomDet from the setup
      const RPCRoll* roll = rpcGeom->roll(rpcId);
      if (roll == nullptr){
        edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
        continue;
      }

      // Get the iterators over the digis associated with this LayerId
      const RPCDigiCollection::Range& range = (*rpcdgIt).second;

      // Getting the roll mask, that includes dead strips, for the given RPCDet
      RollMask mask;
      const int rawId = rpcId.rawId();
      for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      // Call the reconstruction algorithm    
      OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
      
      if(!recHits.empty()) //FIXME: is it really needed?
        recHitCollection->put(rpcId, recHits.begin(), recHits.end());
    }   

    ////////////////////////////// 
    // CPPF
    for ( auto rpcdgIt = CPPF_digis->begin(); rpcdgIt != CPPF_digis->end(); ++rpcdgIt ) {
      // The layerId
      const RPCDetId& rpcId = (*rpcdgIt).first;

      // Get the GeomDet from the setup
      const RPCRoll* roll = rpcGeom->roll(rpcId);
      if (roll == nullptr){
        edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
        continue;
      }

      // Get the iterators over the digis associated with this LayerId
      const RPCDigiCollection::Range& range = (*rpcdgIt).second;

      // Getting the roll mask, that includes dead strips, for the given RPCDet
      RollMask mask;
      const int rawId = rpcId.rawId();
      for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
        if ( tomask.rawId == rawId ) {
          const int bit = tomask.strip;
          mask.set(bit-1);
        }
      }

      // Call the reconstruction algorithm    
      OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
      
      if(!recHits.empty()) //FIXME: is it really needed?
        recHitCollection->put(rpcId, recHits.begin(), recHits.end());
    } 
  }
  

  event.put(std::move(recHitCollection));

}

// // The method which produces the rechits for each digi collection
// void RPCRecHitProducer::fillRPCRecHits(RPCDigiCollection * digis, RPCGeometry * rpcGeom, RPCRecHitCollection * recHitCollection) {
//   for ( auto rpcdgIt = digis->begin(); rpcdgIt != digis->end(); ++rpcdgIt ) {
//     // The layerId
//     const RPCDetId& rpcId = (*rpcdgIt).first;

//     // Get the GeomDet from the setup
//     const RPCRoll* roll = rpcGeom->roll(rpcId);
//     if (roll == nullptr){
//       edm::LogError("BadDigiInput")<<"Failed to find RPCRoll for ID "<<rpcId;
//       continue;
//     }

//     // Get the iterators over the digis associated with this LayerId
//     const RPCDigiCollection::Range& range = (*rpcdgIt).second;

//     // Getting the roll mask, that includes dead strips, for the given RPCDet
//     RollMask mask;
//     const int rawId = rpcId.rawId();
//     for ( const auto& tomask : theRPCMaskedStripsObj->MaskVec ) {
//       if ( tomask.rawId == rawId ) {
//         const int bit = tomask.strip;
//         mask.set(bit-1);
//       }
//     }

//     for ( const auto& tomask : theRPCDeadStripsObj->DeadVec ) {
//       if ( tomask.rawId == rawId ) {
//         const int bit = tomask.strip;
//         mask.set(bit-1);
//       }
//     }

//     // Call the reconstruction algorithm    
//     OwnVector<RPCRecHit> recHits = theAlgo->reconstruct(*roll, rpcId, range, mask);
    
//     if(!recHits.empty()) //FIXME: is it really needed?
//       recHitCollection->put(rpcId, recHits.begin(), recHits.end());
//   }
// }

