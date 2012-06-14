#include "hashtree.h"
#include <time.h>

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
        const Sha1Hash& const rootHash();
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
        static void checkSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg);
        void checkSwarmsToBeRemoved();

        // Looking up swarms by rootHash, internal functions
        int getSwarmLocation( const std::vector<SwarmManager*>& list, const Sha1Hash& rootHash )
        SwarmData* getSwarmData( const Sha1Hash& rootHash );

        // Internal method to find the oldest swarm and deactivate it
        bool deactivateSwarm();

        // Structures to keep track of active swarms
        int maxActiveSwarms_;
        int activeSwarmCount_;
        std::vector<SwarmData*> activeSwarms_;
    public:
        // Singleton
        static SwarmManager& getManager();

        // Add and remove swarms
        void addSwarm( const SwarmData& swarm );
        void removeSwarm( const Sha1Hash& rootHash );

        // Find a swam, either by id or root hash
        SwarmData* findSwarm( int id );
        SwarmData* findSwarm( const Sha1Hash& rootHash );

        // Activate a swarm, so it can be used (not active swarms can't be read from/written to)
        SwarmData* activateSwarm( const Sha1Hash& rootHash );

        // Manage maximum of active swarms
        int getMaximumActiveSwarms();
        void setMaximumActiveSwarms( int newMaxActiveSwarms );
    };
}
