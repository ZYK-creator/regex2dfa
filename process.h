#ifndef MISSBLIT_PROCESS_H
#define MISSBLIT_PROCESS_H

#include <string>
#include <vector>

/* Simple RAII wrapper around a posix pipe 
 * This makes the error handling for Process a lot nicer
 */
class Pipe {
	int fd[2];
	bool open[2];
public:
	/* Construct a new pipe by calling the posix pipe() function */
	Pipe();
	/* calls close() */
	~Pipe();
	/* Close one end of the pipe */
	void close(int side);
	/* Close all remaining open ends of the pipe */
	void close();
	/* Access the underlying file descriptor */
	int &operator[](std::size_t);
};

/* Class for managing a sub-process interactively 
 * This isn't in a bad state, but like all things posix it is far from perfect
 * In particular I still have only a vague idea of how signals work
 * and all the weird edge cases that can happen.
 * 
 * There also isn't a super nice way to nicely kill the child yet with this
 * interface.
 */
class Process {
public:
	/* Constructor launches a new child process */
    Process(const std::string &command);
	Process(const std::string &command, const std::vector<std::string> &args);
	
	/* Wait for process to exit */
	~Process();

	/* Returns true if the process is still running
       Ret code is set to return code if process is done running */
	bool running();
	bool running(int& ret_code);

	/* Writes data to child's stdin */
	void write(std::string input);
	
	/* closes child's stdin, when there is no more input */
	void close_input();

	/* Reads all output */
	std::string read_all();
    
    /* Reads pending output */
    std::string read_some();
    
    /* Reads with current blocking mode*/
    std::string read();
    
    /* set read() to be non-blocking */
    void set_nonblocking();
    /* set read() to be blocking */
    void set_blocking();

    void wait();
private:
	int pid;
	Pipe in;  //Write end of pipe
	Pipe out; //Read end of pipe
};

#endif //MISSBLIT_PROCESS_H
