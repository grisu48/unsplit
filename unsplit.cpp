
/* =============================================================================
 * 
 * Title:       
 * Author:      
 * License:     
 * Description: 
 * 
 * 
 * =============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/sendfile.h>

using namespace std;

static size_t buf_size = 16*1024;		// Read-write buffer sizes

static char* read_buffer = NULL;
static char* write_buffer = NULL;

/** Truncate size
  * If set to value >0, then the cleanup will truncate to this size
  * This is intended to be used, if the program fails to revert the file status
  */
static ssize_t truncateBytes = -1;
static string outFile = "";
static int fd_out = 0;
static bool verbose = false;

static void printUsage(char* progname) {
	cout << "unsplit - A simple in-situ file merger" << endl;
	cout << "  It's intended to be a tool to merge file that have been splitted via the 'split' program" << endl;
	cout << "  2017, Felix Niederwanger" << endl;
	
	cout << "Usage: " << progname << " [OPTIONS] FILES" << endl;
	cout << "OPTIONS" << endl;
	cout << "  -h    --help              Print this help message" << endl;
	cout << "  -v    --verbose           Verbose mode" << endl;
	cout << "  -f FILE                   Take the next argument as input file" << endl;
	cout << "  -o FILE                   Write all input FILES to the given output FILE." << endl;
	cout << "                              This disables the in-situ write" << endl;
	cout << "        --delete-after      Delete input files after merge" << endl;
}

static ssize_t merge(const int in, const int out) {
	ssize_t counter = 0;
	
	#if 0
	ssize_t ret;
	while( (ret = ::read(in, read_buffer, buf_size)) > 0) {
		counter += ret;
		ssize_t s = ::write(out, read_buffer, ret);
		if(s < 0) {
			ret = s;
			break;
		} else if(s != ret) {
			cerr << "write bytes (" << s << ") != read bytes (" << ret << ")" << endl;
		}
	}
	// Return error if happening
	if(ret == 0) return counter;
	else return ret;
	#endif
	
	loff_t off = 0;
	while(true) {
		ssize_t ret = sendfile(out, in, &off, buf_size);
		//splice(in, &off_in, out, &off_out, buf_size, 0);//SPLICE_F_MORE);
		if(ret < 0) return ret;		// Error
		if(ret == 0) break;
		else 
			counter += ret;
	}
	return counter;
}

static void revert() {
	if(truncateBytes > 0) {
		if(outFile.size() > 0) {
			if(fd_out > 0) close(fd_out);
			fd_out = 0;
			
			if(verbose) cerr << "Truncating file to initial size ... " << endl;
			int ret = truncate(outFile.c_str(), (off_t)truncateBytes);
			if(ret != 0) {
				cerr << "Truncate failed: " << strerror(errno) << endl;
			}
		}
		truncateBytes = 0;
	}
}

static void cleanup() {
	if(read_buffer != NULL) delete[] read_buffer;
    if(write_buffer != NULL) delete[] write_buffer;
	revert();
}

static void sig_handler(const int sig_no) {
	switch(sig_no) {
		case SIGINT:
		case SIGTERM:
		case SIGABRT:
			static bool emergency = false;
			if(emergency) exit(EXIT_FAILURE);
			emergency = true;
			revert();
			exit(EXIT_FAILURE);
			return;
		case SIGSEGV:
			cerr << "Segmentation fault" << endl;
			revert();		// XXX : This could be very very bad!!
			exit(EXIT_FAILURE);
	}
}

int main(int argc, char** argv) {
    vector<string> files;
    bool inSitu = true;
    bool deleteAfter = false;
    
    // XXX: Replace with getopts
    for(int i=1;i<argc;i++) {
    	string arg(argv[i]);
    	if(arg.size() == 0) continue;
    	if(arg.at(0) == '-') {
    		if (arg == "-h" || arg == "--help") {
    			printUsage(argv[0]);
    			exit(EXIT_SUCCESS);
    		} else if (arg == "-f") {
    			arg = argv[++i];
    			files.push_back(arg);
    		} else if (arg == "-o") {
    			outFile = argv[++i];
    			inSitu = false;
    		} else if (arg == "-v" || arg == "--verbose") {
    			verbose = true;
    		} else if(arg == "--delete-after") {
    			deleteAfter = true;
    		} else {
    			cerr << "Illegal program argument: " << arg << endl;
    			exit(EXIT_FAILURE);
    		}
    	} else {
    		files.push_back(arg);
    	}
    }
    
    if(files.size() == 0) {
    	cerr << "Error: No files given" << endl;
    	cerr << "Type " << argv[0] << " --help if you need help" << endl;
    	exit(EXIT_FAILURE);
    }
    
    if(files.size() == 1 && inSitu) {
    	cerr << "Nothing to do (Merge 1 file in-situ)" << endl;
    	exit(EXIT_SUCCESS);
    }
    
    read_buffer = new char[buf_size];
    write_buffer  = new char[buf_size];
    atexit(cleanup);
    signal(SIGINT, sig_handler);
    signal(SIGABRT, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGTERM, sig_handler);
    
    size_t bytes = 0;
    if(inSitu) {
    	// First file will be the file where we append
    	outFile = files[0];
    	files.erase(files.begin());
    	
    	int flags = O_WRONLY;
    	fd_out = open(outFile.c_str(), flags);
    	if(fd_out < 0) {
    		cerr << "Cannot open file " << outFile << " for writing: " << strerror(errno) << endl;
    		exit(EXIT_FAILURE);
    	}
    	
    	off_t fsize;
		fsize = lseek(fd_out, 0, SEEK_END);
		if(fsize < 0) {
			cerr << "Error seeking file to end: " << strerror(errno) << endl;
			exit(EXIT_FAILURE);
		} else
			truncateBytes = (ssize_t)fsize;
    	
    	for(vector<string>::const_iterator it = files.begin(); it != files.end(); ++it) {
    		string filename = *it;
    		int fd_in = open(filename.c_str(), O_RDONLY);
    		if(fd_in < 0) {
    			cerr << "Error opening file " << filename << ": " << strerror(errno) << endl;
    			if(bytes == 0)
	    			cerr << "  Aborted before writing data." << endl;
    			else
    				cerr << "  Aborted after " << bytes << " bytes. The resulting file may be corrupted!" << endl;
    			cerr << "  Aborted in the middle of the progress. The resulting file may be corrupted!" << endl;
    			close(fd_out);
    			exit(EXIT_FAILURE);
    		}
    		ssize_t s = merge(fd_in, fd_out);
    		if(s < 0) {
    			cerr << "Error merging file " << filename << ": " << strerror(errno) << endl;
    			if(bytes == 0)
	    			cerr << "  Aborted before writing data." << endl;
    			else
    				cerr << "  Aborted after " << bytes << " bytes. The resulting file may be corrupted!" << endl;
    			close(fd_in);
    			close(fd_out);
    			exit(EXIT_FAILURE);
    		} else if(s == 0) {
    			cerr << "  " << filename << " - 0 bytes file" << endl;
    		} else {
    			if(verbose) cout << "  " << filename << " - " << s << " bytes written to " << outFile << "." << endl;
    		}
    		bytes += s;
    		close(fd_in);
    	}
    	
    	fd_out = 0;
    	
    	// Do not truncate anymore
    	truncateBytes = -1;
    	
    	if(deleteAfter) {
			for(vector<string>::const_iterator it = files.begin(); it != files.end(); ++it) {
				string filename = *it;
				if(verbose) cout << "Delete file " << filename << endl;
				if(::remove(filename.c_str()) < 0) {
					cerr << "Error removing file " << filename << ": " << strerror(errno) << endl;
				}
			}
    	}
    	
    } else {
    	cerr << "Non-in-situ method not yet supported" << endl;
    	exit(EXIT_FAILURE);
    }
    
    
    
    if(inSitu && verbose) cout << "Completed - " << bytes << " bytes written in total" << endl;
    return EXIT_SUCCESS;
}

