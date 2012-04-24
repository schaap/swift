/*
 *  transfer.cpp
 *  some transfer-scope code
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009-2012 TECHNISCHE UNIVERSITEIT DELFT. All rights reserved.
 *
 */
#include <errno.h>
#include <string>
#include <sstream>
#include <cfloat>
#include "swift.h"

#include "ext/seq_picker.cpp" // FIXME FIXME FIXME FIXME
#include "ext/vod_picker.cpp"

using namespace swift;

std::vector<FileTransfer*> FileTransfer::files(20);

#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))


#define TRACKER_RETRY_INTERVAL_START	(5*TINT_SEC)
#define TRACKER_RETRY_INTERVAL_EXP		1.1				// exponent used to increase INTERVAL_START
#define TRACKER_RETRY_INTERVAL_MAX		(1800*TINT_SEC) // 30 minutes

// FIXME: separate Bootstrap() and Download(), then Size(), Progress(), SeqProgress()

FileTransfer::FileTransfer (const char* filename, const Sha1Hash& _root_hash, bool force_check_diskvshash, bool check_netwvshash, uint32_t chunk_size) :
    file_(filename,_root_hash,chunk_size,NULL,force_check_diskvshash,check_netwvshash), cb_installed(0), mychannels_(),
    speedzerocount_(0), tracker_(), tracker_retry_interval_(TRACKER_RETRY_INTERVAL_START), tracker_retry_time_(NOW)
{
    if (files.size()<fd()+1)
        files.resize(fd()+1);
    files[fd()] = this;
    if (ENABLE_VOD_PIECEPICKER) {
    	// Ric: init availability
    	availability_ = new Availability();
    	// Ric: TODO assign picker based on input params...
    	picker_ = new VodPiecePicker(this);
    }
    else
    	picker_ = new SeqPiecePicker(this);
    picker_->Randomize(rand()&63);
    init_time_ = Channel::Time();
    cur_speed_[DDIR_UPLOAD] = MovingAverageSpeed();
    cur_speed_[DDIR_DOWNLOAD] = MovingAverageSpeed();
    max_speed_[DDIR_UPLOAD] = DBL_MAX;
    max_speed_[DDIR_DOWNLOAD] = DBL_MAX;

    // SAFECLOSE
    evtimer_assign(&evclean_,Channel::evbase,&FileTransfer::LibeventCleanCallback,this);
    evtimer_add(&evclean_,tint2tv(5*TINT_SEC));

}


// SAFECLOSE
void FileTransfer::LibeventCleanCallback(int fd, short event, void *arg)
{
	// Arno, 2012-02-24: Why-oh-why, update NOW
	Channel::Time();

	FileTransfer *ft = (FileTransfer *)arg;
	if (ft == NULL)
		return;

	// STL and MS and conditional delete from set not a happy place :-(
	std::set<Channel *>	delset;
	std::set<Channel *>::iterator iter;
	bool hasestablishedpeers=false;
	for (iter=ft->mychannels_.begin(); iter!=ft->mychannels_.end(); iter++)
	{
		Channel *c = *iter;
		if (c != NULL) {
			if (c->IsScheduled4Close())
				delset.insert(c);

			if (c->is_established ()) {
				hasestablishedpeers = true;
				//fprintf(stderr,"%s peer %s\n", ft->file().root_hash().hex().c_str(), c->peer().str() );
			}
		}
	}
	for (iter=delset.begin(); iter!=delset.end(); iter++)
	{
		Channel *c = *iter;
		dprintf("%s #%u clean cb close\n",tintstr(),c->id());
		c->Close();
		ft->mychannels_.erase(c);
		delete c;
    }

	// Arno, 2012-02-24: Check for liveliness.
	ft->ReConnectToTrackerIfAllowed(hasestablishedpeers);

	// Reschedule cleanup
    evtimer_add(&(ft->evclean_),tint2tv(5*TINT_SEC));
}


void FileTransfer::ReConnectToTrackerIfAllowed(bool hasestablishedpeers)
{
	// If I'm not connected to any
	// peers, try to contact the tracker again.
	if (!hasestablishedpeers)
	{
		if (NOW > tracker_retry_time_)
		{
			ConnectToTracker();

			tracker_retry_interval_ *= TRACKER_RETRY_INTERVAL_EXP;
			if (tracker_retry_interval_ > TRACKER_RETRY_INTERVAL_MAX)
				tracker_retry_interval_ = TRACKER_RETRY_INTERVAL_MAX;
			tracker_retry_time_ = NOW + tracker_retry_interval_;
		}
	}
	else
	{
		tracker_retry_interval_ = TRACKER_RETRY_INTERVAL_START;
		tracker_retry_time_ = NOW + tracker_retry_interval_;
	}
}


void FileTransfer::ConnectToTracker()
{
	Channel *c = NULL;
    if (tracker_ != Address())
    	c = new Channel(this,INVALID_SOCKET,tracker_);
    else if (Channel::tracker!=Address())
    	c = new Channel(this);
}


Channel * FileTransfer::FindChannel(const Address &addr, Channel *notc)
{
	std::set<Channel *>::iterator iter;
	for (iter=mychannels_.begin(); iter!=mychannels_.end(); iter++)
	{
		Channel *c = *iter;
		if (c != NULL) {
			if (c != notc && (c->peer() == addr || c->recv_peer() == addr)) {
				return c;
			}
		}
	}
	return NULL;
}




void    Channel::CloseTransfer (FileTransfer* trans) {
    for(int i=0; i<Channel::channels.size(); i++)
        if (Channel::channels[i] && Channel::channels[i]->transfer_==trans)
        {
        	//fprintf(stderr,"Channel::CloseTransfer: delete #%i\n", Channel::channels[i]->id());
        	Channel::channels[i]->Close(); // ARNO
            delete Channel::channels[i];
        }
}


void swift::AddProgressCallback (int transfer,ProgressCallback cb,uint8_t agg) {

	//fprintf(stderr,"swift::AddProgressCallback: transfer %i\n", transfer );

    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;

    //fprintf(stderr,"swift::AddProgressCallback: ft obj %p %p\n", trans, cb );

    trans->cb_agg[trans->cb_installed] = agg;
    trans->callbacks[trans->cb_installed] = cb;
    trans->cb_installed++;
}


void swift::ExternallyRetrieved (int transfer,bin_t piece) {
    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;
    trans->ack_out().set(piece); // that easy
}


void swift::RemoveProgressCallback (int transfer, ProgressCallback cb) {

	//fprintf(stderr,"swift::RemoveProgressCallback: transfer %i\n", transfer );

    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;

    //fprintf(stderr,"swift::RemoveProgressCallback: transfer %i ft obj %p %p\n", transfer, trans, cb );

    for(int i=0; i<trans->cb_installed; i++)
        if (trans->callbacks[i]==cb)
            trans->callbacks[i]=trans->callbacks[--trans->cb_installed];

    for(int i=0; i<trans->cb_installed; i++)
    {
    	fprintf(stderr,"swift::RemoveProgressCallback: transfer %i remain %p\n", transfer, trans->callbacks[i] );
    }
}


FileTransfer::~FileTransfer ()
{
    Channel::CloseTransfer(this);
    files[fd()] = NULL;
    delete picker_;
    delete availability_;
  
    // Arno, 2012-02-06: Cancel cleanup timer, otherwise chaos!
    evtimer_del(&evclean_);
}


FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash()==root_hash)
            return files[i];
    return NULL;
}


int swift:: Find (Sha1Hash hash) {
    FileTransfer* t = FileTransfer::Find(hash);
    if (t)
        return t->fd();
    return -1;
}



bool FileTransfer::OnPexIn (const Address& addr) {

	//fprintf(stderr,"FileTransfer::OnPexIn: %s\n", addr.str() );
	// Arno: this brings safety, but prevents private swift installations.
	// TODO: detect public internet.
	//if (addr.is_private())
	//	return false;
    // Gertjan fix: PEX redo
    if (hs_in_.size()<SWIFT_MAX_CONNECTIONS)
    {
    	// Arno, 2012-02-27: Check if already connected to this peer.
		Channel *c = FindChannel(addr,NULL);
		if (c == NULL)
			new Channel(this,Channel::default_socket(),addr);
		else
			return false;
    }
    return true;
}

//Gertjan
int FileTransfer::RandomChannel (int own_id) {
    binqueue choose_from;
    int i;

    for (i = 0; i < (int) hs_in_.size(); i++) {
        if (hs_in_[i].toUInt() == own_id)
            continue;
        Channel *c = Channel::channel(hs_in_[i].toUInt());
        if (c == NULL || c->transfer().fd() != this->fd()) {
            /* Channel was closed or is not associated with this FileTransfer (anymore). */
            hs_in_[i] = hs_in_[0];
            hs_in_.pop_front();
            i--;
            continue;
        }
        if (!c->is_established())
            continue;
        choose_from.push_back(hs_in_[i]);
    }
    if (choose_from.size() == 0)
        return -1;

    return choose_from[rand() % choose_from.size()].toUInt();
}

void		FileTransfer::OnRecvData(int n)
{
	// Got n ~ 32K
	cur_speed_[DDIR_DOWNLOAD].AddPoint((uint64_t)n);
}

void		FileTransfer::OnSendData(int n)
{
	// Sent n ~ 1K
	cur_speed_[DDIR_UPLOAD].AddPoint((uint64_t)n);
}


void		FileTransfer::OnSendNoData()
{
	// AddPoint(0) everytime we don't AddData gives bad speed measurement
	// batch 32 such events into 1.
	speedzerocount_++;
	if (speedzerocount_ >= 32)
	{
		cur_speed_[DDIR_UPLOAD].AddPoint((uint64_t)0);
		speedzerocount_ = 0;
	}
}


double		FileTransfer::GetCurrentSpeed(data_direction_t ddir)
{
	return cur_speed_[ddir].GetSpeedNeutral();
}


void		FileTransfer::SetMaxSpeed(data_direction_t ddir, double m)
{
	max_speed_[ddir] = m;
	// Arno, 2012-01-04: Be optimistic, forget history.
	cur_speed_[ddir].Reset();
}


double		FileTransfer::GetMaxSpeed(data_direction_t ddir)
{
	return max_speed_[ddir];
}


uint32_t	FileTransfer::GetNumLeechers()
{
	uint32_t count = 0;
	std::set<Channel *>::iterator iter;
    for (iter=mychannels_.begin(); iter!=mychannels_.end(); iter++)
    {
	    Channel *c = *iter;
	    if (c != NULL)
		    if (!c->IsComplete()) // incomplete?
			    count++;
    }
    return count;
}


uint32_t	FileTransfer::GetNumSeeders()
{
	uint32_t count = 0;
	std::set<Channel *>::iterator iter;
    for (iter=mychannels_.begin(); iter!=mychannels_.end(); iter++)
    {
	    Channel *c = *iter;
	    if (c != NULL)
		    if (c->IsComplete()) // complete?
			    count++;
    }
    return count;
}
