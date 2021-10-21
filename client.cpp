#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
using namespace std;

int main(int argc, char *argv[]){
	int opt;
	int p = 1;
	double t = -0.1;
	int e = -1;
	string filename = "";
	int buffercapacity = MAX_MESSAGE;
	string bcapstring = "";
	bool isNewChan = false;
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "p:t:e:f:m:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
			case 'p':
				p = atoi (optarg);
			case 't':
				t = atof(optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'm':
				bcapstring = optarg;
				buffercapacity = atoi (optarg);
				break;
			case 'c':
				isNewChan = true;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", "-m", (char*)bcapstring.c_str(), nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}
	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	if (t == -.1 && e == -1) {
		for (int i = 0; i < 1000; i++) {
			DataRequest d(p, i*0.004, 1);
			chan.cwrite (&d, sizeof (DataRequest)); // question
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			if (!isValidResponse(&reply)){
				exit (0);
				// cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
			}

		}
	}

	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	DataRequest d (p, t, e);
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
	
	int64 rem = filelen;
	FileRequest* f = (FileRequest*) buf2;
	ofstream of (filename);
	char recvBuf [buffercapacity];
	while (rem > 0) {
		f->length = min(rem, (int64)buffercapacity);
		chan.cwrite (buf2, sizeof (buf2));
		chan.cread(recvBuf, buffercapacity);
		of.write(recvBuf, f->length);
		rem -= 	f->length;
	}
	
	if (isNewChan) {
		Request nc (NEWCHAN_REQ_TYPE);
		chan.cwrite(&nc, sizeof(nc));
		char chanName [1024];
		chan.cread(chanName, sizeof(chanName));

		FIFORequestChannel newchan (chanName, FIFORequestChannel::CLIENT_SIDE);

		Request q (QUIT_REQ_TYPE);
		newchan.cwrite (&q, sizeof(Request));
	}
	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;
}
