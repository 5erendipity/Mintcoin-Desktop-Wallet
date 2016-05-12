// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_CHECKPOINT_H
#define BITCOIN_CHECKPOINT_H

#include "uint256.h"

#include <map>
#include "net.h"
#include "util.h"

#define CHECKPOINT_MAX_SPAN (60 * 60 * 4) // max 4 hours before latest block

#ifdef WIN32
#undef STRICT
#undef PERMISSIVE
#undef ADVISORY
#endif

class CBlockIndex;
class CSyncCheckpoint;
class CValidationState;

/** Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{
typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    const MapCheckpoints *mapCheckpoints;
    int64_t nTimeLastCheckpoint;
    int64_t nTransactionsLastCheckpoint;
    double fTransactionsPerDay;
};

// Returns true if block passes checkpoint checks
bool CheckHardened(int nHeight, const uint256& hash);

// Return conservative estimate of total number of blocks, 0 if unknown
int GetTotalBlocksEstimate();

// Returns last CBlockIndex* in mapBlockIndex that is a checkpoint
CBlockIndex* GetLastCheckpoint();

extern uint256 hashSyncCheckpoint;
extern CSyncCheckpoint checkpointMessage;
extern uint256 hashInvalidCheckpoint;
extern CCriticalSection cs_hashSyncCheckpoint;
extern bool fEnabled;

CBlockIndex* GetLastSyncCheckpoint();
bool WriteSyncCheckpoint(const uint256& hashCheckpoint);
bool AcceptPendingSyncCheckpoint(CValidationState &state);
uint256 AutoSelectSyncCheckpoint();
bool CheckSync(const uint256& hashBlock, const CBlockIndex* pindexPrev);
bool WantedByPendingSyncCheckpoint(uint256 hashBlock);
bool ResetSyncCheckpoint(CValidationState &state);
void AskForPendingSyncCheckpoint(CNode* pfrom);
bool SetCheckpointPrivKey(std::string strPrivKey);
bool SendSyncCheckpoint(uint256 hashCheckpoint);
bool IsMatureSyncCheckpoint();
bool IsSyncCheckpointTooOld(unsigned int nSeconds);
bool CheckMasterPubKey(bool reindex);
double GuessVerificationProgress(CBlockIndex* pindex, bool fSigchecks = true);
} //namespace Checkpoints

// ppcoin: synchronized checkpoint
class CUnsignedSyncCheckpoint
{
public:
    int nVersion;
    uint256 hashCheckpoint;      // checkpoint block

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashCheckpoint);
    }

    void SetNull()
    {
        nVersion = 1;
        hashCheckpoint = 0;
    }

    std::string ToString() const
    {
        return strprintf(
                "CSyncCheckpoint(\n"
                "    nVersion       = %d\n"
                "    hashCheckpoint = %s\n"
                ")\n",
            nVersion,
            hashCheckpoint.ToString().c_str());
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};

class CSyncCheckpoint : public CUnsignedSyncCheckpoint
{
public:
    static const std::string strMasterPubKey;
    static std::string strMasterPrivKey;

    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CSyncCheckpoint()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vchMsg);
        READWRITE(vchSig);
    }

    void SetNull()
    {
        CUnsignedSyncCheckpoint::SetNull();
        vchMsg.clear();
        vchSig.clear();
    }

    bool IsNull() const
    {
        return (hashCheckpoint == 0);
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    bool RelayTo(CNode* pnode) const
    {
        // returns true if wasn't already sent
        if (pnode->hashCheckpointKnown != hashCheckpoint)
        {
            pnode->hashCheckpointKnown = hashCheckpoint;
            pnode->PushMessage("checkpoint", *this);
            return true;
        }
        return false;
    }

    bool CheckSignature();
    bool ProcessSyncCheckpoint(CNode* pfrom);
};

#endif // BITCOIN_CHECKPOINT_H