#include <random>
#include <map>
#include <string>
#include <regex>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include "rapunzel/fcgi_connection_manager.h"
#include "regex_tree.h"
#include "process.h"

void handle_request(fcgi::request r) {
    auto query = decode_querystring(r.parameter("QUERY_STRING"));
    std::string regex = query["regex"];
    std::string mode  = query["mode"];
    std::string format = query["format"];
    regex_tree tree(regex);
    auto d = tree.construct_dfa();
    
    //draw either a tree or a DFA as text or png 
    //depending on the query string        
    std::string input;
    if(mode == "dfa") {        
        input = d.graph();
    }
    else { //default
        input = tree.graph();
    }
    std::string output;
    if(format == "text" || mode == "dfa" && d.size() > 32) {
        output = input;
        r << "Content-type: text/html\r\n\r\n<!DOCTYPE html>";
        if(mode == "dfa" && d.size() > 32)
            r << "That graph is way too big D: draw it yourself!<br>";
        r << "<pre>" << output << "</pre>";
    } else {
        Process p("dot", {"-Tpng"});
        p.write(input);
        p.close_input();
        output = p.read_all();
        r << "Content-type: image/png\r\n\r\n";
        r << output;
    }

    
}

int main() {
	fcgi::connection_manager fcgi;
    while(true) {
        fcgi::request r = fcgi.get_request();
        std::thread t(handle_request, std::move(r));
        t.detach();
    }
}
