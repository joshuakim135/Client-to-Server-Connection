#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
using namespace std;

int main(int argc, char *argv[]){

	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	string filename = "";
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}
	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	DataRequest d (p, t, e);
	chan.cwrite (&d, sizeof (DataRequest)); // question
	double reply;
	chan.cread (&reply, sizeof(double)); //answer
	if (isValidResponse(&reply)){
		cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
	}

	
	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [len];
	memcpy (buf2, &fm, sizeof (FileRequest));
	strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	chan.cwrite (buf2, len);  
	int64 filelen;
	chan.cread (&filelen, sizeof(int64));
	if (isValidResponse(&filelen)){
		cout << "File length is: " << filelen << " bytes" << endl;
	}
	
	
	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

}
