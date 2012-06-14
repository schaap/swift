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
    public:
        SwarmData( const Sha1Hash& rootHash );
        SwarmData( const SwarmData& sd );

        bool touch();
        bool isActive();
        const Sha1Hash& rootHash();
        int id();
        bool toBeRemoved();

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
        std::list<int> swarmsToBeRemoved_;
        struct event* eventCheckToBeRemoved_;
        static void CheckSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg);
        void CheckSwarmsToBeRemoved();

        // Looking up swarms by rootHash, internal functions
        int GetSwarmLocation( const std::vector<SwarmData*>& list, const Sha1Hash& rootHash );
        SwarmData* GetSwarmData( const Sha1Hash& rootHash );

        // Internal method to find the oldest swarm and deactivate it
        bool DeactivateSwarm();

        // Structures to keep track of active swarms
        int maxActiveSwarms_;
        int activeSwarmCount_;
        std::vector<SwarmData*> activeSwarms_;
    public:
        // Singleton
        static SwarmManager& GetManager();

        // Add and remove swarms
        SwarmData* AddSwarm( const SwarmData& swarm );
        void RemoveSwarm( const Sha1Hash& rootHash );

        // Find a swam, either by id or root hash
        SwarmData* FindSwarm( int id );
        SwarmData* FindSwarm( const Sha1Hash& rootHash );

        // Activate a swarm, so it can be used (not active swarms can't be read from/written to)
        SwarmData* ActivateSwarm( const Sha1Hash& rootHash );
        void BuildSwarm( SwarmData* swarm );

        // Manage maximum of active swarms
        int GetMaximumActiveSwarms();
        void SetMaximumActiveSwarms( int newMaxActiveSwarms );
    };
}
