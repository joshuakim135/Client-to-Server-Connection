#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

int main(int argc, char *argv[]){
	int opt;
	int p = 1;
	double t = -1;
	int e = -1;
	string filename = "";
	int buffercapacity = MAX_MESSAGE;
	string bcapstring = "";
	bool isNewChan = false;
	bool reqFile = false;
	// take all the arguments first because some of these may go to the server
	// p->patient #, t->time (0-60), e->ecg val (1 or 2), f->
	while ((opt = getopt(argc, argv, "p:t:e:f:m:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				reqFile = true;
				std::cout << "File name: " << filename << endl;
				break;
			case 'p':
				p = atoi (optarg);
				std::cout << "Patient number: " << p << endl;
				break;
			case 't':
				t = atof(optarg);
				std::cout << "time: " << t << endl;
				break;
			case 'e':
				e = atoi (optarg);
				std::cout << "ecg number: " << e << endl;
				break;
			case 'm':
				bcapstring = optarg;
				buffercapacity = atoi (optarg);
				std::cout << "buffer cap: " << buffercapacity << endl;
				break;
			case 'c':
				isNewChan = true;
				std::cout << "Requesting New Channel" << endl;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", "-m", (char*)to_string(buffercapacity).c_str(), nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}

	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	if (t == -1 && e == -1) {
		string contents;
		ostringstream ostr;
		for (int i = 0; i < 1000; i++) {
			double time = i*0.004;
			DataRequest d(p, i*0.004, 1);
			chan.cwrite (&d, sizeof (DataRequest)); // question
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			if (!isValidResponse(&reply)){
				std::cout << "Invalid Response 1" << endl;
				exit (0);
			}

			DataRequest d2(p, i*0.004, 2);
			chan.cwrite (&d2, sizeof (DataRequest));
			double reply2;
			chan.cread(&reply2, sizeof(double));
			if (!isValidResponse(&reply2)) {
				std::cout << "Invalid Response 2" << endl;
				exit(0);
			}

			// print out output for testing
			std::cout << "time: " << time << " -> " << reply << " " << reply2 << endl;
			
			// write to file
			ostr << time << "," << reply << "," << reply2 << endl;
		}
		contents = ostr.str();
		fstream file;
		string fname = to_string(p) + "_first_1000.csv";
		file.open(fname, fstream::out);
		file << contents;
	}

	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	if (reqFile) {
		FileRequest fm (0,0);
		int len = sizeof (FileRequest) + filename.size()+1;
		char buf2 [len];
		memcpy (buf2, &fm, sizeof (FileRequest));
		strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		chan.cwrite (buf2, len);  
		int64 filelen;
		chan.cread (&filelen, sizeof(int64));
		if (isValidResponse(&filelen)){
			std::cout << "File length is: " << filelen << " bytes" << endl;
		}
	
		// original if statemnt content
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
	std::cout << "Client process exited" << endl;
}
