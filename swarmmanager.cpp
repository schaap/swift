#include <string.h>
#include <time.h>

#define SWARMMANAGER_ASSERT_INVARIANTS 1

#include "swift.h"

#define SECONDS_UNTIL_INDEX_REUSE   120
#define SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED   30

#if SWARMMANAGER_ASSERT_INVARIANTS
#include <assert.h>
#else
#define assert()
#define invariant()
#endif

namespace swift {

// FIXME: Difference between seeds (complete) and downloads; allow setting minimum number of seeds?
//          -> Currently not supported, but the basis is there.
//          -> Also needs mechanisms to automatically decide to swap swarms back in, such as a timed check on progress.
// FIXME: Build and run assert methods (switch on/off by define)

SwarmManager SwarmManager::instance_;

SwarmData::SwarmData( const std::string filename, const Sha1Hash& rootHash, const Address& tracker, bool check_hashes, uint32_t chunk_size, bool zerostate ) :
    id_(-1), rootHash_( rootHash ), active_( false ), latestUse_(0), toBeRemoved_(false), stateToBeRemoved_(false), contentToBeRemoved_(false), ft_(NULL),
    filename_( filename ), tracker_( tracker ), checkHashes_( check_hashes ), chunkSize_( chunk_size ), zerostate_( zerostate ), cached_(false)
{
}

SwarmData::SwarmData( const SwarmData& sd ) :
    id_(-1), rootHash_( sd.rootHash_ ), active_( false ), latestUse_(0), toBeRemoved_(false), stateToBeRemoved_(false), contentToBeRemoved_(false), ft_(NULL),
    filename_( sd.filename_ ), tracker_( sd.tracker_ ), checkHashes_( sd.checkHashes_ ), chunkSize_( sd.chunkSize_ ), zerostate_( sd.zerostate_ ), cached_(false)
{
}

SwarmData::~SwarmData() {
    if( ft_ )
        delete ft_;
}

bool SwarmData::Touch() {
    if( !active_ )
        return false;
    latestUse_ = usec_time();
    return true;
}

bool SwarmData::IsActive() {
    return active_;
}

const Sha1Hash& SwarmData::RootHash() {
    return rootHash_;
}

int SwarmData::Id() {
    return id_;
}

bool SwarmData::ToBeRemoved() {
    return toBeRemoved_;
}

// Can return NULL
FileTransfer* SwarmData::GetTransfer(bool touch) {
    if( touch ) {
        if( !Touch() )
            return NULL;
    }
    else {
        if( !IsActive() )
            return NULL;
    }
    assert( ft_ );
    return ft_;
}

std::string& SwarmData::Filename() {
    return filename_;
}

Address& SwarmData::Tracker() {
    return tracker_;
}

uint32_t SwarmData::ChunkSize() {
    return chunkSize_;
}

bool SwarmData::IsZeroState() {
    return zerostate_;
}

void SwarmData::SetMaxSpeed(data_direction_t ddir, double speed) {
    if( speed <= 0 )
        return;
    if( ft_ ) {
        assert( !cached_ );
        // Arno, 2012-05-25: SetMaxSpeed resets the current speed history, so
        // be careful here.
        if( ft_->GetMaxSpeed( ddir ) != speed )
            ft_->SetMaxSpeed( ddir, speed );
    }
    else if( cached_ )
        cachedMaxSpeeds_[ddir] = speed;
}

void SwarmData::AddProgressCallback(ProgressCallback cb, uint8_t agg) {
    if( ft_ ) {
        assert( !cached_ );
        ft_->AddProgressCallback(cb, agg);
    }
    else if( cached_ )
        cachedCallbacks_.push_back( std::pair<ProgressCallback, uint8_t>( cb, agg ) );
}

void SwarmData::RemoveProgressCallback(ProgressCallback cb) {
    if( ft_ ) {
        assert( !cached_ );
        ft_->RemoveProgressCallback(cb);
    }
    else if( cached_ ) {
        for( std::list< std::pair<ProgressCallback, uint8_t> >::iterator iter = cachedCallbacks_.begin(); iter != cachedCallbacks_.end(); iter++ ) {
            if( (*iter).first == cb ) {
                cachedCallbacks_.erase(iter);
                break;
            }
        }
    }
}

uint64_t SwarmData::Size() {
    if( ft_ ) {
        assert( !cached_ );
        return ft_->hashtree()->size();
    }
    else if( cached_ )
        return cachedSize_;
    return 0;
}

bool SwarmData::IsComplete() {
    if( ft_ ) {
        assert( !cached_ );
        return ft_->hashtree()->is_complete();
    }
    else if( cached_ ) {
        assert( ( cachedSize_ == cachedComplete_ ) == cachedIsComplete_ );
        return cachedIsComplete_;
    }
    return false;
}

uint64_t SwarmData::Complete() {
    if( ft_ ) {
        assert( !cached_ );
        return ft_->hashtree()->complete();
    }
    else if( cached_ )
        return cachedComplete_;
    return 0;
}

std::string SwarmData::OSPathName() {
    if( ft_ ) {
        assert( !cached_ );
        return ft_->GetStorage()->GetOSPathName();
    }
    else if( cached_ )
        return cachedOSPathName_;
    return std::string();
}

SwarmManager::SwarmManager() :
    knownSwarms_( 64, std::vector<SwarmData*>() ), swarmList_(), unusedIndices_(),
    swarmsToBeRemoved_(), eventCheckToBeRemoved_(NULL),
    maxActiveSwarms_( 256 ), activeSwarmCount_( 0 ), activeSwarms_()
{
    invariant();
    eventCheckToBeRemoved_ = evtimer_new( Channel::evbase, CheckSwarmsToBeRemovedCallback, this );
    invariant();
}

SwarmManager::~SwarmManager() {
    std::list<SwarmData*> dellist;
    for( std::vector<SwarmData*>::iterator iter = swarmList_.begin(); iter != swarmList_.end(); iter++ )
        dellist.push_back( *iter );
    for( std::list<SwarmData*>::iterator deliter = dellist.begin(); deliter != dellist.end(); deliter++ )
        delete (*deliter);
    event_free( eventCheckToBeRemoved_ );
}

#define rootHashToList( rootHash ) (knownSwarms_[rootHash.bits[0]&63])

SwarmData* SwarmManager::AddSwarm( const std::string filename, const Sha1Hash& hash, const Address& tracker, bool check_hashes, uint32_t chunk_size, bool zerostate ) {
    invariant();
    SwarmData sd( filename, hash, tracker, check_hashes, chunk_size, zerostate );
#if SWARMMANAGER_ASSERT_INVARIANTS
    SwarmData* res = AddSwarm( sd );
    assert( hash == Sha1Hash::ZERO || res == FindSwarm( hash ) );
    assert( res == FindSwarm( res->Id() ) );
    invariant();
    return res;
#else
    return AddSwarm( sd );
#endif
}

// Can return NULL. Can also return a non-active swarm, even though it tries to activate by default.
SwarmData* SwarmManager::AddSwarm( const SwarmData& swarm ) {
    invariant();
    // FIXME: Handle a swarm that has no rootHash yet in a better way: queue it and build the rootHash in the background.
    SwarmData* newSwarm = new SwarmData( swarm );
    if( swarm.rootHash_ == Sha1Hash::ZERO ) {
        BuildSwarm( newSwarm );
        if( !newSwarm->ft_ ) {
            delete newSwarm;
            return NULL;
        }
    }
    std::vector<SwarmData*> list = rootHashToList(newSwarm->rootHash_);
    int loc = GetSwarmLocation( list, newSwarm->rootHash_ );
    if( list[loc]->rootHash_ == newSwarm->rootHash_ ) {
        delete newSwarm;
        // Let's assume here that the rest of the data is, hence, also equal
        assert( swarm.rootHash_ != Sha1Hash::ZERO );
        assert( list[loc] == FindSwarm( swarm.rootHash_ ) );
        invariant();
        return list[loc];
    }
    list.push_back( NULL );
    for( int i = list.size() - 1; i > loc; i-- )
        list[i] = list[i - 1];
    list[loc] = newSwarm;
    if( unusedIndices_.size() > 0 and unusedIndices_.front().since < (usec_time() - SECONDS_UNTIL_INDEX_REUSE) ) {
        newSwarm->id_ = unusedIndices_.front().index;
        unusedIndices_.pop_front();
        swarmList_[newSwarm->id_] = newSwarm;
    }
    else {
        newSwarm->id_ = swarmList_.size();
        swarmList_.push_back( newSwarm );
    }
    if( !ActivateSwarm( newSwarm ) && newSwarm->ft_ ) {
        delete newSwarm->ft_;
        newSwarm->ft_ = NULL;
    }
    assert( swarm.rootHash_ == Sha1Hash::ZERO || newSwarm == FindSwarm( swarm.rootHash_ ) );
    assert( newSwarm == FindSwarm( newSwarm->Id() ) );
    invariant();
    return newSwarm;
}

void SwarmManager::BuildSwarm( SwarmData* swarm ) {
    assert( swarm );
    invariant();
    swarm->ft_ = new FileTransfer( swarm->id_, swarm->filename_, swarm->rootHash_, swarm->checkHashes_, swarm->chunkSize_, swarm->zerostate_ );
    if( !swarm->ft_ )
        return;
    if( swarm->rootHash_ == Sha1Hash::ZERO )
        swarm->rootHash_ = swarm->ft_->root_hash();
    assert( swarm->RootHash() != Sha1Hash::ZERO );
    if( swarm->cached_ ) {
        swarm->cached_ = false;
        swarm->SetMaxSpeed( DDIR_DOWNLOAD, swarm->cachedMaxSpeeds_[DDIR_DOWNLOAD] );
        swarm->SetMaxSpeed( DDIR_UPLOAD, swarm->cachedMaxSpeeds_[DDIR_UPLOAD] );
        for( std::list< std::pair<ProgressCallback, uint8_t> >::iterator iter = swarm->cachedCallbacks_.begin(); iter != swarm->cachedCallbacks_.end(); iter++ )
            swarm->AddProgressCallback( (*iter).first, (*iter).second );
        swarm->cachedStorageFilenames_.clear();
        swarm->cachedCallbacks_.clear();
        swarm->cachedOSPathName_ = std::string();
    }
    // Hashes have been checked, don't check again
    swarm->checkHashes_ = false;
    if( swarm->tracker_ != Address() ) {
        // initiate tracker connections
        // SWIFTPROC
        swarm->ft_->SetTracker( swarm->tracker_ );
        swarm->ft_->ConnectToTracker();
    }
    invariant();
}

// Refuses to remove an active swarm, but flags it for future removal
void SwarmManager::RemoveSwarm( const Sha1Hash& rootHash, bool removeState, bool removeContent ) {
    invariant();
    assert( rootHash != Sha1Hash::ZERO );
    std::vector<SwarmData*> list = rootHashToList(rootHash);
    int loc = GetSwarmLocation( list, rootHash );
    SwarmData* swarm = list[loc];
    if( swarm->active_ ) {
        swarm->toBeRemoved_ = true;
        swarm->stateToBeRemoved_ = removeState;
        swarm->contentToBeRemoved_ = removeContent;
        swarmsToBeRemoved_.push_back(swarm->id_);
        if( !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
            evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
        invariant();
        return;
    }
    if( swarm->rootHash_ == rootHash ) {
        for( int i = loc; i < list.size() - 1; i++ )
            list[i] = list[i+1];
        list.pop_back();
    }
    struct SwarmManager::UnusedIndex ui;
    ui.index = swarm->id_;
    ui.since = usec_time();
    swarmList_[ui.index] = NULL;
    unusedIndices_.push_back(ui);
    invariant();
    assert( !FindSwarm( rootHash ) );
    assert( !FindSwarm( swarm->Id() ) );

    //MULTIFILE
    // Arno, 2012-05-23: Copy all filename to be deleted to a set. This info is lost after
    // swift::Close() and we need to call Close() to let the storage layer close the open files.
    // TODO: remove the dirs we created, if now empty.
    std::set<std::string> delset;
    std::string contentfilename;
    contentfilename = swarm->OSPathName();

    // Delete content + .mhash from filesystem, if desired
    if (removeContent)
        delset.insert(contentfilename);

    if (removeState)
    {
        std::string mhashfilename = contentfilename + ".mhash";
        delset.insert(mhashfilename);

        // Arno, 2012-01-10: .mbinmap gots to go too.
        std::string mbinmapfilename = contentfilename + ".mbinmap";
        delset.insert(mbinmapfilename);
    }

    // MULTIFILE
    bool ready;
    if( swarm->ft_ )
        ready = swarm->ft_->GetStorage()->IsReady();
    else
        ready = swarm->cachedStorageReady_;
    if (removeContent && ready)
    {
        if( swarm->ft_ ) {
            storage_files_t::iterator iter;
            storage_files_t sfs = swarm->ft_->GetStorage()->GetStorageFiles();
            for (iter = sfs.begin(); iter != sfs.end(); iter++) {
                std::string cfn = ((StorageFile*)*iter)->GetOSPathName();
                delset.insert(cfn);
            }
        }
        else {
            std::list<std::string>::iterator iter;
            std::list<std::string> filenames = swarm->cachedStorageFilenames_;
            for( iter = filenames.begin(); iter != filenames.end(); iter++ )
                delset.insert( *iter );
        }
    }

    delete swarm;

    std::set<std::string>::iterator iter;
    for (iter=delset.begin(); iter!=delset.end(); iter++)
    {
        std::string filename = *iter;
        int ret = remove(filename.c_str());
        if (ret < 0)
        {
            print_error("Could not remove file");
        }
    }
    
    invariant();
}

void SwarmManager::CheckSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg) {
    ((SwarmManager*)arg)->CheckSwarmsToBeRemoved();
}

void SwarmManager::CheckSwarmsToBeRemoved() {
    invariant();
    // Remove swarms that are scheduled to be removed
    tint old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    for( std::list<int>::iterator it = swarmsToBeRemoved_.begin(); it != swarmsToBeRemoved_.end(); ) {
        if( swarmList_[*it]->latestUse_ < old ) {
            SwarmData* swarm = swarmList_[*it];
            swarm->active_ = false;
            for( int i = 0; i < activeSwarms_.size(); i++ ) {
                if( activeSwarms_[i] == swarm ) {
                    activeSwarms_[i] = activeSwarms_[activeSwarms_.size()-1];
                    activeSwarms_.pop_back();
                    activeSwarmCount_--;
                    break;
                }
            }
            it = swarmsToBeRemoved_.erase(it);
            RemoveSwarm( swarm->rootHash_, swarm->stateToBeRemoved_, swarm->contentToBeRemoved_ );
        }
        else
            it++;
    }
    
    // If we have too much swarms active, aggressively try to remove swarms
    while( activeSwarmCount_ > maxActiveSwarms_ )
        if( !DeactivateSwarm() )
            break;

    if( swarmsToBeRemoved_.size() > 0 || activeSwarmCount_ > maxActiveSwarms_ )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
    invariant();
}

SwarmData* SwarmManager::FindSwarm( int id ) {
    invariant();
    if( id < 0 || id >= swarmList_.size() )
        return NULL;
    assert( !swarmList_[id] || swarmList_[id]->Id() == id );
    invariant();
    return swarmList_[id];
}

SwarmData* SwarmManager::FindSwarm( const Sha1Hash& rootHash ) {
    invariant();
    SwarmData* swarm = GetSwarmData( rootHash );
    if( swarm && swarm->rootHash_ == rootHash ) {
        assert( swarm->RootHash() == rootHash );
        invariant();
        return swarm;
    }
    invariant();
    return NULL;
}

// Returns NULL if !containsSwarm( rootHash ) or too many swarms are already active
SwarmData* SwarmManager::ActivateSwarm( const Sha1Hash& rootHash ) {
    assert( rootHash != Sha1Hash::ZERO );
    invariant();
    SwarmData* sd = GetSwarmData( rootHash );
    if( !sd || !(sd->rootHash_ == rootHash) || sd->toBeRemoved_ )
        return NULL;
#if SWARMMANAGER_ASSERT_INVARIANTS
    SwarmData* res = ActivateSwarm( sd );
    assert( !res || res->IsActive() );
    invariant();
    return res;
#else
    return ActivateSwarm( sd );
#endif
}

SwarmData* SwarmManager::ActivateSwarm( SwarmData* sd ) {
    assert( sd );
    assert( FindSwarm( sd->Id() ) == sd );
    invariant();

    if( sd->active_ )
        return sd;

    if( activeSwarmCount_ >= maxActiveSwarms_ ) {
        if(! DeactivateSwarm() ) {
            invariant();
            return NULL;
        }
    }

    activeSwarmCount_++;

    if( !sd->ft_ ) {
        BuildSwarm( sd );

        if( !sd->ft_ ) {
            activeSwarmCount_--;
            invariant();
            return NULL;
        }
    }

    sd->active_ = true;
    sd->latestUse_ = 0;
    activeSwarms_.push_back( sd );
    
    invariant();
    return sd;
}

bool SwarmManager::DeactivateSwarm() {
    invariant();
    tint old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    SwarmData* oldest = NULL;
    int oldestloc;
    for( int i = 0; i < activeSwarms_.size(); i++ ) {
        if( activeSwarms_[i]->latestUse_ < old && ( !oldest || ( oldest->latestUse_ > activeSwarms_[i]->latestUse_ ) ) ) {
            oldest = activeSwarms_[i];
            oldestloc = i;
        }
    }
    if( !oldest ) {
        invariant();
        return false;
    }

    // Checkpoint before deactivating
    if( Checkpoint( oldest->id_ ) == -1 && !oldest->zerostate_ ) {
        // Checkpoint failed and it's not due to not being needed in zerostate; better check the hashes next time
        oldest->checkHashes_ = true;
    }

    oldest->active_ = false;
    activeSwarms_[oldestloc] = activeSwarms_[activeSwarms_.size()-1];
    activeSwarms_.pop_back();
    activeSwarmCount_--;
    if( oldest->toBeRemoved_ ) {
        swarmsToBeRemoved_.remove( oldest->id_ );
        RemoveSwarm( oldest->rootHash_, oldest->stateToBeRemoved_, oldest->contentToBeRemoved_ );
    }

    if( oldest->ft_ ) {
        oldest->cachedMaxSpeeds_[DDIR_DOWNLOAD] = oldest->ft_->GetMaxSpeed(DDIR_DOWNLOAD);
        oldest->cachedMaxSpeeds_[DDIR_UPLOAD] = oldest->ft_->GetMaxSpeed(DDIR_UPLOAD);
        oldest->cachedStorageReady_ = oldest->ft_->GetStorage()->IsReady();
        if( oldest->cachedStorageReady_ ) {
            storage_files_t sfs = oldest->ft_->GetStorage()->GetStorageFiles();
            for( storage_files_t::iterator iter = sfs.begin(); iter != sfs.end(); iter++)
                oldest->cachedStorageFilenames_.push_back( ((StorageFile*)*iter)->GetOSPathName() ); 
        }
        oldest->cachedSize_ = oldest->Size();
        oldest->cachedIsComplete_ = oldest->IsComplete();
        oldest->cachedComplete_ = oldest->Complete();
        oldest->cachedOSPathName_ = oldest->OSPathName();
        for( std::list< std::pair<ProgressCallback, uint8_t> >::iterator iter = oldest->ft_->callbacks_.begin(); iter != oldest->ft_->callbacks_.end(); iter++ )
            oldest->cachedCallbacks_.push_back( std::pair<ProgressCallback, uint8_t>( (*iter).first, (*iter).second ) );
        oldest->cached_ = true;
        delete oldest->ft_;
        oldest->ft_ = NULL;
    }

    invariant();
    return true;
}

int SwarmManager::GetMaximumActiveSwarms() {
    return maxActiveSwarms_;
}

void SwarmManager::SetMaximumActiveSwarms( int newMaxSwarms ) {
    invariant();
    if( newMaxSwarms <= 0 )
        return;
    while( newMaxSwarms < activeSwarmCount_ )
        if( !DeactivateSwarm() )
            break;
    maxActiveSwarms_ = newMaxSwarms;
    if( maxActiveSwarms_ < activeSwarmCount_ && !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
    invariant();
}

SwarmData* SwarmManager::GetSwarmData( const Sha1Hash& rootHash ) {
    invariant();
    std::vector<SwarmData*> list = rootHashToList(rootHash);
    int loc = GetSwarmLocation( list, rootHash );
    if( loc >= list.size() ) {
        invariant();
        return NULL;
    }
    invariant();
    return list[loc];
}

int SwarmManager::GetSwarmLocation( const std::vector<SwarmData*>& list, const Sha1Hash& rootHash ) {
    invariant(); // Pure lookup, don't check invariant at the end
    int low = 0;
    int high = list.size() - 1;
    int mid, c, res;
    uint8_t* bits; 
    const uint8_t* bitsTarget = rootHash.bits;
    while( low < high ) {
        mid = (low + high) / 2;
        bits = list[mid]->rootHash_.bits;
        res = memcmp( bits, bitsTarget, Sha1Hash::SIZE );
        if( res < 0 )
            low = mid + 1;
        else if( res > 0 )
            high = mid;
        else {
            invariant();
            assert( mid >= 0 && mid < list.size() );
            return mid;
        }
    }
    invariant();
    assert( low >= 0 && low <= list.size() );
    return low;
}

SwarmManager& SwarmManager::GetManager() {
    return instance_;
}

SwarmManager::Iterator::Iterator() {
    transfer_ = -1;
    (void)operator++();
}
SwarmManager::Iterator::Iterator(int transfer) : transfer_(transfer) {}
SwarmManager::Iterator::Iterator(const Iterator& other) : transfer_(other.transfer_) {}
SwarmManager::Iterator& SwarmManager::Iterator::operator++() {
    transfer_++;
    for( ; transfer_ < SwarmManager::GetManager().swarmList_.size(); transfer_++ ) {
        if( SwarmManager::GetManager().swarmList_[transfer_] )
            break;
    }
    return *this;
}
SwarmManager::Iterator SwarmManager::Iterator::operator++(int) {
    SwarmManager::Iterator tmp(*this);
    (void)operator++();
    return tmp;
}
bool SwarmManager::Iterator::operator==(const SwarmManager::Iterator& other) {
    return transfer_ == other.transfer_;
}
bool SwarmManager::Iterator::operator!=(const SwarmManager::Iterator& other) {
    return transfer_ != other.transfer_;
}
SwarmData* SwarmManager::Iterator::operator*() {
    if( transfer_ < SwarmManager::GetManager().swarmList_.size() )
        return SwarmManager::GetManager().swarmList_[transfer_];
    return NULL;
}
SwarmManager::Iterator SwarmManager::begin() {
    return SwarmManager::Iterator();
}
SwarmManager::Iterator SwarmManager::end() {
    return SwarmManager::Iterator( swarmList_.size() );
}

#if SWARMMANAGER_ASSERT_INVARIANTS
void SwarmManager::invariant() {
    int i, j;
    bool f;
    int c1, c2, c3;
    c1 = 0;
    c3 = 0;
    tint t;
    for( i = 0; i < 64; i++ ) {
        std::vector<SwarmData*> l = knownSwarms_[i];
        for( std::vector<SwarmData*>::iterator iter = l.begin(); iter != l.end(); iter++ ) {
            assert( (*iter) );
            assert( (*iter)->RootHash() != Sha1Hash::ZERO );
            assert( ((*iter)->RootHash().bits[0] & 63) == i );
            f = false;
            for( std::vector<SwarmData*>::iterator iter2 = swarmList_.begin(); iter2 != swarmList_.end(); iter2++ ) {
                if( (*iter) == (*iter2) ) {
                    f = true;
                    break;
                }
            }
            assert( f );
            c1++;
        }
        for( j = 1; j < l.size(); j++ )
            assert( memcmp( l[j-1]->RootHash().bits, l[j]->RootHash().bits, Sha1Hash::SIZE ) < 0 );
        for( j = 0; j < l.size(); j++ )
            assert( GetSwarmLocation( l, l[j]->RootHash() ) == j );
    }
    c2 = 0;
    for( std::vector<SwarmData*>::iterator iter = swarmList_.begin(); iter != swarmList_.end(); iter++ ) {
        if( !(*iter) ) {
            c3++;
            continue;
        }
        if( (*iter)->RootHash() != Sha1Hash::ZERO ) {
            assert( GetSwarmData( (*iter)->RootHash() ) == (*iter) );
            c2++;
        }
        assert( ((bool)(*iter)->ft_) ^ (!(*iter)->IsActive()) );
    }
    assert( !FindSwarm( -1 ) );
    assert( !FindSwarm( Sha1Hash::ZERO ) );
    for( i = 0; i < swarmList_.size(); i++ ) {
        assert( (!swarmList_[i]) || (swarmList_[i]->Id() == i) );
        if( swarmList_[i] ) {
            assert( swarmList_[i] == FindSwarm( i ) );
            assert( (swarmList_[i]->RootHash() == Sha1Hash::ZERO) || (swarmList_[i] == FindSwarm( swarmList_[i]->RootHash() ) ) );
        }
        else
            assert( !FindSwarm( i ) );
    }
    assert( !FindSwarm( swarmList_.size() ) );
    t = 0;
    for( std::list<UnusedIndex>::iterator iter = unusedIndices_.begin(); iter != unusedIndices_.end(); iter++ ) {
        assert( (*iter).index >= 0 );
        assert( (*iter).index < swarmList_.size() );
        assert( !swarmList_[(*iter).index] );
        assert( (*iter).since > t );
        t = (*iter).since;
    }
    assert( c1 == c2 );
    assert( c3 == unusedIndices_.size() );
    c1 = 0;
    for( Iterator iter = begin(); iter != end(); iter++ ) {
        assert( *iter );
        assert( (*iter)->Id() >= 0 );
        assert( (*iter)->Id() < swarmList_.size() );
        assert( swarmList_[(*iter)->Id()] == (*iter) );
        c1++;
    }
    assert( c1 == (swarmList_.size() - c3) );

    c1 = 0;
    for( std::vector<SwarmData*>::iterator iter = swarmList_.begin(); iter != swarmList_.end(); iter++ ) {
        if( (*iter) && (*iter)->IsActive() )
            c1++;
    }
    for( std::vector<SwarmData*>::iterator iter = activeSwarms_.begin(); iter != activeSwarms_.end(); iter++ ) {
        assert( (*iter) );
        assert( (*iter)->IsActive() );
        assert( (*iter)->Id() >= 0 );
        assert( (*iter)->Id() < swarmList_.size() );
        assert( swarmList_[(*iter)->Id()] == (*iter) );
    }
    assert( c1 <= maxActiveSwarms_ || evtimer_pending( eventCheckToBeRemoved_, NULL ) );
    assert( c1 == activeSwarmCount_ );
    assert( activeSwarmCount_ == activeSwarms_.size() );
}
#endif
}
