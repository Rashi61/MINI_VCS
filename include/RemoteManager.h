#pragma once
#include <string>

struct SyncResult {
    bool ok = true;
    int copied = 0;      
    int conflicts = 0;   
    std::string message;
};

class RemoteManager {
public:
    RemoteManager(std::string localDir, std::string remoteDir);

    SyncResult clone();   
    SyncResult push();    
    SyncResult pull();    

    int localCount() const;    
    int remoteCount() const;   
    int aheadCount() const;    
    int behindCount() const;   

private:
    SyncResult copyMissing(const std::string& from, const std::string& to, const char* verb);
    std::string local_, remote_;
};
