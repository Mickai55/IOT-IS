#include <algorithm>

#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>
#include <string.h>
#include <signal.h>

using namespace std;
using namespace Pistache;

struct Profil {
    int lastShowerTemperature;
    int lastMusicVolume;
    bool wasMusicOn;

    Profil(): lastMusicVolume(50), lastShowerTemperature(30), wasMusicOn(false) {}

    Profil(int lastMusicVolume, int lastShowerTemperature, bool wasMusicOn) {
        this->lastMusicVolume = lastMusicVolume;
        this->lastShowerTemperature = lastShowerTemperature;
        this->wasMusicOn = wasMusicOn;
    }
};

std::unordered_map<string, Profil> previouslyConnectedDevices;

void intialize_mocking_data() {
    Profil p1 = Profil(55, 35, false);
    Profil p2 = Profil(85, 25, true);
    Profil p3 = Profil(67, 35, true);
    Profil p4 = Profil(55, 35, true);

    previouslyConnectedDevices["AB123"] = p1; 
    previouslyConnectedDevices["CD123"] = p2; 
    previouslyConnectedDevices["EF123"] = p3; 
    previouslyConnectedDevices["GH123"] = p4; 
}

// General advice: pay atetntion to the namespaces that you use in various contexts. Could prevent headaches.

// This is just a helper function to preety-print the Cookies that one of the enpoints shall receive.
void printCookies(const Http::Request& req) {
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto& c: cookies) {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
}

// Some generic namespace, with a simple function we could use to test the creation of the endpoints.
namespace Generic {

    void handleReady(const Rest::Request&, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "1");
    }

}

// Definition of the MicrowaveEnpoint class 
class ShowerEndpoint {
public:
    explicit ShowerEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr))
    { }

    // Initialization of the server. Additional options can be provided here
    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
            .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        // Server routes are loaded up
        setupRoutes();
    }

    // Server is started threaded.  
    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
    }

    // When signaled server shuts down
    void stop(){
        httpEndpoint->shutdown();
    }

private:
    void setupRoutes() {
        using namespace Rest;
        // Defining various endpoints
        Routes::Get(router, "/turnWaterOn", Routes::bind(&ShowerEndpoint::turnWaterOn, this));
        Routes::Get(router, "/turnWaterOff", Routes::bind(&ShowerEndpoint::turnWaterOff, this));
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&ShowerEndpoint::setSetting, this));
        Routes::Get(router, "/settings/:settingName/", Routes::bind(&ShowerEndpoint::getSetting, this));
    }

    
    void turnWaterOn(const Rest::Request& request, Http::ResponseWriter response) {

        int res = shr.turnWaterOn();
        // Send the response
        if(res == 1){
        response.send(Http::Code::Ok, "Water has been turned on \n");

        }else {
        response.send(Http::Code::Bad_Request, "Cannot turn water on \n");

        }
    }

    void turnWaterOff(const Rest::Request& request, Http::ResponseWriter response) {
      
       
        int res = shr.turnWaterOff();
        // Send the response
        if(res == 1){
        response.send(Http::Code::Ok, "Water has been turned off \n");

        }else {
        response.send(Http::Code::Bad_Request, "Cannot turn water off \n");

        }
    }


    // Endpoint to configure one of the Shower's settings.
    void setSetting(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto settingName = request.param(":settingName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(showerLock);

        
        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        // Setting the shower's setting to value
        int setResponse = shr.set(settingName, val);

        // Sending some confirmation or error response.
        if (setResponse == 1) {
            response.send(Http::Code::Ok, settingName + " was set to " + val + "\n");
         
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + val + "' was not a valid value \n");
        }

    }

    // Setting to get the settings value of one of the configurations of the Shower
    void getSetting(const Rest::Request& request, Http::ResponseWriter response){
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(showerLock);

        string valueSetting = shr.get(settingName);

        if (valueSetting != "") {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting + "\n");
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found\n");
        }
    }

    // Defining the class of the Shower. It should model the entire configuration of the Shower
    class Shower {
    public:
        explicit Shower(){ 
            showerStatus = false; 
            connectedDevice.connected = false; 
            music = false;
            musicVolume = 50;
            waterTemperature.name = "waterTemperature";
            waterTemperature.value = 30;
        }

        // Setting the value for one of the settings. Hardcoded for the defrosting option
        int set(std::string name, std::string value){
            
            if(name == "waterTemperature"){
                waterTemperature.name = name;

                int temp = std::stoi(value);
                if(showerStatus == false || temp < 20 || temp > 59){
                    return 0;
                }
                waterTemperature.value = temp;

                if (connectedDevice.connected) {
                    previouslyConnectedDevices[connectedDevice.name].lastShowerTemperature = temp;
                }

                return 1;
            }

            if(name == "music"){

                if(value == "on"){

                    if(music == true)
                    {return 0;}

                    music = true;

                }else if(value == "off"){

                    if(music == false)
                    {return 0;}

                    music = false;

                    
                }

                if (connectedDevice.connected) {
                    previouslyConnectedDevices[connectedDevice.name].wasMusicOn = music;
                }
                return 1;
            }

            if(name == "musicVolume"){
                if (music == false) {
                    return 0;
                }

                const int volume = std::stoi(value);

                if (volume < 0 || volume > 100) {
                    return 0;
                }

                musicVolume = volume;

                if (connectedDevice.connected) {
                    previouslyConnectedDevices[connectedDevice.name].wasMusicOn = music;
                }

                return 1;
            }

            if(name == "pairDevice"){

                if(value == "disconnect"){

                    if(!connectedDevice.connected){
                        return 0;
                    }
                    
                    connectedDevice.connected = false;
                    connectedDevice.name = "";

                    return 1;
                }
                
                auto profilFromMap = previouslyConnectedDevices.find(value);
                if (profilFromMap != previouslyConnectedDevices.end()) {
                    Profil profil = profilFromMap->second;
                    music = profil.wasMusicOn;
                    musicVolume = profil.lastMusicVolume;
                    waterTemperature.name = "waterTemperature";
                    waterTemperature.value = profil.lastShowerTemperature;
                } else {
                    previouslyConnectedDevices[value] = Profil();
                }
            
                connectedDevice.name = value;
                connectedDevice.connected = true;

                return 1;
                 
             }

            return 0;
        }

        int turnWaterOn(){

            if(showerStatus == true){
                return 0;
            }

            showerStatus = true;
            return 1;
        }


        int turnWaterOff(){

            if(showerStatus == false){
                return 0;
            }

            showerStatus = false;
            return 1;
        }




        // Getter
        string get(std::string name){


            if (name == "waterTemperature"){
            if(showerStatus == false){
                    return "";
                }   
                return std::to_string(waterTemperature.value);
            }

            if(name == "music"){
                return music ? "turned on" : "turned off";
            }

            if(name == "pairDevice"){
                return connectedDevice.connected ? "connected to " + connectedDevice.name : "not connected";
            }

            if(name == "musicVolume"){
                return std::to_string(musicVolume);
            }

            return "";

        }

    private:
        // Defining and instantiating settings.
        struct intSettings{
            std::string name;
            int value;
        }waterTemperature;

        struct stringSettings{
            std::string name;
            bool connected;
        }connectedDevice;

        bool music;

        bool showerStatus;

        int musicVolume;
    };

    // Create the lock which prevents concurrent editing of the same variable
    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock showerLock;

    // Instance of the shower model
    Shower shr;

    // Defining the httpEndpoint and a router.
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

int main(int argc, char *argv[]) {

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
            || sigaddset(&signals, SIGTERM) != 0
            || sigaddset(&signals, SIGINT) != 0
            || sigaddset(&signals, SIGHUP) != 0
            || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    intialize_mocking_data();

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // Instance of the class that defines what the server can do.
    ShowerEndpoint stats(addr);

    // Initialize and start the server
    stats.init(thr);
    stats.start();


    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0)
    {
        std::cout << "received signal " << signal << std::endl;
    }
    else
    {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}