#include <time.h>
#include <vector>
#include <list>
#include "hashtree.h"

namespace swift {
    class SwarmManager;

    class SwarmData {
    protected:
        int id_;
        Sha1Hash rootHash_;
        bool active_;
        tint latestUse_;
        bool toBeRemoved_;
        bool stateToBeRemoved_;
        bool contentToBeRemoved_;
        FileTransfer* ft_;
        std::string filename_;
        Address tracker_;
        bool checkHashes_;
        uint32_t chunkSize_;
        bool zerostate_;
        double cachedMaxSpeeds_[2];
        bool cachedStorageReady_;
        std::list<std::string> cachedStorageFilenames_;
        uint64_t cachedSize_;
        bool cachedIsComplete_;
        uint64_t cachedComplete_;
        std::string cachedOSPathName_;
        std::list< std::pair<ProgressCallback, uint8_t> > cachedCallbacks_;
        uint64_t cachedSeqComplete_; // Only for offset = 0
        bool cached_;
    public:
        SwarmData( const std::string filename, const Sha1Hash& rootHash, const Address& tracker, bool check_hashes, uint32_t chunk_size, bool zerostate );
        SwarmData( const SwarmData& sd );

        ~SwarmData();

        bool Touch();
        bool IsActive();
        const Sha1Hash& RootHash();
        int Id();
        bool ToBeRemoved();
        FileTransfer* GetTransfer(bool touch = true);
        std::string& Filename();
        Address& Tracker();
        uint32_t ChunkSize();
        bool IsZeroState();

        // Find out cached values of non-active swarms
        uint64_t Size();
        bool IsComplete();
        uint64_t Complete();
        uint64_t SeqComplete(int64_t offset = 0);
        std::string OSPathName();

        void SetMaxSpeed(data_direction_t ddir, double speed);
        void AddProgressCallback(ProgressCallback cb, uint8_t agg);
        void RemoveProgressCallback(ProgressCallback cb);

        friend class SwarmManager;
    };

    class SwarmManager {
    protected:
        SwarmManager();
        ~SwarmManager();


        // Singleton
        static SwarmManager instance_;

        // Structures to keep track of all the swarms known to this manager
        // That's two lists of swarms, indeed.
        // The first allows very fast lookups
        // - hash table (bucket is rootHash.bits[0]&63) containing lists ordered by rootHash (binary search possible)
        // The second allows very fast access by numeric identifier (used in toplevel API)
        // - just a vector with a new element for each new one, and a list of available indices
        std::vector< std::vector<SwarmData*> > knownSwarms_;
        std::vector<SwarmData*> swarmList_;
        struct UnusedIndex {
            int index;
            tint since;
        };
        std::list<struct UnusedIndex> unusedIndices_;

        // Structures and functions for deferred removal of active swarms
        struct event* eventCheckToBeRemoved_;
        static void CheckSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg);
        void CheckSwarmsToBeRemoved();

        // Looking up swarms by rootHash, internal functions
        int GetSwarmLocation( const std::vector<SwarmData*>& list, const Sha1Hash& rootHash );
        SwarmData* GetSwarmData( const Sha1Hash& rootHash );

        // Internal activation method
        SwarmData* ActivateSwarm( SwarmData* swarm );
        void BuildSwarm( SwarmData* swarm );

        // Internal method to find the oldest swarm and deactivate it
        bool DeactivateSwarm();
        void DeactivateSwarm( SwarmData* swarm, int activeLoc );

        // Structures to keep track of active swarms
        int maxActiveSwarms_;
        int activeSwarmCount_;
        std::vector<SwarmData*> activeSwarms_;

#if SWARMMANAGER_ASSERT_INVARIANTS
        void invariant();
#endif
    public:
        // Singleton
        static SwarmManager& GetManager();

        // Add and remove swarms
        SwarmData* AddSwarm( const std::string filename, const Sha1Hash& hash, const Address& tracker, bool check_hashes, uint32_t chunk_size, bool zerostate );
        SwarmData* AddSwarm( const SwarmData& swarm );
        void RemoveSwarm( const Sha1Hash& rootHash, bool removeState = false, bool removeContent = false );

        // Find a swam, either by id or root hash
        SwarmData* FindSwarm( int id );
        SwarmData* FindSwarm( const Sha1Hash& rootHash );

        // Activate a swarm, so it can be used (not active swarms can't be read from/written to)
        SwarmData* ActivateSwarm( const Sha1Hash& rootHash );
        void DeactivateSwarm( const Sha1Hash& rootHash );

        // Manage maximum of active swarms
        int GetMaximumActiveSwarms();
        void SetMaximumActiveSwarms( int newMaxActiveSwarms );

        class Iterator : public std::iterator<std::input_iterator_tag, SwarmData*> {
        protected:
            int transfer_;
        public:
            Iterator();
            Iterator(int transfer);
            Iterator(const Iterator& other);
            Iterator& operator++();
            Iterator operator++(int);
            bool operator==(const Iterator& other);
            bool operator!=(const Iterator& other);
            SwarmData* operator*();
        };
        friend class Iterator;
        Iterator begin();
        Iterator end();
    };
}
