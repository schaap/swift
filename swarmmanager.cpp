#include <string.h>
#include <time.h>

#define SWARMMANAGER_ASSERT_INVARIANTS 1

#include "swift.h"

#define SECONDS_UNTIL_INDEX_REUSE   120
#define SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED   30

#if SWARMMANAGER_ASSERT_INVARIANTS
#include <assert.h>
//int levelcount = 0;
#define enter( x )
    //fprintf( stderr, "[%02d] Entered " x "\n", ++levelcount );
#define exit( x )
    //fprintf( stderr, "[%02d] Leaving " x "\n", levelcount-- );
#else
#define assert( x )
#define invariant()
#define enter( x )
#define exit( x )
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
    eventCheckToBeRemoved_(NULL),
    maxActiveSwarms_( 8 ), activeSwarmCount_( 0 ), activeSwarms_()
{
    enter( "cons" );
    // Do not call the invariant here, directly or indirectly: screws up event creation
    exit( "cons" );
}

SwarmManager::~SwarmManager() {
    enter( "dest" );
    std::list<SwarmData*> dellist;
    for( std::vector<SwarmData*>::iterator iter = swarmList_.begin(); iter != swarmList_.end(); iter++ )
        dellist.push_back( *iter );
    for( std::list<SwarmData*>::iterator deliter = dellist.begin(); deliter != dellist.end(); deliter++ )
        delete (*deliter);
    event_free( eventCheckToBeRemoved_ );
    exit( "dest" );
}

#define rootHashToList( rootHash ) (knownSwarms_[rootHash.bits[0]&63])

SwarmData* SwarmManager::AddSwarm( const std::string filename, const Sha1Hash& hash, const Address& tracker, bool check_hashes, uint32_t chunk_size, bool zerostate ) {
    enter( "addswarm( many )" );
    invariant();
    SwarmData sd( filename, hash, tracker, check_hashes, chunk_size, zerostate );
#if SWARMMANAGER_ASSERT_INVARIANTS
    SwarmData* res = AddSwarm( sd );
    assert( hash == Sha1Hash::ZERO || res == FindSwarm( hash ) );
    assert( !res || res == FindSwarm( res->Id() ) );
    invariant();
    exit( "addswarm( many )" );
    return res;
#else
    return AddSwarm( sd );
#endif
}

// Can return NULL. Can also return a non-active swarm, even though it tries to activate by default.
SwarmData* SwarmManager::AddSwarm( const SwarmData& swarm ) {
    enter( "addswarm( swarm )" );
    invariant();
    // FIXME: Handle a swarm that has no rootHash yet in a better way: queue it and build the rootHash in the background.
    SwarmData* newSwarm = new SwarmData( swarm );
    if( swarm.rootHash_ == Sha1Hash::ZERO ) {
        BuildSwarm( newSwarm );
        if( !newSwarm->ft_ ) {
            delete newSwarm;
            exit( "addswarm( swarm ) (1)" );
            return NULL;
        }
    }
    std::vector<SwarmData*>& list = rootHashToList(newSwarm->rootHash_);
    int loc = GetSwarmLocation( list, newSwarm->rootHash_ );
    if( loc < list.size() && list[loc]->rootHash_ == newSwarm->rootHash_ ) {
        delete newSwarm;
        // Let's assume here that the rest of the data is, hence, also equal
        assert( swarm.rootHash_ != Sha1Hash::ZERO );
        assert( list[loc] == FindSwarm( swarm.rootHash_ ) );
        invariant();
        exit( "addswarm( swarm ) (2)" );
        return list[loc];
    }
    assert( loc <= list.size() );
    list.push_back( NULL );
    for( int i = list.size() - 1; i > loc; i-- )
        list[i] = list[i - 1];
    list[loc] = newSwarm;
    assert( rootHashToList(newSwarm->rootHash_)[loc] == newSwarm );
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
    exit( "addswarm( swarm )" );
    return newSwarm;
}

void SwarmManager::BuildSwarm( SwarmData* swarm ) {
    enter( "buildswarm" );
    assert( swarm );
    invariant();
    // Refuse to seed a 0-byte file
    if( swarm->rootHash_ == Sha1Hash::ZERO && file_size_by_path_utf8( swarm->filename_ ) == 0 )
        return;
    swarm->ft_ = new FileTransfer( swarm->id_, swarm->filename_, swarm->rootHash_, swarm->checkHashes_, swarm->chunkSize_, swarm->zerostate_ );
    if( !swarm->ft_ ) {
        exit( "buildswarm (1)" );
        return;
    }
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
    exit( "buildswarm" );
}

// Refuses to remove an active swarm, but flags it for future removal
void SwarmManager::RemoveSwarm( const Sha1Hash& rootHash, bool removeState, bool removeContent ) {
    enter( "removeswarm" );
    invariant();
    assert( rootHash != Sha1Hash::ZERO );
    std::vector<SwarmData*>& list = rootHashToList(rootHash);
    int loc = GetSwarmLocation( list, rootHash );
    if( loc == list.size() ) {
        exit( "removeswarm (1)" );
        return;
    }
    SwarmData* swarm = list[loc];
    if( swarm->active_ ) {
        swarm->toBeRemoved_ = true;
        swarm->stateToBeRemoved_ = removeState;
        swarm->contentToBeRemoved_ = removeContent;
        if( !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
            evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
        invariant();
        exit( "removeswarm (2)" );
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
    exit( "removeswarm" );
}

void SwarmManager::CheckSwarmsToBeRemovedCallback(evutil_socket_t fd, short events, void* arg) {
    enter( "static checkswarms" );
    ((SwarmManager*)arg)->CheckSwarmsToBeRemoved();
    exit( "static checkswarms" );
}

void SwarmManager::CheckSwarmsToBeRemoved() {
    enter( "checkswarms" );
    invariant();
    // Remove swarms that are scheduled to be removed
    tint old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    std::list<int> dellist;
    bool hasMore = false;
    for( int i = 0; i < activeSwarms_.size(); i++ ) {
        assert( activeSwarms_[i] );
        if( activeSwarms_[i]->toBeRemoved_ ) {
            if( activeSwarms_[i]->latestUse_ < old )
                dellist.push_back( i );
            else
                hasMore = true;
        }
    }
    while( dellist.size() > 0 ) {
        int i = dellist.back();
        SwarmData* swarm = activeSwarms_[i];
        activeSwarms_[i] = activeSwarms_[activeSwarms_.size()-1];
        activeSwarms_.pop_back();
        activeSwarmCount_--;
        dellist.pop_back();
        RemoveSwarm( swarm->rootHash_, swarm->stateToBeRemoved_, swarm->contentToBeRemoved_ );
    }
    
    // If we have too much swarms active, aggressively try to remove swarms
    while( activeSwarmCount_ > maxActiveSwarms_ )
        if( !DeactivateSwarm() )
            break;

    if( hasMore || activeSwarmCount_ > maxActiveSwarms_ )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
    invariant();
    exit( "checkswarms" );
}

// Called from invariant()
SwarmData* SwarmManager::FindSwarm( int id ) {
    enter( "findswarm( id )" );
    if( id < 0 || id >= swarmList_.size() ) {
        exit( "findswarm( id ) (1)" );
        return NULL;
    }
    assert( !swarmList_[id] || swarmList_[id]->Id() == id );
    exit( "findswarm( id )" );
    return swarmList_[id];
}

// Called from invariant()
SwarmData* SwarmManager::FindSwarm( const Sha1Hash& rootHash ) {
    enter( "findswarm( hash )" );
    SwarmData* swarm = GetSwarmData( rootHash );
    if( swarm && swarm->rootHash_ == rootHash ) {
        assert( swarm->RootHash() == rootHash );
        exit( "findswarm( hash ) (1)" );
        return swarm;
    }
    exit( "findswarm( hash )" );
    return NULL;
}

// Returns NULL if !containsSwarm( rootHash ) or too many swarms are already active
SwarmData* SwarmManager::ActivateSwarm( const Sha1Hash& rootHash ) {
    enter( "activateswarm( hash )" );
    assert( rootHash != Sha1Hash::ZERO );
    invariant();
    SwarmData* sd = GetSwarmData( rootHash );
    if( !sd || !(sd->rootHash_ == rootHash) || sd->toBeRemoved_ ) {
        exit( "activateswarm( hash ) (1)" );
        return NULL;
    }
#if SWARMMANAGER_ASSERT_INVARIANTS
    SwarmData* res = ActivateSwarm( sd );
    assert( !res || res->IsActive() );
    invariant();
    exit( "activateswarm( hash )" );
    return res;
#else
    return ActivateSwarm( sd );
#endif
}

SwarmData* SwarmManager::ActivateSwarm( SwarmData* sd ) {
    enter( "activateswarm( swarm )" );
    assert( sd );
    assert( FindSwarm( sd->Id() ) == sd );
    // invariant doesn't necessarily hold for sd, here (might have ft_ and !active_)

    if( sd->active_ ) {
        exit( "activateswarm( swarm ) (1)" );
        return sd;
    }

    if( activeSwarmCount_ >= maxActiveSwarms_ ) {
        if(! DeactivateSwarm() ) {
            if( sd->ft_ )
                delete sd->ft_;
            invariant();
            exit( "activateswarm( swarm ) (2)" );
            return NULL;
        }
    }

    activeSwarmCount_++;

    if( !sd->ft_ ) {
        BuildSwarm( sd );

        if( !sd->ft_ ) {
            activeSwarmCount_--;
            invariant();
            exit( "activateswarm( swarm ) (3)" );
            return NULL;
        }
    }

    sd->active_ = true;
    sd->latestUse_ = 0;
    activeSwarms_.push_back( sd );
    
    invariant();
    exit( "activateswarm( swarm )" );
    return sd;
}

bool SwarmManager::DeactivateSwarm() {
    // This can be called from ActivateSwarm(swarm), where the invariant need not hold
    enter( "deactivateswarm" );
    tint old = usec_time() - SECONDS_UNUSED_UNTIL_SWARM_MAY_BE_DEACTIVATED;
    SwarmData* oldest = NULL;
    int oldestloc = 0;
    for( int i = 0; i < activeSwarms_.size(); i++ ) {
        if( activeSwarms_[i]->latestUse_ < old && ( !oldest || ( oldest->latestUse_ > activeSwarms_[i]->latestUse_ ) ) ) {
            oldest = activeSwarms_[i];
            oldestloc = i;
        }
    }
    if( !oldest ) {
        exit( "deactivateswarm (1)" );
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
    if( oldest->toBeRemoved_ )
        RemoveSwarm( oldest->rootHash_, oldest->stateToBeRemoved_, oldest->contentToBeRemoved_ );

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

    exit( "deactivateswarm" );
    return true;
}

int SwarmManager::GetMaximumActiveSwarms() {
    return maxActiveSwarms_;
}

void SwarmManager::SetMaximumActiveSwarms( int newMaxSwarms ) {
    enter( "setmaximumactiveswarms" );
    invariant();
    if( newMaxSwarms <= 0 ) {
        exit( "setmaximumactiveswarms (1)" );
        return;
    }
    while( newMaxSwarms < activeSwarmCount_ )
        if( !DeactivateSwarm() )
            break;
    maxActiveSwarms_ = newMaxSwarms;
    if( maxActiveSwarms_ < activeSwarmCount_ && !evtimer_pending( eventCheckToBeRemoved_, NULL ) )
        evtimer_add( eventCheckToBeRemoved_, tint2tv(5*TINT_SEC) );
    invariant();
    exit( "setmaximumativeswarms" );
}

// Called from invariant()
SwarmData* SwarmManager::GetSwarmData( const Sha1Hash& rootHash ) {
    enter( "getswarmdata" );
    std::vector<SwarmData*>& list = rootHashToList(rootHash);
    int loc = GetSwarmLocation( list, rootHash );
    if( loc >= list.size() ) {
        exit( "getswarmdata (1)" );
        return NULL;
    }
    exit( "getswarmdata" );
    return list[loc];
}

// Called from invariant()
int SwarmManager::GetSwarmLocation( const std::vector<SwarmData*>& list, const Sha1Hash& rootHash ) {
    enter( "getswarmlocation" );
    int low = 0;
    int high = list.size();
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
            assert( mid >= 0 && mid < list.size() );
            exit( "getswarmlocation (1)" );
            return mid;
        }
    }
    assert( low >= 0 && low <= list.size() );
#if SWARMMANAGER_ASSERT_INVARIANTS
    if( low == list.size() ) {
        for( int i = 0; i < list.size(); i++ )
            assert( list[i]->rootHash_ != rootHash );
    }
    exit( "getswarmlocation" );
#endif
    return low;
}

SwarmManager& SwarmManager::GetManager() {
    // Deferred, since Channel::evbase is created later
    if( !instance_.eventCheckToBeRemoved_ ) {
        instance_.eventCheckToBeRemoved_ = evtimer_new( Channel::evbase, CheckSwarmsToBeRemovedCallback, &instance_ );
    }
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
    enter( "inv" );
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
    exit( "inv" );
}
#endif
}
