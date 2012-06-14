#include <string.h>
#include <time.h>
#include "swarmmanager.h"
#include "compat.h"

#define SECONDS_UNTIL_INDEX_REUSE   120
#define SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED   30

namespace swift {

// FIXME: Difference between seeds (complete) and downloads; allow setting minimum number of seeds?
// FIXME: Build and run assert methods (switch on/off by define)

static SwarmManager SwarmManager::instance_;

SwarmData::SwarmData( const Sha1Hash& rootHash ) : id_(-1), rootHash_( rootHash ), active_( false ), latestUse_(0), toBeRemoved_(false) {}

SwarmData::SwarmData( const SwarmData& sd ) : id_(-1), rootHash_( sd.rootHash_ ), active_( false ), latestUse_(0), toBeRemoved_(false) {}

bool SwarmData::touch() {
    if( !active_ )
        return false;
    latestUse_ = usec_time();
    return true;
}

bool SwarmData::isActive() {
    return active_;
}

const Sha1Hash& const SwarmData::rootHash() {
    return rootHash_;
}

int SwarmData::id() {
    return id_;
}

bool SwarmData::toBeRemoved() {
    return toBeRemoved_;
}

// FIXME: Add all class variables to the constructor
SwarmManager::SwarmManager() : knownSwarms_( 64, std::vector<SwarmData*> ), maxActiveSwarms_( 256 ), activeSwarmCount_( 0 ), activeSwarms(), eventCheckToBeRemoved_(0) {
    eventCheckToBeRemoved_ = evtimer_new( Channel::evbase, checkSwarmsToBeRemovedCallback, this );
}

SwarmManager::~SwarmManager() {
    event_free( eventCheckToBeRemoved_ );
}

#define rootHashToList( rootHash ) (knownSwarms_[rootHash.bits[0]&63])

SwarmData* SwarmManager::addSwarm( const SwarmData& swarm ) {
    // FIXME: How to handle a swarm that has no rootHash yet? Refuse? Or queue and build rootHash in the meantime?
    std::vector<SwarmData*> list = rootHashToList(swarm.rootHash_);
    int loc = getSwarmLocation( list, swarm.rootHash_ );
    if( list[loc]->rootHash_ == swarm.rootHash_ ) {
        // Let's assume here that the rest of the data is, hence, also equal
        return list[loc];
    }
    SwarmData* newSwarm = new SwarmData( swarm );
    list.insert( loc, newSwarm );
    if( unusedIndices_.size() > 0 and unusedIndices_.front().since < (usec_time() - SECONDS_UNTIL_INDEX_REUSE) ) {
        newSwarm->id_ = unusedIndices_.front().index;
        unusedIndices_.pop_front();
        swarmList_[newSwarm->id_] = newSwarm;
    }
    else {
        newSwarm->id_ = swarmList_.size();
        swarmList_.push_back( newSwarm );
    }
    return newSwarm;
}

void SwarmManager::buildSwarm( SwarmData* swarm ) {
    // FIXME: Add variables to the swarm data: std::string filename, bool check_hashes, uint32_t chunk_size, bool zerostate, FileTransfer* ft_
    swarm->ft_ = new FileTransfer( swarm->filename_, swarm->rootHash_, swarm->checkHashes_, swarm->chunkSize_, swarm->zerostate_ );
    if( !ft_ )
        return;

}

// Refuses to remove an active swarm, but flags it for future removal
void SwarmManager::removeSwarm( const Sha1Hash& rootHash ) {
    std::vector<SwarmData*> list = rootHashToList(rootHash_);
    int loc = getSwarmLocation( list, rootHash );
    SwarmData* swarm = list[loc];
    if( swarm->active_ ) {
        swarm->toBeRemoved_ = true;
        swarmsToBeRemoved_.push_back(swarm->id_);
        if( !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
            evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
        return;
    }
    if( swarm->rootHash_ == rootHash )
        list.erase(loc);
    struct unusedIndex ui;
    ui.index = swarm->id_;
    ui.since = usec_time();
    swarmList_[ui.index] = NULL;
    unusedIndices_.push_back(ui);
    delete swarm;
}

static void SwarmManager::checkSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg) {
    ((SwarmManager*)arg)->checkSwarmsToBeRemoved();
}

void SwarmManager::checkSwarmsToBeRemoved() {
    // Remove swarms that are scheduled to be removed
    tint old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    for( list<int>::iterator it = swarmsToBeRemoved_.begin(); it != swarmsToBeRemoved_.end(); ) {
        if( swarmList_[*it].latestUse_ < old ) {
            SwarmData* swarm = swarmList_[*it];
            swarm->active_ = false;
            for( int i = 0; i < activeSwarms_.size(); i++ ) {
                if( activeSwarms_[i] == swarm ) {
                    activeSwarms_[i] = activeSwarms_[activeSwarms_.size()-1];
                    activeSwarms_.erase( activeSwarms_.size()-1 );
                    activeSwarmCount_--;
                    break;
                }
            }
            it = swarmsToBeRemoved_.erase(it);
            removeSwarm( oldest->rootHash_ );
        }
        else
            it++;
    }
    
    // If we have too much swarms active, aggressively try to remove swarms
    while( activeSwarmCount_ > maxActiveSwarms_ )
        if( !deactivateSwarm() )
            break;

    if( swarmsToBeRemoved_.size() > 0 || activeSwarmCount_ > maxActiveSwarms_ )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
}

SwarmData* SwarmManager::findSwarm( int id ) {
    if( id >= swarmList_.size() )
        return NULL;
    return swarmList_[id];
}

SwarmData* SwarmManager::findSwarm( const Sha1Hash& rootHash ) {
    SwarmData* swarm = getSwarmData( rootHash );
    if( swarm && swarm->rootHash_ == rootHash )
        return swarm;
    return NULL;
}

// Returns NULL if !containsSwarm( rootHash ) or too many swarms are already active
SwarmData* SwarmManager::activateSwarm( const Sha1Hash& rootHash ) {
    SwarmData* sd = getSwarmData( rootHash );
    if( !sd || sd->rootHash != rootHash || sd->toBeRemoved_ )
        return NULL;

    if( sd->active_ )
        return sd;

    if( activeSwarmCount_ >= maxActiveSwarms_ ) {
        if(! deactivateSwarm() )
            return NULL;
    }

    activeSwarmCount_++;

    // FIXME: Activate swarm

    sd->active_ = true;
    sd->latestUse_ = 0;
    activeSwarms_.push_back( sd );
    
    return sd;
}

bool SwarmManager::deactivateSwarm() {
    tine old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    SwarmData* oldest = NULL;
    int oldestloc;
    for( int i = 0; i < activeSwarms_.size(); i++ ) {
        if( activeSwarms_[i].latestUse_ < old && ( !oldest || ( oldest->latestUse_ > activeSwarms_[i].latestUse_ ) ) ) {
            oldest = activeSwarms_[i];
            oldestloc = i;
        }
    }
    if( !oldest )
        return false;

    oldest->active_ = false;
    activeSwarms_[oldestloc] = activeSwarms_[activeSwarms_.size()-1];
    activeSwarms_.erase( activeSwarms_.size()-1 );
    activeSwarmCount_--;
    if( oldest->toBeRemoved_ ) {
        swarmsToBeRemoved_.remove( oldest->id_ );
        removeSwarm( oldest->rootHash_ );
    }

    // FIXME: Deactivate swarm

    return true;
}

int SwarmManager::getMaximumActiveSwarms() {
    return maxActiveSwarms_;
}

void SwarmManager::setMaximumActiveSwarms( int newMaxSwarms ) {
    if( newMaxSwarms <= 0 )
        return;
    while( newMaxSwarms < activeSwarmCount_ )
        if( !deactivateSwarm() )
            break;
    maxActiveSwarms_ = newMaxSwarms;
    if( maxActiveSwarms < activeSwarmCount_ && !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*tint_sec) );
}

SwarmData* SwarmManager::getSwarmData( const Sha1Hash& rootHash ) {
    std::vector<SwarmData*> list = rootHashToList(rootHash_);
    int loc = getSwarmLocation( list, rootHash );
    if( loc >= list.size() )
        return NULL;
    return list[loc];
}

int SwarmManager::getSwarmLocation( const std::vector<SwarmData*>& list, const Sha1Hash& rootHash ) {
    int low = 0;
    int high = list.size() - 1;
    int mid, c, res;
    uint8_t* bits; 
    uint8_t* bitsTarget = rootHash.bits;
    while( low < high ) {
        mid = (low + high) / 2;
        bits = list[mid]->bits;
        res = memcmp( list[mid]->bits, bitsTarget, Sha1Hash::SIZE );
        if( res < 0 )
            low = mid + 1;
        else if( res > 0 )
            high = mid;
        else
            return mid;
    }
    return low;
}
