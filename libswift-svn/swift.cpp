/*
 *  swift.cpp
 *  swift the multiparty transport protocol
 *
 *  Created by Victor Grishchenko on 2/15/10.
 *  Copyright 2009-2012 TECHNISCHE UNIVERSITEIT DELFT. All rights reserved.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "compat.h"
#include "swift.h"
#include <cfloat>

using namespace swift;


// Local constants
#define RESCAN_DIR_INTERVAL	30 // seconds


// Local prototypes
#define quit(...) {fprintf(stderr,__VA_ARGS__); exit(1); }
int OpenSwiftFile(const TCHAR* filename, const Sha1Hash& hash, Address tracker, bool force_check_diskvshash, uint32_t chunk_size);
int OpenSwiftDirectory(const TCHAR* dirname, Address tracker, bool force_check_diskvshash, uint32_t chunk_size);

void ReportCallback(int fd, short event, void *arg);
void EndCallback(int fd, short event, void *arg);
void RescanDirCallback(int fd, short event, void *arg);


// Gateway stuff
bool InstallHTTPGateway(struct event_base *evbase,Address addr,uint32_t chunk_size, double *maxspeed);
bool InstallStatsGateway(struct event_base *evbase,Address addr);
bool InstallCmdGateway (struct event_base *evbase,Address cmdaddr,Address httpaddr);
bool HTTPIsSending();
bool StatsQuit();
void CmdGwUpdateDLStatesCallback();


// Global variables
struct event evreport, evrescan, evend;
int* single_fds = 0;
int n_single_fds = 0;
bool file_enable_checkpoint = false;
bool* files_checkpointed = 0;
bool report_progress = false;
bool quiet=false;
bool exitoncomplete=false;
bool httpgw_enabled=false,cmdgw_enabled=false;
// Gertjan fix
bool do_nat_test = false;

char **scan_dirnames = 0;
int n_scan_dirnames = 0;
uint32_t chunk_size = SWIFT_DEFAULT_CHUNK_SIZE;
Address tracker;

/*
 * SCHAAP:
 *
 * FIXME: Multifile stuff breaks ratelimiting!
 * So, somewhere the core should ratelimit more globally instead of locally.
 * Not my problem, really.
 */


int main (int argc, char** argv)
{
    static struct option long_options[] =
    {
        {"hash",    required_argument, 0, 'h'},
        {"file",    required_argument, 0, 'f'},
        {"dir",     required_argument, 0, 'd'}, // SEEDDIR reuse
        {"listen",  required_argument, 0, 'l'},
        {"tracker", required_argument, 0, 't'},
        {"debug",   no_argument, 0, 'D'},
        {"progress",no_argument, 0, 'p'},
        {"httpgw",  required_argument, 0, 'g'},
        {"wait",    optional_argument, 0, 'w'},
        {"nat-test",no_argument, 0, 'N'},
        {"statsgw", required_argument, 0, 's'}, // SWIFTPROC
        {"cmdgw",   required_argument, 0, 'c'}, // SWIFTPROC
        {"destdir", required_argument, 0, 'o'}, // SWIFTPROC
        {"uprate",  required_argument, 0, 'u'}, // RATELIMIT
        {"downrate",required_argument, 0, 'y'}, // RATELIMIT
        {"checkpoint",no_argument, 0, 'H'},
        {"chunksize",required_argument, 0, 'z'}, // CHUNKSIZE
        {"printurl",no_argument, 0, 'm'},
        {"seed-file",required_argument, 0, 'x'},
        {0, 0, 0, 0}
    };

    Sha1Hash* root_hashes = 0;
    int n_root_hashes = 0;
    char** filenames = 0;
    int n_filenames = 0;
    char** seedfilenames = 0;
    int n_seedfilenames = 0;
    const char *destdir = 0, *trackerargstr = 0; // UNICODE?
    bool printurl=false;
    Address bindaddr;
    Address httpaddr;
    Address statsaddr;
    Address cmdaddr;
    tint wait_time = 0;
    double maxspeed[2] = {DBL_MAX,DBL_MAX};
    bool download_and_quit = true;
    bool ratelimits_given = false;

    LibraryInit();
    Channel::evbase = event_base_new();

    int c,n;
    while ( -1 != (c = getopt_long (argc, argv, ":h:f:d:l:t:D:pg:s:c:o:u:y:z:wBNHmx:", long_options, 0)) ) {
        switch (c) {
            case 'h':
                if (strlen(optarg)!=40)
                    quit("SHA1 hash must be 40 hex symbols\n");
                n_root_hashes++;
                root_hashes = (Sha1Hash*) realloc( root_hashes, n_root_hashes * sizeof( Sha1Hash ) );
                root_hashes[n_root_hashes - 1] = Sha1Hash(true,optarg); // FIXME ambiguity
                if (root_hashes[n_root_hashes - 1]==Sha1Hash::ZERO)
                    quit("SHA1 hash must be 40 hex symbols\n");
                fprintf( stderr, "DEBUG: root_hash\n" );
                if( n_root_hashes - n_filenames > 1 ) {
                    fprintf( stderr, "DEBUG: patch up filenames\n" );
                    n_filenames++;
                    filenames = (char**) realloc( filenames, n_filenames * sizeof( char* ) );
                    filenames[n_filenames - 1] = NULL;
                }
                break;
            case 'f':
                n_filenames++;
                filenames = (char**) realloc( filenames, n_filenames * sizeof( char* ) );
                filenames[n_filenames-1] = strdup(optarg);
                fprintf( stderr, "DEBUG: file %s\n", filenames[n_filenames-1]);
                if( n_filenames - n_root_hashes > 1 ) {
                    fprintf( stderr, "DEBUG: patch up root_hashes\n" );
                    n_root_hashes++;
                    root_hashes = (Sha1Hash*) realloc( root_hashes, n_root_hashes * sizeof( Sha1Hash ) );
                    root_hashes[n_root_hashes - 1] = Sha1Hash(Sha1Hash::ZERO);
                }
                break;
            case 'x':
                n_seedfilenames++;
                seedfilenames = (char**) realloc( seedfilenames, n_seedfilenames * sizeof( char* ) );
                seedfilenames[n_seedfilenames-1] = strdup(optarg);
                fprintf( stderr, "DEBUG: seedfile %s\n", seedfilenames[n_seedfilenames-1]);
                break;
            case 'd':
                n_scan_dirnames++;
                scan_dirnames = (char**)realloc(scan_dirnames, n_scan_dirnames * sizeof( char* ));
                scan_dirnames[n_scan_dirnames - 1] = strdup(optarg);
                break;
            case 'l':
                bindaddr = Address(optarg);
                if (bindaddr==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                wait_time = TINT_NEVER;
                break;
            case 't':
                tracker = Address(optarg);
                trackerargstr = strdup(optarg); // UNICODE
                if (tracker==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                SetTracker(tracker);
                break;
            case 'D':
                Channel::debug_file = optarg ? fopen(optarg,"a") : stderr;
                break;
            // Arno hack: get opt diff Win32 doesn't allow -D without arg
            case 'B':
            	fprintf(stderr,"SETTING DEBUG TO STDOUT\n");
                Channel::debug_file = stderr;
                break;
            case 'p':
                report_progress = true;
                break;
            case 'g':
            	httpgw_enabled = true;
                httpaddr = Address(optarg);
				wait_time = TINT_NEVER; // seed
                break;
            case 'w':
                if (optarg) {
                    char unit = 'u';
                    long long int w = 0;
                    if (sscanf(optarg,"%lli%c",&w,&unit)!=2)
                        quit("time format: 1234[umsMHD], e.g. 1M = one minute\n");
                    wait_time = (swift::tint)w;

                    switch (unit) {
                        case 'D': wait_time *= 24;
                        case 'H': wait_time *= 60;
                        case 'M': wait_time *= 60;
                        case 's': wait_time *= 1000;
                        case 'm': wait_time *= 1000;
                        case 'u': break;
                        default:  quit("time format: 1234[umsMHD], e.g. 1D = one day\n");
                    }
                } else
                    wait_time = TINT_NEVER;
                break;
            case 'N': // Gertjan fix
                do_nat_test = true;
                break;
            case 's': // SWIFTPROC
                statsaddr = Address(optarg);
                if (statsaddr==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                break;
            case 'c': // SWIFTPROC
            	cmdgw_enabled = true;
                cmdaddr = Address(optarg);
                if (cmdaddr==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                wait_time = TINT_NEVER; // seed
                break;
            case 'o': // SWIFTPROC
                destdir = strdup(optarg); // UNICODE
                break;
            case 'u': // RATELIMIT
            	n = sscanf(optarg,"%lf",&maxspeed[DDIR_UPLOAD]);
            	if (n != 1)
            		quit("uprate must be KiB/s as float\n");
            	maxspeed[DDIR_UPLOAD] *= 1024.0;
                ratelimits_given = true;
            	break;
            case 'y': // RATELIMIT
            	n = sscanf(optarg,"%lf",&maxspeed[DDIR_DOWNLOAD]);
            	if (n != 1)
            		quit("downrate must be KiB/s as float\n");
            	maxspeed[DDIR_DOWNLOAD] *= 1024.0;
                ratelimits_given = true;
            	break;
            case 'H': //CHECKPOINT
                file_enable_checkpoint = true;
                break;
            case 'z': // CHUNKSIZE
            	n = sscanf(optarg,"%i",&chunk_size);
            	if (n != 1)
            		quit("chunk size must be bytes as int\n");
            	break;
            case 'm': // printurl
            	printurl = true;
            	quiet = true;
            	wait_time = 0;
            	break;
        }

    }   // arguments parsed

    // Complete the filenames and root_hashes arrays.
    while( n_filenames > n_root_hashes ) {
        fprintf( stderr, "DEBUG: patch up root_hashes after\n" );
        n_root_hashes++;
        root_hashes = (Sha1Hash*) realloc( root_hashes, n_root_hashes * sizeof( Sha1Hash ) );
        root_hashes[n_root_hashes-1] = Sha1Hash(Sha1Hash::ZERO);
    }
    while( n_root_hashes > n_filenames ) {
        fprintf( stderr, "DEBUG: patch up filenames after\n" );
        n_filenames++;
        filenames = (char**) realloc( filenames, n_filenames * sizeof( char* ) );
        filenames[n_filenames-1] = NULL;
    }
    // Convert the seedfilenames to filenames with empty root_hashes.
    // Effectively the backwards compatibility option is the actual option internally and the new -x is just to remove ambiguity.
    if( n_seedfilenames > 0 ) {
        fprintf(stderr, "DEBUG: Merging -x: %d\n", n_seedfilenames );
        root_hashes = (Sha1Hash*) realloc( root_hashes, (n_root_hashes+n_seedfilenames) * sizeof(Sha1Hash) );
        filenames = (char**) realloc( filenames, (n_filenames+n_seedfilenames) * sizeof( char* ) );
        for( int i = 0; i < n_seedfilenames; i++ ) {
            root_hashes[n_root_hashes++] = Sha1Hash(Sha1Hash::ZERO);
            filenames[n_filenames++] = seedfilenames[i];
        }
        free( seedfilenames );
        seedfilenames = NULL;
        n_seedfilenames = 0;
    }

    if( ratelimits_given && ( n_root_hashes > 1 || n_scan_dirnames > 0 ) ) {
        // Just print a warning to be sure the user knows about this.
        fprintf( stderr, "Warning! Upload and download rate limits are only partly functional with multiple uploads or downloads. They will only be set on each single file upload or download (-h, -f and -x) separately. Global rate limiting is not available.\n" );
    }

    if (httpgw_enabled)
    {
    	// Change current directory to a temporary one
#ifdef _WIN32
    	if (destdir == 0) {
    		std::string destdirstr = gettmpdir();
    		!::SetCurrentDirectory(destdirstr.c_str());
    	}
    	else
    		!::SetCurrentDirectory(destdir);
        TCHAR szDirectory[MAX_PATH] = "";

        !::GetCurrentDirectory(sizeof(szDirectory) - 1, szDirectory);
        fprintf(stderr,"CWD %s\n",szDirectory);
#else
        if (destdir == 0)
        	chdir(gettmpdir().c_str());
        else
        	chdir(destdir);
#endif
    }
      
    if (bindaddr!=Address()) { // seeding
        if (Listen(bindaddr)<=0)
            quit("cant listen to %s\n",bindaddr.str())
    } else if (tracker!=Address() || httpgw_enabled || cmdgw_enabled) { // leeching
    	evutil_socket_t sock = INVALID_SOCKET;
        for (int i=0; i<=10; i++) {
            bindaddr = Address((uint32_t)INADDR_ANY,0);
            sock = Listen(bindaddr);
            if (sock>0)
                break;
            if (i==10)
                quit("cant listen on %s\n",bindaddr.str());
        }
        if (!quiet)
        	fprintf(stderr,"swift: My listen port is %d\n", BoundAddress(sock).port() );
    }

    if (tracker!=Address())
        SetTracker(tracker);

    if (httpgw_enabled) {
        InstallHTTPGateway(Channel::evbase,httpaddr,chunk_size,maxspeed);
        download_and_quit = false;
    }
    if (cmdgw_enabled) {
		InstallCmdGateway(Channel::evbase,cmdaddr,httpaddr);
        download_and_quit = false;
    }

    // TRIALM36: Allow browser to retrieve stats via AJAX and as HTML page
    if (statsaddr != Address())
    	InstallStatsGateway(Channel::evbase,statsaddr);


    if (!cmdgw_enabled && !httpgw_enabled)
    {
		int ret = -1;
		if (n_filenames > 0 || n_root_hashes > 0) {
			// Single files
            for( int i = 0; i < n_root_hashes; i++ ) {
                if (root_hashes[i]!=Sha1Hash::ZERO) {
                    if( !filenames[i] )
                        filenames[i] = strdup(root_hashes[i].hex().c_str());
                }
                else {
                    // We're uploading as well
                    download_and_quit = false;
                }

                int fd = OpenSwiftFile(filenames[i],root_hashes[i],Address(),false,chunk_size);
                if (fd < 0)
                    quit("cannot open file %s",filenames[i]);
                if (printurl) {
                    if (trackerargstr == NULL)
                        trackerargstr = "";
                    printf("tswift://%s/%s$%i\n", trackerargstr, RootMerkleHash(fd).hex().c_str(), chunk_size);

                    // Arno, 2012-01-04: LivingLab: Create checkpoint such that content
                    // can be copied to scanned dir and quickly loaded
                    swift::Checkpoint(fd);
                }
                else
                    printf("Root hash: %s\n", RootMerkleHash(fd).hex().c_str());

                // RATELIMIT
                FileTransfer *ft = FileTransfer::file(fd);
                ft->SetMaxSpeed(DDIR_DOWNLOAD,maxspeed[DDIR_DOWNLOAD]);
                ft->SetMaxSpeed(DDIR_UPLOAD,maxspeed[DDIR_UPLOAD]);

                // ret isn't really used, other than to check it's no -1
                ret = 0;
            }
		}
		if (n_scan_dirnames > 0) {
            for( int i = 0; i < n_scan_dirnames; i++ ) {
    			ret = OpenSwiftDirectory(scan_dirnames[i],Address(),false,chunk_size);
                if( ret < 0 )
                    break;
            }
            // We're uploading as well
            download_and_quit = false;
        }

		// No file/dir nor HTTP gateway nor CMD gateway, will never know what to swarm
		if (ret == -1) {
			fprintf(stderr,"Usage:\n");
			fprintf(stderr,"  -h, --hash\troot Merkle hash for the transmission (multiple allowed)\n");
			fprintf(stderr,"  -f, --file\tname of file to use (root hash by default, multiple allowed)\n");
            fprintf(stderr,"  -x, --seed-file\tname of the file to seed (multiple allowed)\n");
            fprintf(stderr,"  -d, --dir\tname of the directory to monitor for files to be uploaded (multiple allowed)\n" );
			fprintf(stderr,"  -l, --listen\t[ip:|host:]port to listen to (default: random)\n");
			fprintf(stderr,"  -t, --tracker\t[ip:|host:]port of the tracker (default: none)\n");
			fprintf(stderr,"  -D, --debug\tfile name for debugging logs (default: stdout)\n");
			fprintf(stderr,"  -B\tdebugging logs to stdout (win32 hack)\n");
			fprintf(stderr,"  -p, --progress\treport transfer progress\n");
			fprintf(stderr,"  -g, --httpgw\t[ip:|host:]port to bind HTTP content gateway to (no default)\n");
			fprintf(stderr,"  -s, --statsgw\t[ip:|host:]port to bind HTTP stats listen socket to (no default)\n");
			fprintf(stderr,"  -c, --cmdgw\t[ip:|host:]port to bind CMD listen socket to (no default)\n");
			fprintf(stderr,"  -o, --destdir\tdirectory for saving data (default: none)\n");
			fprintf(stderr,"  -u, --uprate\tupload rate limit in KiB/s (default: unlimited)\n");
			fprintf(stderr,"  -y, --downrate\tdownload rate limit in KiB/s (default: unlimited)\n");
			fprintf(stderr,"  -w, --wait\tlimit running time, e.g. 1[DHMs] (default: infinite with -l, -g)\n");
			fprintf(stderr,"  -H, --checkpoint\tcreate checkpoint of file when complete for fast restart\n");
			fprintf(stderr,"  -z, --chunksize\tchunk size in bytes (default: %d)\n", SWIFT_DEFAULT_CHUNK_SIZE);
			fprintf(stderr,"  -m, --printurl\tcompose URL from tracker, file and chunksize\n");
            fprintf(stderr,"For backwards compatibility -f and -h can be used together and will be paired from left to right to provide downloads to named files. Any -f that can't be paired with a -h is interpreted as -x.\n");
            fprintf(stderr,"To avoid ambiguity, always use -x for files to be seeded and keep -h/-f pairs strictly interleaved and in that order.\n");
			return 1;
		}
    }

    // Arno, 2012-01-04: Allow download and quit mode
    if (download_and_quit && wait_time == 0) {
    	wait_time = TINT_NEVER;
    	exitoncomplete = true;
    }

    // End after wait_time
    if ((long)wait_time > 0) {
    	evtimer_assign(&evend, Channel::evbase, EndCallback, NULL);
    	evtimer_add(&evend, tint2tv(wait_time));
    }

    // Enter mainloop, if daemonizing
    if (wait_time == TINT_NEVER || (long)wait_time > 0) {
		// Arno: always, for statsgw, rate control, etc.
		evtimer_assign(&evreport, Channel::evbase, ReportCallback, NULL);
		evtimer_add(&evreport, tint2tv(TINT_SEC));


		// Arno:
		if (n_scan_dirnames > 0) {
            evtimer_assign(&evrescan, Channel::evbase, RescanDirCallback, NULL);
            evtimer_add(&evrescan, tint2tv(RESCAN_DIR_INTERVAL*TINT_SEC));
		}


		fprintf(stderr,"swift: Mainloop\n");
		// Enter libevent mainloop
		event_base_dispatch(Channel::evbase);

		// event_base_loopexit() was called, shutting down
    }

    // Arno, 2012-01-03: Close all transfers
	for (int i=0; i<FileTransfer::files.size(); i++) {
		if (FileTransfer::files[i] != NULL)
            Close(FileTransfer::files[i]->fd());
    }

    if (Channel::debug_file)
        fclose(Channel::debug_file);

    swift::Shutdown();

    return 0;
}



int OpenSwiftFile(const TCHAR* filename, const Sha1Hash& hash, Address tracker, bool force_check_diskvshash, uint32_t chunk_size)
{
	std::string bfn;
	bfn.assign(filename);
	bfn.append(".mbinmap");
	const char *binmap_filename = bfn.c_str();

	// Arno, 2012-01-03: Hack to discover root hash of a file on disk, such that
	// we don't load it twice while rescanning a dir of content.
	HashTree *ht = new HashTree(true,binmap_filename);

	//	fprintf(stderr,"swift: parsedir: File %s may have hash %s\n", filename, ht->root_hash().hex().c_str() );

	int fd = swift::Find(ht->root_hash());
	if (fd == -1) {
		if (!quiet)
			fprintf(stderr,"swift: parsedir: Opening %s\n", filename);

		fd = swift::Open(filename,hash,tracker,force_check_diskvshash,true,chunk_size);
        if( fd >= 0 ) {
            n_single_fds++;
            single_fds = (int*) realloc( single_fds, n_single_fds * sizeof( int ) );
            files_checkpointed = (bool*) realloc( files_checkpointed, n_single_fds * sizeof( bool ) );
            single_fds[n_single_fds-1] = fd;
            files_checkpointed[n_single_fds-1] = false;
        }
	}
	else if (!quiet)
		fprintf(stderr,"swift: parsedir: Ignoring loaded %s\n", filename);
	return fd;
}


int OpenSwiftDirectory(const TCHAR* dirname, Address tracker, bool force_check_diskvshash, uint32_t chunk_size)
{
#ifdef _WIN32
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	TCHAR pathsearch[MAX_PATH];
	strcpy(pathsearch,dirname);
	strcat(pathsearch,"\\*.*");
	hFind = FindFirstFile(pathsearch, &FindFileData);
	if(hFind != INVALID_HANDLE_VALUE) {
	    do {
	    	//fprintf(stderr,"swift: parsedir: %s\n", FindFileData.cFileName);
	    	if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && !strstr(FindFileData.cFileName,".mhash") && !strstr(FindFileData.cFileName,".mbinmap") ) {
				TCHAR path[MAX_PATH];
				strcpy(path,dirname);
				strcat(path,"\\");
				strcat(path,FindFileData.cFileName);

				int fd = OpenSwiftFile(path,Sha1Hash::ZERO,tracker,force_check_diskvshash,chunk_size);
				if (fd >= 0)
					Checkpoint(fd);
	    	}
	    } while(FindNextFile(hFind, &FindFileData));

	    FindClose(hFind);
	    return 1;
	}
	else
		return -1;
#else
	DIR *dirp = opendir(dirname);
	if (dirp == NULL)
		return -1;

	while(1)
	{
		struct dirent *de = readdir(dirp);
		if (de == NULL)
			break;
		if ((de->d_type & DT_DIR) !=0 || strstr(de->d_name,".mhash") || strstr(de->d_name,".mbinmap"))
			continue;
		char path[PATH_MAX];
		strcpy(path,dirname);
		strcat(path,"/");
		strcat(path,de->d_name);

		int fd = OpenSwiftFile(path,Sha1Hash::ZERO,tracker,force_check_diskvshash,chunk_size);
		if (fd >= 0)
			Checkpoint(fd);
	}
	closedir(dirp);
	return 1;
#endif
}



int CleanSwiftDirectory(const TCHAR* dirname)
{
	std::set<int>	delset;
	std::vector<FileTransfer*>::iterator iter;
	for (iter=FileTransfer::files.begin(); iter!=FileTransfer::files.end(); iter++)
	{
		FileTransfer *ft = *iter;
		if (ft != NULL) {
			std::string filename = ft->file().filename();
#ifdef WIN32
			struct _stat buf;
#else
			struct stat buf;
#endif
			fprintf(stderr,"swift: clean: Checking %s\n", filename.c_str() );
			int res = stat( filename.c_str(), &buf );
			if( res < 0 && errno == ENOENT) {
				fprintf(stderr,"swift: clean: Missing %s\n", filename.c_str() );
				delset.insert(ft->fd());
			}
		}
	}

	std::set<int>::iterator	iiter;
	for (iiter=delset.begin(); iiter!=delset.end(); iiter++)
	{
		int fd = *iiter;
		fprintf(stderr,"swift: clean: Deleting transfer %d\n", fd );
		swift::Close(fd);
	}

	return 1;
}





void ReportCallback(int fd, short event, void *arg) {
	// Called every second to print/calc some stats

	if (n_single_fds  > 0)
	{
        bool allComplete = false;
        uint64_t completed = 0;
        uint64_t size = 0;
        uint64_t seqcompleted = 0;
        double upspeed = 0;
        double downspeed = 0;

        for( int i = 0; i < n_single_fds; i++ ) {
            int single_fd = single_fds[i];
            FileTransfer* ft = FileTransfer::file(single_fd);
            if( allComplete && !IsComplete(single_fd) )
                allComplete = false;
            if( report_progress ) {
                completed += Complete(single_fd);
                size += Size(single_fd);
                seqcompleted += SeqComplete(single_fd);

                upspeed += ft->GetCurrentSpeed(DDIR_UPLOAD);
                downspeed += ft->GetCurrentSpeed(DDIR_DOWNLOAD);
            }
            ft->OnRecvData(0);
            ft->OnSendData(0);

            // CHECKPOINT
            if (file_enable_checkpoint && !files_checkpointed[i] && IsComplete(single_fd))
            {
                std::string binmap_filename = ft->file().filename();
                binmap_filename.append(".mbinmap");
                fprintf(stderr,"swift: Complete, checkpointing %s\n", binmap_filename.c_str() );
                FILE *fp = fopen(binmap_filename.c_str(),"wb");
                if (!fp) {
                    print_error("cannot open mbinmap for writing");
                    return;
                }
                if (ft->file().serialize(fp) < 0)
                    print_error("writing to mbinmap");
                else
                    files_checkpointed[i] = true;
                fclose(fp);
            }
        }
		if (report_progress) {
			fprintf(stderr,
				"%s %lli of %lli (seq %lli) %lli dgram %lli bytes up, "	\
				"%lli dgram %lli bytes down\n",
				allComplete ? "DONE" : "done",
				(long long int)completed, (long long int)size, (long long int)seqcompleted,
				(long long int)Channel::global_dgrams_up, (long long int)Channel::global_raw_bytes_up,
				(long long int)Channel::global_dgrams_down, (long long int)Channel::global_raw_bytes_down );
        	fprintf(stderr,"upload %lf\n",upspeed);
        	fprintf(stderr,"dwload %lf\n",downspeed);
		}

    	if (exitoncomplete && allComplete)
    		// Download and stop mode
    	    event_base_loopexit(Channel::evbase, NULL);

	}
    if (httpgw_enabled)
    {
        fprintf(stderr,".");

        // ARNOSMPTODO: Restore fail behaviour when used in SwarmPlayer 3000.
        if (!HTTPIsSending()) {
        	// TODO
        	//event_base_loopexit(Channel::evbase, NULL);
            return;
        }
    }
    if (StatsQuit())
    {
    	// SwarmPlayer 3000: User click "Quit" button in webUI.
    	struct timeval tv;
    	tv.tv_sec = 1;
    	int ret = event_base_loopexit(Channel::evbase,&tv);
    }
	// SWIFTPROC
	// ARNOSMPTODO: SCALE: perhaps less than once a second if many swarms
	CmdGwUpdateDLStatesCallback();

	// Gertjan fix
	// Arno, 2011-10-04: Temp disable
    //if (do_nat_test)
    //     nat_test_update();

	evtimer_add(&evreport, tint2tv(TINT_SEC));
}

void EndCallback(int fd, short event, void *arg) {
	// Called when wait timer expires == fixed time daemon
    event_base_loopexit(Channel::evbase, NULL);
}


void RescanDirCallback(int fd, short event, void *arg) {

	// SEEDDIR
	// Rescan dir: CAREFUL: this is blocking, better prepare .m* files first
	// by running swift separately and then copy content + *.m* to scanned dir,
	// such that a fast restore from checkpoint is done.
	//
    for( int i = 0; i < n_scan_dirnames; i++ ) {
    	OpenSwiftDirectory(scan_dirnames[i],tracker,false,chunk_size);

	    CleanSwiftDirectory(scan_dirnames[i]);
    }

	evtimer_add(&evrescan, tint2tv(RESCAN_DIR_INTERVAL*TINT_SEC));
}



#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return main(__argc,__argv);
}
#endif

