// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util.h>

#include <consensus/consensus.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
	
	// Genesis block
    if (pindexLast == nullptr)
        return nProofOfWorkLimit;
    if (pindexLast->nHeight+1 == params.BCDHeight)
    	return nProofOfWorkLimit;

    if (pindexLast->nHeight+1 == params.BCDHeight+1)
    	return UintToArith256(params.BCDBeginPowLimit).GetCompact();

    int height, interval;
    if (pindexLast->nHeight+1 > params.BCDHeight) {
    	height = pindexLast->nHeight+1 - params.BCDHeight;
    	interval = 72;
    }else{
    	height = pindexLast->nHeight+1;
    	interval = params.DifficultyAdjustmentInterval();
    }

    // Only change once per difficulty adjustment interval
    if (height % interval != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (interval-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;
		
    int limit, powTargetTimespan;
    if (pindexLast->nHeight+1 > params.BCDHeight) {
    	powTargetTimespan = 72 * params.nPowTargetSpacing;
    	limit = 2;
    }else{
    	powTargetTimespan = params.nPowTargetTimespan;
    	limit = 4;
    }
    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
	int64_t realActualTimespan = nActualTimespan;
    if (nActualTimespan < powTargetTimespan/limit)
        nActualTimespan = powTargetTimespan/limit;
    if (nActualTimespan > powTargetTimespan*limit)
        nActualTimespan = powTargetTimespan*limit;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
	arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
	bnOld = bnNew;
    bnNew *= nActualTimespan;
    bnNew /= powTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;
		
    LogPrintf("GetNextWorkRequired RETARGET at nHeight = %d\n", pindexLast->nHeight+1);
    LogPrintf("params.nPowTargetTimespan = %d    nActualTimespan = %d    realActualTimespan = %d\n", powTargetTimespan, nActualTimespan, realActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, bnOld.ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());
    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
