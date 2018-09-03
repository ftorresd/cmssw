
import FWCore.ParameterSet.Config as cms

rpcRecHits = cms.EDProducer("RPCRecHitProducer",
    recAlgoConfig = cms.PSet(

    ),
    recAlgo = cms.string('RPCRecHitStandardAlgo'),
    rpcDigiLabel = cms.InputTag('muonRPCDigis'),
    rpcTwinMuxDigiLabel = cms.InputTag('muonRPCDigis'),
    rpcOMTFDigiLabel = cms.InputTag('muonRPCDigis'),
    rpcCPPFDigiLabel = cms.InputTag('muonRPCDigis'),
    isLegacy = cms.bool(True),
    maskSource = cms.string('File'),
    maskvecfile = cms.FileInPath('RecoLocalMuon/RPCRecHit/data/RPCMaskVec.dat'),
    deadSource = cms.string('File'),
    deadvecfile = cms.FileInPath('RecoLocalMuon/RPCRecHit/data/RPCDeadVec.dat')
)

#disabling DIGI2RAW,RAW2DIGI chain for Phase2
from Configuration.Eras.Modifier_phase2_muon_cff import phase2_muon
phase2_muon.toModify(rpcRecHits, rpcDigiLabel = cms.InputTag('simMuonRPCDigis'))
from Configuration.ProcessModifiers.premix_stage2_cff import premix_stage2
(premix_stage2 & phase2_muon).toModify(rpcRecHits,
    rpcDigiLabel = "mixData"
)
