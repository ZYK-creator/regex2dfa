#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstring>
#include "process.h"

using namespace std;
	
Pipe::Pipe() : open{1,1} {
	int res = pipe(fd);
	if(res == -1)
		throw std::runtime_error("pipe() returned -1");
}

Pipe::~Pipe() {
	close();
}

void Pipe::close(int i) {
	if(i != 0 && i != 1) 
		throw std::out_of_range(  std::string("Pipe index ") + std::to_string(i)
		                        + " is out of range");
	if(open[i]) {
        	open[i] = false;
		int res = ::close(fd[i]);
		if(res == -1)
			throw std::runtime_error("close() returned -1");
	}
}

void Pipe::close() {
	//Consider a try catch here, to at least attempt to close the second
	//end of the pipe if the first end fails to close...
	close(0);
	close(1);
}

int &Pipe::operator[](std::size_t i) {
	if(i != 0 && i != 1) 
		throw std::out_of_range(  std::string("Pipe index ") + std::to_string(i)
		                        + " is out of range");
	return fd[i];
}


Process::Process(const std::string &command) : Process(command, {})
{}

Process::Process(const std::string &command,
                 const std::vector<std::string> &args)
{
	//convert the arguments into an exec friendly form
	std::vector<const char *> c_args;
	c_args.reserve(args.size()+2);
	c_args.push_back(command.c_str());
	for(const std::string &s : args) 
		c_args.push_back(s.c_str());
	c_args.push_back(nullptr);

	pid = fork();
	if(pid == -1)
		throw std::runtime_error("fork returned -1");
	if(!pid) {
		/* Child process */
		/* Close unused pipes */
		in.close(1); //input  write
		out.close(0); //output read
		/* set up std io for pipes */
		dup2(in[0],  STDIN_FILENO ); //set stdin  to read  channel of in
		dup2(out[1], STDOUT_FILENO); //set stdout to write channel of out
		dup2(out[1], STDERR_FILENO); //set stderr to write channel of out

		/* execute program */
		int res = execvp(command.c_str(),
		                 const_cast<char * const *>(c_args.data()));
        	throw std::runtime_error(std::string("execv returned ")
	                         + std::to_string(res) + ": "
	                         + strerror(errno));
	}
	else {
		/* Parent process */
		/* Close unused pipes */
		in.close(0); //input read
		out.close(1); //output write
	}
}

void Process::set_nonblocking() {
	int flags;
	flags = fcntl(out[0], F_GETFL, 0);
	if(flags == -1)
		throw std::runtime_error("fcntl returned -1");
	flags = fcntl(out[0], F_SETFL, flags | O_NONBLOCK);
	if(flags == -1) 
		throw std::runtime_error("fcntl returned -1");
}

void Process::set_blocking() {
	int flags;
	flags = fcntl(out[0], F_GETFL, 0);
	if(flags == -1) {
		throw std::runtime_error("fcntl returned -1");
	}
	flags = fcntl(out[0], F_SETFL, flags & ~O_NONBLOCK);     
	if(flags == -1) 
		throw std::runtime_error("fcntl returned -1");
}

/* TODO: properly destruct process */
Process::~Process() {
	in.close();
	out.close();
	if(running()) {
		//kill(pid, SIGTERM);
		wait();
		//throw std::runtime_error("Process - messy child shutdown");
	}
}

/* TODO: handles signals and stopped processes and such */
bool Process::running(int &ret_code) {
	int stat_val;
	if(!waitpid(pid, &stat_val, WNOHANG))
		return true;
	ret_code = WEXITSTATUS(stat_val);
	return false;
}
bool Process::running() {
	int dummy;
	return running(dummy);
}
	
/* TODO: handle blocking or incomplete writes */
void Process::write(std::string input) {
	set_blocking();
	auto remaining = input.size();
	while(remaining) {
		auto count = ::write(in[1], input.c_str(), input.size());
		if(count == -1) {
			throw std::runtime_error("Write returned -1 :(");
		}
		else {
			remaining -= count;
		}
	}
}

std::string Process::read_some() {
	set_nonblocking();
	return read();
}

std::string Process::read_all() {
	set_blocking();
	return read();
}

std::string Process::read() {
	std::string res;
	const int block_size = 1024;
	std::vector<char> buff(block_size);
	while(true) {
		auto count = ::read(out[0], buff.data(), block_size);
		if(count == 0) {
			break;
        	}
		else if(count == -1) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				throw std::runtime_error("read returned something unexpected");
		}
		else {
			res.insert(std::end(res), std::begin(buff),
			           std::begin(buff) + count);
			continue;
		}
	}
	return res;
}

void Process::close_input() {
	in.close(1);
}

void Process::wait() {
	int res;
	waitpid(pid, &res, 0);
}
