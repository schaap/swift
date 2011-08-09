/*
 *  transfer.cpp
 *  some transfer-scope code
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <errno.h>
#include <string>
#include <sstream>
#include "swift.h"

#include "ext/seq_picker.cpp" // FIXME FIXME FIXME FIXME 

using namespace swift;

std::vector<FileTransfer*> FileTransfer::files(20);

#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))

// FIXME: separate Bootstrap() and Download(), then Size(), Progress(), SeqProgress()

FileTransfer::FileTransfer (const char* filename, const Sha1Hash& _root_hash) :
    file_(filename,_root_hash), hs_in_offset_(0), cb_installed(0), files_index_(-1)
{
    // NOTE: This should probably be guarded against multithreaded access faults (very easy to break this array)
    for(int i=0; i<files.size();i++) {
        if(!files[i]) {
            files_index_=i;
            break;
        }
    }
    if(files_index_==-1) {
        files_index_=files.size()+1;
        files.resize(files_index_);
    }
    files[files_index_] = this;
    picker_ = new SeqPiecePicker(this);
    picker_->Randomize(rand()&63);
    init_time_ = Datagram::Time();
}


void    Channel::CloseTransfer (FileTransfer* trans) {
    for(int i=0; i<Channel::channels.size(); i++) 
        if (Channel::channels[i] && Channel::channels[i]->transfer_==trans) 
            delete Channel::channels[i];
}


void FileTransfer::AddProgressCallback (ProgressCallback cb, uint8_t agg) {
    cb_agg[cb_installed] = agg;
    callbacks[cb_installed] = cb;
    cb_installed++;
}


void swift::ExternallyRetrieved (FileTransfer* trans,bin64_t piece) {
    if (!trans)
        return;
    trans->ack_out().set(piece); // that easy
}


void FileTransfer::RemoveProgressCallback (ProgressCallback cb) {
    for(int i=0; i<cb_installed; i++) {
        if (callbacks[i]==cb) {
            callbacks[i]=callbacks[--cb_installed];
            cb_agg[i]=cb_agg[cb_installed];
        }
    }
}

void FileTransfer::callCallbacks(bin64_t& cover) {
    for(int i=0; i<cb_installed; i++) {
        if(cover.layer()>=cb_agg[i])
            callbacks[i](this,cover);
    }
}


FileTransfer::~FileTransfer ()
{
    Channel::CloseTransfer(this);
    files[files_index_] = NULL;
    delete picker_;
}


FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash()==root_hash)
            return files[i];
    return NULL;
}


FileTransfer*       swift:: Find (Sha1Hash hash) {
    FileTransfer* t = FileTransfer::Find(hash);
    if (t)
        return t;
    return NULL;
}



void            FileTransfer::OnPexIn (const Address& addr) {
    for(int i=0; i<hs_in_.size(); i++) {
        Channel* c = Channel::channel(hs_in_[i]);
        if (c && c->transfer().files_index_==files_index_ && c->peer()==addr)
            return; // already connected
    }
    if (hs_in_.size()<20) {
        new Channel(this,Datagram::default_socket(),addr);
    } else {
        pex_in_.push_back(addr);
        if (pex_in_.size()>1000)
            pex_in_.pop_front();
    }
}


int        FileTransfer::RevealChannel (int& pex_out_) { // FIXME brainfuck
    pex_out_ -= hs_in_offset_;
    if (pex_out_<0)
        pex_out_ = 0;
    while (pex_out_<hs_in_.size()) {
        Channel* c = Channel::channel(hs_in_[pex_out_]);
        if (c && c->transfer().files_index_==files_index_) {
            if (c->is_established()) {
                pex_out_ += hs_in_offset_ + 1;
                return c->id();
            } else
                pex_out_++;
        } else {
            hs_in_[pex_out_] = hs_in_[0];
            hs_in_.pop_front();
            hs_in_offset_++;
        }
    }
    pex_out_ += hs_in_offset_;
    return -1;
}

