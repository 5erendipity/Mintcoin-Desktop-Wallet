
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits);
/** Calculate the minimum amount of work a received block needs, without knowing its direct parent */
bool CheckMinWork(unsigned int nBits, unsigned int nBase, int64_t nTime);
/** Calculate the minimum amount of stake a received block needs, without knowing its direct parent */
bool CheckMinStake(unsigned int nBits, unsigned int nBase, int64_t nTime, unsigned int nBlockTime);
uint256 GetBlockProof(const CBlockIndex& block);

const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake);

#endif // BITCOIN_POW_H